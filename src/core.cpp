// SPDX-License-Identifier: MIT
// Original implementation. Image contents and device descriptors are untrusted.
#include "kavrune/core.hpp"
#include "kavrune/version.hpp"

#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <linux/fs.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace kavrune {
namespace {
constexpr size_t chunk_size = 4 * 1024 * 1024;
constexpr uint64_t sector_size = 2048;

class Fd {
    int value_ = -1;
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(other.value_) { other.value_ = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { if (value_ >= 0) ::close(value_); value_ = other.value_; other.value_ = -1; }
        return *this;
    }
    int get() const { return value_; }
};

[[noreturn]] void system_error(const std::string& operation) {
    const int saved = errno;
    throw Error(operation + ": " + std::strerror(saved));
}

struct stat file_stat(int fd) {
    struct stat value {};
    if (::fstat(fd, &value) != 0) system_error("Could not inspect the open descriptor");
    return value;
}

Fd open_source(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) system_error("Could not open the image (links are not accepted)");
    Fd result(fd);
    const auto st = file_stat(fd);
    if (!S_ISREG(st.st_mode)) throw Error("The image must be a regular file.");
    if (st.st_size <= 0) throw Error("The image is empty.");
    return result;
}

void cancelled(const std::atomic_bool& cancel) {
    if (cancel.load(std::memory_order_relaxed)) throw Cancelled();
}

void read_exact(int fd, void* bytes, size_t count, uint64_t offset) {
    auto* out = static_cast<unsigned char*>(bytes);
    size_t done = 0;
    while (done < count) {
        const auto n = ::pread(fd, out + done, count - done, static_cast<off_t>(offset + done));
        if (n < 0) { if (errno == EINTR) continue; system_error("Read failed"); }
        if (n == 0) throw Error("Unexpected end of data; the source or device changed or is too short.");
        done += static_cast<size_t>(n);
    }
}

void write_exact(int fd, const void* bytes, size_t count, uint64_t offset) {
    const auto* in = static_cast<const unsigned char*>(bytes);
    size_t done = 0;
    while (done < count) {
        const auto n = ::pwrite(fd, in + done, count - done, static_cast<off_t>(offset + done));
        if (n < 0) { if (errno == EINTR) continue; system_error("Write failed"); }
        if (n == 0) throw Error("The device accepted no data.");
        done += static_cast<size_t>(n);
    }
}

void sync_fd(int fd) {
    while (::fsync(fd) != 0) {
        if (errno == EINTR) continue;
        system_error("Could not synchronize the written data");
    }
}

Image identity_for(const std::string& path, const struct stat& st) {
    Image image;
    image.path = path;
    image.name = std::filesystem::path(path).filename().string();
    image.size = static_cast<uint64_t>(st.st_size);
    image.inode = static_cast<uint64_t>(st.st_ino);
    image.device = static_cast<uint64_t>(st.st_dev);
    image.mtime_sec = st.st_mtim.tv_sec;
    image.mtime_nsec = st.st_mtim.tv_nsec;
    image.ctime_sec = st.st_ctim.tv_sec;
    image.ctime_nsec = st.st_ctim.tv_nsec;
    return image;
}

void validate_source(const Image& image, int fd) {
    const auto valid = [&](const struct stat& st) {
        return S_ISREG(st.st_mode) && st.st_size > 0 &&
            static_cast<uint64_t>(st.st_size) == image.size &&
            static_cast<uint64_t>(st.st_ino) == image.inode &&
            static_cast<uint64_t>(st.st_dev) == image.device &&
            st.st_mtim.tv_sec == image.mtime_sec && st.st_mtim.tv_nsec == image.mtime_nsec &&
            st.st_ctim.tv_sec == image.ctime_sec && st.st_ctim.tv_nsec == image.ctime_nsec;
    };
    struct stat path_stat {};
    if (!valid(file_stat(fd)) || ::lstat(image.path.c_str(), &path_stat) != 0 || !valid(path_stat))
        throw Error("The image changed after inspection. Select and inspect it again.");
}

void validate_descriptor(int fd, uint64_t capacity, bool writable) {
    if (capacity == 0 || capacity > static_cast<uint64_t>(INT64_MAX))
        throw Error("The device capacity is invalid.");
    const auto st = file_stat(fd);
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0) system_error("Could not inspect device access");
    if (writable && ((flags & O_ACCMODE) == O_RDONLY || (flags & O_APPEND)))
        throw Error("The target descriptor must permit positioned writing and must not use append mode.");
    if (!writable && (flags & O_ACCMODE) == O_WRONLY)
        throw Error("The device descriptor does not permit reading.");
    if (S_ISREG(st.st_mode)) {
        if (st.st_size < 0 || capacity > static_cast<uint64_t>(st.st_size))
            throw Error("The declared capacity exceeds the open file's size.");
    } else if (S_ISBLK(st.st_mode)) {
        uint64_t actual = 0;
        if (::ioctl(fd, BLKGETSIZE64, &actual) != 0) system_error("Could not validate device capacity");
        if (actual != capacity) throw Error("The device capacity changed. Refresh the device selection.");
    } else {
        throw Error("The descriptor is neither a disk device nor a regular test file.");
    }
}

class Digest {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_{EVP_MD_CTX_new(), EVP_MD_CTX_free};
public:
    explicit Digest(const std::string& algorithm) {
        const EVP_MD* method = nullptr;
        if (algorithm == "SHA-256" || algorithm == "SHA256" || algorithm == "sha256") method = EVP_sha256();
        else if (algorithm == "SHA-512" || algorithm == "SHA512" || algorithm == "sha512") method = EVP_sha512();
        else if (algorithm == "SHA-1" || algorithm == "SHA1" || algorithm == "sha1") method = EVP_sha1();
        else if (algorithm == "MD5" || algorithm == "md5") method = EVP_md5();
        else throw Error("Unsupported checksum algorithm. Choose SHA-256, SHA-512, SHA-1, or MD5.");
        if (!context_ || EVP_DigestInit_ex(context_.get(), method, nullptr) != 1)
            throw Error("The checksum algorithm is unavailable in this system's crypto provider.");
    }
    void update(const void* bytes, size_t count) {
        if (EVP_DigestUpdate(context_.get(), bytes, count) != 1) throw Error("Could not calculate the checksum.");
    }
    std::string finish() {
        std::array<unsigned char, EVP_MAX_MD_SIZE> bytes {};
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(context_.get(), bytes.data(), &length) != 1) throw Error("Could not finish the checksum.");
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < length; ++i) output << std::setw(2) << static_cast<unsigned int>(bytes[i]);
        return output.str();
    }
};

class Reporter {
    const ProgressFn& callback_;
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    std::string stage_;
public:
    explicit Reporter(const ProgressFn& callback) : callback_(callback) {}
    void emit(const std::string& stage, uint64_t done, uint64_t total, const std::string& detail = {}) {
        if (stage != stage_) { stage_ = stage; start_ = std::chrono::steady_clock::now(); }
        if (!callback_) return;
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        const bool streaming = stage == "Writing" || stage == "Verifying" || stage == "Checksum" || stage == "Backing up" || stage == "Scanning" || stage == "Checking source";
        callback_({stage, done, total, streaming && seconds > 0 ? static_cast<double>(done) / seconds : 0, detail});
    }
};

