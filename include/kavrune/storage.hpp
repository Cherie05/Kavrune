// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <gio/gio.h>

namespace kavrune {
class Storage {
    GDBusConnection* connection_ = nullptr;
public:
    Storage();
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    std::vector<Device> devices(); // includes protected disks, with safety_reason
    Device revalidate(const Device& expected);
    bool source_on_device(const std::string& source_path, const Device& target);
    void unmount(const Device& expected);
    int open_device(const Device& expected, bool writable); // owned fd; caller closes
    void rescan(const Device& expected);
    void format(const Device& expected, const FormatOptions& options, const ProgressFn& progress, const std::atomic_bool& cancel);
};
bool same_identity(const Device& expected, const Device& actual);
std::string device_safety_reason(const Device& device, bool system_backing, bool holders, bool swap);
void validate_format_options(const FormatOptions& options, uint64_t size);
}
