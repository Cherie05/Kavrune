// SPDX-License-Identifier: MIT
#include "kavrune/storage.hpp"
#include <gio/gunixfdlist.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <linux/fs.h>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace kavrune {
namespace {
namespace fs = std::filesystem;
constexpr const char* service = "org.freedesktop.UDisks2";
constexpr const char* base = "/org/freedesktop/UDisks2";
constexpr const char* block_iface = "org.freedesktop.UDisks2.Block";
constexpr const char* drive_iface = "org.freedesktop.UDisks2.Drive";
constexpr const char* partition_iface = "org.freedesktop.UDisks2.Partition";
constexpr const char* filesystem_iface = "org.freedesktop.UDisks2.Filesystem";
constexpr const char* swap_iface = "org.freedesktop.UDisks2.Swapspace";
constexpr const char* table_iface = "org.freedesktop.UDisks2.PartitionTable";
using Variant = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
Variant variant(GVariant* value) { return Variant(value, &g_variant_unref); }
using Objects = std::map<std::string, Variant>;

[[noreturn]] void dbus_error(const std::string& action, GError* error) {
    std::string message = error ? error->message : "No response from the storage service";
    if (error) g_error_free(error);
    throw Error(action + ": " + message);
}
Variant call(GDBusConnection* connection, const std::string& object, const char* interface,
             const char* method, GVariant* parameters, const GVariantType* reply_type,
             int timeout = 30000) {
    GError* error = nullptr;
    auto result = g_dbus_connection_call_sync(connection, service, object.c_str(), interface,
        method, parameters, reply_type, G_DBUS_CALL_FLAGS_NONE, timeout, nullptr, &error);
    if (!result) dbus_error(method, error);
    return variant(result);
}
Objects snapshot(GDBusConnection* connection) {
    auto reply = call(connection, base, "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
                      nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"));
    auto entries = variant(g_variant_get_child_value(reply.get(), 0));
    GVariantIter iterator;
    g_variant_iter_init(&iterator, entries.get());
    const gchar* path = nullptr;
    GVariant* interfaces = nullptr;
    Objects objects;
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}", &path, &interfaces))
        objects.emplace(path, variant(interfaces));
    return objects;
}
Variant properties(const Objects& objects, const std::string& path, const char* interface) {
    auto object = objects.find(path);
    if (object == objects.end()) return variant(nullptr);
    return variant(g_variant_lookup_value(object->second.get(), interface, G_VARIANT_TYPE("a{sv}")));
}
std::string string_property(GVariant* properties, const char* key, bool object_path = false) {
    if (!properties) return {};
    const gchar* value = nullptr;
    if (!g_variant_lookup(properties, key, object_path ? "&o" : "&s", &value)) return {};
    return value;
}
bool boolean_property(GVariant* properties, const char* key, bool fallback = false) {
    gboolean value = fallback;
    if (properties) g_variant_lookup(properties, key, "b", &value);
    return value;
}
uint64_t integer_property(GVariant* properties, const char* key) {
    guint64 value = 0;
    if (properties) g_variant_lookup(properties, key, "t", &value);
    return value;
}
std::string bytestring(GVariant* value) {
    if (!value || !g_variant_is_of_type(value, G_VARIANT_TYPE("ay"))) return {};
    gsize size = 0;
    const auto* bytes = static_cast<const char*>(g_variant_get_fixed_array(value, &size, 1));
    if (size == 0 || !bytes || bytes[size - 1] != '\0') return {};
    if (std::memchr(bytes, '\0', size - 1)) return {};
    return std::string(bytes, size - 1);
}
std::string device_path(GVariant* properties) {
    auto value = variant(g_variant_lookup_value(properties, "Device", G_VARIANT_TYPE("ay")));
    return bytestring(value.get());
}
std::vector<std::string> mountpoints(GVariant* properties) {
    std::vector<std::string> paths;
    if (!properties) return paths;
    auto value = variant(g_variant_lookup_value(properties, "MountPoints", G_VARIANT_TYPE("aay")));
    if (!value) return paths;
    for (gsize index = 0; index < g_variant_n_children(value.get()); ++index) {
        auto entry = variant(g_variant_get_child_value(value.get(), index));
        auto path = bytestring(entry.get());
        if (!path.empty()) paths.push_back(path);
    }
    return paths;
}
std::string unescape_mount(const std::string& encoded) {
    std::string result;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '\\' && i + 3 < encoded.size() &&
            encoded[i + 1] >= '0' && encoded[i + 1] <= '7' &&
            encoded[i + 2] >= '0' && encoded[i + 2] <= '7' &&
            encoded[i + 3] >= '0' && encoded[i + 3] <= '7') {
            const unsigned value = (encoded[i + 1] - '0') * 64U +
                (encoded[i + 2] - '0') * 8U + (encoded[i + 3] - '0');
            if (value == 0 || value > 255) throw Error("Invalid kernel mount path");
            result += static_cast<char>(value);
            i += 3;
        } else result += encoded[i];
    }
    return result;
}
uint64_t parse_device_number(const std::string& text) {
    const auto colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 == text.size() ||
        text.find(':', colon + 1) != std::string::npos ||
        std::any_of(text.begin(), text.end(), [](char c) { return c != ':' && (c < '0' || c > '9'); }))
        throw Error("Invalid kernel device number");
    unsigned major_number = 0, minor_number = 0;
    char separator = 0;
    std::istringstream input(text);
    if (!(input >> major_number >> separator >> minor_number) || separator != ':' ||
        input.peek() != std::char_traits<char>::eof())
        throw Error("Invalid kernel device number");
    auto number = makedev(major_number, minor_number);
    if (major(number) != major_number || minor(number) != minor_number)
        throw Error("Kernel device number is out of range");
    return static_cast<uint64_t>(number);
}
std::string read_line(const fs::path& path) {
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input, line)) throw Error("Cannot read storage topology: " + path.string());
    return line;
}
struct Mount { uint64_t number; std::string path, filesystem, source; };
std::vector<Mount> parse_mounts(std::istream& input) {
    std::vector<Mount> mounts;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string id, parent, number, root, path, options, token, filesystem, source;
        if (!(fields >> id >> parent >> number >> root >> path >> options))
            throw Error("Cannot interpret system mount information");
        bool separator = false;
        while (fields >> token) if (token == "-") { separator = true; break; }
        if (!separator || !(fields >> filesystem >> source)) throw Error("Incomplete system mount information");
        mounts.push_back({parse_device_number(number), unescape_mount(path), filesystem, unescape_mount(source)});
    }
    if (!input.eof()) throw Error("Cannot finish reading system mount information");
    if (mounts.empty()) throw Error("No system mount information is available");
    return mounts;
}
std::vector<Mount> read_mounts() {
    std::ifstream input("/proc/self/mountinfo");
    if (!input) throw Error("Cannot read the current system mounts");
    return parse_mounts(input);
}
fs::path sysfs_for(uint64_t number) {
    const auto native = static_cast<dev_t>(number);
    return fs::canonical("/sys/dev/block/" + std::to_string(major(native)) + ":" + std::to_string(minor(native)));
}
std::set<uint64_t> mount_ancestors(const Mount& mount);
std::set<uint64_t> file_ancestors(const std::string& path);
void resolve_ancestors(uint64_t number, std::set<uint64_t>& result, std::set<uint64_t>& visiting, unsigned depth) {
    if (depth > 64 || !visiting.insert(number).second) throw Error("Storage topology is cyclic or too deep");
    auto node = sysfs_for(number);
    if (fs::exists(node / "partition")) {
        resolve_ancestors(parse_device_number(read_line(node.parent_path() / "dev")), result, visiting, depth + 1);
    } else {
        const auto slaves = node / "slaves";
        bool has_slaves = false;
        if (!fs::is_directory(slaves)) throw Error("Cannot inspect storage dependencies");
        for (const auto& entry : fs::directory_iterator(slaves)) {
            has_slaves = true;
            resolve_ancestors(parse_device_number(read_line(entry.path() / "dev")), result, visiting, depth + 1);
        }
        if (!has_slaves) {
            if (fs::exists(node / "loop" / "backing_file")) {
                auto backing = read_line(node / "loop" / "backing_file");
                if (backing.empty() || backing.front() != '/') throw Error("Cannot identify a loop device backing file");
                auto backing_devices = file_ancestors(unescape_mount(backing));
                result.insert(backing_devices.begin(), backing_devices.end());
            } else if (node.string().find("/devices/virtual/") != std::string::npos) {
                if (node.filename().string().starts_with("zram")) {
                    // zram swap has no disk backing; it cannot be selected as a target.
                } else throw Error("A virtual block device has unresolved physical storage");
            } else result.insert(number);
        }
    }
    visiting.erase(number);
}
std::set<uint64_t> ancestors(uint64_t number) {
    std::set<uint64_t> result, visiting;
    resolve_ancestors(number, result, visiting, 0);
    return result;
}
std::set<uint64_t> btrfs_ancestors(const Mount& mount) {
    // Btrfs mountinfo can expose an anonymous device number. Resolve every member
    // through the kernel filesystem registry instead of trusting only mount.source.
    std::string source = mount.source;
    if (auto suffix = source.find('['); suffix != std::string::npos) source.resize(suffix);
    struct stat st{};
    if (stat(source.c_str(), &st) != 0 || !S_ISBLK(st.st_mode))
        throw Error("Cannot resolve every device in a Btrfs filesystem");
    const fs::path registry("/sys/fs/btrfs");
    if (!fs::is_directory(registry)) throw Error("The Btrfs device registry is unavailable");
    for (const auto& filesystem : fs::directory_iterator(registry)) {
        const auto devices = filesystem.path() / "devices";
        if (!fs::is_directory(devices)) continue;
        std::set<uint64_t> members;
        for (const auto& entry : fs::directory_iterator(devices))
            members.insert(parse_device_number(read_line(entry.path() / "dev")));
        if (!members.contains(static_cast<uint64_t>(st.st_rdev))) continue;
        std::set<uint64_t> result;
        for (const auto member : members) {
            const auto backing = ancestors(member);
            result.insert(backing.begin(), backing.end());
        }
        return result;
    }
    throw Error("Cannot match the Btrfs filesystem to its complete device set");
}
std::set<uint64_t> mount_ancestors(const Mount& mount) {
    if (mount.filesystem == "btrfs") return btrfs_ancestors(mount);
    if (mount.filesystem == "fuseblk") {
        struct stat st{};
        if (stat(mount.source.c_str(), &st) != 0 || !S_ISBLK(st.st_mode))
            throw Error("The FUSE block filesystem has no verifiable device source");
        return ancestors(static_cast<uint64_t>(st.st_rdev));
    }
    if (mount.filesystem == "overlay" || mount.filesystem.starts_with("fuse"))
        throw Error("This filesystem does not expose verifiable physical disk ancestry");
    if (major(static_cast<dev_t>(mount.number)) == 0)
        throw Error("The filesystem has no resolvable block device");
    return ancestors(mount.number);
}
bool within(const std::string& path, const std::string& parent) {
    return path == parent || (path.starts_with(parent) && (parent == "/" || path[parent.size()] == '/'));
}
std::set<uint64_t> file_ancestors(const std::string& path) {
    // A file-backed loop chain must share a depth bound across filesystem
    // resolutions, which may otherwise re-enter this helper recursively.
    static thread_local unsigned resolution_depth = 0;
    struct DepthGuard {
        unsigned& depth;
        explicit DepthGuard(unsigned& value) : depth(value) {
            if (depth >= 64) throw Error("File-backed storage topology is cyclic or too deep");
            ++depth;
        }
        ~DepthGuard() { --depth; }
    } guard(resolution_depth);
    const auto canonical = fs::canonical(path).string();
    struct stat st{};
    if (stat(canonical.c_str(), &st) != 0) throw Error("Cannot inspect the filesystem containing the file");
    if (S_ISBLK(st.st_mode)) return ancestors(static_cast<uint64_t>(st.st_rdev));
    const auto mounts = read_mounts();
    const Mount* best = nullptr;
    for (const auto& mount : mounts)
        if (within(canonical, mount.path) && (!best || mount.path.size() > best->path.size())) best = &mount;
    if (!best) throw Error("Cannot identify the filesystem containing the file");
    if (best->filesystem == "tmpfs" || best->filesystem == "ramfs") return {};
    return mount_ancestors(*best);
}
bool pseudo_filesystem(const std::string& type) {
    static const std::set<std::string> pseudo = {"proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "ramfs",
        "cgroup", "cgroup2", "securityfs", "pstore", "debugfs", "tracefs", "configfs", "mqueue",
        "hugetlbfs", "fusectl", "rpc_pipefs", "binfmt_misc", "efivarfs", "bpf", "nsfs", "autofs"};
    return pseudo.contains(type);
}
bool resolvable_mount_candidate(const Mount& mount) {
    return major(static_cast<dev_t>(mount.number)) != 0 || mount.filesystem == "btrfs" || mount.filesystem == "fuseblk";
}
std::set<size_t> critical_mounts(const std::vector<Mount>& mounts, const std::vector<std::string>& directories) {
    std::set<size_t> result;
    for (const auto& directory : directories) {
        size_t length = 0;
        std::set<size_t> selected;
        for (size_t index = 0; index < mounts.size(); ++index) {
            if (!within(directory, mounts[index].path) || mounts[index].path.size() < length) continue;
            if (mounts[index].path.size() > length) selected.clear();
            length = mounts[index].path.size();
            selected.insert(index);
        }
        if (selected.empty()) throw Error("Cannot identify a critical filesystem mount");
        // Stacked mounts can share a mountpoint. Protect every candidate rather
        // than assuming the mountinfo record order identifies the visible one.
        result.insert(selected.begin(), selected.end());
    }
    return result;
}
bool should_protect_mount(const Mount& mount, bool critical) {
    if (critical) return !pseudo_filesystem(mount.filesystem) || mount.path == "/";
    if (pseudo_filesystem(mount.filesystem)) return false;
    if (within(mount.path, "/media") || within(mount.path, "/run/media")) return false;
    return resolvable_mount_candidate(mount);
}
struct Protection {
    std::set<uint64_t> system, swap;
    std::string uncertainty;
};
Protection protections() {
    Protection result;
    try {
        auto mounts = read_mounts();
        std::vector<std::string> directories;
        for (const auto* path : {"/", "/home", "/boot", "/boot/efi", "/usr", "/var", "/etc", "/opt", "/srv", "/root"}) {
            std::error_code error;
            if (!fs::exists(path, error)) {
                if (error) throw Error("Cannot inspect a critical system directory");
                continue;
            }
            directories.push_back(fs::canonical(path).string());
        }
        const auto critical = critical_mounts(mounts, directories);
        bool found_root = false;
        for (size_t index = 0; index < mounts.size(); ++index) {
            const auto& mount = mounts[index];
            if (mount.path == "/") found_root = true;
            if (!should_protect_mount(mount, critical.contains(index))) continue;
            const auto backing = mount_ancestors(mount);
            result.system.insert(backing.begin(), backing.end());
        }
        if (!found_root) throw Error("The root filesystem cannot be identified");
        std::ifstream swaps("/proc/swaps");
        std::string line;
        if (!swaps || !std::getline(swaps, line)) throw Error("Cannot read active swap information");
        while (std::getline(swaps, line)) {
            std::istringstream fields(line);
            std::string path;
            if (!(fields >> path)) throw Error("Cannot interpret active swap information");
            const auto backing = file_ancestors(unescape_mount(path));
            result.swap.insert(backing.begin(), backing.end());
        }
        if (!swaps.eof()) throw Error("Cannot finish reading active swap information");
    } catch (const std::exception& error) {
        result.uncertainty = std::string("System disk protection cannot be verified: ") + error.what();
    }
    return result;
}
bool has_holders(uint64_t number) {
    const auto node = sysfs_for(number);
    const auto holders = node / "holders";
    if (!fs::is_directory(holders)) throw Error("Cannot inspect active device holders");
    if (fs::directory_iterator(holders) != fs::directory_iterator()) return true;
    for (const auto& child : fs::directory_iterator(node)) {
        if (!fs::is_directory(child.path()) || !fs::exists(child.path() / "partition")) continue;
        const auto child_holders = child.path() / "holders";
        if (!fs::is_directory(child_holders)) throw Error("Cannot inspect active partition holders");
        if (fs::directory_iterator(child_holders) != fs::directory_iterator()) return true;
    }
    return false;
}
GVariant* options() {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    // Authorization is handled by the desktop's Polkit agent. The application
    // stays unprivileged and never installs a permissive authorization rule.
    return g_variant_builder_end(&builder);
}
void require_unmounted(const Device& device) {
    if (!device.mount_points.empty()) throw Error("The target still contains mounted filesystems");
    for (const auto& mount : read_mounts()) {
        if (pseudo_filesystem(mount.filesystem) || !resolvable_mount_candidate(mount)) continue;
        try {
            if (mount_ancestors(mount).contains(device.device_number))
                throw Error("The target still backs a mounted filesystem");
        } catch (const Error& error) {
            // Unresolvable critical mounts were already rejected by revalidate.
            // Visible block-device media mounts must also be resolved here.
            throw Error(std::string("Cannot prove that the target is unmounted: ") + error.what());
        }
    }
}
void check_format_options(const FormatOptions& input, uint64_t size) {
    if (input.scheme != "gpt" && input.scheme != "mbr") throw Error("Choose GPT or MBR partitioning");
    if (input.filesystem != "vfat" && input.filesystem != "exfat" && input.filesystem != "ext4")
        throw Error("Choose FAT32, exFAT, or ext4");
    if (size < 64ULL * 1024 * 1024) throw Error("Formatting requires a drive of at least 64 MiB");
    if (input.scheme == "mbr" && size > 2ULL * 1024 * 1024 * 1024 * 1024)
        throw Error("Use GPT for drives larger than 2 TiB");
    const size_t maximum = input.filesystem == "ext4" ? 16 : input.filesystem == "vfat" ? 11 : 15;
    if (input.label.empty() || input.label.size() > maximum)
        throw Error("The label must contain 1–" + std::to_string(maximum) + " ASCII letters, digits, spaces, underscores, or hyphens");
    for (const auto character : input.label) {
        if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') || character == ' ' || character == '_' || character == '-'))
            throw Error("Labels accept only ASCII letters, digits, spaces, underscores, and hyphens");
    }
    if (input.label.front() == ' ' || input.label.back() == ' ')
        throw Error("The label cannot start or end with a space");
}
} // namespace