class DirectReadback {
    int fd_;
    int original_flags_ = -1;
    size_t sector_ = 1;
    std::unique_ptr<unsigned char, decltype(&std::free)> aligned_{nullptr, std::free};
public:
    DirectReadback(int fd, bool block, uint64_t image_size, uint64_t capacity) : fd_(fd) {
        if (!block) {
            // Regular files are the test/simulation backend. Drop eligible cached pages as a best effort.
            (void)::posix_fadvise(fd, 0, static_cast<off_t>(image_size), POSIX_FADV_DONTNEED);
            return;
        }
        int logical_sector = 0;
        if (::ioctl(fd, BLKSSZGET, &logical_sector) != 0) system_error("Could not determine the readback alignment");
        if (logical_sector < 512 || logical_sector > 65536 || (logical_sector & (logical_sector - 1)) != 0)
            throw Error("The device reports an unsupported logical sector size.");
        sector_ = static_cast<size_t>(logical_sector);
        const uint64_t rounded = ((image_size + sector_ - 1) / sector_) * sector_;
        if (rounded > capacity || capacity % sector_ != 0)
            throw Error("The image cannot be read back within the target's aligned capacity.");
        void* memory = nullptr;
        if (::posix_memalign(&memory, std::max<size_t>(4096, sector_), chunk_size) != 0)
            throw Error("Could not allocate an aligned readback buffer.");
        aligned_.reset(static_cast<unsigned char*>(memory));
        original_flags_ = ::fcntl(fd_, F_GETFL);
        if (original_flags_ < 0) system_error("Could not inspect readback descriptor flags");
        if (::fcntl(fd_, F_SETFL, original_flags_ | O_DIRECT) < 0) {
            original_flags_ = -1;
            system_error("Physical readback could not be enabled; verification is unavailable");
        }
        const int actual_flags = ::fcntl(fd_, F_GETFL);
        if (actual_flags < 0 || !(actual_flags & O_DIRECT)) {
            (void)::fcntl(fd_, F_SETFL, original_flags_);
            original_flags_ = -1;
            throw Error("The device did not enable physical readback; verification is unavailable.");
        }
    }
    ~DirectReadback() { if (original_flags_ >= 0) (void)::fcntl(fd_, F_SETFL, original_flags_); }
    DirectReadback(const DirectReadback&) = delete;
    DirectReadback& operator=(const DirectReadback&) = delete;
    unsigned char* buffer(unsigned char* fallback) { return aligned_ ? aligned_.get() : fallback; }
    void read(void* bytes, size_t image_bytes, uint64_t offset) {
        const size_t count = ((image_bytes + sector_ - 1) / sector_) * sector_;
        auto* out = static_cast<unsigned char*>(bytes);
        size_t done = 0;
        while (done < count) {
            const auto read = ::pread(fd_, out + done, count - done, static_cast<off_t>(offset + done));
            if (read < 0) { if (errno == EINTR) continue; system_error("Physical readback failed"); }
            if (read == 0) throw Error("The device ended before readback verification completed.");
            if (static_cast<size_t>(read) % sector_ != 0) throw Error("The device returned an unaligned physical readback.");
            done += static_cast<size_t>(read);
        }
    }
    void restore() {
        if (original_flags_ < 0) return;
        if (::fcntl(fd_, F_SETFL, original_flags_) < 0) system_error("Could not restore the device descriptor flags");
        original_flags_ = -1;
    }
};

