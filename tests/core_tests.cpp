// SPDX-License-Identifier: MIT
// All destructive I/O in this suite uses disposable regular files, never devices.
#include "kavrune/core.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace kavrune;
namespace {
int assertions = 0;
void check(bool condition, const std::string& message) {
    ++assertions;
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void fails(Function&& operation, const std::string& description) {
    bool failed = false;
    try { operation(); } catch (const Error&) { failed = true; }
    check(failed, description);
}
struct Descriptor {
    int fd;
    explicit Descriptor(const std::filesystem::path& path, int flags = O_RDWR) : fd(::open(path.c_str(), flags | O_CLOEXEC)) {
        if (fd < 0) throw std::runtime_error("could not open test file");
    }
    ~Descriptor() { ::close(fd); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
};
struct Temp {
    std::filesystem::path path;
    Temp() {
        std::array<char, 64> pattern {};
        std::strcpy(pattern.data(), "/tmp/kavrune-core-test-XXXXXX");
        char* created = ::mkdtemp(pattern.data());
        if (!created) throw std::runtime_error("mkdtemp failed");
        path = created;
    }
    ~Temp() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    std::filesystem::path file(const std::string& name) const { return path / name; }
};
void put(const std::filesystem::path& path, const std::vector<unsigned char>& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!output) throw std::runtime_error("fixture write failed");
}
std::vector<unsigned char> get(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (size < 0) throw std::runtime_error("fixture read size failed");
    std::vector<unsigned char> data(static_cast<size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!input) throw std::runtime_error("fixture read failed");
    return data;
}
std::vector<unsigned char> pattern(size_t size) {
    std::vector<unsigned char> data(size);
    for (size_t i = 0; i < size; ++i) data[i] = static_cast<unsigned char>((i * 17 + (i / 251)) % 256);
    return data;
}
void le16(unsigned char* p, uint16_t value) { p[0] = value & 255; p[1] = value >> 8; }
void le32(unsigned char* p, uint32_t value) { for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (i * 8)); }
void be32(unsigned char* p, uint32_t value) { for (int i = 0; i < 4; ++i) p[3 - i] = static_cast<unsigned char>(value >> (i * 8)); }
void le64(unsigned char* p, uint64_t value) { for (int i = 0; i < 8; ++i) p[i] = static_cast<unsigned char>(value >> (i * 8)); }
size_t iso_record(unsigned char* p, const std::string& name, uint32_t extent, uint32_t bytes, bool directory) {
    const auto length = static_cast<unsigned char>(33 + name.size() + (name.size() % 2 == 0 ? 1 : 0));
    p[0] = length;
    le32(p + 2, extent); be32(p + 6, extent);
    le32(p + 10, bytes); be32(p + 14, bytes);
    p[25] = directory ? 2 : 0;
    p[28] = 1; p[31] = 1;
    p[32] = static_cast<unsigned char>(name.size());
    std::memcpy(p + 33, name.data(), name.size());
    return length;
}
std::vector<unsigned char> iso_fixture(bool hybrid, bool windows = false, bool bootable = false) {
    std::vector<unsigned char> data(32 * 2048);
    auto* pvd = data.data() + 16 * 2048;
    pvd[0] = 1; std::memcpy(pvd + 1, "CD001", 5); pvd[6] = 1;
    std::memcpy(pvd + 40, "ORIGINAL TEST IMAGE", 19);
    le32(pvd + 80, 32); be32(pvd + 84, 32);
    le16(pvd + 128, 2048); pvd[130] = 8; pvd[131] = 0;
    iso_record(pvd + 156, std::string(1, '\0'), 20, 2048, true);
    auto* root = data.data() + 20 * 2048;
    size_t pos = iso_record(root, std::string(1, '\0'), 20, 2048, true);
    pos += iso_record(root + pos, std::string(1, '\1'), 20, 2048, true);
    if (windows) {
        pos += iso_record(root + pos, "SOURCES", 21, 2048, true);
        iso_record(root + pos, "BOOTMGR;1", 23, 16, false);
        iso_record(data.data() + 21 * 2048, "INSTALL.WIM;1", 24, 16, false);
    }
    const size_t end_sector = bootable ? 18 : 17;
    auto* terminator = data.data() + end_sector * 2048;
    terminator[0] = 255; std::memcpy(terminator + 1, "CD001", 5); terminator[6] = 1;
    if (hybrid) {
        data[510] = 0x55; data[511] = 0xaa;
        data[446] = 0x80; data[450] = 0x17;
        le32(data.data() + 446 + 8, 0); le32(data.data() + 446 + 12, static_cast<uint32_t>(data.size() / 512));
    }
    if (bootable) {
        auto* descriptor = data.data() + 17 * 2048;
        std::memcpy(descriptor + 1, "CD001", 5); descriptor[6] = 1;
        std::memcpy(descriptor + 7, "EL TORITO SPECIFICATION", 22);
        le32(descriptor + 71, 22);
        auto* catalog = data.data() + 22 * 2048;
        catalog[0] = 1; catalog[30] = 0x55; catalog[31] = 0xaa;
        le16(catalog + 28, static_cast<uint16_t>(0U - 1U - 0xaa55U));
        catalog[32] = 0x88; le16(catalog + 38, 4); le32(catalog + 40, 23);
    }
    return data;
}
uint32_t crc32(const unsigned char* bytes, size_t count) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
std::vector<unsigned char> gpt_fixture() {
    std::vector<unsigned char> data(256 * 512);
    data[510] = 0x55; data[511] = 0xaa; data[450] = 0xee;
    le32(data.data() + 454, 1); le32(data.data() + 458, 255);
    auto* header = data.data() + 512;
    std::memcpy(header, "EFI PART", 8);
    le32(header + 8, 0x10000); le32(header + 12, 92);
    le64(header + 24, 1); le64(header + 32, 255);
    le64(header + 40, 34); le64(header + 48, 222);
    le64(header + 72, 2); le32(header + 80, 128); le32(header + 84, 128);
    auto* entry = data.data() + 1024; entry[0] = 1;
    le64(entry + 32, 34); le64(entry + 40, 222);
    le32(header + 88, crc32(entry, 16384));
    le32(header + 16, crc32(header, 92));
    return data;
}

void inspection_tests(const Temp& temp) {
    const auto empty = temp.file("empty.img"); put(empty, {});
    fails([&] { inspect_image(empty); }, "empty image must fail");
    fails([&] { inspect_image(temp.file("missing.img")); }, "missing image must fail");
    fails([&] { inspect_image(temp.path); }, "directory source must fail");
    const auto opaque = temp.file("opaque.img"); put(opaque, pattern(333));
    const auto raw = inspect_image(opaque);
    check(raw.raw_writable && !raw.bootable && raw.size == 333 && raw.kind == "Opaque raw disk image", "raw image description");
    const auto link = temp.file("link.img"); std::filesystem::create_symlink(opaque, link);
    fails([&] { inspect_image(link); }, "symlink image must fail");
    const auto fifo = temp.file("fifo.img"); check(::mkfifo(fifo.c_str(), 0600) == 0, "fixture fifo creation");
    fails([&] { inspect_image(fifo); }, "fifo must fail without waiting");
    const auto invalid_iso = temp.file("invalid.iso"); put(invalid_iso, pattern(333));
    fails([&] { inspect_image(invalid_iso); }, "arbitrary ISO filename must fail");
    const auto unknown = temp.file("data.txt"); put(unknown, pattern(333));
    fails([&] { inspect_image(unknown); }, "opaque files require raw suffix");
    const auto optical = temp.file("optical.iso"); put(optical, iso_fixture(false));
    check(!inspect_image(optical).raw_writable, "nonhybrid optical ISO is rejected for raw write");
    const auto hybrid = temp.file("content.different-name"); put(hybrid, iso_fixture(true, false, true));
    const auto hybrid_info = inspect_image(hybrid);
    check(hybrid_info.raw_writable && hybrid_info.bootable && hybrid_info.kind == "Hybrid ISO", "hybrid detected by content including El Torito");
    const auto windows = temp.file("linux-name.iso"); put(windows, iso_fixture(false, true));
    const auto windows_info = inspect_image(windows);
    check(windows_info.kind == "Windows installation ISO" && !windows_info.raw_writable, "Windows markers detected without relying on name");
    const auto hybrid_windows = temp.file("hybrid-windows.iso"); put(hybrid_windows, iso_fixture(true, true));
    check(!inspect_image(hybrid_windows).raw_writable, "Windows extracted-media support is gated");
    auto broken = iso_fixture(true); broken[16 * 2048 + 84] ^= 1;
    const auto corrupt = temp.file("corrupt.iso"); put(corrupt, broken);
    fails([&] { inspect_image(corrupt); }, "ISO dual endian mismatch must fail");
    broken = iso_fixture(true); le32(broken.data() + 16 * 2048 + 158, UINT32_MAX); be32(broken.data() + 16 * 2048 + 162, UINT32_MAX);
    put(corrupt, broken); fails([&] { inspect_image(corrupt); }, "ISO extent overflow must fail");
    broken = iso_fixture(true, false, true); broken[22 * 2048 + 28] ^= 1;
    put(corrupt, broken); fails([&] { inspect_image(corrupt); }, "invalid El Torito checksum must fail");
    broken = iso_fixture(true); std::fill(broken.begin() + 17 * 2048, broken.begin() + 18 * 2048, 0);
    put(corrupt, broken); fails([&] { inspect_image(corrupt); }, "ISO descriptor termination must be validated");
    std::vector<unsigned char> gzip{0x1f, 0x8b, 0x08, 0}; put(corrupt, gzip);
    fails([&] { inspect_image(corrupt); }, "gzip rejected by signature");
    const auto compressed = temp.file("arbitrary.img.xz"); put(compressed, pattern(44));
    fails([&] { inspect_image(compressed); }, "compressed extensions rejected");
    put(corrupt, {'Q', 'F', 'I', 0xfb}); fails([&] { inspect_image(corrupt); }, "qcow container rejected");
    const auto gpt = temp.file("gpt.img"); put(gpt, gpt_fixture());
    check(inspect_image(gpt).kind == "GPT disk image", "valid GPT detected");
    broken = gpt_fixture(); broken[512 + 16] ^= 1; put(gpt, broken);
    fails([&] { inspect_image(gpt); }, "GPT header CRC invalid rejected");
    broken = gpt_fixture(); broken[1024] ^= 1; put(gpt, broken);
    fails([&] { inspect_image(gpt); }, "GPT entry CRC invalid rejected");
    auto udf = std::vector<unsigned char>(20 * 2048); std::memcpy(udf.data() + 16 * 2048 + 1, "BEA01", 5);
    const auto udf_path = temp.file("udf.iso"); put(udf_path, udf);
    check(!inspect_image(udf_path).raw_writable, "UDF content recognized but write gated");
}

void checksum_tests(const Temp& temp) {
    std::atomic_bool cancel = false;
    const auto file = temp.file("abc.img"); put(file, {'a', 'b', 'c'});
    check(checksum(file, "SHA-256", {}, cancel) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 reference vector");
    check(checksum(file, "SHA-512", {}, cancel) == "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", "SHA-512 reference vector");
    check(checksum(file, "MD5", {}, cancel) == "900150983cd24fb0d6963f7d28e17f72", "MD5 comparison vector");
    check(checksum(file, "SHA-1", {}, cancel) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 comparison vector");
    fails([&] { checksum(file, "CRC32", {}, cancel); }, "unsupported algorithm fails");
    cancel = true;
    fails([&] { checksum(file, "SHA-256", {}, cancel); }, "pre-cancelled checksum fails");
    cancel = false;
    fails([&] { checksum(file, "SHA-256", [&](const Progress& p) { if (p.done) put(file, {'c', 'b', 'a', 'd'}); }, cancel); }, "mutation during checksum fails");
}

void writing_tests(const Temp& temp) {
    std::atomic_bool cancel = false;
    const auto source_path = temp.file("source.img"), target_path = temp.file("target.img");
    const auto source_data = pattern(9 * 1024 * 1024 + 17);
    const auto target_data = std::vector<unsigned char>(source_data.size() + 37, 0xf5);
    put(source_path, source_data); put(target_path, target_data);
    auto source = inspect_image(source_path);
    Descriptor target(target_path);
    std::vector<std::string> stages;
    const auto hash = write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) { stages.push_back(p.stage); }, cancel);
    auto written = get(target_path);
    check(std::equal(source_data.begin(), source_data.end(), written.begin()), "multi-chunk image was copied exactly");
    check(std::all_of(written.begin() + static_cast<std::ptrdiff_t>(source_data.size()), written.end(), [](unsigned char c) { return c == 0xf5; }), "target tail is untouched");
    check(hash == checksum(source_path, "SHA-256", {}, cancel), "writer returns source SHA-256");
    check(std::find(stages.begin(), stages.end(), "Synchronizing") != stages.end() && stages.back() == "Complete", "sync and complete stages reported");
    check(::fcntl(target.fd, F_GETFD) >= 0, "caller retains target fd ownership");
    put(target_path, target_data);
    fails([&] { write_image(source, target.fd, source.size - 1, true, {}, cancel); }, "oversized source fails");
    check(get(target_path) == target_data, "oversize validation occurs before writing");
    fails([&] { write_image(source, target.fd, target_data.size() + 1, true, {}, cancel); }, "fake file capacity fails");
    check(get(target_path) == target_data, "declared capacity cannot expand file");
    Descriptor same(source_path);
    fails([&] { write_image(source, same.fd, source.size, true, {}, cancel); }, "source target same inode fails");
    auto disabled = source; disabled.raw_writable = false;
    fails([&] { write_image(disabled, target.fd, target_data.size(), true, {}, cancel); }, "non-raw-writable image fails");
    Descriptor readonly(target_path, O_RDONLY), append(target_path, O_RDWR | O_APPEND), writeonly(target_path, O_WRONLY);
    fails([&] { write_image(source, readonly.fd, target_data.size(), true, {}, cancel); }, "readonly target fails");
    fails([&] { write_image(source, append.fd, target_data.size(), true, {}, cancel); }, "append target fails");
    fails([&] { write_image(source, writeonly.fd, target_data.size(), true, {}, cancel); }, "verification requires readable fd before write");
    check(get(target_path) == target_data, "access validation occurs before writing");
    cancel = true;
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "pre-cancelled writer fails");
    check(get(target_path) == target_data, "pre-cancelled writer leaves target unchanged");
    cancel = false;
    bool cancelled_exception = false;
    try { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) { if (p.stage == "Writing" && p.done) cancel = true; }, cancel); }
    catch (const Cancelled&) { cancelled_exception = true; }
    check(cancelled_exception, "cancel during write reports Cancelled");
    check(get(target_path).size() == target_data.size(), "cancelled write does not expand target");
    cancel = false; put(target_path, target_data);
    fails([&] { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) {
        if (p.stage == "Verifying" && p.done == 0) { const unsigned char byte = 0xff; check(::pwrite(target.fd, &byte, 1, 10) == 1, "test corruption injection"); }
    }, cancel); }, "readback corruption must fail");
    put(target_path, target_data);
    bool sync_seen = false;
    fails([&] { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) {
        if (p.stage == "Synchronizing") { sync_seen = true; cancel = true; }
    }, cancel); }, "cancel during synchronization waits then fails");
    check(sync_seen && std::equal(source_data.begin(), source_data.end(), get(target_path).begin()), "uncancellable sync occurs after full write");
    cancel = false; put(target_path, target_data);
    // Mutation after inspection must fail before target I/O.
    put(source_path, pattern(source_data.size() + 1));
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "source growth invalidates plan");
    check(get(target_path) == target_data, "changed source leaves target unchanged");
    put(source_path, source_data); source = inspect_image(source_path);
    const auto alternate = temp.file("replacement.img"); put(alternate, source_data);
    std::filesystem::rename(alternate, source_path);
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "inode replacement invalidates plan");
    check(get(target_path) == target_data, "inode replacement leaves target unchanged");
    source = inspect_image(source_path);
    fails([&] { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) {
        if (p.stage == "Writing" && p.done == 0) put(source_path, {'a'});
    }, cancel); }, "callback mutation before first byte revalidated");
    check(get(target_path) == target_data, "callback mutation before write is safe");
    put(source_path, source_data); source = inspect_image(source_path);
    fails([&] { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) {
        if (p.stage == "Writing" && p.done == 0) check(::ftruncate(target.fd, 1) == 0, "target shrink fixture");
    }, cancel); }, "target capacity changed in initial callback must fail before write");
    check(get(target_path).size() == 1, "target shrink does not cause subsequent expansion");
    put(target_path, target_data);
    put(source_path, source_data); source = inspect_image(source_path);
    fails([&] { write_image(source, target.fd, target_data.size(), false, [&](const Progress& p) {
        if (p.stage == "Writing" && p.done == source.size) put(source_path, pattern(source_data.size() + 1));
    }, cancel); }, "mutation after write pass prevents reported success");
    put(source_path, source_data); source = inspect_image(source_path);
    put(target_path, target_data);
    bool callback_error = false;
    try { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) { if (p.stage == "Writing" && p.done) throw std::runtime_error("test callback failure"); }, cancel); }
    catch (const std::runtime_error&) { callback_error = true; }
    check(callback_error, "callback failure unwinds safely");
    // A caller-provided bad descriptor is never used as a shell or arbitrary device path.
    fails([&] { write_image(source, -1, target_data.size(), true, {}, cancel); }, "invalid descriptor rejected");
    put(source_path, source_data); source = inspect_image(source_path);
    put(target_path, target_data);
    struct stat original {};
    check(::stat(source_path.c_str(), &original) == 0, "capture source timestamps");
    {
        Descriptor editable(source_path);
        const unsigned char changed = source_data[20] ^ 0xff;
        check(::pwrite(editable.fd, &changed, 1, 20) == 1, "same-size source mutation");
        const timespec times[] = {original.st_atim, original.st_mtim};
        check(::futimens(editable.fd, times) == 0, "restore source modification time");
    }
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "ctime detects same-size mutation with restored mtime");
    check(get(target_path) == target_data, "restored-mtime mutation leaves target unchanged");
    put(source_path, source_data); source = inspect_image(source_path);
    source.expected_sha256 = std::string(64, '0');
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "selected source fingerprint mismatch fails");
    check(get(target_path) == target_data, "fingerprint mismatch occurs before target writes");
    source.expected_sha256 = "invalid";
    fails([&] { write_image(source, target.fd, target_data.size(), true, {}, cancel); }, "malformed source fingerprint fails");
    check(get(target_path) == target_data, "malformed fingerprint leaves target unchanged");
    source.expected_sha256 = checksum(source_path, "SHA-256", {}, cancel);
    std::transform(source.expected_sha256.begin(), source.expected_sha256.end(), source.expected_sha256.begin(), [](unsigned char c) { return c >= 'a' && c <= 'f' ? static_cast<char>(c - 'a' + 'A') : static_cast<char>(c); });
    stages.clear();
    const auto bound_hash = write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) { stages.push_back(p.stage); }, cancel);
    check(bound_hash == hash && std::equal(source_data.begin(), source_data.end(), get(target_path).begin()), "valid uppercase fingerprint writes the selected image");
    check(!stages.empty() && stages.front() == "Checking source" && stages.back() == "Complete", "bound source is checked before writing");
    put(target_path, target_data);
    cancel = false;
    fails([&] { write_image(source, target.fd, target_data.size(), true, [&](const Progress& p) { if (p.stage == "Checking source" && p.done) cancel = true; }, cancel); }, "source fingerprint check can be cancelled");
    check(get(target_path) == target_data, "cancel during source check leaves target unchanged");
}