void validate_format_options(const FormatOptions& input, uint64_t size) {
    check_format_options(input, size);
}
bool same_identity(const Device& expected, const Device& actual) {
    return !expected.object_path.empty() && !expected.drive_path.empty() &&
        !expected.path.empty() && !expected.sysfs_path.empty() && !expected.serial.empty() && !expected.id.empty() &&
        expected.size > 0 && expected.device_number > 0 && expected.time_detected > 0 &&
        expected.object_path == actual.object_path && expected.drive_path == actual.drive_path &&
        expected.path == actual.path && expected.sysfs_path == actual.sysfs_path &&
        expected.serial == actual.serial && expected.id == actual.id && expected.size == actual.size &&
        expected.device_number == actual.device_number && expected.connection == actual.connection &&
        expected.time_detected == actual.time_detected && expected.time_media_detected == actual.time_media_detected &&
        expected.vendor == actual.vendor && expected.model == actual.model &&
        expected.removable == actual.removable && expected.read_only == actual.read_only &&
        expected.hint_system == actual.hint_system;
}
std::string device_safety_reason(const Device& device, bool system_backing, bool holders, bool swap) {
    if (system_backing) return "Protected: this drive backs a system filesystem";
    if (swap) return "Protected: this drive contains active swap";
    if (holders) return "Protected: this drive is used by an active encrypted, RAID, or logical device";
    if (device.hint_system) return "Protected: UDisks identifies this as a system drive";
    if (device.read_only) return "Protected: this drive is read-only";
    if (!device.removable || (device.connection != "usb" && device.connection != "ieee1394" && device.connection != "sdio"))
        return "Protected: only removable USB, FireWire, and SD drives are supported";
    if (device.object_path.empty() || device.drive_path.empty() || device.drive_path == "/" ||
        device.path.empty() || device.sysfs_path.empty() || device.device_number == 0 || device.size == 0 ||
        device.serial.empty() || device.id.empty() || device.time_detected == 0)
        return "Protected: a stable hardware identity and capacity could not be verified";
    return {};
}
Storage::Storage() {
    GError* error = nullptr;
    connection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!connection_) dbus_error("Connecting to the system storage service", error);
}
Storage::~Storage() { if (connection_) g_object_unref(connection_); }
std::vector<Device> Storage::devices() {
    const auto objects = snapshot(connection_);
    const auto protection = protections();
    std::vector<Device> result;
    for (const auto& [object_path, ignored] : objects) {
        (void)ignored;
        auto block = properties(objects, object_path, block_iface);
        if (!block || properties(objects, object_path, partition_iface)) continue;
        const auto drive_path = string_property(block.get(), "Drive", true);
        auto drive = properties(objects, drive_path, drive_iface);
        if (!drive || boolean_property(drive.get(), "Optical") ||
            !boolean_property(drive.get(), "MediaAvailable")) continue;
        Device device;
        device.object_path = object_path;
        device.drive_path = drive_path;
        device.path = device_path(block.get());
        device.vendor = string_property(drive.get(), "Vendor");
        device.model = string_property(drive.get(), "Model");
        device.serial = string_property(drive.get(), "Serial");
        // Drive.Id describes hardware, unlike Block.Id on removable media, which
        // can derive from the current filesystem and change after a rewrite.
        device.id = string_property(drive.get(), "Id");
        device.connection = string_property(drive.get(), "ConnectionBus");
        device.time_detected = integer_property(drive.get(), "TimeDetected");
        device.time_media_detected = integer_property(drive.get(), "TimeMediaDetected");
        device.removable = boolean_property(drive.get(), "Removable");
        device.read_only = boolean_property(block.get(), "ReadOnly", true);
        device.hint_system = boolean_property(block.get(), "HintSystem", true);
        device.size = integer_property(block.get(), "Size");
        device.device_number = integer_property(block.get(), "DeviceNumber");
        auto filesystem = properties(objects, object_path, filesystem_iface);
        device.mount_points = mountpoints(filesystem.get());
        bool swap = protection.swap.contains(device.device_number);
        bool configured = false;
        auto inspect_child = [&](const std::string& path) {
            auto child = properties(objects, path, block_iface);
            if (child) {
                auto configuration = variant(g_variant_lookup_value(child.get(), "Configuration", G_VARIANT_TYPE("a(sa{sv})")));
                if (configuration && g_variant_n_children(configuration.get()) > 0) configured = true;
                if (string_property(child.get(), "MDRaid", true) != "/" ||
                    string_property(child.get(), "MDRaidMember", true) != "/") configured = true;
            }
            auto child_swap = properties(objects, path, swap_iface);
            swap = swap || boolean_property(child_swap.get(), "Active");
        };
        inspect_child(object_path);
        for (const auto& [path, child_ignored] : objects) {
            (void)child_ignored;
            auto partition = properties(objects, path, partition_iface);
            if (!partition || string_property(partition.get(), "Table", true) != object_path) continue;
            device.partitions.push_back(path);
            auto child_filesystem = properties(objects, path, filesystem_iface);
            const auto points = mountpoints(child_filesystem.get());
            device.mount_points.insert(device.mount_points.end(), points.begin(), points.end());
            inspect_child(path);
        }
        try {
            device.sysfs_path = sysfs_for(device.device_number).string();
            struct stat st{};
            if (stat(device.path.c_str(), &st) != 0 || !S_ISBLK(st.st_mode) ||
                static_cast<uint64_t>(st.st_rdev) != device.device_number)
                throw Error("The device node does not match the kernel hardware identity");
            if (fs::exists(fs::path(device.sysfs_path) / "partition"))
                throw Error("The target is a partition rather than a whole drive");
            device.safety_reason = device_safety_reason(device,
                protection.system.contains(device.device_number), has_holders(device.device_number), swap);
            if (device.safety_reason.empty() && configured)
                device.safety_reason = "Protected: this drive has system configuration or RAID membership";
            if (device.safety_reason.empty() && !protection.uncertainty.empty())
                device.safety_reason = protection.uncertainty;
        } catch (const std::exception& error) {
            device.safety_reason = std::string("Protected: ") + error.what();
        }
        result.push_back(std::move(device));
    }
    std::sort(result.begin(), result.end(), [](const Device& left, const Device& right) { return left.path < right.path; });
    return result;
}
Device Storage::revalidate(const Device& expected) {
    for (auto& device : devices()) {
        if (device.object_path != expected.object_path) continue;
        if (!same_identity(expected, device)) throw Error("The drive identity changed. Refresh and select it again");
        if (!device.eligible()) throw Error(device.safety_reason);
        return device;
    }
    throw Error("The selected drive is no longer connected");
}
bool Storage::source_on_device(const std::string& source_path, const Device& target) {
    const auto actual = revalidate(target);
    try { return file_ancestors(source_path).contains(actual.device_number); }
    catch (const std::exception& error) {
        throw Error(std::string("Cannot prove the file is stored away from the target drive: ") + error.what());
    }
}
void Storage::unmount(const Device& expected) {
    auto actual = revalidate(expected);
    auto paths = actual.partitions;
    paths.push_back(actual.object_path);
    const auto initial_objects = snapshot(connection_);
    auto deepest_mount = [&](const std::string& path) {
        auto filesystem = properties(initial_objects, path, filesystem_iface);
        size_t depth = 0;
        for (const auto& point : mountpoints(filesystem.get())) depth = std::max(depth, point.size());
        return depth;
    };
    std::sort(paths.begin(), paths.end(), [&](const std::string& left, const std::string& right) {
        return deepest_mount(left) > deepest_mount(right);
    });
    for (const auto& path : paths) {
        actual = revalidate(expected);
        // A partition path can be reused after hotplug; bind it to this drive
        // again in a fresh snapshot immediately before asking for unmount.
        const auto objects = snapshot(connection_);
        auto block = properties(objects, path, block_iface);
        if (!block) continue;
        if (string_property(block.get(), "Drive", true) != actual.drive_path)
            throw Error("A target partition changed its hardware identity");
        if (path != actual.object_path) {
            auto partition = properties(objects, path, partition_iface);
            if (!partition || string_property(partition.get(), "Table", true) != actual.object_path)
                throw Error("A target partition changed its parent drive");
        }
        auto filesystem = properties(objects, path, filesystem_iface);
        if (mountpoints(filesystem.get()).empty()) continue;
        call(connection_, path, filesystem_iface, "Unmount", g_variant_new("(@a{sv})", options()), G_VARIANT_TYPE("()"));
    }
    require_unmounted(revalidate(expected));
}
int Storage::open_device(const Device& expected, bool writable) {
    const auto actual = revalidate(expected);
    require_unmounted(actual);
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "flags", g_variant_new_int32(O_EXCL | O_CLOEXEC));
    GError* error = nullptr;
    GUnixFDList* list = nullptr;
    auto reply = variant(g_dbus_connection_call_with_unix_fd_list_sync(connection_, service,
        actual.object_path.c_str(), block_iface, "OpenDevice",
        g_variant_new("(s@a{sv})", writable ? "rw" : "r", g_variant_builder_end(&builder)),
        G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, 120000, nullptr, &list, nullptr, &error));
    if (!reply) { if (list) g_object_unref(list); dbus_error("Opening the selected drive", error); }
    gint index = -1;
    g_variant_get(reply.get(), "(h)", &index);
    const int fd = list ? g_unix_fd_list_get(list, index, &error) : -1;
    if (list) g_object_unref(list);
    if (fd < 0) dbus_error("Receiving the storage file descriptor", error);
    try {
        if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) throw Error("Cannot protect the storage file descriptor");
        struct stat st{};
        uint64_t capacity = 0;
        int read_only = 0;
        if (fstat(fd, &st) != 0 || !S_ISBLK(st.st_mode) ||
            static_cast<uint64_t>(st.st_rdev) != actual.device_number)
            throw Error("The opened device does not match the confirmed drive");
        if (ioctl(fd, BLKGETSIZE64, &capacity) != 0 || capacity != actual.size)
            throw Error("The opened drive capacity changed");
        if (ioctl(fd, BLKROGET, &read_only) != 0 || (writable && read_only))
            throw Error("The drive's write protection cannot be verified");
        require_unmounted(revalidate(expected));
        return fd;
    } catch (...) { close(fd); throw; }
}
void Storage::rescan(const Device& expected) {
    const auto actual = revalidate(expected);
    call(connection_, actual.object_path, block_iface, "Rescan", g_variant_new("(@a{sv})", options()), G_VARIANT_TYPE("()"));
}
void Storage::format(const Device& expected, const FormatOptions& input, const ProgressFn& progress, const std::atomic_bool& cancel) {
    validate_format_options(input, expected.size);
    if (cancel.load()) throw Cancelled();
    // Check missing filesystem tools before changing the target at all.
    const auto capability = call(connection_, std::string(base) + "/Manager", "org.freedesktop.UDisks2.Manager",
        "CanFormat", g_variant_new("(s)", input.filesystem.c_str()), G_VARIANT_TYPE("((bs))"));
    gboolean available = FALSE;
    const gchar* utility = nullptr;
    g_variant_get(capability.get(), "((b&s))", &available, &utility);
    if (!available) throw Error("Formatting is unavailable. Install the required filesystem utility: " + std::string(utility ? utility : "unknown"));
    unmount(expected);
    const auto actual = revalidate(expected);
    require_unmounted(actual);
    if (cancel.load()) throw Cancelled();
    if (progress) progress({"Creating partition table", 0, 2, 0,
        "UDisks is formatting the drive. This step must finish before cancellation can take effect."});
    // No tear-down, user-provided mkfs arguments, erase options, fstab changes,
    // or shell text are accepted. UDisks and Polkit authorize each destructive method.
    call(connection_, actual.object_path, block_iface, "Format",
         g_variant_new("(s@a{sv})", input.scheme == "gpt" ? "gpt" : "dos", options()),
         G_VARIANT_TYPE("()"), G_MAXINT);
    if (cancel.load()) throw Cancelled();
    const auto refreshed = revalidate(expected);
    require_unmounted(refreshed);
    if (progress) progress({"Creating " + input.filesystem + " filesystem", 1, 2, 0,
        "The filesystem operation must finish before cancellation can take effect."});
    std::string type;
    if (input.scheme == "gpt") type = input.filesystem == "ext4" ?
        "0FC63DAF-8483-4772-8E79-3D69D8477DE4" : "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7";
    else type = input.filesystem == "ext4" ? "0x83" : input.filesystem == "vfat" ? "0x0c" : "0x07";
    GVariantBuilder format_options;
    g_variant_builder_init(&format_options, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&format_options, "{sv}", "label", g_variant_new_string(input.label.c_str()));
    g_variant_builder_add(&format_options, "{sv}", "update-partition-type", g_variant_new_boolean(TRUE));
    if (input.filesystem == "ext4")
        g_variant_builder_add(&format_options, "{sv}", "take-ownership", g_variant_new_boolean(TRUE));
    if (input.filesystem == "vfat") {
        // This is a fixed option vector, never input from a user. Without -F32,
        // mkfs.fat may silently choose FAT16 for a small drive.
        const gchar* fat32_arguments[] = {"-F", "32", nullptr};
        g_variant_builder_add(&format_options, "{sv}", "mkfs-args", g_variant_new_strv(fat32_arguments, -1));
    }
    const auto reply = call(connection_, refreshed.object_path, table_iface, "CreatePartitionAndFormat",
        g_variant_new("(ttss@a{sv}s@a{sv})", static_cast<guint64>(1024 * 1024), static_cast<guint64>(0),
            type.c_str(), input.scheme == "gpt" ? input.label.c_str() : "", options(),
            input.filesystem.c_str(), g_variant_builder_end(&format_options)), G_VARIANT_TYPE("(o)"), G_MAXINT);
    (void)reply;
    if (cancel.load()) throw Cancelled();
    rescan(expected);
    if (progress) progress({"Formatting complete", 2, 2, 0, "The drive contains one data partition. Formatting does not create bootable media."});
}
} // namespace kavrune