uint16_t le16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint32_t be32(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
uint64_t le64(const unsigned char* p) { return static_cast<uint64_t>(le32(p)) | (static_cast<uint64_t>(le32(p + 4)) << 32); }

uint32_t crc32(const unsigned char* bytes, size_t count) {
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

std::string upper(std::string text) {
    for (auto& c : text) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return text;
}

struct MbrLayout { bool valid = false, disk = false, protective = false; };
MbrLayout inspect_mbr(const std::vector<unsigned char>& head, uint64_t size, bool bootable_iso) {
    if (head.size() < 512 || head[510] != 0x55 || head[511] != 0xaa) return {};
    MbrLayout result;
    for (size_t i = 0; i < 4; ++i) {
        const auto* p = head.data() + 446 + 16 * i;
        const uint32_t start = le32(p + 8), length = le32(p + 12);
        if (p[4] == 0 && start == 0 && length == 0 && p[0] == 0) continue;
        if ((p[0] != 0 && p[0] != 0x80) || length == 0) return {};
        if (p[4] == 0) {
            // Hybrid generators use type-zero entries for the whole ISO, or a
            // one-sector boot flag placeholder. The placeholder alone is not a
            // usable disk layout. Neither convention applies to raw images.
            if (!bootable_iso || start != 0 || (length != 1 && length != size / 512) || size % 512 != 0) return {};
            result.valid = true;
            result.disk = result.disk || (length == size / 512 && length > 1);
            continue;
        }
        const uint64_t end = static_cast<uint64_t>(start) + length;
        // Protective MBRs saturate at 2 TiB. They are only accepted with a valid GPT below.
        if (p[4] != 0xee && end > size / 512) return {};
        result.valid = true; result.disk = true; result.protective = result.protective || p[4] == 0xee;
    }
    return result;
}

bool valid_gpt(int fd, const std::vector<unsigned char>& head, uint64_t size, bool hybrid_iso) {
    if (head.size() < 1024 || std::memcmp(head.data() + 512, "EFI PART", 8) != 0) return false;
    const auto* h = head.data() + 512;
    const uint32_t header_size = le32(h + 12);
    if (le32(h + 8) != 0x00010000 || header_size < 92 || header_size > 512 || le32(h + 20) != 0)
        throw Error("The GPT image header is malformed.");
    std::vector<unsigned char> header(h, h + header_size);
    const uint32_t expected_crc = le32(h + 16);
    std::fill(header.begin() + 16, header.begin() + 20, 0);
    if (crc32(header.data(), header.size()) != expected_crc) throw Error("The GPT image header checksum is invalid.");
    const uint64_t sectors = size / 512;
    const uint64_t current = le64(h + 24), backup = le64(h + 32);
    const uint64_t first = le64(h + 40), last = le64(h + 48), entries_lba = le64(h + 72);
    const uint32_t entries_count = le32(h + 80), entry_size = le32(h + 84);
    if (current != 1 || backup <= 1 || backup >= sectors || first < 2 || first > last || last >= sectors ||
        entries_count == 0 || entries_count > 16384 || entry_size < 128 || entry_size > 4096 || entry_size % 128 != 0 ||
        entries_lba < 2 || entries_lba >= first)
        throw Error("The GPT image layout is invalid or exceeds inspection limits.");
    const uint64_t bytes = static_cast<uint64_t>(entries_count) * entry_size;
    if (bytes > chunk_size || entries_lba * 512 > size || bytes > size - entries_lba * 512 ||
        entries_lba * 512 + bytes > first * 512)
        throw Error("The GPT partition list is invalid or exceeds inspection limits.");
    std::vector<unsigned char> entries(static_cast<size_t>(bytes));
    read_exact(fd, entries.data(), entries.size(), entries_lba * 512);
    if (crc32(entries.data(), entries.size()) != le32(h + 88)) throw Error("The GPT partition list checksum is invalid.");
    bool populated_entry_seen = false;
    for (uint32_t i = 0; i < entries_count; ++i) {
        const auto* entry = entries.data() + static_cast<size_t>(i) * entry_size;
        if (std::all_of(entry, entry + 16, [](unsigned char c) { return c == 0; })) continue;
        const uint64_t begin = le64(entry + 32), end = le64(entry + 40);
        // Hybrid generators allow configurable type GUIDs for the ISO carrier.
        // Only its first populated partition can include sector zero and GPT
        // metadata; raw disk images retain their strict usable-range checks.
        const bool iso_carrier = hybrid_iso && !populated_entry_seen && begin == 0 && end >= first;
        populated_entry_seen = true;
        if (begin > end || end >= sectors)
            throw Error("A GPT partition extends beyond the image or has reversed bounds.");
        if ((!iso_carrier && begin < first) || end > last)
            throw Error("A GPT partition lies outside the declared usable range.");
    }
    return true;
}

struct IsoEntry { std::string name; uint32_t extent = 0, size = 0; bool directory = false; };

std::vector<IsoEntry> iso_directory(int fd, uint64_t image_size, uint32_t extent, uint32_t bytes) {
    const uint64_t offset = static_cast<uint64_t>(extent) * sector_size;
    if (bytes == 0 || offset > image_size || bytes > image_size - offset)
        throw Error("An ISO directory extends outside the image or is empty.");
    if (bytes > chunk_size)
        throw ImageError(ImageIssue::inspection_limit, "An ISO directory exceeds the bounded inspection limit.",
                         "This image has more directory metadata than this version can inspect. Use the image publisher's USB preparation instructions.");
    std::vector<unsigned char> data(bytes);
    read_exact(fd, data.data(), data.size(), offset);
    std::vector<IsoEntry> entries;
    for (size_t position = 0; position < data.size();) {
        const unsigned int length = data[position];
        if (length == 0) { position = ((position / sector_size) + 1) * sector_size; continue; }
        if (length < 34 || length > data.size() - position || (position % sector_size) + length > sector_size)
            throw Error("An ISO directory record is malformed.");
        const auto* record = data.data() + position;
        const size_t name_length = record[32];
        if (name_length == 0 || 33 + name_length > length || le32(record + 2) != be32(record + 6) || le32(record + 10) != be32(record + 14))
            throw Error("An ISO directory record is malformed.");
        if (!(name_length == 1 && (record[33] == 0 || record[33] == 1))) {
            std::string name(reinterpret_cast<const char*>(record + 33), name_length);
            name = upper(name);
            const auto suffix = name.find(';');
            if (suffix != std::string::npos) name.resize(suffix);
            entries.push_back({name, le32(record + 2), le32(record + 10), (record[25] & 2) != 0});
        }
        position += length;
    }
    return entries;
}

bool windows_iso(int fd, uint64_t size, const unsigned char* pvd) {
    const auto* root = pvd + 156;
    if (root[0] < 34 || le32(root + 2) != be32(root + 6) || le32(root + 10) != be32(root + 14))
        throw Error("The ISO root directory descriptor is malformed.");
    const auto entries = iso_directory(fd, size, le32(root + 2), le32(root + 10));
    bool bootmgr = false, setup = false, installer = false, sources_inspected = false;
    for (const auto& entry : entries) {
        if (entry.name == "BOOTMGR" || entry.name == "BOOTMGR.EFI") bootmgr = true;
        if (entry.name == "SETUP.EXE") setup = true;
        if (entry.name == "SOURCES" && entry.directory && !sources_inspected) {
            sources_inspected = true; // At most two directory reads, regardless of hostile duplicate records.
            for (const auto& source : iso_directory(fd, size, entry.extent, entry.size))
                if (source.name == "INSTALL.WIM" || source.name == "INSTALL.ESD" || source.name == "BOOT.WIM") installer = true;
        }
    }
    return installer && (bootmgr || setup);
}

bool el_torito(int fd, uint64_t size, const unsigned char* descriptor) {
    if (std::memcmp(descriptor + 7, "EL TORITO SPECIFICATION", 22) != 0) return false;
    const uint64_t offset = static_cast<uint64_t>(le32(descriptor + 71)) * sector_size;
    if (offset > size || 64 > size - offset) throw Error("The El Torito boot catalog lies outside the image.");
    constexpr uint64_t record_bytes = 32, maximum_records = 4096;
    auto record_at = [&](uint64_t index) {
        if (index >= maximum_records)
            throw ImageError(ImageIssue::inspection_limit, "The El Torito catalog exceeds the bounded inspection limit.",
                             "This image has more boot entries than this version can inspect. Use the image publisher's USB preparation instructions.");
        const uint64_t position = offset + index * record_bytes;
        if (position > size || record_bytes > size - position) throw Error("The El Torito boot catalog is truncated.");
        std::array<unsigned char, record_bytes> record {};
        read_exact(fd, record.data(), record.size(), position);
        return record;
    };
    const auto validation = record_at(0);
    uint16_t sum = 0;
    for (size_t i = 0; i < 32; i += 2) sum = static_cast<uint16_t>(sum + le16(validation.data() + i));
    if (validation[0] != 1 || validation[30] != 0x55 || validation[31] != 0xaa || sum != 0)
        throw Error("The El Torito boot catalog validation entry is invalid.");
    auto boot_entry = [&](const auto& entry, unsigned char platform) {
        if (entry[0] != 0 && entry[0] != 0x88) throw Error("The El Torito boot entry is invalid.");
        if (entry[0] == 0) return false;
        const uint64_t boot_offset = static_cast<uint64_t>(le32(entry.data() + 8)) * sector_size;
        const uint64_t boot_bytes = static_cast<uint64_t>(le16(entry.data() + 6)) * 512;
        // UEFI permits zero or one sector to mean the image continues to the
        // end of the medium (UEFI 2.11, section 13.3.2.1).
        const bool efi_remainder = platform == 0xef && boot_bytes <= 512;
        if ((entry[1] & 0x0f) > 4 || boot_offset >= size ||
            (!efi_remainder && (boot_bytes == 0 || boot_bytes > size - boot_offset)))
            throw Error("The El Torito boot image lies outside the image.");
        return true;
    };
    bool bootable = boot_entry(record_at(1), validation[1]);
    uint64_t index = 2;
    for (;;) {
        // Catalogs without sections end after their default entry and padding.
        if (offset + (index + 1) * record_bytes > size) return bootable;
        const auto header = record_at(index++);
        if (header[0] == 0) return bootable;
        if (header[0] != 0x90 && header[0] != 0x91) throw Error("The El Torito section header is invalid.");
        const uint16_t count = le16(header.data() + 2);
        if (count == 0) throw Error("The El Torito boot section is empty.");
        if (count > maximum_records - index)
            throw ImageError(ImageIssue::inspection_limit, "The El Torito section exceeds the bounded inspection limit.",
                             "This image has more boot entries than this version can inspect. Use the image publisher's USB preparation instructions.");
        for (uint16_t i = 0; i < count; ++i) {
            const auto entry = record_at(index++);
            bootable = boot_entry(entry, header[1]) || bootable;
            // Section counts describe boot entries; their extension records
            // follow each entry and do not consume that count.
            bool more_extensions = (entry[1] & 0x20) != 0;
            while (more_extensions) {
                const auto extension = record_at(index++);
                if (extension[0] != 0x44) throw Error("An El Torito boot entry extension is missing or invalid.");
                more_extensions = (extension[1] & 0x20) != 0;
            }
        }
        if (header[0] == 0x91) return bootable;
    }
}

std::mutex log_mutex;
constexpr size_t log_limit = 256 * 1024;
constexpr size_t log_record_limit = 8192;

std::string state_directory() {
    const char* xdg = std::getenv("XDG_STATE_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path base;
    if (xdg && xdg[0] == '/') base = xdg;
    else if (home && home[0] == '/') base = std::filesystem::path(home) / ".local/state";
    else throw Error("An absolute user state directory is required for logging.");
    return (base / "kavrune").lexically_normal().string();
}

Fd secure_state_directory() {
    const auto path = std::filesystem::path(state_directory());
    Fd current(::open("/", O_RDONLY | O_CLOEXEC | O_DIRECTORY));
    if (current.get() < 0) system_error("Could not open the state directory root");
    for (const auto& component : path.relative_path()) {
        const auto name = component.string();
        if (name.empty() || name == "." || name == "..") throw Error("The state directory path is invalid.");
        int next = ::openat(current.get(), name.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        if (next < 0 && errno == ENOENT) {
            if (::mkdirat(current.get(), name.c_str(), 0700) != 0 && errno != EEXIST) system_error("Could not create the state directory");
            next = ::openat(current.get(), name.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
        }
        if (next < 0) system_error("Could not securely open the state directory");
        current = Fd(next);
    }
    const auto st = file_stat(current.get());
    if (st.st_uid != ::geteuid()) throw Error("The application state directory belongs to another user.");
    if (::fchmod(current.get(), 0700) != 0) system_error("Could not secure the state directory");
    return current;
}

bool same_file(const struct stat& first, const struct stat& second) {
    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

void validate_state_directory(int directory) {
    struct stat current {};
    if (::fstatat(AT_FDCWD, state_directory().c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0 ||
        !S_ISDIR(current.st_mode) || !same_file(file_stat(directory), current))
        throw Error("The diagnostics folder was moved or replaced. Reload activity and try again.");
}

void validate_log_entry(int directory, const char* name, int descriptor) {
    const auto opened = file_stat(descriptor);
    struct stat current {};
    if (!S_ISREG(opened.st_mode) || opened.st_uid != ::geteuid() || opened.st_nlink != 1 ||
        ::fstatat(directory, name, &current, AT_SYMLINK_NOFOLLOW) != 0 ||
        !S_ISREG(current.st_mode) || !same_file(opened, current))
        throw Error("A diagnostics file has an unsafe owner, type, link count, or changed identity.");
    validate_state_directory(directory);
}

Fd open_log_entry(int directory, const char* name, bool create) {
    Fd file(::openat(directory, name, O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | (create ? O_CREAT : 0), 0600));
    if (file.get() < 0) {
        if (!create && errno == ENOENT) return file;
        system_error("Could not securely open local diagnostics");
    }
    validate_log_entry(directory, name, file.get());
    if (::fchmod(file.get(), 0600) != 0) system_error("Could not secure diagnostics permissions");
    // Diagnostics are optional: never wait behind another process, including
    // one suspended while saving or inspecting activity.
    while (::flock(file.get(), LOCK_EX | LOCK_NB) != 0) {
        if (errno != EINTR) system_error("Local diagnostics are busy; try again after the other operation");
    }
    validate_log_entry(directory, name, file.get());
    return file;
}

struct LogSession {
    Fd directory = secure_state_directory();
    // This fixed lock is never renamed or removed, so read/rotate/clear cannot
    // acquire locks on different generations of the active log file.
    Fd lock = open_log_entry(directory.get(), ".events.lock", true);
    void validate() const { validate_log_entry(directory.get(), ".events.lock", lock.get()); }
};

std::string bounded_log_tail(int descriptor) {
    const auto st = file_stat(descriptor);
    if (st.st_size < 0) throw Error("The diagnostics size is invalid.");
    const auto size = static_cast<uint64_t>(st.st_size);
    const auto bytes = static_cast<size_t>(std::min<uint64_t>(size, log_limit));
    std::string text(bytes, '\0');
    if (bytes) read_exact(descriptor, text.data(), bytes, size - bytes);
    if (size > bytes) {
        // Never present a suffix fragment as a complete historical record.
        const auto newline = text.find('\n');
        text.erase(0, newline == std::string::npos ? text.size() : newline + 1);
    }
    return text;
}

std::string clean_diagnostic_text(const std::string& input, size_t limit, bool multiline = false) {
    std::string text;
    const size_t end = std::min(input.size(), limit);
    text.reserve(end);
    for (size_t i = 0; i < end;) {
        const auto first = static_cast<unsigned char>(input[i]);
        if (first < 0x80) {
            text += (first >= 0x20 && first != 0x7f) || (multiline && first == '\n') ? static_cast<char>(first) : ' ';
            ++i;
            continue;
        }
        const size_t count = first >= 0xc2 && first <= 0xdf ? 2 : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        uint32_t codepoint = count ? first & ((1U << (7 - count)) - 1) : 0;
        bool valid = count && count <= end - i;
        for (size_t j = 1; valid && j < count; ++j) {
            const auto next = static_cast<unsigned char>(input[i + j]);
            valid = (next & 0xc0) == 0x80;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        valid = valid && (count != 3 || codepoint >= 0x800) && (count != 4 || codepoint >= 0x10000) &&
                codepoint <= 0x10ffff && !(codepoint >= 0xd800 && codepoint <= 0xdfff);
        if (!valid) { text += ' '; ++i; continue; }
        // Control and directional override characters must not forge records
        // or conceal what a user is about to share. Ordinary Unicode remains.
        if ((codepoint >= 0x80 && codepoint <= 0x9f) || codepoint == 0x2028 || codepoint == 0x2029 ||
            (codepoint >= 0x202a && codepoint <= 0x202e) || (codepoint >= 0x2066 && codepoint <= 0x2069)) text += ' ';
        else text.append(input, i, count);
        i += count;
    }
    return text;
}

void truncate_log(int descriptor) {
    if (::ftruncate(descriptor, 0) != 0) system_error("Could not clear local diagnostics");
}
}

Image identify_file(const std::string& path) {
    Fd source = open_source(path);
    return identity_for(path, file_stat(source.get()));
}

Image inspect_image(const std::string& path) {
    const auto extension = upper(std::filesystem::path(path).extension().string());
    if (extension == ".CRDOWNLOAD" || extension == ".PART" || extension == ".PARTIAL" || extension == ".DOWNLOAD")
        throw ImageError(ImageIssue::incomplete, "The selected file has an unfinished-download suffix.",
                         "Wait for your browser to finish downloading, then choose the completed ISO or disk image. Keep the original filename.");
    Fd source = open_source(path);
    Image image = identity_for(path, file_stat(source.get()));
    try {
    const size_t head_size = static_cast<size_t>(std::min<uint64_t>(image.size, 64 * sector_size));
    std::vector<unsigned char> head(head_size);
    read_exact(source.get(), head.data(), head.size(), 0);
    const auto starts = [&](const char* signature, size_t count) {
        return head.size() >= count && std::memcmp(head.data(), signature, count) == 0;
    };
    if (starts("\x1f\x8b", 2) || starts("\xfd" "7zXZ\0", 6) || starts("\x28\xb5\x2f\xfd", 4) ||
        starts("BZh", 3) || starts("PK\x03\x04", 4) || starts("7z\xbc\xaf\x27\x1c", 6) ||
        starts("\x04\x22\x4d\x18", 4) || extension == ".GZ" || extension == ".XZ" || extension == ".ZST" || extension == ".BZ2" || extension == ".ZIP" || extension == ".7Z" || extension == ".LZ4")
        throw ImageError(ImageIssue::compressed, "The selected file is compressed or archived.",
                         "Extract this download first, then choose the ISO, IMG or RAW file inside it. This version does not write compressed images directly.");
    if (starts("QFI\xfb", 4) || starts("vhdxfile", 8) || starts("KDMV", 4) || extension == ".VDI")
        throw ImageError(ImageIssue::container, "The selected file is a virtual disk container.",
                         "Convert the virtual disk to a raw disk image with a trusted conversion tool, then choose that image.");
    if (image.size >= 512) {
        std::array<unsigned char, 8> footer {};
        read_exact(source.get(), footer.data(), footer.size(), image.size - 512);
        if (std::memcmp(footer.data(), "conectix", 8) == 0)
            throw ImageError(ImageIssue::container, "The selected file is a VHD virtual disk container.",
                             "Convert the virtual disk to a raw disk image with a trusted conversion tool, then choose that image.");
    }
    std::array<unsigned char, sector_size> pvd {};
    bool iso = false, udf = false, boot = false, terminated = false;
    unsigned udf_state = 0;
    constexpr uint64_t maximum_descriptors = 256;
    uint64_t descriptor_count = 0;
    for (uint64_t sector = 16; sector * sector_size + sector_size <= image.size; ++sector) {
        if (descriptor_count++ == maximum_descriptors)
            throw ImageError(ImageIssue::inspection_limit, "The image has too many volume descriptors for bounded inspection.",
                             "This image uses more metadata than this version can inspect. Use the publisher's USB preparation instructions.");
        std::array<unsigned char, sector_size> block {};
        read_exact(source.get(), block.data(), block.size(), sector * sector_size);
        const auto* descriptor = block.data();
        const auto identifier = [&](const char* value) { return std::memcmp(descriptor + 1, value, 5) == 0; };
        if (identifier("BEA01")) {
            if (udf_state != 0) throw Error("The UDF volume recognition sequence is malformed.");
            udf_state = 1; continue;
        }
        if (identifier("NSR02") || identifier("NSR03")) {
            if (udf_state != 1) throw Error("The UDF volume recognition sequence is malformed.");
            udf_state = 2; continue;
        }
        if (identifier("TEA01")) {
            if (udf_state != 2) throw Error("The UDF volume recognition sequence is malformed.");
            udf_state = 3; udf = true; continue;
        }
        if (identifier("BOOT2") && (udf_state == 1 || udf_state == 2)) continue;
        if (std::memcmp(descriptor + 1, "CD001", 5) != 0) {
            if ((iso && !terminated) || udf_state == 1 || udf_state == 2)
                throw Error("The image volume descriptor sequence is incomplete or malformed.");
            break;
        }
        if (terminated && descriptor[0] != 255) throw Error("ISO descriptors occur after their sequence terminator.");
        if (descriptor[6] != 1 && !(descriptor[0] == 2 && descriptor[6] == 2))
            throw Error("The ISO volume descriptor version is invalid.");
        if (descriptor[0] == 1) { std::copy(descriptor, descriptor + sector_size, pvd.begin()); iso = true; }
        else if (descriptor[0] == 0) boot = el_torito(source.get(), image.size, descriptor) || boot;
        else if (descriptor[0] == 255) terminated = true;
        else if (descriptor[0] != 2 && descriptor[0] != 3) throw Error("The ISO volume descriptor type is invalid.");
    }
    if (udf_state == 1 || udf_state == 2) throw Error("The UDF volume recognition sequence is incomplete.");
    bool windows = false;
    if (iso) {
        const uint32_t volume_sectors = le32(pvd.data() + 80);
        if (!terminated || volume_sectors == 0 || volume_sectors != be32(pvd.data() + 84) ||
            le16(pvd.data() + 128) != sector_size || static_cast<uint16_t>((pvd[130] << 8) | pvd[131]) != sector_size ||
            static_cast<uint64_t>(volume_sectors) * sector_size > image.size)
            throw Error("The ISO filesystem size or volume descriptor is invalid.");
        windows = windows_iso(source.get(), static_cast<uint64_t>(volume_sectors) * sector_size, pvd.data());
    }
    const auto mbr = inspect_mbr(head, image.size, iso && boot);
    const bool gpt = valid_gpt(source.get(), head, image.size, iso && boot && mbr.valid);
    const bool disk_layout = gpt || (mbr.valid && mbr.disk && !mbr.protective);
    if (iso) {
        image.kind = windows ? "Windows installation ISO" : (disk_layout ? "Hybrid ISO" : "Optical ISO");
        image.bootable = boot;
        image.raw_writable = disk_layout && !windows;
        image.description = windows ? "This Windows installer needs file extraction and large-file handling, which this version does not provide. You can check its checksum here. Use a supported Windows USB creation method." :
            (disk_layout ? "This ISO has a USB disk layout. Kavrune can copy it and verify the written data. Boot compatibility depends on the image and computer." :
                "This disc ISO has no USB disk layout. You can check its checksum here. Choose a USB-ready image or follow the publisher's USB preparation instructions.");
    } else if (udf) {
        image.kind = "UDF optical image";
        image.description = "This image uses the UDF disc format. You can check its checksum here. Creating a bootable USB needs an extraction method that this version does not provide.";
    } else if (gpt || disk_layout) {
        image.kind = gpt ? "GPT disk image" : "MBR disk image";
        image.raw_writable = true;
        image.description = "Disk image with a validated partition layout. Raw writing preserves its data and partitions; boot compatibility is unknown.";
    } else if (extension == ".IMG" || extension == ".RAW") {
        image.kind = "Opaque raw disk image";
        image.raw_writable = true;
        image.description = "Unstructured raw image. Its bytes can be restored, but no boot structures or operating system were identified.";
    } else {
        throw ImageError(ImageIssue::unrecognized, "No supported image structure was found in this file.",
                         "Choose the completed ISO, IMG or RAW download itself. Check its publisher's checksum if the file should be a disk image; some disc formats need the publisher's own USB preparation method.");
    }
    validate_source(image, source.get());
    return image;
    } catch (const ImageError&) {
        throw;
    } catch (const Error& error) {
        throw ImageError(ImageIssue::malformed, error.what(),
                         "This image could not be validated. Check the publisher's checksum and that the download finished. Choose the file again after checking it; no USB has been changed.");
    }
}

std::string checksum(const std::string& path, const std::string& algorithm, const ProgressFn& progress, const std::atomic_bool& cancel) {
    cancelled(cancel);
    Fd source = open_source(path);
    const Image identity = identity_for(path, file_stat(source.get()));
    Digest digest(algorithm);
    Reporter reporter(progress);
    std::vector<unsigned char> buffer(chunk_size);
    reporter.emit("Checksum", 0, identity.size);
    for (uint64_t offset = 0; offset < identity.size;) {
        cancelled(cancel);
        const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), identity.size - offset));
        read_exact(source.get(), buffer.data(), count, offset);
        digest.update(buffer.data(), count);
        offset += count;
        reporter.emit("Checksum", offset, identity.size);
    }
    cancelled(cancel);
    validate_source(identity, source.get());
    return digest.finish();
}

std::string write_image(const Image& image, int target_fd, uint64_t capacity, bool verify, const ProgressFn& progress, const std::atomic_bool& cancel) {
    cancelled(cancel);
    if (!image.raw_writable) throw Error("This image is not compatible with raw USB writing.");
    if (image.size == 0 || image.size > capacity) throw Error("The image is larger than the target capacity.");
    Fd source = open_source(image.path);
    validate_source(image, source.get());
    validate_descriptor(target_fd, capacity, true);
    const auto source_stat = file_stat(source.get()), target_stat = file_stat(target_fd);
    if (source_stat.st_dev == target_stat.st_dev && source_stat.st_ino == target_stat.st_ino)
        throw Error("The image and target refer to the same file.");
    const int flags = ::fcntl(target_fd, F_GETFL);
    if (verify && (flags & O_ACCMODE) == O_WRONLY) throw Error("Verification requires a target descriptor that permits reading.");
    std::string required_hash = image.expected_sha256;
    for (auto& character : required_hash) if (character >= 'A' && character <= 'F') character = static_cast<char>(character - 'A' + 'a');
    if (!required_hash.empty() && (required_hash.size() != 64 || required_hash.find_first_not_of("0123456789abcdef") != std::string::npos))
        throw Error("The selected image fingerprint is invalid. Choose the image again.");
    Reporter reporter(progress);
    Digest written("SHA-256");
    std::vector<unsigned char> buffer(chunk_size);
    bool touched = false, synchronized = false;
    uint64_t accepted = 0;
    try {
        if (!required_hash.empty()) {
            Digest checked("SHA-256");
            reporter.emit("Checking source", 0, image.size, "Checking that the image still matches the file selected before confirmation.");
            for (uint64_t offset = 0; offset < image.size;) {
                cancelled(cancel);
                const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), image.size - offset));
                read_exact(source.get(), buffer.data(), count, offset);
                checked.update(buffer.data(), count);
                offset += count;
                reporter.emit("Checking source", offset, image.size);
            }
            validate_source(image, source.get());
            if (checked.finish() != required_hash)
                throw Error("The image no longer matches the selected checksum. No image bytes were written; choose the image again.");
        }
        reporter.emit("Writing", 0, image.size);
        // Progress callbacks can run application code, so revalidate immediately before writing.
        cancelled(cancel);
        validate_source(image, source.get());
        validate_descriptor(target_fd, capacity, true);
        for (uint64_t offset = 0; offset < image.size;) {
            cancelled(cancel);
            const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), image.size - offset));
            read_exact(source.get(), buffer.data(), count, offset);
            touched = true;
            write_exact(target_fd, buffer.data(), count, offset);
            written.update(buffer.data(), count);
            offset += count;
            accepted = offset;
            reporter.emit("Writing", offset, image.size);
        }
        validate_source(image, source.get());
        reporter.emit("Synchronizing", image.size, image.size, "Cancellation waits until buffered writes have been synchronized.");
        sync_fd(target_fd);
        synchronized = true;
        cancelled(cancel);
        const std::string expected = written.finish();
        if (!required_hash.empty() && expected != required_hash)
            throw Error("The image changed while writing. The USB does not match the selected checksum; recreate it before use.");
        if (verify) {
            Digest readback("SHA-256");
            DirectReadback direct(target_fd, S_ISBLK(target_stat.st_mode), image.size, capacity);
            unsigned char* verify_buffer = direct.buffer(buffer.data());
            reporter.emit("Verifying", 0, image.size);
            for (uint64_t offset = 0; offset < image.size;) {
                cancelled(cancel);
                const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), image.size - offset));
                direct.read(verify_buffer, count, offset);
                readback.update(verify_buffer, count);
                offset += count;
                reporter.emit("Verifying", offset, image.size);
            }
            cancelled(cancel);
            if (readback.finish() != expected) throw Error("Readback verification failed. The drive's data does not match the written image.");
            validate_source(image, source.get());
            direct.restore();
        }
        reporter.emit("Complete", image.size, image.size, verify ? "Written data passed SHA-256 readback verification." : "Data synchronized; readback verification was not requested.");
        return expected;
    } catch (...) {
        if (touched && !synchronized) {
            // Errors and cancellation must flush any accepted writes before returning control.
            try { reporter.emit("Synchronizing", accepted, image.size, "Synchronizing accepted writes before stopping; cancellation cannot interrupt this step."); } catch (...) {}
            sync_fd(target_fd);
        }
        throw;
    }
}