void backup_and_scan_tests(const Temp& temp) {
    std::atomic_bool cancel = false;
    const auto source_path = temp.file("backup-source.img"), backup_path = temp.file("saved.img");
    const auto data = pattern(4 * 1024 * 1024 + 13); put(source_path, data);
    Descriptor source(source_path, O_RDONLY);
    const auto hash = backup_image(source.fd, data.size(), backup_path, {}, cancel);
    check(get(backup_path) == data, "backup data matches descriptor");
    check(hash == checksum(source_path, "SHA256", {}, cancel), "backup checksum matches source");
    struct stat backup_stat {}; check(::stat(backup_path.c_str(), &backup_stat) == 0 && (backup_stat.st_mode & 0777) == 0600, "backup permissions are private");
    fails([&] { backup_image(source.fd, data.size(), backup_path, {}, cancel); }, "backup cannot overwrite existing file");
    check(get(backup_path) == data, "existing backup remains unchanged");
    const auto link = temp.file("backup-link.img"); std::filesystem::create_symlink(source_path, link);
    fails([&] { backup_image(source.fd, data.size(), link, {}, cancel); }, "backup destination symlink rejected");
    const auto partial = temp.file("partial.img");
    fails([&] { backup_image(source.fd, data.size(), partial, [&](const Progress& p) { if (p.done) cancel = true; }, cancel); }, "cancelled backup fails");
    check(!std::filesystem::exists(partial), "cancelled partial backup removed");
    cancel = false;
    fails([&] { backup_image(source.fd, data.size() + 1, partial, {}, cancel); }, "backup capacity too large fails before file creation");
    check(!std::filesystem::exists(partial), "invalid capacity does not create file");
    fails([&] { backup_image(source.fd, data.size(), partial, [&](const Progress& p) {
        if (p.stage == "Backing up" && p.done == data.size()) {
            Descriptor writable(source_path); const unsigned char byte = 0xff;
            check(::pwrite(writable.fd, &byte, 1, 0) == 1, "backup mutation fixture");
        }
    }, cancel); }, "regular backup source mutation prevents success");
    check(!std::filesystem::exists(partial), "changed backup source removes partial output");
    put(source_path, data);
    // If another actor replaces the partial path, cleanup must not unlink their file.
    const auto preserved = temp.file("preserved.img");
    fails([&] { backup_image(source.fd, data.size(), partial, [&](const Progress& p) {
        if (p.stage == "Backing up" && p.done) {
            std::filesystem::rename(partial, preserved); put(partial, {'s', 'a', 'f', 'e'}); cancel = true;
        }
    }, cancel); }, "cancel backup during destination replacement");
    check(get(partial) == std::vector<unsigned char>({'s', 'a', 'f', 'e'}), "cleanup preserves replacement inode");
    cancel = false;
    uint64_t scanned = 0;
    read_scan(source.fd, data.size(), [&](const Progress& p) { scanned = p.done; }, cancel);
    check(scanned == data.size() && get(source_path) == data, "read scan is complete and read only");
    fails([&] { read_scan(source.fd, data.size() + 1, {}, cancel); }, "read scan refuses fake capacity");
    fails([&] { read_scan(source.fd, data.size(), [&](const Progress& p) { if (p.done) cancel = true; }, cancel); }, "read scan supports cancellation");
    cancel = false;
    fails([&] { read_scan(source.fd, data.size(), [&](const Progress& p) { if (p.stage == "Scanning" && p.done == 0) { Descriptor writable(source_path); check(::ftruncate(writable.fd, 1) == 0, "scan shrink fixture"); } }, cancel); }, "read scan reports short device reads");
}

