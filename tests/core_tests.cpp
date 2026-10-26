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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
template<class Function> void image_fails(Function&& operation, ImageIssue issue, const std::string& description) {
    bool failed = false;
    try { operation(); }
    catch (const ImageError& error) {
        failed = error.issue() == issue && !error.advice().empty();
    }
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

std::vector<unsigned char> section_boot_iso_fixture(bool hybrid = true, bool windows = false) {
    auto data = iso_fixture(hybrid, windows, true);
    auto* catalog = data.data() + 22 * 2048;
    // No bootable default entry: the boot image appears in a final UEFI section.
    // This is the layout used by UEFI-only optical boot catalogs.
    std::fill_n(catalog + 32, 32, 0);
    catalog[64] = 0x91;
    catalog[65] = 0xef;
    le16(catalog + 66, 1);
    catalog[96] = 0x88;
    le16(catalog + 102, 4);
    le32(catalog + 104, 23);
    return data;
}

void udf_descriptor(unsigned char* descriptor, const char* identifier) {
    std::fill_n(descriptor, 2048, 0);
    std::memcpy(descriptor + 1, identifier, 5);
    descriptor[6] = 1;
}

std::vector<unsigned char> udf_bridge_fixture(bool before_iso, bool windows = false) {
    auto data = iso_fixture(true, windows, true);
    const auto descriptors = std::vector<unsigned char>(data.begin() + 16 * 2048, data.begin() + 19 * 2048);
    // Relocate the directory extents before using sectors 19 through 21 for
    // additional descriptors; the boot catalog and image stay at their LBAs.
    std::copy_n(data.data() + 20 * 2048, 2048, data.data() + 26 * 2048);
    auto* pvd = data.data() + 16 * 2048;
    le32(pvd + 158, 26); be32(pvd + 162, 26);
    auto* root = data.data() + 26 * 2048;
    le32(root + 2, 26); be32(root + 6, 26);
    const size_t parent = root[0];
    le32(root + parent + 2, 26); be32(root + parent + 6, 26);
    if (windows) {
        std::copy_n(data.data() + 21 * 2048, 2048, data.data() + 27 * 2048);
        const size_t sources = parent + root[parent];
        le32(root + sources + 2, 27); be32(root + sources + 6, 27);
    }
    if (before_iso) {
        std::copy_n(data.data() + 16 * 2048, 2048, data.data() + 19 * 2048);
        std::copy_n(descriptors.data() + 2048, 4096, data.data() + 20 * 2048);
        udf_descriptor(data.data() + 16 * 2048, "BEA01");
        udf_descriptor(data.data() + 17 * 2048, "NSR03");
        udf_descriptor(data.data() + 18 * 2048, "TEA01");
    } else {
        udf_descriptor(data.data() + 19 * 2048, "BEA01");
        udf_descriptor(data.data() + 20 * 2048, "NSR02");
        udf_descriptor(data.data() + 21 * 2048, "TEA01");
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

void refresh_isohybrid_gpt(std::vector<unsigned char>& data) {
    constexpr size_t table_bytes = 248 * 128;
    auto* header = data.data() + 512;
    le32(header + 88, crc32(data.data() + 1024, table_bytes));
    le32(header + 16, 0);
    le32(header + 16, crc32(header, 92));
    std::copy_n(data.data() + 1024, table_bytes, data.data() + 193 * 512);
    auto* backup = data.data() + 255 * 512;
    std::copy_n(header, 512, backup);
    le64(backup + 24, 255); le64(backup + 32, 1); le64(backup + 72, 193);
    le32(backup + 16, 0);
    le32(backup + 16, crc32(backup, 92));
}

std::vector<unsigned char> gpt_isohybrid_fixture(bool windows = false) {
    // Original small fixture: the ISO carrier deliberately includes its own
    // partition metadata, as used by some bootable ISO generators.
    auto data = iso_fixture(false, windows, true);
    data.resize(256 * 512);
    auto* pvd = data.data() + 16 * 2048;
    le32(pvd + 80, 64); be32(pvd + 84, 64);
    data[510] = 0x55; data[511] = 0xaa;
    data[446] = 0x80;
    le32(data.data() + 454, 0); le32(data.data() + 458, 256);
    data[466] = 0xef;
    le32(data.data() + 470, 96); le32(data.data() + 474, 8);
    auto* header = data.data() + 512;
    std::memcpy(header, "EFI PART", 8);
    le32(header + 8, 0x10000); le32(header + 12, 92);
    le64(header + 24, 1); le64(header + 32, 255);
    le64(header + 40, 64); le64(header + 48, 192);
    header[56] = 1;
    le64(header + 72, 2); le32(header + 80, 248); le32(header + 84, 128);
    constexpr std::array<unsigned char, 16> basic_data_guid{
        0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
        0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7};
    auto* carrier = data.data() + 1024;
    std::copy(basic_data_guid.begin(), basic_data_guid.end(), carrier);
    carrier[16] = 2;
    le64(carrier + 32, 0); le64(carrier + 40, 191);
    auto* efi = carrier + 128;
    std::copy(basic_data_guid.begin(), basic_data_guid.end(), efi);
    efi[16] = 3;
    le64(efi + 32, 96); le64(efi + 40, 103);
    refresh_isohybrid_gpt(data);
    return data;
}

void isohybrid_inspection_tests(const Temp& temp) {
    const auto image = temp.file("hybrid-gpt.img");
    const auto fixture = gpt_isohybrid_fixture();
    put(image, fixture);
    const auto info = inspect_image(image);
    check(info.kind == "Hybrid ISO" && info.raw_writable && info.bootable,
          "bootable GPT ISO with an active type-zero MBR carrier is supported");
    const auto reject = [&](const std::function<void(std::vector<unsigned char>&)>& modify,
                            const std::string& description, bool refresh = true) {
        auto broken = fixture;
        modify(broken);
        if (refresh) refresh_isohybrid_gpt(broken);
        put(image, broken);
        fails([&] { inspect_image(image); }, description);
    };
    reject([](auto& data) { std::fill_n(data.data() + 16 * 2048 + 1, 5, 0); },
           "a raw GPT prefix without a validated ISO cannot use the sector-zero exception");
    reject([](auto& data) { data[22 * 2048 + 32] = 0; },
           "a nonbootable ISO cannot use the sector-zero exception");
    reject([](auto& data) { data[446] = 0x7f; },
           "hybrid MBR carriers reject invalid boot flag values");
    reject([](auto& data) { le32(data.data() + 458, 255); },
           "the hybrid MBR carrier must cover the whole image");
    reject([](auto& data) { le32(data.data() + 458, 257); },
           "the hybrid MBR carrier cannot extend beyond the image");
    reject([](auto& data) { le32(data.data() + 470, 250); },
           "remaining hybrid MBR entries must also fit within the image");
    reject([](auto& data) { data[511] = 0; },
           "a hybrid MBR requires its signature");
    reject([](auto& data) { std::swap_ranges(data.data() + 1024, data.data() + 1152, data.data() + 1152); },
           "only the first GPT entry may be the sector-zero ISO carrier");
    reject([](auto& data) { le64(data.data() + 1024 + 32, 1); },
           "a nonzero GPT start below the usable range remains invalid");
    reject([](auto& data) { le64(data.data() + 1024 + 40, 63); },
           "the sector-zero GPT carrier must reach the usable range");
    reject([](auto& data) { le64(data.data() + 1024 + 40, 193); },
           "the sector-zero GPT carrier cannot exceed the last usable LBA");
    reject([](auto& data) { le64(data.data() + 1024 + 40, 256); },
           "a hybrid GPT partition cannot extend beyond the image");
    reject([](auto& data) {
        le64(data.data() + 1024 + 32, 96); le64(data.data() + 1024 + 40, 95);
    }, "reversed GPT partition bounds remain invalid in a hybrid ISO");
    reject([](auto& data) { data[512 + 16] ^= 1; },
           "hybrid recognition does not bypass the GPT header CRC", false);
    reject([](auto& data) { data[1024 + 16] ^= 1; },
           "hybrid recognition does not bypass the GPT table CRC", false);
    reject([](auto& data) { data[16 * 2048 + 84] ^= 1; },
           "hybrid recognition still rejects malformed ISO volume descriptors");
    reject([](auto& data) { data[22 * 2048 + 28] ^= 1; },
           "hybrid recognition still validates the El Torito catalog checksum");
    reject([](auto& data) { data.resize(data.size() - 1); },
           "truncated hybrid ISO images remain invalid", false);
    put(image, gpt_isohybrid_fixture(true));
    const auto windows = inspect_image(image);
    check(windows.kind == "Windows installation ISO" && !windows.raw_writable,
          "recognizing a GPT ISO carrier does not enable raw writing of Windows installers");
}

void compatibility_tests(const Temp& temp) {
    const auto file = temp.file("compatibility.iso");
    const auto inspect = [&](const std::vector<unsigned char>& data) {
        put(file, data);
        return inspect_image(file);
    };
    auto data = section_boot_iso_fixture();
    auto image = inspect(data);
    check(image.kind == "Hybrid ISO" && image.bootable && image.raw_writable,
          "a UEFI-only final El Torito section is a supported boot catalog");
    for (const uint16_t count : {0, 1}) {
        data = section_boot_iso_fixture();
        le16(data.data() + 22 * 2048 + 102, count);
        image = inspect(data);
        check(image.bootable && image.raw_writable,
              "UEFI section entries with zero or one load sector use the EFI whole-image convention");
        data = iso_fixture(true, false, true);
        auto* validation = data.data() + 22 * 2048;
        validation[1] = 0xef;
        le16(validation + 28, 0);
        uint16_t sum = 0;
        for (size_t i = 0; i < 32; i += 2)
            sum = static_cast<uint16_t>(sum + static_cast<uint16_t>(validation[i] | (validation[i + 1] << 8)));
        le16(validation + 28, static_cast<uint16_t>(0U - sum));
        le16(validation + 38, count);
        image = inspect(data);
        check(image.bootable && image.raw_writable,
              "UEFI default entries with zero or one load sector use the EFI whole-image convention");
    }
    data = section_boot_iso_fixture();
    // An earlier BIOS section with no boot image must not hide a later UEFI one.
    auto* catalog = data.data() + 22 * 2048;
    std::fill_n(catalog + 64, 128, 0);
    catalog[64] = 0x90;
    le16(catalog + 66, 1);
    catalog[128] = 0x91; catalog[129] = 0xef;
    le16(catalog + 130, 1);
    catalog[160] = 0x88; le16(catalog + 166, 4); le32(catalog + 168, 23);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "a UEFI boot image after an unbootable BIOS section is discovered");
    catalog[96] = 0x88; le16(catalog + 102, 4); le32(catalog + 104, 23);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "a catalog with BIOS and UEFI boot sections is supported");
    le32(catalog + 168, 32);
    put(file, data);
    fails([&] { inspect_image(file); },
          "an earlier valid BIOS boot entry does not bypass validation of a later invalid UEFI entry");
    data = section_boot_iso_fixture();
    data[22 * 2048 + 96] = 0;
    check(!inspect(data).bootable, "a section with no bootable entry is not reported bootable");
    const auto reject_catalog = [&](const std::function<void(unsigned char*)>& modify,
                                    const std::string& description) {
        auto broken = section_boot_iso_fixture();
        modify(broken.data() + 22 * 2048);
        put(file, broken);
        fails([&] { inspect_image(file); }, description);
    };
    reject_catalog([](auto* p) { p[96] = 0x89; }, "invalid section boot indicators are rejected");
    reject_catalog([](auto* p) { le32(p + 104, 32); }, "out-of-image UEFI boot extents are rejected");
    reject_catalog([](auto* p) { le16(p + 66, UINT16_MAX); }, "oversized El Torito section lists are rejected");
    reject_catalog([](auto* p) { p[64] = 0x92; }, "invalid El Torito section header indicators are rejected");
    reject_catalog([](auto* p) { p[28] ^= 1; }, "UEFI-only catalog validation checksums are enforced");
    for (const size_t extensions : {1, 2}) {
        data = section_boot_iso_fixture();
        auto* entries = data.data() + 22 * 2048;
        std::fill_n(entries + 64, 256, 0);
        entries[64] = 0x90;
        le16(entries + 66, 1);
        entries[96] = 0x88; entries[97] = 0x20;
        le16(entries + 102, 4); le32(entries + 104, 23);
        for (size_t i = 0; i < extensions; ++i) {
            entries[128 + 32 * i] = 0x44;
            entries[129 + 32 * i] = i + 1 < extensions ? 0x20 : 0;
        }
        auto* final_section = entries + 128 + 32 * extensions;
        final_section[0] = 0x91; final_section[1] = 0xef;
        le16(final_section + 2, 1);
        final_section[32] = 0x88;
        le16(final_section + 38, 4); le32(final_section + 40, 23);
        image = inspect(data);
        check(image.bootable && image.raw_writable,
              extensions == 1 ? "El Torito section counts exclude a boot entry's extension record before the next section" :
                                "El Torito chained extension records do not consume the section boot-entry count");
    }
    reject_catalog([](auto* p) { p[97] = 0x20; },
                   "a section boot entry that promises an absent extension is rejected");
    reject_catalog([](auto* p) { p[96] = 0x44; },
                   "an orphan extension cannot take the place of a section boot entry");
    reject_catalog([](auto* p) { p[97] = 0x20; p[128] = 0x44; p[129] = 0x20; },
                   "a catalog extension that promises a missing continuation is rejected");
    data = iso_fixture(true, false, true);
    le16(data.data() + 22 * 2048 + 38, 0);
    put(file, data);
    fails([&] { inspect_image(file); }, "BIOS boot entries still require a nonzero load-sector count");
    data = iso_fixture(true, false, true);
    data[450] = 0;
    image = inspect(data);
    check(image.bootable && image.raw_writable && image.kind == "Hybrid ISO",
          "a bootable ISO with an active full-image type-zero MBR carrier works without GPT");
    for (const unsigned char type : {0x17, 0x83, 0xef}) {
        data = gpt_isohybrid_fixture();
        data[450] = type;
        image = inspect(data);
        check(image.bootable && image.raw_writable,
              "a nonzero full-image MBR carrier type supports the hybrid GPT sector-zero convention");
    }
    for (bool gpt : {false, true}) {
        data = gpt ? gpt_isohybrid_fixture() : iso_fixture(true, false, true);
        data[450] = 0;
        le32(data.data() + 458, 1);
        image = inspect(data);
        check(image.bootable && image.raw_writable == gpt,
              gpt ? "a type-zero one-sector dummy MBR supports a validated bootable GPT ISO" :
                    "a type-zero one-sector dummy alone does not claim a usable USB disk layout");
    }
    data = gpt_isohybrid_fixture();
    constexpr std::array<unsigned char, 16> efi_system_guid{
        0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
        0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b};
    std::copy(efi_system_guid.begin(), efi_system_guid.end(), data.data() + 1024);
    refresh_isohybrid_gpt(data);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "an EFI System GPT carrier at sector zero is recognized for a validated bootable hybrid ISO");
    data = gpt_isohybrid_fixture();
    for (size_t i = 0; i < 16; ++i) data[1024 + i] = static_cast<unsigned char>(i + 1);
    refresh_isohybrid_gpt(data);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "a generator-selected nonzero GPT type GUID is supported for the validated ISO carrier");
    data = gpt_isohybrid_fixture();
    std::copy_n(data.data() + 1024, 256, data.data() + 1024 + 256);
    std::fill_n(data.data() + 1024, 256, 0);
    refresh_isohybrid_gpt(data);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "unused GPT entry slots before the first populated ISO carrier are supported");
    data = iso_fixture(true, false, true);
    std::swap_ranges(data.data() + 16 * 2048, data.data() + 17 * 2048, data.data() + 17 * 2048);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "an El Torito descriptor before the primary volume descriptor is supported");
    data = iso_fixture(true, false, true);
    std::copy_n(data.data() + 18 * 2048, 2048, data.data() + 19 * 2048);
    std::copy_n(data.data() + 17 * 2048, 2048, data.data() + 18 * 2048);
    auto* enhanced = data.data() + 17 * 2048;
    std::fill_n(enhanced, 2048, 0);
    enhanced[0] = 2; std::memcpy(enhanced + 1, "CD001", 5); enhanced[6] = 2;
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "an ISO type-two enhanced volume descriptor may use version two");
    enhanced[6] = 9;
    put(file, data);
    fails([&] { inspect_image(file); }, "unknown ISO descriptor versions are rejected");
    data = iso_fixture(true, false, true);
    std::copy_n(data.data() + 18 * 2048, 2048, data.data() + 19 * 2048);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "multiple consecutive volume descriptor terminators are valid ISO metadata");
    std::copy_n(data.data() + 16 * 2048, 2048, data.data() + 19 * 2048);
    put(file, data);
    image_fails([&] { inspect_image(file); }, ImageIssue::malformed,
                "ordinary ISO descriptors after a terminator remain invalid");
    for (bool before : {false, true}) {
        image = inspect(udf_bridge_fixture(before));
        check(image.kind == "Hybrid ISO" && image.bootable && image.raw_writable,
              before ? "a complete UDF recognition sequence before ISO descriptors is supported" :
                       "a complete UDF recognition sequence after ISO descriptors is supported");
        image = inspect(udf_bridge_fixture(before, true));
        check(image.kind == "Windows installation ISO" && !image.raw_writable,
              before ? "UDF descriptors before ISO do not bypass Windows installer gating" :
                       "UDF descriptors after ISO do not bypass Windows installer gating");
    }
    data = udf_bridge_fixture(true);
    std::copy_n(data.data() + 22 * 2048, 2048, data.data() + 28 * 2048);
    std::copy_backward(data.data() + 17 * 2048, data.data() + 22 * 2048, data.data() + 23 * 2048);
    udf_descriptor(data.data() + 17 * 2048, "BOOT2");
    le32(data.data() + 21 * 2048 + 71, 28);
    image = inspect(data);
    check(image.bootable && image.raw_writable,
          "a UDF recognition sequence permits an intervening BOOT2 descriptor");
    image = inspect(section_boot_iso_fixture(true, true));
    check(image.kind == "Windows installation ISO" && image.bootable && !image.raw_writable,
          "a UEFI-only Windows installer remains blocked for raw USB writing");
    image = inspect(section_boot_iso_fixture(false));
    check(image.kind == "Optical ISO" && image.bootable && !image.raw_writable,
          "a UEFI optical-only boot catalog does not claim raw USB compatibility without a disk layout");
    data = gpt_isohybrid_fixture();
    std::copy_n(section_boot_iso_fixture().data() + 22 * 2048, 2048, data.data() + 22 * 2048);
    image = inspect(data);
    check(image.raw_writable && image.bootable,
          "UEFI-only section catalogs enable a validated hybrid GPT carrier");
    const auto disguised = temp.file("truncated.raw");
    data = iso_fixture(true, false, true);
    data.resize(data.size() - 2048);
    put(disguised, data);
    fails([&] { inspect_image(disguised); }, "raw extensions do not bypass a truncated recognized ISO");
    data = iso_fixture(true, false, true);
    le32(data.data() + 16 * 2048 + 158, UINT32_MAX);
    be32(data.data() + 16 * 2048 + 162, UINT32_MAX);
    put(disguised, data);
    fails([&] { inspect_image(disguised); }, "raw extensions do not bypass invalid ISO directory bounds");
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
    fails([&] { identify_file(link); }, "checksum-only identity also refuses symbolic links");
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
    auto udf = std::vector<unsigned char>(20 * 2048);
    udf_descriptor(udf.data() + 16 * 2048, "BEA01");
    udf_descriptor(udf.data() + 17 * 2048, "NSR03");
    udf_descriptor(udf.data() + 18 * 2048, "TEA01");
    const auto udf_path = temp.file("udf.iso"); put(udf_path, udf);
    check(!inspect_image(udf_path).raw_writable, "UDF content recognized but write gated");
    std::fill_n(udf.data() + 18 * 2048, 2048, 0);
    put(udf_path, udf);
    image_fails([&] { inspect_image(udf_path); }, ImageIssue::malformed,
                "incomplete UDF recognition sequences receive an actionable malformed-image result");
    for (const std::string suffix : {".iso.crdownload", ".iso.part", ".iso.partial", ".iso.download"}) {
        const auto partial = temp.file("unfinished" + suffix);
        put(partial, iso_fixture(true, false, true));
        image_fails([&] { inspect_image(partial); }, ImageIssue::incomplete,
                    "browser and downloader temporary suffixes receive an actionable incomplete-download result");
    }
    put(compressed, pattern(44));
    image_fails([&] { inspect_image(compressed); }, ImageIssue::compressed,
                "compressed image formats receive a decompression action");
    put(corrupt, {'Q', 'F', 'I', 0xfb});
    image_fails([&] { inspect_image(corrupt); }, ImageIssue::container,
                "virtual disk containers receive a raw-conversion action");
    put(invalid_iso, pattern(333));
    image_fails([&] { inspect_image(invalid_iso); }, ImageIssue::unrecognized,
                "unrecognized content receives a specific image-selection action");
    broken = iso_fixture(true); broken[16 * 2048 + 84] ^= 1;
    put(corrupt, broken);
    image_fails([&] { inspect_image(corrupt); }, ImageIssue::malformed,
                "malformed ISO metadata receives a specific download-validation action");
    auto large_directory = iso_fixture(true);
    large_directory.resize(9 * 1024 * 1024);
    auto* large_pvd = large_directory.data() + 16 * 2048;
    le32(large_pvd + 80, static_cast<uint32_t>(large_directory.size() / 2048));
    be32(large_pvd + 84, static_cast<uint32_t>(large_directory.size() / 2048));
    le32(large_pvd + 166, 8 * 1024 * 1024 + 2048);
    be32(large_pvd + 170, 8 * 1024 * 1024 + 2048);
    put(corrupt, large_directory);
    image_fails([&] { inspect_image(corrupt); }, ImageIssue::inspection_limit,
                "oversized metadata receives an inspection-limit result instead of a corruption claim");
}