SelfTestResult run_self_test(const ProgressFn& progress, const std::atomic_bool& cancel) {
    try {
        cancelled(cancel);
        struct Scratch {
            std::string path;
            Fd directory;
            Scratch() {
                std::error_code error;
                const auto base = std::filesystem::temp_directory_path(error);
                if (error) throw Error("The temporary folder is unavailable: " + error.message());
                const std::string name = (base / "kavrune-self-test-XXXXXX").string();
                std::vector<char> pattern(name.begin(), name.end());
                pattern.push_back('\0');
                const char* created = ::mkdtemp(pattern.data());
                if (!created) system_error("Could not create a private temporary test folder");
                path = created;
                directory = Fd(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
                if (directory.get() < 0) {
                    const int saved = errno;
                    ::rmdir(path.c_str());
                    errno = saved;
                    system_error("Could not open the temporary test folder");
                }
            }
            ~Scratch() {
                // Only remove the two entries created inside our private directory.
                // Never recursively remove caller-provided paths or follow links.
                ::unlinkat(directory.get(), "source.img", 0);
                ::unlinkat(directory.get(), "written.img", 0);
                ::rmdir(path.c_str());
            }
        } scratch;
        constexpr uint64_t bytes = 8ULL * 1024 * 1024 + 37;
        const std::string source_path = scratch.path + "/source.img";
        const std::string target_path = scratch.path + "/written.img";
        Fd source(::openat(scratch.directory.get(), "source.img", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
        if (source.get() < 0) system_error("Could not create the temporary test image");
        Fd target(::openat(scratch.directory.get(), "written.img", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
        if (target.get() < 0) system_error("Could not create the temporary test destination");
        if (!S_ISREG(file_stat(source.get()).st_mode) || !S_ISREG(file_stat(target.get()).st_mode))
            throw Error("The safe test requires ordinary temporary files.");
        if (::ftruncate(target.get(), static_cast<off_t>(bytes)) != 0)
            system_error("Could not reserve space for the temporary test");
        Reporter reporter(progress);
        reporter.emit("Preparing test", 0, bytes, "Creating sample data in a private temporary folder. No USB drive is changed.");
        std::vector<unsigned char> buffer(chunk_size);
        for (uint64_t offset = 0; offset < bytes;) {
            cancelled(cancel);
            const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), bytes - offset));
            for (size_t index = 0; index < count; ++index) {
                const uint64_t position = offset + index;
                buffer[index] = static_cast<unsigned char>((position * 37U + (position >> 12U) + (position >> 20U)) & 0xffU);
            }
            // A deliberately unstructured .img is a raw-copy fixture, not a bootable OS.
            if (offset == 0) std::fill(buffer.begin(), buffer.begin() + 1024, 0);
            write_exact(source.get(), buffer.data(), count, offset);
            offset += count;
            reporter.emit("Preparing test", offset, bytes);
        }
        sync_fd(source.get());
        Image image = inspect_image(source_path);
        const std::string expected = checksum(source_path, "SHA256", {}, cancel);
        image.expected_sha256 = expected;
        const ProgressFn writing = [&](const Progress& status) {
            if (progress && status.stage != "Complete") progress(status);
        };
        const std::string written = write_image(image, target.get(), bytes, true, writing, cancel);
        const std::string actual = checksum(target_path, "SHA256", {}, cancel);
        if (written != expected || actual != expected)
            throw Error("The safe test failed: the copied sample does not match the original.");
        cancelled(cancel);
        reporter.emit("Complete", bytes, bytes, "Safe test passed. Writing and verification work with temporary files; no USB drive was changed.");
        return {bytes, actual};
    } catch (const Cancelled&) {
        throw Cancelled("Safe test cancelled. Temporary test files were removed; no USB drive was changed.");
    }
}

std::string backup_image(int source_fd, uint64_t capacity, const std::string& destination, const ProgressFn& progress, const std::atomic_bool& cancel) {
    cancelled(cancel);
    validate_descriptor(source_fd, capacity, false);
    const auto initial_source = file_stat(source_fd);
    const auto destination_path = std::filesystem::path(destination);
    const auto filename = destination_path.filename().string();
    if (filename.empty() || filename == "." || filename == "..") throw Error("Choose a file name for the backup.");
    const auto parent_path = destination_path.has_parent_path() ? destination_path.parent_path() : std::filesystem::path(".");
    Fd parent(::open(parent_path.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY));
    if (parent.get() < 0) system_error("Could not open the backup destination directory");
    const auto parent_identity = file_stat(parent.get());
    Fd output(::openat(parent.get(), filename.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL | O_NOFOLLOW, 0600));
    if (output.get() < 0) system_error("Could not create the backup (existing files are never overwritten)");
    const auto created = file_stat(output.get());
    bool complete = false;
    try {
        Reporter reporter(progress);
        Digest digest("SHA-256");
        std::vector<unsigned char> buffer(chunk_size);
        reporter.emit("Backing up", 0, capacity);
        for (uint64_t offset = 0; offset < capacity;) {
            cancelled(cancel);
            const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), capacity - offset));
            read_exact(source_fd, buffer.data(), count, offset);
            write_exact(output.get(), buffer.data(), count, offset);
            digest.update(buffer.data(), count);
            offset += count;
            reporter.emit("Backing up", offset, capacity);
        }
        validate_descriptor(source_fd, capacity, false);
        if (S_ISREG(initial_source.st_mode)) {
            const auto final_source = file_stat(source_fd);
            if (final_source.st_dev != initial_source.st_dev || final_source.st_ino != initial_source.st_ino ||
                final_source.st_size != initial_source.st_size ||
                final_source.st_mtim.tv_sec != initial_source.st_mtim.tv_sec || final_source.st_mtim.tv_nsec != initial_source.st_mtim.tv_nsec ||
                final_source.st_ctim.tv_sec != initial_source.st_ctim.tv_sec || final_source.st_ctim.tv_nsec != initial_source.st_ctim.tv_nsec)
                throw Error("The backup source changed while it was being read. The incomplete backup was discarded.");
        }
        const auto validate_destination = [&] {
            struct stat current_parent {};
            if (::stat(parent_path.c_str(), &current_parent) != 0 || !S_ISDIR(current_parent.st_mode) ||
                current_parent.st_dev != parent_identity.st_dev || current_parent.st_ino != parent_identity.st_ino)
                throw Error("The backup destination folder was moved or replaced while saving. No replacement file will be removed.");
            struct stat current_destination {};
            if (::fstatat(parent.get(), filename.c_str(), &current_destination, AT_SYMLINK_NOFOLLOW) != 0 ||
                current_destination.st_dev != created.st_dev || current_destination.st_ino != created.st_ino || !S_ISREG(current_destination.st_mode))
                throw Error("The backup destination was moved or replaced while saving. No replacement file will be removed.");
        };
        validate_destination();
        reporter.emit("Synchronizing", capacity, capacity);
        sync_fd(output.get());
        cancelled(cancel);
        sync_fd(parent.get());
        // Saving can block for a long time. Do not claim the chosen path contains
        // this backup if its file or containing folder changed during that wait.
        validate_destination();
        const auto result = digest.finish();
        reporter.emit("Complete", capacity, capacity, "Raw backup created with SHA-256 checksum.");
        complete = true;
        return result;
    } catch (...) {
        if (!complete) {
            struct stat current {};
            if (::fstatat(parent.get(), filename.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
                current.st_dev == created.st_dev && current.st_ino == created.st_ino && S_ISREG(current.st_mode))
                ::unlinkat(parent.get(), filename.c_str(), 0);
        }
        throw;
    }
}

