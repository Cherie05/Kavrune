// SPDX-License-Identifier: MIT
#include "kavrune/core.hpp"
#include "kavrune/storage.hpp"
#include <algorithm>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace af = kavrune;
namespace {
std::atomic_bool cancelled{false};
static_assert(std::atomic_bool::is_always_lock_free);
void stop(int) { cancelled.store(true, std::memory_order_relaxed); }
struct Fd {
    int fd;
    explicit Fd(int value) : fd(value) {}
    ~Fd() { if (fd >= 0) ::close(fd); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
std::string json(const std::string& value) {
    gchar* valid = g_utf8_make_valid(value.data(), static_cast<gssize>(value.size()));
    const std::string safe(valid);
    g_free(valid);
    std::ostringstream out;
    out << '"';
    for (unsigned char c : safe) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
            else out << char(c);
        }
    }
    return out.str() + '"';
}
const char* image_issue_name(af::ImageIssue issue) {
    switch (issue) {
    case af::ImageIssue::incomplete: return "incomplete";
    case af::ImageIssue::compressed: return "compressed";
    case af::ImageIssue::container: return "container";
    case af::ImageIssue::malformed: return "malformed";
    case af::ImageIssue::unrecognized: return "unrecognized";
    case af::ImageIssue::inspection_limit: return "inspection_limit";
    }
    return "unrecognized";
}
void report(const af::Progress& p) {
    std::cerr << '\r' << p.stage << "  " << af::human_size(p.done) << " / " << af::human_size(p.total);
    if (p.total) std::cerr << "  " << int(100.0 * double(p.done) / double(p.total)) << '%';
    if (p.bytes_per_second > 0) std::cerr << "  " << af::human_size(uint64_t(p.bytes_per_second)) << "/s";
    std::cerr << "             " << std::flush;
}
using Options = std::map<std::string, std::string>;
Options options(int argc, char** argv, int begin, const std::vector<std::string>& flags) {
    Options result;
    for (int i = begin; i < argc; ++i) {
        std::string key = argv[i];
        if (std::find(flags.begin(), flags.end(), key) == flags.end()) throw af::Error("Unknown option: " + key);
        if (result.count(key)) throw af::Error("Duplicate option: " + key);
        if (key == "--all" || key == "--json") result[key] = "true";
        else {
            if (++i >= argc) throw af::Error("Missing value for " + key);
            result[key] = argv[i];
        }
    }
    return result;
}
std::string required(const Options& opts, const std::string& name) {
    auto it = opts.find(name);
    if (it == opts.end() || it->second.empty()) throw af::Error("Required option: " + name);
    return it->second;
}
std::string optional(const Options& opts, const std::string& name, const std::string& fallback) {
    auto it = opts.find(name);
    return it == opts.end() ? fallback : it->second;
}
af::Device target(af::Storage& storage, const Options& opts) {
    auto identifier = required(opts, "--device");
    std::vector<af::Device> found;
    for (const auto& d : storage.devices()) if (d.id == identifier) found.push_back(d);
    if (found.size() != 1) throw af::Error("The device ID must identify one connected drive. Refresh devices first.");
    auto d = found.front();
    if (!d.eligible()) throw af::Error("Drive protected: " + d.safety_reason);
    if (required(opts, "--confirm") != d.path) throw af::Error("Confirmation must match the displayed device path exactly: " + d.path);
    std::cerr << "Target: " << d.display_name() << " | " << d.path << " | " << af::human_size(d.size)
              << " | serial " << d.serial << '\n';
    return storage.revalidate(d);
}
void help() {
    std::cout << "Kavrune 0.1.0 — native Linux USB media utility (development preview)\n\n"
      "Usage:\n"
      "  kavrune-cli devices [--all] [--json]\n"
      "  kavrune-cli inspect IMAGE [--json]\n"
      "  kavrune-cli checksum IMAGE [--algorithm SHA256] [--expected HEX]\n"
      "  kavrune-cli self-test [--json]\n"
      "  kavrune-cli simulate IMAGE NEW_FILE\n"
      "  kavrune-cli write IMAGE --device ID --confirm /dev/DEVICE\n"
      "  kavrune-cli format --device ID --confirm /dev/DEVICE [--scheme gpt|mbr]\n"
      "                        [--filesystem vfat|exfat|ext4] [--label NAME]\n"
      "  kavrune-cli backup NEW_IMAGE --device ID --confirm /dev/DEVICE\n"
      "  kavrune-cli scan --device ID --confirm /dev/DEVICE\n\n"
      "write and format ERASE the selected USB drive. A Polkit agent is required.\n"
      "All write operations include SHA-256 readback verification.\n"
      "simulate uses a newly created regular file and never opens a block device.\n"
      "self-test checks writing and verification with disposable files; no ISO or USB needed.\n"
      "backup refuses to overwrite files; backup and scan unmount the selected drive.\n"
      "Use Ctrl+C to cancel streaming operations; synchronization finishes safely.\n";
}
}
int main(int argc, char** argv) {
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "help") { help(); return 0; }
        if (::geteuid() == 0) throw af::Error("Run Kavrune as a normal user. UDisks2 and Polkit authorize individual storage operations.");
        std::string command = argv[1];
        if (command == "self-test") {
            const auto opts = options(argc, argv, 2, {"--json"});
            const auto result = af::run_self_test(report, cancelled);
            std::cerr << '\n';
            if (opts.count("--json"))
                std::cout << "{\"passed\":true,\"bytes\":" << result.bytes << ",\"sha256\":" << json(result.sha256)
                          << ",\"physical_usb_tested\":false}\n";
            else std::cout << "Safe test passed. Writing and SHA-256 verification succeeded.\n"
                           << "No USB drives were accessed or changed.\n"
                           << "Tested " << af::human_size(result.bytes) << ". SHA-256: " << result.sha256 << '\n'
                           << "Real USB devices and boot compatibility still need separate testing.\n";
            return 0;
        }
        if (command == "inspect") {
            if (argc < 3) throw af::Error("Specify an image file.");
            auto opts = options(argc, argv, 3, {"--json"});
            af::Image image;
            try { image = af::inspect_image(argv[2]); }
            catch (const af::ImageError& error) {
                if (!opts.count("--json")) throw;
                std::cout << "{\"error\":" << json(error.what()) << ",\"issue\":" << json(image_issue_name(error.issue()))
                          << ",\"advice\":" << json(error.advice()) << "}\n";
                return 1;
            }
            if (opts.count("--json")) std::cout << "{\"name\":" << json(image.name) << ",\"size\":" << image.size
                << ",\"kind\":" << json(image.kind) << ",\"description\":" << json(image.description)
                << ",\"bootable\":" << (image.bootable ? "true" : "false") << ",\"raw_writable\":" << (image.raw_writable ? "true" : "false") << "}\n";
            else std::cout << image.name << "\n" << image.kind << " | " << af::human_size(image.size) << "\n"
                           << image.description << "\nRaw writing: " << (image.raw_writable ? "supported" : "blocked") << '\n';
            return 0;
        }
        if (command == "checksum") {
            if (argc < 3) throw af::Error("Specify an image file.");
            auto opts = options(argc, argv, 3, {"--algorithm", "--expected"});
            auto algorithm = optional(opts, "--algorithm", "SHA256");
            auto hash = af::checksum(argv[2], algorithm, report, cancelled);
            std::cerr << '\n';
            std::cout << hash << '\n';
            if (opts.count("--expected")) {
                std::string expected = opts.at("--expected");
                for (auto& c : expected) c = char(std::tolower(static_cast<unsigned char>(c)));
                if (expected != hash) throw af::Error("Checksum mismatch.");
                std::cerr << "Checksum matches.\n";
            }
            return 0;
        }
        if (command == "simulate") {
            if (argc != 4) throw af::Error("Usage: kavrune-cli simulate IMAGE NEW_FILE");
            auto image = af::inspect_image(argv[2]);
            if (!image.raw_writable) throw af::Error(image.description);
            const std::string output = argv[3];
            Fd fd(::open(output.c_str(), O_CREAT | O_EXCL | O_NOFOLLOW | O_RDWR | O_CLOEXEC, 0600));
            if (fd.fd < 0) throw af::Error("Simulation target must be a new regular file in an existing writable directory.");
            struct stat created{};
            if (::fstat(fd.fd, &created) < 0 || !S_ISREG(created.st_mode)) throw af::Error("Invalid simulation target.");
            try {
                if (::ftruncate(fd.fd, static_cast<off_t>(image.size)) != 0) throw af::Error("Could not size simulation file.");
                auto hash = af::write_image(image, fd.fd, image.size, true, report, cancelled);
                std::cerr << '\n';
                std::cout << "Simulation written and verified. SHA-256: " << hash << '\n';
            } catch (...) {
                struct stat current{};
                if (::lstat(output.c_str(), &current) == 0 && current.st_dev == created.st_dev && current.st_ino == created.st_ino) ::unlink(output.c_str());
                throw;
            }
            return 0;
        }
        if (command == "devices") {
            auto opts = options(argc, argv, 2, {"--all", "--json"});
            af::Storage storage;
            auto devices = storage.devices();
            bool first = true;
            if (opts.count("--json")) std::cout << '[';
            for (const auto& d : devices) {
                if (!d.eligible() && !opts.count("--all")) continue;
                if (opts.count("--json")) {
                    if (!first) std::cout << ',';
                    std::cout << "{\"id\":" << json(d.id) << ",\"name\":" << json(d.display_name()) << ",\"path\":" << json(d.path)
                         << ",\"serial\":" << json(d.serial) << ",\"size\":" << d.size << ",\"eligible\":" << (d.eligible() ? "true" : "false")
                         << ",\"reason\":" << json(d.safety_reason) << '}';
                } else std::cout << d.display_name() << " | " << d.path << " | " << af::human_size(d.size)
                    << "\n  ID: " << d.id << "\n  " << (d.eligible() ? "Eligible external drive" : "Protected: " + d.safety_reason) << '\n';
                first = false;
            }
            if (opts.count("--json")) std::cout << "]\n";
            else if (first) std::cout << "No eligible external USB drives connected.\n";
            return 0;
        }
        const bool image_arg = command == "write" || command == "backup";
        if (command != "write" && command != "format" && command != "backup" && command != "scan") throw af::Error("Unknown command. Use --help.");
        if (image_arg && argc < 3) throw af::Error("Specify the image path.");
        auto allowed = std::vector<std::string>{"--device", "--confirm"};
        if (command == "format") allowed.insert(allowed.end(), {"--scheme", "--filesystem", "--label"});
        auto opts = options(argc, argv, image_arg ? 3 : 2, allowed);
        af::Storage storage;
        auto d = target(storage, opts);
        if (command == "format") {
            af::FormatOptions fmt;
            fmt.scheme = optional(opts, "--scheme", "gpt");
            fmt.filesystem = optional(opts, "--filesystem", "vfat");
            fmt.label = optional(opts, "--label", "KAVRUNE");
            std::cerr << "ALL files and partitions on the target will be erased.\n";
            storage.format(d, fmt, report, cancelled);
            std::cout << "\nDrive formatted.\n";
        } else if (command == "write") {
            auto image = af::inspect_image(argv[2]);
            if (!image.raw_writable) throw af::Error(image.description);
            if (image.size > d.size) throw af::Error("The image is larger than the selected drive.");
            if (storage.source_on_device(image.path, d)) throw af::Error("The source image is stored on the target drive. Move it to another disk first.");
            std::cerr << "ALL files and partitions on the target will be erased.\n";
            storage.unmount(d);
            std::string hash;
            {
                Fd fd(storage.open_device(d, true));
                hash = af::write_image(image, fd.fd, d.size, true, report, cancelled);
            }
            storage.rescan(d);
            std::cerr << '\n';
            std::cout << "Image written, verified and rescanned. SHA-256: " << hash << '\n';
        } else {
            if (command == "backup") {
                auto parent = std::filesystem::absolute(argv[2]).parent_path().string();
                if (storage.source_on_device(parent, d)) throw af::Error("The backup destination cannot be on the source drive.");
            }
            storage.unmount(d);
            Fd fd(storage.open_device(d, false));
            if (command == "backup") {
                const auto hash = af::backup_image(fd.fd, d.size, argv[2], report, cancelled);
                std::cout << "\nBackup complete. SHA-256: " << hash << '\n';
            }
            else { af::read_scan(fd.fd, d.size, report, cancelled); std::cout << "\nRead scan complete; all reported bytes were readable. This does not test actual flash capacity.\n"; }
        }
        try { af::log_event("CLI operation completed: " + command); }
        catch (const std::exception& e) { std::cerr << "Operation completed; event log unavailable: " << e.what() << '\n'; }
        return 0;
    } catch (const af::Cancelled& e) { std::cerr << '\n' << e.what() << '\n'; return 130; }
      catch (const af::ImageError& e) { std::cerr << "\nKavrune: " << e.advice() << "\nDetails: " << e.what() << '\n'; return 1; }
      catch (const std::exception& e) { std::cerr << "\nKavrune: " << e.what() << '\n'; return 1; }
}
