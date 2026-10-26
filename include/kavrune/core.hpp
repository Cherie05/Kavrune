// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace kavrune {
class Error : public std::runtime_error { public: using std::runtime_error::runtime_error; };
enum class ImageIssue { incomplete, compressed, container, malformed, unrecognized, inspection_limit };
class ImageError : public Error {
    ImageIssue issue_;
    std::string advice_;
public:
    ImageError(ImageIssue issue, const std::string& detail, const std::string& advice)
        : Error(detail), issue_(issue), advice_(advice) {}
    ImageIssue issue() const noexcept { return issue_; }
    const std::string& advice() const noexcept { return advice_; }
};
class Cancelled : public Error {
public:
    Cancelled() : Error("Operation cancelled. The drive may contain incomplete media; recreate it before use.") {}
    explicit Cancelled(const std::string& message) : Error(message) {}
};
struct Progress {
    std::string stage;
    uint64_t done = 0, total = 0;
    double bytes_per_second = 0;
    std::string detail;
};
using ProgressFn = std::function<void(const Progress&)>;
struct Image {
    std::string path, name, kind, description, expected_sha256;
    uint64_t size = 0, inode = 0, device = 0;
    int64_t mtime_sec = 0, mtime_nsec = 0;
    int64_t ctime_sec = 0, ctime_nsec = 0;
    bool bootable = false, raw_writable = false;
};
struct Device {
    std::string object_path, drive_path, path, vendor, model, serial, id, connection, sysfs_path;
    uint64_t size = 0, device_number = 0, time_detected = 0, time_media_detected = 0;
    bool removable = false, read_only = false, hint_system = false;
    std::vector<std::string> partitions, mount_points;
    std::string safety_reason;
    bool eligible() const { return safety_reason.empty(); }
    std::string display_name() const;
};
struct FormatOptions {
    std::string scheme = "gpt", filesystem = "vfat", label = "KAVRUNE";
};
struct SelfTestResult {
    uint64_t bytes = 0;
    std::string sha256;
};
Image inspect_image(const std::string& path);
// File identity only; this does not establish a format or write compatibility.
Image identify_file(const std::string& path);
std::string checksum(const std::string& path, const std::string& algorithm, const ProgressFn& progress, const std::atomic_bool& cancel);
std::string write_image(const Image& image, int target_fd, uint64_t capacity, bool verify, const ProgressFn& progress, const std::atomic_bool& cancel);
std::string backup_image(int source_fd, uint64_t capacity, const std::string& destination, const ProgressFn& progress, const std::atomic_bool& cancel);
void read_scan(int source_fd, uint64_t capacity, const ProgressFn& progress, const std::atomic_bool& cancel);
// Exercises the shared writer using private temporary regular files only.
SelfTestResult run_self_test(const ProgressFn& progress, const std::atomic_bool& cancel);
std::string human_size(uint64_t bytes);
enum class LogLevel { info, warning, error };
void log_event(const std::string& message, LogLevel level = LogLevel::info);
std::string log_path();
// Local, bounded diagnostics only. No function uploads or reads drive contents.
// Export creates a new owner-only file; existing destinations are never replaced.
std::string read_diagnostics();
void clear_diagnostics();
void export_diagnostics(const std::string& destination);
}