void read_scan(int source_fd, uint64_t capacity, const ProgressFn& progress, const std::atomic_bool& cancel) {
    cancelled(cancel);
    validate_descriptor(source_fd, capacity, false);
    Reporter reporter(progress);
    std::vector<unsigned char> buffer(chunk_size);
    reporter.emit("Scanning", 0, capacity, "Read-only scan detects read errors; it does not prove physical capacity or data integrity.");
    for (uint64_t offset = 0; offset < capacity;) {
        cancelled(cancel);
        const size_t count = static_cast<size_t>(std::min<uint64_t>(buffer.size(), capacity - offset));
        read_exact(source_fd, buffer.data(), count, offset);
        offset += count;
        reporter.emit("Scanning", offset, capacity);
    }
    cancelled(cancel);
    reporter.emit("Complete", capacity, capacity, "All requested bytes were readable. This is not a fake-capacity test.");
}

std::string human_size(uint64_t bytes) {
    constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024 && unit < 6) { value /= 1024; ++unit; }
    std::ostringstream output;
    output << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << ' ' << units[unit];
    return output.str();
}

std::string Device::display_name() const {
    std::string product = vendor;
    if (!model.empty()) { if (!product.empty()) product += ' '; product += model; }
    if (product.empty()) product = "USB storage";
    return product + " — " + human_size(size) + " (" + path + ")";
}