void checksum_tests(const Temp& temp) {
    std::atomic_bool cancel = false;
    const auto file = temp.file("abc.img"); put(file, {'a', 'b', 'c'});
    check(checksum(file, "SHA-256", {}, cancel) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 reference vector");
    check(checksum(file, "SHA-512", {}, cancel) == "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", "SHA-512 reference vector");
    check(checksum(file, "MD5", {}, cancel) == "900150983cd24fb0d6963f7d28e17f72", "MD5 comparison vector");
    check(checksum(file, "SHA-1", {}, cancel) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 comparison vector");
    const auto unknown = temp.file("checksum-only.txt"); put(unknown, {'a', 'b', 'c'});
    const auto identity = identify_file(unknown);
    check(identity.path == unknown && identity.name == "checksum-only.txt" && identity.size == 3 && identity.inode != 0,
          "checksum-only selection records the identity of unrecognized file contents");
    check(!identity.raw_writable && !identity.bootable && identity.kind.empty(),
          "identifying a checksum-only file cannot grant USB writing or claim recognized boot structures");
    check(checksum(unknown, "SHA-256", {}, cancel) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "an unrecognized file can still be checked against its publisher's checksum");
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
    const auto sync_destination = temp.file("sync-destination.img"), sync_preserved = temp.file("sync-preserved.img");
    fails([&] { backup_image(source.fd, data.size(), sync_destination, [&](const Progress& p) {
        if (p.stage == "Synchronizing") {
            std::filesystem::rename(sync_destination, sync_preserved);
            put(sync_destination, {'k', 'e', 'e', 'p'});
        }
    }, cancel); }, "backup destination replaced during synchronization cannot report success");
    check(get(sync_destination) == std::vector<unsigned char>({'k', 'e', 'e', 'p'}), "synchronization cleanup preserves replacement inode");
    check(get(sync_preserved) == data, "synchronization replacement leaves moved backup data intact");
    const auto original_parent = temp.file("backup-folder"), moved_parent = temp.file("moved-backup-folder");
    std::filesystem::create_directory(original_parent);
    const auto folder_destination = original_parent / "saved.img";
    fails([&] { backup_image(source.fd, data.size(), folder_destination, [&](const Progress& p) {
        if (p.stage == "Synchronizing") {
            std::filesystem::rename(original_parent, moved_parent);
            std::filesystem::create_directory(original_parent);
            put(folder_destination, {'k', 'e', 'e', 'p'});
        }
    }, cancel); }, "backup folder replaced during synchronization cannot report success for the original path");
    check(get(folder_destination) == std::vector<unsigned char>({'k', 'e', 'e', 'p'}), "folder replacement cleanup preserves the new destination");
    check(!std::filesystem::exists(moved_parent / "saved.img"), "folder replacement removes only the unfinished backup through the pinned parent");
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
    std::filesystem::remove(path);
    check(::mkfifo(path.c_str(), 0600) == 0, "event log FIFO fixture");
    fails([&] { log_event("FIFO without reader"); }, "event log FIFO cannot block without a reader");
    {
        Descriptor reader(path, O_RDONLY | O_NONBLOCK);
        fails([&] { log_event("FIFO with reader"); }, "event log FIFO with a reader is rejected by type");
    }
    std::filesystem::remove(path);
    log_event("regular logging still works");
    check(get(path).size() > 0, "regular event log recreated after rejecting unsafe entries");
    {
        // flock locks belong to open file descriptions, so this independent
        // descriptor reproduces a different process holding the log lock.
        Descriptor locked(path, O_WRONLY);
        check(::flock(locked.fd, LOCK_EX | LOCK_NB) == 0, "event log contention fixture holds exclusive lock");
        fails([&] { log_event("contended log"); }, "event logging cannot block on another writer's lock");
        check(::flock(locked.fd, LOCK_UN) == 0, "event log contention fixture releases lock");
    }
    log_event("logging resumes after contention");
    const auto recovered_log = get(path);
    check(std::count(recovered_log.begin(), recovered_log.end(), '\n') == 2, "logging recovers after contention without adding a failed record");
    log_event("warning marker", LogLevel::warning);
    log_event("error marker", LogLevel::error);
    const auto report = read_diagnostics();
    check(report.find("Kavrune ") == 0 && report.find("local diagnostics") != std::string::npos,
          "diagnostic report identifies the application and installed build version");
    check(report.find("level=warning message=warning marker") != std::string::npos &&
          report.find("level=error message=error marker") != std::string::npos && report.find("Z level=info") != std::string::npos,
          "new records use explicit UTC timestamps and structured severity");
    log_event("Unicode: Ω 日本語 \xff\xfe\xe2\x80\xae disguised\nnext");
    const auto unicode_report = read_diagnostics();
    check(unicode_report.find("Ω 日本語") != std::string::npos && unicode_report.find('\xff') == std::string::npos &&
          unicode_report.find("\xe2\x80\xae") == std::string::npos && unicode_report.find("disguised next") != std::string::npos,
          "diagnostics preserve ordinary Unicode and remove invalid UTF-8, direction overrides and forged newlines");
    clear_diagnostics();
    log_event(std::string(1000000, 'x'));
    check(get(path).size() == 8192, "individual event records are capped at eight KiB including metadata");
    const auto previous_path = path + ".1";
    for (int i = 0; i < 90; ++i) log_event("rotation " + std::to_string(i) + " " + std::string(8000, 'x'));
    check(std::filesystem::file_size(path) <= 256 * 1024 && std::filesystem::file_size(previous_path) <= 256 * 1024,
          "rotation bounds both current and previous log storage");
    const auto rotated_report = read_diagnostics();
    check(rotated_report.find("Previous activity") != std::string::npos && rotated_report.find("rotation 89 ") != std::string::npos &&
          rotated_report.size() < 513 * 1024, "bounded report includes the previous generation and newest record");
    {
        Descriptor oversized(path);
        constexpr off_t giant_size = 64 * 1024 * 1024;
        const std::string tail = "\nlegacy tail marker\n";
        check(::ftruncate(oversized.fd, giant_size) == 0 &&
              ::pwrite(oversized.fd, tail.data(), tail.size(), giant_size - static_cast<off_t>(tail.size())) == static_cast<ssize_t>(tail.size()),
              "oversized legacy log fixture is sparse and disposable");
    }
    check(read_diagnostics().find("legacy tail marker") != std::string::npos && read_diagnostics().size() < 513 * 1024,
          "oversized existing logs are read through a bounded tail");
    log_event("after oversized log");
    check(std::filesystem::file_size(path) < 8192 && std::filesystem::file_size(previous_path) <= 256 * 1024 &&
          read_diagnostics().find("legacy tail marker") != std::string::npos,
          "logging trims oversized legacy files while keeping their bounded recent records");
    const auto exported = temp.file("diagnostics Ω.txt");
    export_diagnostics(exported);
    const auto exported_bytes = get(exported);
    check(std::string(exported_bytes.begin(), exported_bytes.end()) == read_diagnostics(), "export is an exact snapshot of locally inspectable diagnostics");
    check(::stat(exported.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600, "exported diagnostics request owner-only permissions");
    fails([&] { export_diagnostics(exported); }, "export refuses an existing destination");
    check(get(exported) == exported_bytes, "refused export preserves the existing report");
    const auto outside = temp.file("private outside.txt");
    const std::vector<unsigned char> secret = {'k', 'e', 'e', 'p'};
    put(outside, secret);
    const auto export_link = temp.file("export-link.txt");
    std::filesystem::create_symlink(outside, export_link);
    fails([&] { export_diagnostics(export_link); }, "export refuses a symbolic-link destination");
    check(get(outside) == secret, "refused export cannot replace a linked destination");
    std::filesystem::remove(export_link);
    std::filesystem::create_hard_link(outside, export_link);
    fails([&] { export_diagnostics(export_link); }, "export refuses an existing hard-linked destination");
    std::filesystem::remove(previous_path);
    std::filesystem::create_symlink(outside, previous_path);
    const auto before_unsafe_clear = get(path);
    fails([&] { clear_diagnostics(); }, "clear rejects a symbolic-link previous generation");
    check(get(path) == before_unsafe_clear && get(outside) == secret, "clear validates all records before modifying any file");
    fails([&] { read_diagnostics(); }, "diagnostic inspection refuses a symbolic-link generation");
    std::filesystem::remove(previous_path);
    std::filesystem::create_hard_link(outside, previous_path);
    fails([&] { clear_diagnostics(); }, "clear rejects a hard-linked generation");
    std::filesystem::remove(previous_path);
    {
        Descriptor oversized(path);
        check(::ftruncate(oversized.fd, 512 * 1024) == 0, "unsafe archive rotation fixture");
    }
    std::filesystem::create_symlink(outside, previous_path);
    fails([&] { log_event("unsafe rotation"); }, "rotation refuses a substituted archive link");
    check(std::filesystem::file_size(path) == 512 * 1024 && get(outside) == secret,
          "failed rotation leaves the active log and outside destination untouched");
    std::filesystem::remove(previous_path);
    clear_diagnostics();
    log_event("before clear");
    std::filesystem::create_hard_link(path, previous_path);
    fails([&] { read_diagnostics(); }, "diagnostics reject hard-linked active records");
    fails([&] { clear_diagnostics(); }, "clear rejects hard-linked active records");
    fails([&] { log_event("hard linked write"); }, "append rejects hard-linked active records");
    std::filesystem::remove(previous_path);
    // Hold the stable lock in another process, then verify every public
    // diagnostics operation fails promptly without modifying existing data.
    int ready_pipe[2] {}, release_pipe[2] {};
    check(::pipe(ready_pipe) == 0 && ::pipe(release_pipe) == 0, "interprocess diagnostics lock pipes");
    const pid_t child = ::fork();
    if (child == 0) {
        ::close(ready_pipe[0]); ::close(release_pipe[1]);
        const int locked = ::open((state / "kavrune/.events.lock").c_str(), O_RDWR | O_CLOEXEC);
        const char ready = locked >= 0 && ::flock(locked, LOCK_EX | LOCK_NB) == 0 ? 'y' : 'n';
        if (::write(ready_pipe[1], &ready, 1) != 1) ::_exit(2);
        char release = 0;
        if (::read(release_pipe[0], &release, 1) != 1) ::_exit(3);
        if (locked >= 0) ::close(locked);
        ::_exit(ready == 'y' ? 0 : 1);
    }
    if (child < 0) throw std::runtime_error("diagnostics lock fork failed");
    ::close(ready_pipe[1]); ::close(release_pipe[0]);
    char child_ready = 0;
    check(::read(ready_pipe[0], &child_ready, 1) == 1 && child_ready == 'y', "another process owns the stable diagnostics lock");
    const auto before_contention = get(path);
    fails([&] { clear_diagnostics(); }, "clear cannot race a process that owns the stable lock");
    fails([&] { read_diagnostics(); }, "inspection does not block on another process");
    fails([&] { log_event("locked append"); }, "append does not block on the stable process lock");
    const auto locked_export = temp.file("locked-export.txt");
    fails([&] { export_diagnostics(locked_export); }, "export does not block on the stable process lock");
    check(!std::filesystem::exists(locked_export) && get(path) == before_contention,
          "contention creates no partial export and cannot clear records");
    const char release = 'x';
    check(::write(release_pipe[1], &release, 1) == 1, "release the child diagnostics lock");
    ::close(ready_pipe[0]); ::close(release_pipe[1]);
    int child_status = 0;
    check(::waitpid(child, &child_status, 0) == child && WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0,
          "interprocess lock fixture exits cleanly");
    clear_diagnostics();
    std::thread concurrent_writer([] { for (int i = 0; i < 100; ++i) log_event("concurrent before deletion"); });
    for (int i = 0; i < 40; ++i) clear_diagnostics();
    concurrent_writer.join();
    clear_diagnostics();
    log_event("after deletion marker");
    check(read_diagnostics().find("concurrent before deletion") == std::string::npos &&
          read_diagnostics().find("after deletion marker") != std::string::npos,
          "concurrent clear and append cannot resurrect activity older than the final clear");
    // An entry swapped to a symlink during repeated clears must never direct
    // deletion to the outside file. All I/O uses pinned, checked descriptors.
    std::atomic_bool racing{true};
    std::thread replacer([&] {
        while (racing.load(std::memory_order_relaxed)) {
            std::error_code ignored;
            std::filesystem::create_symlink(outside, previous_path, ignored);
            std::filesystem::remove(previous_path, ignored);
        }
    });
    for (int i = 0; i < 100; ++i) {
        try { clear_diagnostics(); } catch (const Error&) {}
    }
    racing = false;
    replacer.join();
    check(get(outside) == secret, "clear races against symlink replacement cannot modify outside files");
    std::filesystem::remove(previous_path);
    clear_diagnostics();
    check(get(path).empty() && read_diagnostics().find("No saved activity.") != std::string::npos,
          "clearing saved diagnostics removes both retained generations");
    const auto stable_lock = state / "kavrune/.events.lock";
    std::filesystem::remove(stable_lock);
    std::filesystem::create_symlink(outside, stable_lock);
    fails([&] { clear_diagnostics(); }, "a substituted stable lock cannot bypass secure clear");
    check(get(outside) == secret, "unsafe stable lock never changes outside data");
    std::filesystem::remove(stable_lock);
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
        isohybrid_inspection_tests(temp);
        compatibility_tests(temp);
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