void log_and_utility_tests(const Temp& temp) {
    check(human_size(0) == "0 B" && human_size(1024) == "1.0 KiB" && human_size(1024ULL * 1024 * 1024) == "1.0 GiB", "binary capacity formatting");
    Device device; device.vendor = "Example"; device.model = "Storage"; device.path = "/dev/example"; device.size = 1024;
    check(device.display_name().find("Example Storage") != std::string::npos && device.display_name().find("1.0 KiB") != std::string::npos, "display identifies target model and size");
    const char* prior = std::getenv("XDG_STATE_HOME"); const std::string prior_value = prior ? prior : "";
    const auto state = temp.file("state"); check(::setenv("XDG_STATE_HOME", state.c_str(), 1) == 0, "test log state override");
    const auto path = log_path();
    log_event("test message\nforged line\r\x1b[31m");
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back([t] { for (int i = 0; i < 20; ++i) log_event("thread " + std::to_string(t)); });
    for (auto& thread : threads) thread.join();
    auto log = get(path);
    check(std::count(log.begin(), log.end(), '\n') == 81, "thread-safe logs cannot inject newline records");
    struct stat st {}; check(::stat(path.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600, "log file permissions");
    check(::stat((state / "kavrune").c_str(), &st) == 0 && (st.st_mode & 0777) == 0700, "log directory permissions");
    std::filesystem::remove(path); std::filesystem::create_symlink(temp.file("abc.img"), path);
    fails([&] { log_event("symlink log"); }, "log symlink rejected");
    if (prior) ::setenv("XDG_STATE_HOME", prior_value.c_str(), 1); else ::unsetenv("XDG_STATE_HOME");
}

void safe_self_test_tests(const Temp& temp) {
    const auto scratch = temp.file("self-tests");
    std::filesystem::create_directory(scratch);
    struct RestoreTemp {
        bool present;
        std::string previous;
        RestoreTemp() : present(std::getenv("TMPDIR") != nullptr), previous(present ? std::getenv("TMPDIR") : "") {}
        ~RestoreTemp() { if (present) ::setenv("TMPDIR", previous.c_str(), 1); else ::unsetenv("TMPDIR"); }
    } environment;
    check(::setenv("TMPDIR", scratch.c_str(), 1) == 0, "safe test uses an isolated temporary parent");
    std::atomic_bool cancel{false};
    std::set<std::string> stages;
    const auto result = run_self_test([&](const Progress& p) { stages.insert(p.stage); }, cancel);
    check(result.bytes == 8ULL * 1024 * 1024 + 37 && result.sha256.size() == 64, "safe test verifies an image larger than one streaming chunk");
    check(stages.contains("Writing") && stages.contains("Verifying"), "safe test exercises actual writing and readback");
    check(std::filesystem::is_empty(scratch), "successful safe test removes its temporary files and directory");
    fails([&] { run_self_test([&](const Progress& p) {
        if (p.stage == "Writing" && p.done) cancel = true;
    }, cancel); }, "safe test cancellation interrupts streaming");
    check(std::filesystem::is_empty(scratch), "cancelled safe test removes accepted partial output");
    cancel = false;
    fails([&] { run_self_test([](const Progress& p) {
        if (p.stage == "Preparing test" && p.done) throw Error("Injected test failure");
    }, cancel); }, "safe test reports preparation failure");
    check(std::filesystem::is_empty(scratch), "failed safe test leaves no sample or output files");
    cancel = true;
    try {
        run_self_test({}, cancel);
        check(false, "already-cancelled safe test must stop");
    } catch (const Cancelled& error) {
        check(std::string(error.what()).find("no USB drive was changed") != std::string::npos,
              "safe cancellation explains that no USB drive was changed");
    }
    check(std::filesystem::is_empty(scratch), "early cancellation creates no temporary media");
}
}

int main() {
    try {
        Temp temp;
        inspection_tests(temp);
        checksum_tests(temp);
        writing_tests(temp);
        backup_and_scan_tests(temp);
        log_and_utility_tests(temp);
        safe_self_test_tests(temp);
        std::cout << "Passed " << assertions << " core assertions; only temporary regular files were written.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Core test failed: " << error.what() << '\n';
        return 1;
    }
}