std::string log_path() {
    std::lock_guard<std::mutex> lock(log_mutex);
    auto directory = secure_state_directory();
    return state_directory() + "/events.log";
}

void log_event(const std::string& message, LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex);
    LogSession session;
    Fd output = open_log_entry(session.directory.get(), "events.log", true);
    std::time_t now = std::time(nullptr);
    struct tm calendar {};
    ::gmtime_r(&now, &calendar);
    std::ostringstream line;
    const char* severity = level == LogLevel::error ? "error" : level == LogLevel::warning ? "warning" : "info";
    line << std::put_time(&calendar, "%Y-%m-%dT%H:%M:%SZ") << " level=" << severity << " message=";
    const auto prefix = line.str();
    const auto bytes = prefix + clean_diagnostic_text(message, log_record_limit - prefix.size() - 1) + '\n';
    auto size = file_stat(output.get()).st_size;
    if (size < 0) throw Error("The diagnostics size is invalid.");
    if (static_cast<uint64_t>(size) + bytes.size() > log_limit) {
        // Fixed filenames plus pinned descriptors avoid rename/unlink races.
        // Keep one bounded previous generation; trim oversized legacy logs
        // without ever allocating memory proportional to their claimed size.
        Fd previous = open_log_entry(session.directory.get(), "events.log.1", true);
        const auto retained = bounded_log_tail(output.get());
        session.validate();
        validate_log_entry(session.directory.get(), "events.log", output.get());
        validate_log_entry(session.directory.get(), "events.log.1", previous.get());
        truncate_log(previous.get());
        if (!retained.empty()) write_exact(previous.get(), retained.data(), retained.size(), 0);
        truncate_log(output.get());
        size = 0;
    }
    session.validate();
    validate_log_entry(session.directory.get(), "events.log", output.get());
    write_exact(output.get(), bytes.data(), bytes.size(), static_cast<uint64_t>(size));
}

