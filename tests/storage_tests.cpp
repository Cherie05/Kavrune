// SPDX-License-Identifier: MIT
// Including the implementation lets fixtures exercise private parsers without
// exporting Linux topology internals as an application API. This target links
// the core library and GIO, not kavrune-storage. No Storage is instantiated.
#include "../src/storage.cpp"
#include <iostream>

namespace {
unsigned checks = 0;
void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error("Failed: " + message);
}
template<typename Function> void rejected(Function function, const std::string& message) {
    bool failed = false;
    try { function(); } catch (const kavrune::Error&) { failed = true; }
    check(failed, message);
}
kavrune::Device usb() {
    kavrune::Device device;
    device.object_path = "/org/freedesktop/UDisks2/block_devices/sdb";
    device.drive_path = "/org/freedesktop/UDisks2/drives/Test_USB_SERIAL";
    device.path = "/dev/sdb";
    device.vendor = "Example";
    device.model = "Test USB";
    device.serial = "SERIAL-EXAMPLE";
    device.id = "Example_Test_USB_SERIAL";
    device.connection = "usb";
    device.sysfs_path = "/sys/devices/pci/usb1/block/sdb";
    device.size = 16ULL * 1024 * 1024 * 1024;
    device.device_number = static_cast<uint64_t>(makedev(8, 16));
    device.time_detected = 1000000;
    device.time_media_detected = 1000000;
    device.removable = true;
    return device;
}
void identity_tests() {
    using namespace kavrune;
    auto expected = usb();
    check(same_identity(expected, expected), "the confirmed fingerprint matches");
    for (auto member : {&Device::object_path, &Device::drive_path, &Device::path, &Device::vendor,
                        &Device::model, &Device::serial, &Device::id, &Device::connection, &Device::sysfs_path}) {
        auto actual = expected;
        actual.*member += "-changed";
        check(!same_identity(expected, actual), "every identity string is checked");
    }
    for (auto member : {&Device::size, &Device::device_number, &Device::time_detected, &Device::time_media_detected}) {
        auto actual = expected;
        ++(actual.*member);
        check(!same_identity(expected, actual), "capacity, dev_t, and hotplug generation are checked");
    }
    for (auto member : {&Device::removable, &Device::read_only, &Device::hint_system}) {
        auto actual = expected;
        actual.*member = !(actual.*member);
        check(!same_identity(expected, actual), "changed safety attributes are checked");
    }
    auto actual = expected;
    actual.mount_points = {"/media/user/new-mount"};
    actual.partitions = {"/org/freedesktop/UDisks2/block_devices/sdb1"};
    actual.safety_reason = "changed runtime state";
    check(same_identity(expected, actual), "media layout changes do not impersonate hardware changes");
    for (auto member : {&Device::object_path, &Device::drive_path, &Device::path, &Device::serial, &Device::id, &Device::sysfs_path}) {
        actual = expected;
        actual.*member = "";
        check(!same_identity(actual, actual), "missing identity is never self-validating");
    }
    for (auto member : {&Device::size, &Device::device_number, &Device::time_detected}) {
        actual = expected;
        actual.*member = 0;
        check(!same_identity(actual, actual), "zero capacity/device/generation is refused");
    }
}
void eligibility_tests() {
    using namespace kavrune;
    auto device = usb();
    check(device_safety_reason(device, false, false, false).empty(), "known unused removable USB is eligible");
    check(!device_safety_reason(device, true, false, false).empty(), "a removable system USB is protected");
    check(!device_safety_reason(device, false, true, false).empty(), "active layered holders protect a disk");
    check(!device_safety_reason(device, false, false, true).empty(), "active swap protects a disk");
    for (auto connection : {"ata", "nvme", "", "unknown"}) {
        device = usb(); device.connection = connection;
        check(!device_safety_reason(device, false, false, false).empty(), "internal or unknown buses cannot be selected");
    }
    for (auto connection : {"usb", "sdio", "ieee1394"}) {
        device = usb(); device.connection = connection;
        check(device_safety_reason(device, false, false, false).empty(), "supported removable buses are eligible");
    }
    device = usb(); device.removable = false;
    check(!device_safety_reason(device, false, false, false).empty(), "non-removable USB disks are denied");
    device = usb(); device.read_only = true;
    check(!device_safety_reason(device, false, false, false).empty(), "read-only disks cannot be destructive targets");
    device = usb(); device.hint_system = true;
    check(!device_safety_reason(device, false, false, false).empty(), "UDisks system hint is honored");
    device = usb(); device.serial.clear();
    check(!device_safety_reason(device, false, false, false).empty(), "unknown serial is denied");
    device = usb(); device.id.clear();
    check(!device_safety_reason(device, false, false, false).empty(), "unknown stable hardware ID is denied");
    device = usb(); device.time_detected = 0;
    check(!device_safety_reason(device, false, false, false).empty(), "unknown hotplug generation is denied");
}
void format_tests() {
    using namespace kavrune;
    const auto capacity = usb().size;
    for (const auto scheme : {"gpt", "mbr"}) for (const auto filesystem : {"vfat", "exfat", "ext4"}) {
        validate_format_options({scheme, filesystem, "TEST-123"}, capacity);
        check(true, "all advertised format combinations are accepted");
    }
    for (const auto scheme : {"dos", "GPT", "gpt;rm", ""})
        rejected([&] { validate_format_options({scheme, "vfat", "TEST"}, capacity); }, "unexpected partition formats are denied");
    for (const auto filesystem : {"swap", "ntfs", "empty", "ext4;rm", ""})
        rejected([&] { validate_format_options({"gpt", filesystem, "TEST"}, capacity); }, "unexpected filesystem formats are denied");
    for (const auto label : {"", " rm", "rm ", "$(rm)", "`rm`", "a/b", "a;rm", "a\nb", "été", "a\\b", "'quoted'"})
        rejected([&] { validate_format_options({"gpt", "vfat", label}, capacity); }, "unsafe or ambiguous labels are denied");
    rejected([&] { validate_format_options({"gpt", "vfat", "TWELVECHARS1"}, capacity); }, "FAT labels are at most eleven bytes");
    rejected([&] { validate_format_options({"gpt", "exfat", "1234567890123456"}, capacity); }, "exFAT labels are at most fifteen ASCII bytes");
    rejected([&] { validate_format_options({"gpt", "ext4", "12345678901234567"}, capacity); }, "ext4 labels are at most sixteen bytes");
    rejected([&] { validate_format_options({"gpt", "vfat", "TEST"}, 63ULL * 1024 * 1024); }, "insufficient capacity is denied");
    rejected([&] { validate_format_options({"mbr", "vfat", "TEST"}, 2ULL * 1024 * 1024 * 1024 * 1024 + 1); }, "MBR capacity limit is honored");
    validate_format_options({"gpt", "ext4", "1234567890123456"}, 4ULL * 1024 * 1024 * 1024 * 1024);
    check(true, "GPT supports large data drives");
}
void topology_fixtures() {
    using namespace kavrune;
    std::istringstream fixture(
        "1 0 252:1 / / rw shared:1 - ext4 /dev/mapper/root rw\n"
        "2 1 8:17 / /media/My\\040USB rw - vfat /dev/sdb1 rw\n"
        "3 1 8:17 /users /home rw - ext4 /dev/sdb1 rw\n"
        "4 1 0:45 / /var/lib/docker/overlay2/test/merged rw - overlay overlay rw\n"
        "5 1 0:50 / /run/user/1000/doc rw - fuse.portal portal rw\n"
        "6 1 8:2 / /boot rw - ext4 /dev/sda2 rw\n"
        "7 6 8:1 / /boot/efi rw - vfat /dev/sda1 rw\n"
        "8 1 0:90 /@ /mnt/btrfs rw - btrfs /dev/sdc1 rw,subvol=/@\n");
    const auto mounts = parse_mounts(fixture);
    check(mounts.size() == 8, "mountinfo parses optional fields");
    check(mounts[1].path == "/media/My USB", "kernel escaped mount paths are decoded");
    check(mounts[2].number == static_cast<uint64_t>(makedev(8, 17)), "bind mounts preserve the backing dev_t");
    const auto critical = critical_mounts(mounts, {"/", "/home", "/usr", "/var", "/boot", "/boot/efi"});
    check(critical == std::set<size_t>({0, 2, 5, 6}), "critical filesystem selection uses deepest containing mount");
    auto stacked = mounts;
    stacked.push_back({static_cast<uint64_t>(makedev(8, 33)), "/home", "ext4", "/dev/sdc1"});
    check(critical_mounts(stacked, {"/home"}) == std::set<size_t>({2, 8}), "stacked critical mounts protect every possible backing disk");
    check(should_protect_mount(mounts[0], true), "root is always protected");
    check(!should_protect_mount(mounts[1], false), "desktop external media may be unmounted");
    check(should_protect_mount(mounts[2], true), "USB media bound as /home remains protected");
    const auto account_critical = critical_mounts(mounts, {"/", "/media/My USB/alice"});
    check(account_critical == std::set<size_t>({0, 1}), "a configured home inside /media selects its USB backing mount");
    check(should_protect_mount(mounts[1], account_critical.contains(1)), "a user's actual home on desktop USB media is protected");
    Mount ordinary_usb{static_cast<uint64_t>(makedev(8, 49)), "/media/Ordinary USB", "vfat", "/dev/sdd1"};
    check(!should_protect_mount(ordinary_usb, false), "ordinary desktop USB media remains available after home protection");
    check(!should_protect_mount(mounts[3], false), "auxiliary Docker overlay does not globally disable USB writing");
    check(!should_protect_mount(mounts[4], false), "auxiliary desktop portal does not globally disable USB writing");
    check(should_protect_mount(mounts[7], false), "Btrfs filesystems require the entire device set");
    check(!within("/mediax/data", "/media"), "prefix names cannot bypass mount protection");
    check(within("/home/user", "/home"), "directory boundaries are honored");
    check(!within("/homeuser", "/home"), "critical-directory sibling paths are distinct");
    Mount overlay_root{static_cast<uint64_t>(makedev(0, 10)), "/", "overlay", "overlay"};
    check(should_protect_mount(overlay_root, true), "unresolved overlay root cannot be silently ignored");
    rejected([&] { mount_ancestors(overlay_root); }, "overlay root fails closed before disk access");
    Mount fuse_home{static_cast<uint64_t>(makedev(0, 11)), "/home", "fuse.custom", "custom"};
    check(should_protect_mount(fuse_home, true), "a FUSE home filesystem is safety-critical");
    rejected([&] { mount_ancestors(fuse_home); }, "unknown FUSE critical storage ancestry fails closed");
    Mount tmp_root{static_cast<uint64_t>(makedev(0, 12)), "/", "tmpfs", "tmpfs"};
    check(should_protect_mount(tmp_root, true), "memory-backed root has no assumed disk ancestry");
    rejected([&] { mount_ancestors(tmp_root); }, "memory-backed root fails closed");
    for (const auto bad : {"", "8", "8-1", "8:1extra", "-1:2", "8:4294967296"})
        rejected([&] { (void)parse_device_number(bad); }, "malformed device numbers are denied");
    check(unescape_mount("a\\134b\\011c") == "a\\b\tc", "backslashes and tabs decode from kernel paths");
    rejected([&] { (void)unescape_mount("a\\000b"); }, "embedded NUL paths cannot be interpreted");
    for (const auto bad : {"", "1 0 8:1 / / rw", "1 0 8:1 / / rw - ext4", "1 0 8bad / / rw - ext4 /dev/sda1 rw"})
        rejected([&] { std::istringstream input(bad); (void)parse_mounts(input); }, "incomplete mount records cannot pass protection");
}
void home_directory_fixtures() {
    using namespace kavrune;
    char pattern[] = "/tmp/kavrune-home-test-XXXXXX";
    const char* created = ::mkdtemp(pattern);
    if (!created) throw std::runtime_error("Cannot create home protection fixtures");
    struct Cleanup {
        fs::path root;
        ~Cleanup() { std::error_code ignored; fs::remove_all(root, ignored); }
    } cleanup{created};
    const auto configured = cleanup.root / "account";
    const auto environment = cleanup.root / "usb-media" / "user";
    const auto link = cleanup.root / "home-link";
    fs::create_directories(configured);
    fs::create_directories(environment);
    fs::create_directory_symlink(environment, link);
    const auto homes = home_directories(configured.string(), link.string());
    check(homes == std::vector<std::string>({configured.string(), environment.string()}), "account and overridden symlink HOME are both canonicalized and protected");
    const std::vector<Mount> mounts{
        {static_cast<uint64_t>(makedev(8, 1)), "/", "ext4", "/dev/sda1"},
        {static_cast<uint64_t>(makedev(8, 17)), environment.parent_path().string(), "ext4", "/dev/sdb1"}
    };
    check(critical_mounts(mounts, homes) == std::set<size_t>({0, 1}), "symlink HOME protects the filesystem containing its destination");
    check(home_directories(configured.string(), configured.string()).size() == 1, "matching account and environment homes are deduplicated");
    check(home_directories(configured.string(), "").size() == 1, "missing HOME cannot hide the account home");
    check(home_directories(configured.string(), (cleanup.root / "absent").string()).size() == 1, "nonexistent optional HOME does not discard account protection");
    rejected([&] { (void)home_directories("", ""); }, "missing account home fails closed");
    rejected([&] { (void)home_directories("relative-home", ""); }, "relative account home fails closed");
    rejected([&] { (void)home_directories((cleanup.root / "absent").string(), ""); }, "nonexistent account home fails closed");
    const auto file = cleanup.root / "file";
    std::ofstream(file) << "fixture";
    rejected([&] { (void)home_directories(file.string(), ""); }, "account home must be a directory");
    rejected([&] { (void)home_directories(configured.string(), file.string()); }, "existing unverifiable HOME cannot bypass protection");
    check(fs::path(account_home()).is_absolute(), "the current account home is resolved through the account database");
}
}
int main() {
    try {
        identity_tests();
        eligibility_tests();
        format_tests();
        topology_fixtures();
        home_directory_fixtures();
        std::cout << "Storage safety: " << checks << " checks passed (no devices opened).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