std::string read_diagnostics() {
    std::lock_guard<std::mutex> lock(log_mutex);
    LogSession session;
    Fd previous = open_log_entry(session.directory.get(), "events.log.1", false);
    Fd current = open_log_entry(session.directory.get(), "events.log", false);
    std::string report = "Kavrune " KAVRUNE_VERSION " — local diagnostics\n"
        "Review before sharing: records may contain private filenames, paths, device identifiers and checksums.\n"
        "Retention: current and previous logs, up to 256 KiB each; no age-based expiry. Nothing is uploaded.\n\n";
    bool has_records = false;
    for (const auto* file : {&previous, &current}) {
        if (file->get() < 0) continue;
        const auto content = bounded_log_tail(file->get());
        if (!content.empty()) {
            report += file == &previous ? "Previous activity\n" : "Current activity\n";
            report += clean_diagnostic_text(content, log_limit, true);
            if (report.back() != '\n') report += '\n';
            has_records = true;
        }
    }
    session.validate();
    if (previous.get() >= 0) validate_log_entry(session.directory.get(), "events.log.1", previous.get());
    if (current.get() >= 0) validate_log_entry(session.directory.get(), "events.log", current.get());
    if (!has_records) report += "No saved activity.\n";
    return report;
}

void clear_diagnostics() {
    std::lock_guard<std::mutex> lock(log_mutex);
    LogSession session;
    Fd current = open_log_entry(session.directory.get(), "events.log", false);
    Fd previous = open_log_entry(session.directory.get(), "events.log.1", false);
    // Open, validate and lock both before changing either. This also prevents
    // clearing one log when a second entry is an unsafe replacement.
    session.validate();
    if (current.get() >= 0) validate_log_entry(session.directory.get(), "events.log", current.get());
    if (previous.get() >= 0) validate_log_entry(session.directory.get(), "events.log.1", previous.get());
    if (current.get() >= 0) truncate_log(current.get());
    if (previous.get() >= 0) truncate_log(previous.get());
    session.validate();
    if (current.get() >= 0) validate_log_entry(session.directory.get(), "events.log", current.get());
    if (previous.get() >= 0) validate_log_entry(session.directory.get(), "events.log.1", previous.get());
}

void export_diagnostics(const std::string& destination) {
    const auto report = read_diagnostics();
    const auto path = std::filesystem::path(destination);
    const auto filename = path.filename().string();
    if (filename.empty() || filename == "." || filename == ".." || destination.find('\0') != std::string::npos)
        throw Error("Choose a valid filename for the diagnostics report.");
    const auto parent_path = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    Fd parent(::open(parent_path.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY));
    if (parent.get() < 0) system_error("Could not open the report destination folder");
    const auto parent_identity = file_stat(parent.get());
    Fd output(::openat(parent.get(), filename.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600));
    if (output.get() < 0) system_error("Could not create the report (existing files are never overwritten)");
    const auto created = file_stat(output.get());
    try {
        const auto validate = [&] {
            struct stat directory {}, current {};
            const auto opened = file_stat(output.get());
            if (!S_ISREG(opened.st_mode) || opened.st_uid != ::geteuid() || opened.st_nlink != 1 ||
                ::stat(parent_path.c_str(), &directory) != 0 || !S_ISDIR(directory.st_mode) || !same_file(directory, parent_identity) ||
                ::fstatat(parent.get(), filename.c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0 ||
                !S_ISREG(current.st_mode) || !same_file(current, created))
                throw Error("The report destination was moved, linked or replaced. Choose another filename and try again.");
        };
        validate();
        if (::fchmod(output.get(), 0600) != 0) system_error("Could not secure report permissions");
        write_exact(output.get(), report.data(), report.size(), 0);
        sync_fd(output.get());
        validate();
        sync_fd(parent.get());
        validate();
    } catch (...) {
        struct stat current {};
        if (::fstatat(parent.get(), filename.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
            same_file(current, created) && S_ISREG(current.st_mode) && current.st_nlink == 1)
            ::unlinkat(parent.get(), filename.c_str(), 0);
        throw;
    }
}
}
