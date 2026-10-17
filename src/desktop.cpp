// SPDX-License-Identifier: MIT
// Original Kavrune desktop implementation. No third-party application UI code.
#include "kavrune/core.hpp"
#include "kavrune/storage.hpp"
// Keep the API compatible with the Ubuntu 24.04 libadwaita baseline.
#define ADW_VERSION_MIN_REQUIRED ADW_VERSION_1_4
#include <adwaita.h>
#include <glib/gi18n.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace kavrune;
namespace {
using Fn = std::function<void()>;
GtkWidget* label(const std::string& text, const char* css = nullptr) {
    auto* w = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(w), 0);
    gtk_label_set_wrap(GTK_LABEL(w), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(w), PANGO_WRAP_WORD_CHAR);
    if (css) gtk_widget_add_css_class(w, css);
    return w;
}
GtkWidget* box(GtkOrientation orientation, int spacing) { return gtk_box_new(orientation, spacing); }
void append(GtkWidget* parent, GtkWidget* child) { gtk_box_append(GTK_BOX(parent), child); }
void click(GtkWidget* widget, Fn fn) {
    auto* callback = new Fn(std::move(fn));
    g_signal_connect_data(widget, "clicked", G_CALLBACK(+[](GtkButton*, gpointer p) { (*static_cast<Fn*>(p))(); }), callback,
        +[](gpointer p, GClosure*) { delete static_cast<Fn*>(p); }, G_CONNECT_DEFAULT);
}
GtkWidget* button(const char* text, const char* css = nullptr) {
    auto* w = gtk_button_new_with_label(text);
    if (css) gtk_widget_add_css_class(w, css);
    return w;
}
std::string base_name(const std::string& path) { const auto at = path.find_last_of('/'); return at == std::string::npos ? path : path.substr(at + 1); }
const char* image_issue_heading(ImageIssue issue) {
    switch (issue) {
    case ImageIssue::incomplete: return _("Download not finished");
    case ImageIssue::compressed: return _("Extract this download first");
    case ImageIssue::container: return _("Convert this disk image first");
    case ImageIssue::malformed: return _("Check this download");
    case ImageIssue::unrecognized: return _("Choose a disk image");
    case ImageIssue::inspection_limit: return _("Image needs another creation method");
    }
    return _("Check this image");
}
bool same_source(const Image& first, const Image& second) {
    return first.inode == second.inode && first.device == second.device && first.size == second.size &&
        first.mtime_sec == second.mtime_sec && first.mtime_nsec == second.mtime_nsec &&
        first.ctime_sec == second.ctime_sec && first.ctime_nsec == second.ctime_nsec;
}
std::string identity_text(const Device& d) {
    return d.path + "  ·  " + human_size(d.size) + "  ·  " + (d.connection.empty() ? _("External storage") : d.connection) +
        "\n" + _("Serial: ") + (d.serial.empty() ? _("not reported") : d.serial) +
        (d.mount_points.empty() ? "" : "\n" + std::to_string(d.mount_points.size()) + _(" mounted volume(s); they will be unmounted."));
}
struct UniqueFd {
    int fd;
    explicit UniqueFd(int f) : fd(f) {}
    ~UniqueFd() { if (fd >= 0) close(fd); }
    UniqueFd(const UniqueFd&) = delete;
};
class Desktop : public std::enable_shared_from_this<Desktop> {
    AdwApplication* app_;
    GtkWidget *window_ = nullptr, *stack_ = nullptr, *sidebar_ = nullptr, *split_ = nullptr, *navigation_ = nullptr;
    GtkWidget *progress_card_ = nullptr, *progress_bar_ = nullptr, *progress_title_ = nullptr, *progress_detail_ = nullptr, *cancel_button_ = nullptr;
    GtkWidget *progress_technical_ = nullptr, *progress_expander_ = nullptr;
    GtkWidget *source_name_ = nullptr, *source_description_ = nullptr, *source_size_ = nullptr, *image_hash_ = nullptr, *source_technical_ = nullptr;
    GtkWidget *create_hint_ = nullptr, *review_text_ = nullptr, *image_status_ = nullptr;
    GtkWidget *check_name_ = nullptr, *check_result_ = nullptr, *expected_ = nullptr, *algorithm_ = nullptr;
    GtkWidget *create_ = nullptr, *format_ = nullptr, *backup_ = nullptr, *scan_ = nullptr, *check_ = nullptr;
    GtkWidget *scheme_ = nullptr, *filesystem_ = nullptr, *volume_label_ = nullptr, *activity_view_ = nullptr, *device_status_ = nullptr;
    GtkStringList* device_model_ = nullptr;
    std::vector<GtkWidget*> controls_, dropdowns_, device_details_, device_technical_, device_empty_, device_rows_;
    std::vector<Device> devices_;
    Image image_;
    std::string digest_, digest_algorithm_ = "sha256", source_issue_hint_;
    std::string device_report_;
    bool log_warning_shown_ = false;
    bool checksum_failed_ = false;
    std::atomic_bool alive_{true};
    std::shared_ptr<std::atomic_bool> cancel_ = std::make_shared<std::atomic_bool>(false);
    bool busy_ = false, can_cancel_ = false, refreshing_ = false, syncing_selection_ = false, close_after_ = false;
    guint timer_ = 0;
    unsigned selected_ = GTK_INVALID_LIST_POSITION;

    void post(Fn fn) {
        auto* call = new Fn(std::move(fn));
        // Attaching an idle source always dispatches on the GTK main thread.
        // g_main_context_invoke can run directly on a worker that acquires the context.
        auto* source = g_idle_source_new();
        g_source_set_callback(source, +[](gpointer data) -> gboolean {
            (*static_cast<Fn*>(data))(); return G_SOURCE_REMOVE;
        }, call, +[](gpointer data) { delete static_cast<Fn*>(data); });
        g_source_attach(source, g_main_context_default()); g_source_unref(source);
    }
    GtkWidget* control(GtkWidget* w) { controls_.push_back(w); return w; }
    GtkWidget* hero(const char* eyebrow, const char* title, const char* subtitle) {
        auto* v = box(GTK_ORIENTATION_VERTICAL, 9);
        append(v, label(eyebrow, "eyebrow")); append(v, label(title, "hero-title")); append(v, label(subtitle, "hero-subtitle"));
        gtk_widget_set_margin_bottom(v, 7); return v;
    }
    GtkWidget* card(const char* number, const char* title, const char* subtitle, GtkWidget** contents) {
        auto* outer = box(GTK_ORIENTATION_VERTICAL, 14); gtk_widget_add_css_class(outer, "step-card");
        auto* top = box(GTK_ORIENTATION_HORIZONTAL, 13);
        auto* n = label(number, "step-number"); gtk_widget_set_valign(n, GTK_ALIGN_START); append(top, n);
        auto* titles = box(GTK_ORIENTATION_VERTICAL, 4); gtk_widget_set_hexpand(titles, TRUE);
        append(titles, label(title, "card-title")); if (subtitle) append(titles, label(subtitle, "card-caption")); append(top, titles); append(outer, top);
        *contents = box(GTK_ORIENTATION_VERTICAL, 10); append(outer, *contents); return outer;
    }
    GtkWidget* note(const char* text, bool warning = false) {
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 12); gtk_widget_add_css_class(row, warning ? "warning-note" : "safety-note");
        auto* icon = gtk_image_new_from_icon_name(warning ? "dialog-warning-symbolic" : "emblem-ok-symbolic");
        gtk_widget_set_valign(icon, GTK_ALIGN_START); if (!warning) gtk_widget_add_css_class(icon, "safety-icon");
        append(row, icon); auto* t = label(text, "card-caption"); gtk_widget_set_hexpand(t, TRUE); append(row, t); return row;
    }
    GtkWidget* page(const char* name) {
        auto* v = box(GTK_ORIENTATION_VERTICAL, 18); gtk_widget_add_css_class(v, "page-content");
        auto* clamp = adw_clamp_new(); adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 830); adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 640); adw_clamp_set_child(ADW_CLAMP(clamp), v);
        auto* scroll = gtk_scrolled_window_new(); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), clamp); gtk_widget_set_vexpand(scroll, TRUE);
        gtk_stack_add_named(GTK_STACK(stack_), scroll, name); return v;
    }
    GtkWidget* target_card(const char* number = "2") {
        GtkWidget* contents; auto* outer = card(number, _("Plug in and select your USB"), _("Choose the USB you want to use. Check its name and size carefully."), &contents);
        auto* empty = box(GTK_ORIENTATION_HORIZONTAL, 12); gtk_widget_add_css_class(empty, "empty-state");
        auto* icon = gtk_image_new_from_icon_name("drive-removable-media-symbolic"); gtk_image_set_pixel_size(GTK_IMAGE(icon), 32); append(empty, icon);
        auto* text = box(GTK_ORIENTATION_VERTICAL, 4); gtk_widget_set_hexpand(text, TRUE);
        append(text, label(_("Insert a USB drive"), "card-title"));
        append(text, label(_("Eligible external drives appear here. Protected or unsupported drives are hidden; see Activity for the reason."), "card-caption")); append(empty, text); append(contents, empty); device_empty_.push_back(empty);
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 10);
        auto* drop = control(gtk_drop_down_new(G_LIST_MODEL(g_object_ref(device_model_)), nullptr));
        gtk_drop_down_set_enable_search(GTK_DROP_DOWN(drop), TRUE); gtk_widget_set_hexpand(drop, TRUE); gtk_widget_add_css_class(drop, "device-dropdown");
        gtk_widget_set_tooltip_text(drop, _("External destination drive")); dropdowns_.push_back(drop);
        g_signal_connect(drop, "notify::selected", G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer data) {
            auto* self = static_cast<Desktop*>(data); if (self->syncing_selection_) return;
            self->selected_ = gtk_drop_down_get_selected(GTK_DROP_DOWN(o)); self->sync_selection(); self->update_controls();
        }), this);
        append(row, drop); append(contents, row); device_rows_.push_back(row);
        auto* details = label(_("Select your USB from the list."), "card-caption"); gtk_label_set_selectable(GTK_LABEL(details), TRUE); device_details_.push_back(details); append(contents, details);
        auto* bottom = box(GTK_ORIENTATION_HORIZONTAL, 10);
        auto* expander = gtk_expander_new(_("Drive details")); gtk_widget_set_hexpand(expander, TRUE);
        auto* technical = label(_("Select a drive to see its device path and serial number."), "technical-text"); gtk_label_set_selectable(GTK_LABEL(technical), TRUE);
        device_technical_.push_back(technical); gtk_expander_set_child(GTK_EXPANDER(expander), technical); append(bottom, expander);
        auto* refresh = control(button(_("Refresh"), "secondary-button")); gtk_widget_set_tooltip_text(refresh, _("Refresh connected drives"));
        click(refresh, [this] { refresh_devices(); }); append(bottom, refresh); append(contents, bottom); return outer;
    }
    GtkWidget* action_row(GtkWidget* primary, const char* hint) {
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 12); auto* t = label(hint, "card-caption"); gtk_widget_set_hexpand(t, TRUE);
        append(row, t); append(row, primary); gtk_widget_set_valign(primary, GTK_ALIGN_CENTER); return row;
    }
    const Device* selected_device() const { return selected_ < devices_.size() ? &devices_[selected_] : nullptr; }
    void sync_selection() {
        syncing_selection_ = true;
        for (auto* w : dropdowns_) gtk_drop_down_set_selected(GTK_DROP_DOWN(w), selected_);
        const auto* d = selected_device(); std::string text = d ? _("Selected: ") + (d->model.empty() ? _("External drive") : d->model) + " · " + human_size(d->size) : devices_.empty() ? "" : _("Select your USB from the list. Kavrune never chooses a drive for you.");
        for (auto* w : device_details_) gtk_label_set_text(GTK_LABEL(w), text.c_str());
        for (auto* w : device_technical_) gtk_label_set_text(GTK_LABEL(w), d ? identity_text(*d).c_str() : _("No drive selected."));
        for (auto* w : device_empty_) gtk_widget_set_visible(w, devices_.empty());
        for (auto* w : device_rows_) gtk_widget_set_visible(w, !devices_.empty());
        syncing_selection_ = false;
    }
    void update_controls() {
        for (auto* w : controls_) gtk_widget_set_sensitive(w, !busy_);
        const auto* d = selected_device(); const bool target = d && d->eligible();
        const bool ready = !busy_ && target && image_.raw_writable && image_.size && image_.size <= d->size && !checksum_failed_;
        gtk_widget_set_sensitive(create_, ready);
        gtk_widget_set_sensitive(format_, !busy_ && target); gtk_widget_set_sensitive(backup_, !busy_ && target); gtk_widget_set_sensitive(scan_, !busy_ && target);
        gtk_widget_set_sensitive(check_, !busy_ && !image_.path.empty()); gtk_widget_set_sensitive(cancel_button_, busy_ && can_cancel_);
        gtk_widget_set_visible(cancel_button_, busy_);
        if (!image_.path.empty()) {
            gtk_label_set_text(GTK_LABEL(image_status_), checksum_failed_ ? _("Checksum mismatch — blocked") : image_.raw_writable ? (image_.bootable ? _("Ready to write") : _("Check image source")) : image_.kind == "Checksum-only file" ? _("Checksum only") : _("Different creation method needed"));
            gtk_widget_remove_css_class(image_status_, "success"); gtk_widget_remove_css_class(image_status_, "warning");
            gtk_widget_add_css_class(image_status_, !checksum_failed_ && image_.raw_writable && image_.bootable ? "success" : "warning");
        }
        std::string hint;
        if (busy_) hint = _("Wait for the current operation to finish, or cancel it.");
        else if (image_.path.empty()) hint = source_issue_hint_.empty() ? _("Next: choose your operating system image in step 1.") : source_issue_hint_;
        else if (!image_.raw_writable) hint = image_.kind == "Checksum-only file" ? _("This file can be checked, but cannot create a USB. Choose a complete USB-ready system image in step 1.") : _("This image needs a different USB creation method. See the explanation in step 1. You can still check its checksum.");
        else if (checksum_failed_) hint = _("The checksum did not match. Choose the image again or compare it with the correct publisher checksum before continuing.");
        else if (devices_.empty()) hint = _("Next: insert a USB drive. Then select it in step 2.");
        else if (!target) hint = _("Next: select your USB in step 2.");
        else if (image_.size > d->size) hint = _("This USB is too small. Select a larger drive.");
        else hint = _("Ready. Back up anything on this USB before continuing.");
        gtk_label_set_text(GTK_LABEL(create_hint_), hint.c_str());
        gtk_widget_set_tooltip_text(create_, hint.c_str());
        const std::string review = ready ? image_.name + " → " + (d->model.empty() ? _("External drive") : d->model) + " (" + human_size(d->size) + ")" : _("Your image and USB will appear here when both are selected.");
        gtk_label_set_text(GTK_LABEL(review_text_), review.c_str());
    }
    void record(const std::string& text) {
        std::string log_warning;
        try { log_event(text); }
        catch (const std::exception& error) {
            if (!log_warning_shown_) { log_warning = std::string(_("Local log unavailable: ")) + error.what() + "\n"; log_warning_shown_ = true; }
        }
        auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(activity_view_)); GtkTextIter end;
        gtk_text_buffer_get_end_iter(buffer, &end); GDateTime* now = g_date_time_new_now_local(); gchar* clock = g_date_time_format(now, "%H:%M:%S");
        const std::string line = log_warning + std::string(clock) + "  " + text + "\n"; g_free(clock); g_date_time_unref(now);
        gtk_text_buffer_insert(buffer, &end, line.c_str(), -1);
    }
    void report(const std::string& title, const std::string& detail, bool failed = false) {
        gtk_widget_set_visible(progress_card_, TRUE); gtk_label_set_text(GTK_LABEL(progress_title_), title.c_str()); gtk_label_set_text(GTK_LABEL(progress_detail_), detail.c_str());
        gtk_widget_remove_css_class(progress_card_, "error"); if (failed) gtk_widget_add_css_class(progress_card_, "error");
    }
    void technical_result(const std::string& detail) {
        gtk_label_set_text(GTK_LABEL(progress_technical_), detail.c_str());
        gtk_widget_set_visible(progress_expander_, !detail.empty());
    }
    std::string failure_advice(const std::string& error, bool changes_drive) const {
        std::string lower = error;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(g_ascii_tolower(c)); });
        if (lower.find("authoriz") != std::string::npos || lower.find("permission") != std::string::npos)
            return _("Permission was not granted. Try again and approve the system authorization prompt.");
        if (lower.find("changed") != std::string::npos || lower.find("disappear") != std::string::npos || lower.find("disconnect") != std::string::npos)
            return _("The image or drive changed. Reconnect the USB and choose your image and drive again.");
        if (lower.find("space") != std::string::npos || lower.find("capacity") != std::string::npos || lower.find("larger") != std::string::npos)
            return _("There is not enough space. Choose a larger USB or free space in the backup folder.");
        if (lower.find("compressed") != std::string::npos)
            return _("This version needs an uncompressed ISO, IMG or RAW file. Extract your download first, then choose the extracted image.");
        if (lower.find("image") != std::string::npos && !changes_drive)
            return _("This image could not be used. Choose a supported ISO, IMG or RAW file from a trusted publisher. Details below explain the problem.");
        if (changes_drive) return _("The USB may contain incomplete data. Reconnect it, review the details below, and create it again before using it.");
        return _("Check that the file or USB is still connected and readable, then try again. Details below explain what happened.");
    }
    void message(const char* title, const std::string& detail) {
        auto* dialog = adw_message_dialog_new(GTK_WINDOW(window_), title, detail.c_str());
        adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "close", _("Close")); adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog), "close"); gtk_window_present(GTK_WINDOW(dialog));
    }
    void refresh_devices() {
        if (refreshing_ || busy_ || !alive_) return;
        refreshing_ = true;
        auto self = shared_from_this();
        try { std::thread([self] {
            std::vector<Device> found; std::string error;
            try { Storage storage; found = storage.devices(); } catch (const std::exception& e) { error = e.what(); }
            self->post([self, found = std::move(found), error] {
                if (!self->alive_) return;
                self->refreshing_ = false;
                // A polling response cannot change the immutable target of an operation.
                if (self->busy_) return;
                std::string discovery_report;
                if (!error.empty()) {
                    gtk_label_set_text(GTK_LABEL(self->device_status_), _("Storage service unavailable")); self->devices_.clear(); self->selected_ = GTK_INVALID_LIST_POSITION;
                    discovery_report = std::string(_("Drive discovery failed: ")) + error;
                }
                else {
                    const auto* old = self->selected_device(); const std::optional<Device> previous = old ? std::optional<Device>(*old) : std::nullopt;
                    self->devices_.clear(); unsigned protected_count = 0;
                    for (const auto& d : found) {
                        if (d.eligible()) self->devices_.push_back(d);
                        else { ++protected_count; discovery_report += d.path + ": " + d.safety_reason + "\n"; }
                    }
                    self->selected_ = GTK_INVALID_LIST_POSITION;
                    for (unsigned i = 0; i < self->devices_.size(); ++i)
                        if (previous && same_identity(*previous, self->devices_[i])) self->selected_ = i;
                    // Never select a different physical drive after disconnect/hotplug.
                    std::string status = std::to_string(self->devices_.size()) + _(" available · ") + std::to_string(protected_count) + _(" protected");
                    gtk_label_set_text(GTK_LABEL(self->device_status_), status.c_str());
                    discovery_report = status + "\n" + discovery_report;
                }
                if (discovery_report != self->device_report_) { self->device_report_ = discovery_report; self->record(discovery_report); }
                self->syncing_selection_ = true;
                gtk_string_list_splice(self->device_model_, 0, g_list_model_get_n_items(G_LIST_MODEL(self->device_model_)), nullptr);
                for (const auto& d : self->devices_) { const std::string text = (d.model.empty() ? _("External drive") : d.model) + " · " + human_size(d.size) + " · " + d.path; gtk_string_list_append(self->device_model_, text.c_str()); }
                self->syncing_selection_ = false; self->sync_selection(); self->update_controls();
            });
        }).detach(); }
        catch (const std::exception& error) { refreshing_ = false; record(std::string(_("Could not start drive discovery: ")) + error.what()); }
    }
    using Work = std::function<std::string(const ProgressFn&, const std::atomic_bool&)>;
    using Failure = std::function<void(const std::string&, const std::string&)>;
    void work(const char* title, Work task, std::function<void(const std::string&)> success = {}, bool cancellable = true, bool changes_drive = false, Failure failed = {}, bool uses_test_files = false) {
        if (busy_) return;
        busy_ = true; can_cancel_ = cancellable; cancel_ = std::make_shared<std::atomic_bool>(false);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar_), 0); technical_result(""); report(title, _("Preparing and checking safety…")); update_controls(); record(title);
        auto self = shared_from_this(); const auto cancellation = cancel_;
        try { std::thread([self, task = std::move(task), success = std::move(success), cancellation, changes_drive, failed, uses_test_files] {
            ProgressFn progress = [self, uses_test_files, last = std::chrono::steady_clock::time_point{}](const Progress& p) mutable {
                const auto now = std::chrono::steady_clock::now(); if (now - last < std::chrono::milliseconds(120) && p.done != p.total) return; last = now;
                self->post([self, p, uses_test_files] {
                    if (!self->alive_ || !self->busy_) return;
                    if (self->cancel_->load()) return;
                    const bool streaming = p.stage == "Writing" || p.stage == "Verifying" || p.stage == "Checksum" || p.stage == "Backing up" || p.stage == "Scanning" || p.stage == "Checking source";
                    std::string stage = p.stage == "Writing" ? _("Writing to USB") : p.stage == "Verifying" ? _("Checking written data") : p.stage == "Checksum" ? _("Checking image file") : p.stage == "Backing up" ? _("Saving backup") : p.stage == "Scanning" ? _("Reading USB") : p.stage == "Checking source" ? _("Checking the selected image again") : p.stage;
                    if (uses_test_files && p.stage == "Writing") stage = _("Writing a temporary test file");
                    if (uses_test_files && p.stage == "Verifying") stage = _("Checking temporary test data");
                    if (uses_test_files && p.stage == "Checking source") stage = _("Checking the temporary test image");
                    gtk_label_set_text(GTK_LABEL(self->progress_title_), stage.c_str());
                    const double fraction = p.total ? std::min(1.0, static_cast<double>(p.done) / static_cast<double>(p.total)) : 0;
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->progress_bar_), fraction);
                    std::ostringstream status; if (p.total) status << static_cast<int>(fraction * 100) << "%";
                    if (streaming && p.total) status << "  ·  " << human_size(p.done) << " / " << human_size(p.total);
                    if (p.bytes_per_second > 0) status << "  ·  " << human_size(static_cast<uint64_t>(p.bytes_per_second)) << "/s";
                    if (streaming && p.bytes_per_second > 0 && p.total > p.done) {
                        const double remaining = static_cast<double>(p.total - p.done) / p.bytes_per_second;
                        if (remaining < 60) status << _("  ·  Less than a minute left in this step");
                        else status << _("  ·  About ") << static_cast<uint64_t>(remaining / 60) + 1 << _(" min left in this step");
                    }
                    if (uses_test_files) status << _("  ·  No USB is used or changed.");
                    else if (p.stage == "Writing") status << _("  ·  Keep the USB connected; checking comes next.");
                    else if (p.stage == "Verifying") status << _("  ·  Keep the USB connected until checking finishes.");
                    if (!p.detail.empty()) self->technical_result(p.detail);
                    gtk_label_set_text(GTK_LABEL(self->progress_detail_), status.str().c_str());
                });
            };
            std::string result, error, image_advice; std::optional<ImageIssue> image_issue; bool stopped = false;
            try { result = task(progress, *cancellation); }
            catch (const Cancelled& e) { error = changes_drive ? e.what() : _("Operation cancelled. Your source image or drive was left unchanged."); stopped = true; }
            catch (const ImageError& e) { error = e.what(); image_issue = e.issue(); image_advice = e.advice(); }
            catch (const std::exception& e) { error = e.what(); }
            self->post([self, result, error, image_issue, image_advice, stopped, success, changes_drive, failed] {
                if (!self->alive_) return;
                self->busy_ = false; self->can_cancel_ = false;
                if (error.empty()) {
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->progress_bar_), 1); self->report(_("Completed"), result); self->record(result);
                    try { if (success) success(result); }
                    catch (const std::exception& display_error) { self->report(_("Operation completed; display update failed"), display_error.what(), true); }
                }
                else {
                    const bool image_guidance = image_issue.has_value() && !changes_drive;
                    const std::string heading = stopped ? _("Cancelled") : image_guidance ? image_issue_heading(*image_issue) : _("Could not finish");
                    const std::string advice = stopped ? error : image_guidance ? image_advice : self->failure_advice(error, changes_drive);
                    self->report(heading, advice, !stopped && !image_guidance);
                    self->technical_result(error); self->record(std::string(stopped ? "Cancelled: " : image_guidance ? "Image guidance: " : "Failed: ") + error);
                    if (failed) failed(heading, advice);
                }
                self->update_controls(); self->refresh_devices();
                if (self->close_after_) { self->alive_ = false; gtk_window_destroy(GTK_WINDOW(self->window_)); }
            });
        }).detach(); }
        catch (const std::exception& error) {
            busy_ = false; can_cancel_ = false;
            const std::string heading = _("Could not start operation"), advice = _("Try the operation again. Technical details are available below.");
            report(heading, advice, true); technical_result(error.what()); if (failed) failed(heading, advice); update_controls();
        }
    }
    void choose_image(bool checksum_only = false) {
        if (busy_) return;
        auto* dialog = gtk_file_dialog_new(); gtk_file_dialog_set_title(dialog, checksum_only ? _("Choose a download to check") : _("Choose a disk image"));
        auto* filters = g_list_store_new(GTK_TYPE_FILE_FILTER); auto* filter = gtk_file_filter_new(); gtk_file_filter_set_name(filter, _("Disk images (ISO, IMG, RAW)"));
        for (const char* suffix : {"*.iso", "*.ISO", "*.img", "*.IMG", "*.raw", "*.RAW"}) gtk_file_filter_add_pattern(filter, suffix);
        g_list_store_append(filters, filter); g_object_unref(filter); filter = gtk_file_filter_new(); gtk_file_filter_set_name(filter, _("All files")); gtk_file_filter_add_pattern(filter, "*"); g_list_store_append(filters, filter);
        gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
        if (checksum_only) gtk_file_dialog_set_default_filter(dialog, filter);
        g_object_unref(filter); g_object_unref(filters);
        struct ImageChoice { std::shared_ptr<Desktop> desktop; bool checksum_only; };
        gtk_file_dialog_open(dialog, GTK_WINDOW(window_), nullptr, +[](GObject* source, GAsyncResult* result, gpointer data) {
            std::unique_ptr<ImageChoice> choice(static_cast<ImageChoice*>(data)); auto self = choice->desktop; GError* error = nullptr;
            GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
            if (!self->alive_) { g_clear_object(&file); g_clear_error(&error); return; }
            if (!file) { if (error && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) self->message(_("Could not open image"), error->message); g_clear_error(&error); return; }
            char* path = g_file_get_path(file); g_object_unref(file);
            if (!path) { self->message(_("Local file required"), _("Download this image to a local folder first.")); return; }
            std::string chosen(path); g_free(path); self->inspect(chosen, choice->checksum_only);
        }, new ImageChoice{shared_from_this(), checksum_only}); g_object_unref(dialog);
    }
    void reset_source(const char* reason) {
        image_ = {}; digest_.clear(); source_issue_hint_.clear(); checksum_failed_ = false;
        gtk_editable_set_text(GTK_EDITABLE(expected_), "");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(algorithm_), 0);
        gtk_label_set_text(GTK_LABEL(source_name_), _("No image selected")); gtk_label_set_text(GTK_LABEL(source_size_), "");
        gtk_label_set_text(GTK_LABEL(source_description_), reason); gtk_label_set_text(GTK_LABEL(image_status_), _("Choose an image"));
        gtk_widget_remove_css_class(image_status_, "success"); gtk_widget_remove_css_class(image_status_, "warning");
        gtk_label_set_text(GTK_LABEL(source_technical_), _("Image details appear after you choose a file."));
        gtk_label_set_text(GTK_LABEL(image_hash_), _("No checksum available")); gtk_label_set_text(GTK_LABEL(check_name_), _("No image selected"));
        gtk_label_set_text(GTK_LABEL(check_result_), _("Choose an image to calculate its checksum."));
    }
    void source_issue(const std::string& path, const std::string& heading, const std::string& advice) {
        reset_source(advice.c_str());
        source_issue_hint_ = advice;
        gtk_label_set_text(GTK_LABEL(source_name_), base_name(path).c_str());
        gtk_label_set_text(GTK_LABEL(image_status_), heading.c_str());
        gtk_widget_add_css_class(image_status_, "warning");
        gtk_label_set_text(GTK_LABEL(source_technical_), path.c_str());
        gtk_label_set_text(GTK_LABEL(check_name_), base_name(path).c_str());
        gtk_label_set_text(GTK_LABEL(check_result_), advice.c_str());
    }
    void inspect(const std::string& path, bool checksum_only = false) {
        if (busy_) return;
        reset_source(_("Checking the file before use…")); update_controls(); gtk_label_set_text(GTK_LABEL(source_name_), base_name(path).c_str());
        gtk_label_set_text(GTK_LABEL(image_status_), _("Checking image…"));
        gtk_label_set_text(GTK_LABEL(source_size_), ""); gtk_label_set_text(GTK_LABEL(check_name_), base_name(path).c_str());
        gtk_label_set_text(GTK_LABEL(check_result_), _("Calculating SHA-256…"));
        gtk_label_set_text(GTK_LABEL(source_description_), _("Inspecting image contents…")); gtk_label_set_text(GTK_LABEL(image_hash_), _("Calculating SHA-256…"));
        auto inspected = std::make_shared<Image>(); auto digest = std::make_shared<std::string>();
        work(checksum_only ? _("Checking downloaded file") : _("Inspecting source image"), [path, inspected, digest, checksum_only](const ProgressFn& progress, const std::atomic_bool& cancel) {
            try { *inspected = inspect_image(path); }
            catch (const ImageError& error) {
                if (!checksum_only || error.issue() == ImageIssue::incomplete) throw;
                *inspected = identify_file(path);
                inspected->kind = "Checksum-only file"; inspected->description = error.advice();
                inspected->raw_writable = false;
            }
            *digest = checksum(path, "sha256", progress, cancel);
            const auto checked = identify_file(path);
            if (!same_source(*inspected, checked))
                throw Error(_("The image changed while it was being checked. Choose it again."));
            inspected->expected_sha256 = *digest;
            return std::string(_("Image inspected. SHA-256 checksum calculated."));
        }, [this, inspected, digest](const std::string&) {
            image_ = *inspected; digest_ = *digest; digest_algorithm_ = "sha256";
            gtk_label_set_text(GTK_LABEL(source_name_), image_.name.c_str());
            std::string explanation;
            if (image_.kind.find("Windows") != std::string::npos) explanation = _("Windows installers need a different USB creation method. Windows support is not available in this version.");
            else if (image_.kind == "Checksum-only file") explanation = image_.description + " " + _("This file is available for checksum checks only. Choose a complete USB-ready image to create a USB.");
            else if (!image_.raw_writable) explanation = image_.description + " " + _("You can still use Check download to compare its checksum with the publisher's.");
            else if (!image_.bootable) explanation = _("This disk image can be copied, but Kavrune could not confirm boot information. Use a trusted image intended for USB drives.");
            else explanation = _("This image can be written to a USB. Its existing boot information will be kept.");
            gtk_label_set_text(GTK_LABEL(source_description_), explanation.c_str());
            gtk_label_set_text(GTK_LABEL(source_size_), human_size(image_.size).c_str());
            gtk_label_set_text(GTK_LABEL(image_status_), image_.raw_writable ? (image_.bootable ? _("Ready to write") : _("Check image source")) : image_.kind == "Checksum-only file" ? _("Checksum only") : _("Different creation method needed"));
            gtk_widget_add_css_class(image_status_, image_.raw_writable && image_.bootable ? "success" : "warning");
            gtk_label_set_text(GTK_LABEL(source_technical_), (image_.kind + "\n" + image_.description + "\n" + image_.path).c_str());
            gtk_label_set_text(GTK_LABEL(check_name_), (image_.name + "\n" + human_size(image_.size)).c_str()); gtk_label_set_text(GTK_LABEL(image_hash_), digest_.c_str());
            gtk_label_set_text(GTK_LABEL(check_result_), digest_.c_str());
            report(image_.kind == "Checksum-only file" ? _("File ready for checksum comparison") : _("Image checked"), explanation);
            technical_result(image_.description);
            update_controls();
        }, true, false, [this, path](const std::string& heading, const std::string& advice) { source_issue(path, heading, advice); });
    }
    void calculate_checksum() {
        if (image_.path.empty()) return;
        const Image source = image_; const std::string algo = gtk_drop_down_get_selected(GTK_DROP_DOWN(algorithm_)) == 1 ? "sha512" : "sha256";
        std::string expected = gtk_editable_get_text(GTK_EDITABLE(expected_));
        expected.erase(std::remove_if(expected.begin(), expected.end(), [](unsigned char c) { return g_ascii_isspace(c); }), expected.end());
        std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char c) { return static_cast<char>(g_ascii_tolower(c)); });
        const size_t length = algo == "sha256" ? 64 : 128;
        if (!expected.empty() && (expected.size() != length || expected.find_first_not_of("0123456789abcdef") != std::string::npos)) {
            message(_("Check the expected checksum"), _("Paste only the hexadecimal checksum for the selected algorithm.")); return;
        }
        work(_("Calculating image checksum"), [source, algo](const ProgressFn& progress, const std::atomic_bool& cancel) {
                if (!same_source(source, identify_file(source.path))) throw Error(_("The file changed after selection. Choose it again."));
                const auto result = checksum(source.path, algo, progress, cancel);
                if (!same_source(source, identify_file(source.path))) throw Error(_("The file changed while checking. Choose it again."));
                return result;
            },
            [this, expected, algo](const std::string& result) {
                digest_ = result; digest_algorithm_ = algo; gtk_label_set_text(GTK_LABEL(check_result_), result.c_str());
                if (expected.empty()) report(_("Checksum calculated"), _("Compare this checksum with one published by the image publisher."));
                else if (expected == result) { checksum_failed_ = false; report(_("Checksum matches"), image_.raw_writable ? _("The image matches the publisher's checksum. You can continue creating your USB.") : image_.kind == "Checksum-only file" ? _("The file matches the publisher's checksum. Checksum verification does not make this file suitable for creating a USB.") : _("The file matches the publisher's checksum. It still needs the creation method explained in step 1.")); record(_("Expected checksum matched.")); }
                else { checksum_failed_ = true; report(_("Checksum does not match"), _("Creating a USB is blocked. Check that you pasted the correct checksum, or download the image again and choose it again."), true); record(_("Expected checksum did not match.")); }
                update_controls();
            }, true, false, [this, source](const std::string& heading, const std::string& advice) { source_issue(source.path, heading, advice); });
    }
    void confirm_device(const char* action, const Device& device, bool destructive, const std::string& plan, Fn execute) {
        std::string details = device.display_name() + "\n" + identity_text(device) + "\n\n" + plan;
        if (destructive) details += "\n\n" + std::string(_("All existing files and partitions on this drive will be lost. This operation is not a secure full-drive erase."));
        else details += "\n\n" + std::string(_("Mounted volumes will be unmounted. This operation reads the drive without changing its contents."));
        auto* dialog = adw_message_dialog_new(GTK_WINDOW(window_), action, details.c_str());
        adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "cancel", _("Cancel")); adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "proceed", action);
        adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(dialog), "cancel"); adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog), "cancel");
        if (destructive) {
            adw_message_dialog_set_response_appearance(ADW_MESSAGE_DIALOG(dialog), "proceed", ADW_RESPONSE_DESTRUCTIVE);
            adw_message_dialog_set_response_enabled(ADW_MESSAGE_DIALOG(dialog), "proceed", FALSE);
            auto* extra = box(GTK_ORIENTATION_VERTICAL, 8); const std::string required = base_name(device.path);
            append(extra, label(std::string(_("Type the drive name to confirm: ")) + required, "card-caption")); auto* entry = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(entry), required.c_str()); append(extra, entry); adw_message_dialog_set_extra_child(ADW_MESSAGE_DIALOG(dialog), extra);
            struct ConfirmEntry { AdwMessageDialog* dialog; std::string required; };
            g_signal_connect_data(entry, "changed", G_CALLBACK(+[](GtkEditable* e, gpointer data) {
                auto* c = static_cast<ConfirmEntry*>(data); adw_message_dialog_set_response_enabled(c->dialog, "proceed", c->required == gtk_editable_get_text(e));
            }), new ConfirmEntry{ADW_MESSAGE_DIALOG(dialog), required}, +[](gpointer p, GClosure*) { delete static_cast<ConfirmEntry*>(p); }, G_CONNECT_DEFAULT);
        }
        auto* callback = new Fn(std::move(execute));
        g_signal_connect_data(dialog, "response", G_CALLBACK(+[](AdwMessageDialog*, const char* response, gpointer data) {
            if (std::string(response) == "proceed") (*static_cast<Fn*>(data))();
        }), callback, +[](gpointer p, GClosure*) { delete static_cast<Fn*>(p); }, G_CONNECT_DEFAULT); gtk_window_present(GTK_WINDOW(dialog));
    }
    void create_media() {
        const auto* selected = selected_device(); if (busy_ || !selected || !selected->eligible() || !image_.raw_writable || !image_.size || image_.size > selected->size || checksum_failed_) return;
        const Device target = *selected; const Image source = image_;
        const std::string plan = std::string(_("Source: ")) + source.name + " (" + human_size(source.size) + ")\n" +
            _("Plan: unmount volumes, write the original image layout, flush all data, verify every written byte, and refresh the drive.");
        confirm_device(_("Erase USB and create"), target, true, plan, [this, target, source] {
            if (checksum_failed_) return;
            work(_("Creating your USB"), [target, source](const ProgressFn& progress, const std::atomic_bool& cancel) {
                Storage storage; const auto checked = storage.revalidate(target);
                const Image current_source = inspect_image(source.path);
                if (!same_source(source, current_source))
                    throw Error(_("The source image changed after selection. Choose it again before creating media."));
                if (storage.source_on_device(source.path, checked)) throw Error(_("The source image is stored on the target drive. Move it to another drive first."));
                if (cancel) throw Cancelled();
                storage.unmount(checked); const auto current = storage.revalidate(checked);
                std::string hash; { UniqueFd fd(storage.open_device(current, true)); hash = write_image(source, fd.fd, current.size, true, progress, cancel); }
                storage.rescan(current); return hash;
            }, [this](const std::string& hash) {
                report(_("Written and verified"), _("Every written byte matches your image. Safely eject the USB in your file manager. To boot from it, restart the computer and choose USB in its boot menu. The key and supported images depend on your computer."));
                technical_result(_("Written image SHA-256: ") + hash);
                record(_("USB written and verified. SHA-256: ") + hash);
            }, true, true);
        });
    }
    void format_drive() {
        const auto* selected = selected_device(); if (busy_ || !selected) return; const Device target = *selected;
        FormatOptions options; options.scheme = gtk_drop_down_get_selected(GTK_DROP_DOWN(scheme_)) ? "mbr" : "gpt";
        const unsigned fs = gtk_drop_down_get_selected(GTK_DROP_DOWN(filesystem_)); options.filesystem = fs == 1 ? "exfat" : fs == 2 ? "ext4" : "vfat";
        options.label = gtk_editable_get_text(GTK_EDITABLE(volume_label_));
        const std::string plan = std::string(_("Create a new ")) + (options.scheme == "gpt" ? "GPT" : "MBR") + _(" partition table with one ") +
            (options.filesystem == "vfat" ? "FAT32" : options.filesystem == "exfat" ? "exFAT" : "ext4") + _(" volume. Label: ") + options.label;
        confirm_device(_("Erase and format drive"), target, true, plan, [this, target, options] {
            work(_("Formatting drive"), [target, options](const ProgressFn& progress, const std::atomic_bool& cancel) {
                Storage storage; storage.format(target, options, progress, cancel); return std::string(_("Drive formatted successfully. It is ready for files."));
            }, {}, false, true);
        });
    }
    void backup_drive() {
        const auto* selected = selected_device(); if (busy_ || !selected) return; const Device target = *selected;
        auto* dialog = gtk_file_dialog_new(); gtk_file_dialog_set_title(dialog, _("Save a full drive backup")); gtk_file_dialog_set_initial_name(dialog, (base_name(target.path) + "-backup.img").c_str());
        struct SaveContext { std::shared_ptr<Desktop> self; Device target; };
        gtk_file_dialog_save(dialog, GTK_WINDOW(window_), nullptr, +[](GObject* source, GAsyncResult* result, gpointer data) {
            std::unique_ptr<SaveContext> ctx(static_cast<SaveContext*>(data)); GError* error = nullptr; GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);
            if (!ctx->self->alive_) { g_clear_object(&file); g_clear_error(&error); return; }
            if (!file) { if (error && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) ctx->self->message(_("Could not choose backup location"), error->message); g_clear_error(&error); return; }
            char* p = g_file_get_path(file); g_object_unref(file); if (!p) { ctx->self->message(_("Local folder required"), _("Choose a local folder for this backup.")); return; }
            std::string destination(p); g_free(p); const Device target = ctx->target; auto self = ctx->self;
            self->confirm_device(_("Back up drive"), target, false, std::string(_("Save the whole drive (") ) + human_size(target.size) + _(") to:\n") + destination + _("\nExisting files will never be overwritten."), [self, target, destination] {
                self->work(_("Backing up drive"), [target, destination](const ProgressFn& progress, const std::atomic_bool& cancel) {
                    Storage storage; const auto checked = storage.revalidate(target);
                    const std::string parent = std::filesystem::path(destination).parent_path().string();
                    if (storage.source_on_device(parent, checked)) throw Error(_("The backup cannot be saved onto the drive being backed up."));
                    if (cancel) throw Cancelled();
                    storage.unmount(checked); const auto current = storage.revalidate(checked); UniqueFd fd(storage.open_device(current, false));
                    const auto hash = backup_image(fd.fd, current.size, destination, progress, cancel);
                    return std::string(_("Backup saved. SHA-256: ")) + hash;
                });
            });
        }, new SaveContext{shared_from_this(), target}); g_object_unref(dialog);
    }
    void scan_drive() {
        const auto* selected = selected_device(); if (busy_ || !selected) return; const Device target = *selected;
        confirm_device(_("Scan drive"), target, false, _("Read every byte to check for unreadable areas. This scan cannot prove genuine capacity or predict future drive failure."), [this, target] {
            work(_("Scanning drive"), [target](const ProgressFn& progress, const std::atomic_bool& cancel) {
                Storage storage; const auto checked = storage.revalidate(target); if (cancel) throw Cancelled(); storage.unmount(checked);
                const auto current = storage.revalidate(checked); UniqueFd fd(storage.open_device(current, false)); read_scan(fd.fd, current.size, progress, cancel);
                return std::string(_("Full read scan passed. No unreadable bytes were found. This does not verify genuine capacity."));
            });
        });
    }
    void safe_test() {
        if (busy_) return;
        work(_("Running a safe test"), [](const ProgressFn& progress, const std::atomic_bool& cancel) {
            const auto result = run_self_test(progress, cancel);
            return human_size(result.bytes) + "\nSHA-256: " + result.sha256;
        }, [this](const std::string& result) {
            report(_("Safe test passed"), _("Writing and verification work with temporary files. No USB was changed. An actual USB and your computer's boot firmware still need testing."));
            technical_result(result + _("\nTemporary test files were removed. No physical drive was opened."));
            record(_("Safe test passed; no USB was changed."));
        }, true, false, {}, true);
    }
    void build_help() {
        auto* v = page("help"); append(v, hero(_("START HERE"), _("How to use Kavrune"), _("Make a USB in three steps. You do not need to know partition tables or filesystems to create one.")));
        GtkWidget* contents;
        append(v, card("✓", _("Try a safe test first"), _("Check writing and verification using temporary files on your computer. No USB is needed or changed."), &contents));
        auto* test = control(button(_("Try a safe test"), "suggested-action")); gtk_widget_set_halign(test, GTK_ALIGN_START); click(test, [this] { safe_test(); }); append(contents, test);
        append(contents, label(_("A passing safe test checks the software's file-writing path. It cannot confirm a physical USB or guarantee a computer will boot."), "card-caption"));
        append(v, card("1", _("Download and choose an image"), _("An ISO or IMG is a file containing an operating system, such as a Linux distribution."), &contents));
        append(contents, label(_("Get a USB-ready image from the operating system's official website. In Create USB, choose that downloaded file. Kavrune checks the file before allowing you to continue. To check the download against the publisher's fingerprint, use Check download and paste their checksum."), "card-caption"));
        append(v, card("2", _("Connect a USB and keep a backup"), _("Everything on the selected USB will be erased during creation."), &contents));
        append(contents, label(_("Copy files you need to another drive first, or use Back up USB to save a complete image. Select your USB by its name and capacity. Kavrune does not select it automatically. Do not use a drive that holds your only copy of important files."), "card-caption"));
        append(v, card("3", _("Create, wait, and boot"), _("Choose Create USB, confirm the exact drive, then keep it connected."), &contents));
        append(contents, label(_("First Kavrune writes the image. Then it reads the written data back and checks every byte. Wait for Written and verified. Safely eject the USB in your file manager, then restart the computer and choose the USB in its boot menu. The menu key and supported images depend on the computer; check its manufacturer's instructions."), "card-caption"));
        append(v, card("?", _("Create USB or Format USB?"), nullptr, &contents));
        append(contents, label(_("Create USB copies a system image, including its own boot layout. Use it to try or install Linux, or to restore a disk backup. Format USB erases the drive and makes it ready for normal files. Formatting alone does not make an operating system USB."), "card-caption"));
        append(v, card("?", _("Why is my USB missing?"), nullptr, &contents));
        append(contents, label(_("Reconnect it and choose Refresh. Kavrune hides internal, in-use, read-only and other protected drives. Some adapters or drives do not provide the identity information needed for safe writing. Activity lists the protection reason. If the storage service is unavailable, check that UDisks2 is running."), "card-caption"));
        append(v, card("i", _("What this preview supports"), nullptr, &contents));
        append(contents, label(_("USB-ready Linux images and raw disk backups, full verification, formatting, backups, image checksums and read scans. Windows installer creation, persistent Linux storage, compressed images, fake-capacity tests and automatic image downloads are not available. Physical drive and boot testing, accessibility review and enterprise security qualification remain pending."), "card-caption"));
        append(v, note(_("Preview software, supplied without warranty. Storage operations can cause data loss. Keep backups and check the selected drive before confirming an erase."), true));
    }
    void build_create() {
        auto* v = page("create"); append(v, hero(_("USB CREATION"), _("Create your bootable USB"), _("Put a Linux system image on a USB so you can try or install it on a computer.")));
        auto* intro = box(GTK_ORIENTATION_HORIZONTAL, 12); auto* intro_text = label(_("First time? Test Kavrune safely without a USB."), "card-caption"); gtk_widget_set_hexpand(intro_text, TRUE); append(intro, intro_text);
        auto* test = control(button(_("Try a safe test"), "secondary-button")); click(test, [this] { safe_test(); }); append(intro, test); append(v, intro);
        GtkWidget* contents; append(v, card("1", _("Choose your system image"), _("Download an ISO or IMG from your operating system's official website, then choose the file here."), &contents));
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 14); auto* icon = gtk_image_new_from_icon_name("application-x-cd-image-symbolic"); gtk_image_set_pixel_size(GTK_IMAGE(icon), 25); gtk_widget_add_css_class(icon, "source-icon"); gtk_widget_set_valign(icon, GTK_ALIGN_START); append(row, icon);
        auto* info = box(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_set_hexpand(info, TRUE); source_name_ = label(_("Choose an operating system image"), "file-name");
        source_description_ = label(_("An image file contains the system you want to put on your USB."), "card-caption"); source_size_ = label("", "card-caption");
        gtk_label_set_selectable(GTK_LABEL(source_name_), TRUE); append(info, source_name_); append(info, source_description_); append(info, source_size_); append(row, info);
        append(contents, row);
        auto* source_actions = box(GTK_ORIENTATION_HORIZONTAL, 12); auto* choose = control(button(_("Choose image…"), "secondary-button")); click(choose, [this] { choose_image(); }); append(source_actions, choose);
        image_status_ = label(_("Choose an image"), "status-badge"); append(source_actions, image_status_); append(contents, source_actions);
        auto* expander = gtk_expander_new(_("Image details and checksum")); auto* details = box(GTK_ORIENTATION_VERTICAL, 8);
        source_technical_ = label(_("Image details appear after you choose a file."), "technical-text"); gtk_label_set_selectable(GTK_LABEL(source_technical_), TRUE); append(details, source_technical_);
        append(details, label("SHA-256", "card-caption")); image_hash_ = label(_("Calculated automatically after selection"), "checksum");
        gtk_label_set_selectable(GTK_LABEL(image_hash_), TRUE); gtk_label_set_wrap_mode(GTK_LABEL(image_hash_), PANGO_WRAP_CHAR); append(details, image_hash_); gtk_expander_set_child(GTK_EXPANDER(expander), details); append(contents, expander);
        append(v, target_card());
        append(v, card("3", _("Review and create"), _("Creating this USB erases everything currently on it."), &contents));
        review_text_ = label(_("Your image and USB will appear here when both are selected."), "review-summary"); append(contents, review_text_);
        append(contents, note(_("Kavrune checks every written byte before reporting success. Keep the USB connected until both writing and checking finish.")));
        create_hint_ = label(_("Next: choose your operating system image in step 1."), "next-step"); append(contents, create_hint_);
        create_ = button(_("Create USB…"), "suggested-action"); gtk_widget_add_css_class(create_, "action-button"); gtk_widget_set_halign(create_, GTK_ALIGN_END); click(create_, [this] { create_media(); }); append(contents, create_);
    }
    void build_format() {
        auto* v = page("format"); append(v, hero(_("EVERYDAY FILES"), _("Use your USB for files"), _("Erase the USB and make space for photos, documents and other files. This does not install an operating system."))); append(v, target_card("1"));
        GtkWidget* contents; append(v, card("2", _("Set up your USB"), _("The defaults work for most computers. Change these settings only if you need to."), &contents));
        auto field = [this, contents](const char* text, GtkWidget* w) { auto* row = box(GTK_ORIENTATION_VERTICAL, 6); append(row, label(text, "card-title")); gtk_widget_set_hexpand(w, TRUE); append(row, control(w)); append(contents, row); };
        const char* schemes[] = {"GPT (modern devices)", "MBR (legacy compatibility)", nullptr}; scheme_ = gtk_drop_down_new_from_strings(schemes); field(_("Partition table"), scheme_);
        const char* filesystems[] = {"FAT32 · widely compatible", "exFAT · large files", "ext4 · Linux", nullptr}; filesystem_ = gtk_drop_down_new_from_strings(filesystems); field(_("Filesystem"), filesystem_);
        volume_label_ = gtk_entry_new(); gtk_editable_set_text(GTK_EDITABLE(volume_label_), "KAVRUNE"); gtk_entry_set_max_length(GTK_ENTRY(volume_label_), 16); field(_("Volume label"), volume_label_);
        append(v, note(_("Formatting removes files and partitions. FAT32 limits individual files to 4 GiB. exFAT works well for larger files; ext4 is intended for Linux."), true));
        format_ = button(_("Format drive"), "destructive-action"); gtk_widget_add_css_class(format_, "action-button"); click(format_, [this] { format_drive(); });
        append(v, action_row(format_, _("Formatting cannot be cancelled once it starts.")));
    }
    void build_backup() {
        auto* v = page("backup"); append(v, hero(_("KEEP YOUR DATA"), _("Back up your USB"), _("Save a complete copy of your USB in one file before you erase or change it."))); append(v, target_card("1"));
        GtkWidget* contents; append(v, card("02", _("Choose where to save"), _("The backup is an uncompressed IMG file, the size of the whole drive."), &contents));
        append(contents, label(_("Choose a destination on a different drive with enough free space. Existing files are protected against overwrite. A cancelled backup is removed automatically."), "card-caption"));
        append(v, note(_("The source is opened read-only. Mounted volumes are unmounted to keep the backup consistent. Restore a backup later using Create USB.")));
        backup_ = button(_("Save backup…"), "suggested-action"); gtk_widget_add_css_class(backup_, "action-button"); click(backup_, [this] { backup_drive(); }); append(v, action_row(backup_, _("Your source drive contents are preserved.")));
    }
    void build_check() {
        auto* v = page("check"); append(v, hero(_("CHECK YOUR DOWNLOAD"), _("Is your image file complete?"), _("A checksum is a file's fingerprint. Compare it with the one on the publisher's website to check your download.")));
        GtkWidget* contents; append(v, card("01", _("Select your image"), _("Checksums validate file integrity. Use a trusted publisher's checksum."), &contents));
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 16); check_name_ = label(_("No image selected"), "file-name"); gtk_widget_set_hexpand(check_name_, TRUE); append(row, check_name_);
        auto* choose = control(button(_("Choose image…"), "secondary-button")); click(choose, [this] { choose_image(true); }); append(row, choose); append(contents, row);
        append(v, card("02", _("Compare a checksum"), _("Expected value is optional. Paste the hexadecimal value only."), &contents));
        const char* algorithms[] = {"SHA-256", "SHA-512", nullptr}; algorithm_ = control(gtk_drop_down_new_from_strings(algorithms)); append(contents, algorithm_);
        expected_ = control(gtk_entry_new()); gtk_entry_set_placeholder_text(GTK_ENTRY(expected_), _("Expected checksum from the publisher")); gtk_entry_set_max_length(GTK_ENTRY(expected_), 256); gtk_widget_set_tooltip_text(expected_, _("Expected hexadecimal checksum")); append(contents, expected_);
        check_result_ = label(_("The calculated checksum will appear here."), "checksum"); gtk_label_set_selectable(GTK_LABEL(check_result_), TRUE); gtk_label_set_wrap_mode(GTK_LABEL(check_result_), PANGO_WRAP_CHAR); append(contents, check_result_);
        auto* copy = button(_("Copy checksum"), "secondary-button"); click(copy, [this] { if (!digest_.empty()) gdk_clipboard_set_text(gtk_widget_get_clipboard(window_), digest_.c_str()); }); gtk_widget_set_halign(copy, GTK_ALIGN_START); append(contents, copy);
        check_ = button(_("Calculate and compare"), "suggested-action"); gtk_widget_add_css_class(check_, "action-button"); click(check_, [this] { calculate_checksum(); }); append(v, action_row(check_, _("Reads only the selected image file.")));
    }
    void build_scan() {
        auto* v = page("scan"); append(v, hero(_("CHECK YOUR USB"), _("Check for reading problems"), _("Read the USB to find data that cannot be read. Your files are kept."))); append(v, target_card("1"));
        GtkWidget* contents; append(v, card("02", _("Full read scan"), _("Every address reported by the drive is read once."), &contents));
        append(contents, label(_("This may take hours on a large drive. A successful scan means its contents were readable during this operation. It does not detect fake capacity, test write reliability, or predict future failures."), "card-caption"));
        append(v, note(_("The drive is opened read-only. Its volumes are unmounted before scanning. You can cancel the scan at any time.")));
        scan_ = button(_("Scan drive"), "suggested-action"); gtk_widget_add_css_class(scan_, "action-button"); click(scan_, [this] { scan_drive(); }); append(v, action_row(scan_, _("No data is written by this scan.")));
    }
    void about() {
        auto* dialog = adw_about_window_new(); adw_about_window_set_application_name(ADW_ABOUT_WINDOW(dialog), "Kavrune");
        adw_about_window_set_application_icon(ADW_ABOUT_WINDOW(dialog), "org.kavrune.Kavrune"); adw_about_window_set_version(ADW_ABOUT_WINDOW(dialog), "0.1.0 Preview");
        adw_about_window_set_developer_name(ADW_ABOUT_WINDOW(dialog), _("Kavrune contributors")); adw_about_window_set_license_type(ADW_ABOUT_WINDOW(dialog), GTK_LICENSE_MIT_X11);
        adw_about_window_set_comments(ADW_ABOUT_WINDOW(dialog), _("An independent Linux-native media utility.\n\nPreview scope: raw hybrid images, mandatory readback verification, formatting, drive backup, checksums, and read-only scans.\n\nWindows installer creation, persistence, compressed images, fake-capacity testing and boot certification are not implemented in this preview. Hardware qualification and security review are required before enterprise deployment.\n\nOriginal project source and artwork are MIT licensed. Dependencies retain their respective licenses."));
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window_)); gtk_window_set_modal(GTK_WINDOW(dialog), TRUE); gtk_window_present(GTK_WINDOW(dialog));
    }
    void build_activity() {
        auto* v = page("activity"); append(v, hero(_("LOCAL HISTORY"), _("What happened in Kavrune"), _("See operation results and technical details. You can copy this activity when reporting a problem.")));
        activity_view_ = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(activity_view_), FALSE); gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(activity_view_), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(activity_view_), GTK_WRAP_WORD_CHAR); gtk_widget_add_css_class(activity_view_, "activity-text");
        auto* scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(scroll, -1, 280); gtk_widget_set_vexpand(scroll, TRUE); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), activity_view_); gtk_widget_add_css_class(scroll, "step-card"); append(v, scroll);
        std::string log_location;
        try { log_location = log_path(); } catch (const std::exception& error) { log_location = std::string(_("Local log unavailable: ")) + error.what(); }
        auto* location = label(log_location, "card-caption"); gtk_label_set_selectable(GTK_LABEL(location), TRUE); append(v, location);
        auto* row = box(GTK_ORIENTATION_HORIZONTAL, 10); auto* copy = button(_("Copy activity"), "secondary-button"); click(copy, [this] {
            auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(activity_view_)); GtkTextIter start, end; gtk_text_buffer_get_bounds(buffer, &start, &end);
            gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE); gdk_clipboard_set_text(gtk_widget_get_clipboard(window_), text); g_free(text);
        }); append(row, copy); auto* a = button(_("About Kavrune"), "secondary-button"); click(a, [this] { about(); }); append(row, a); append(v, row);
        append(v, note(_("Version 0.1 preview. Hardware qualification, boot compatibility testing and security review remain release requirements. Windows installer creation and persistence are planned features.")));
    }
    void build() {
        window_ = adw_application_window_new(GTK_APPLICATION(app_)); gtk_window_set_title(GTK_WINDOW(window_), "Kavrune"); gtk_window_set_default_size(GTK_WINDOW(window_), 1120, 900);
        gtk_widget_set_size_request(window_, 420, 480);
        gtk_widget_add_css_class(window_, "kavrune"); gtk_window_set_icon_name(GTK_WINDOW(window_), "org.kavrune.Kavrune");
        g_signal_connect(window_, "close-request", G_CALLBACK(+[](GtkWindow*, gpointer data) -> gboolean {
            auto* self = static_cast<Desktop*>(data);
            if (self->busy_) {
                self->close_after_ = true; if (self->can_cancel_) self->cancel_->store(true);
                self->report(_("Closing when the operation stops"), self->can_cancel_ ? _("Cancellation requested. Waiting for the drive to be safely released…") : _("Formatting must finish before the app can close."));
                return TRUE;
            }
            self->alive_ = false; return FALSE;
        }), this);
        auto* outer = box(GTK_ORIENTATION_VERTICAL, 0); auto* header = adw_header_bar_new();
        auto* title = adw_window_title_new("Kavrune", _("USB creator for Linux")); adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
        auto* badge = label(_("PREVIEW 0.1"), "preview-badge"); gtk_label_set_wrap(GTK_LABEL(badge), FALSE); adw_header_bar_pack_start(ADW_HEADER_BAR(header), badge);
        auto* help = gtk_button_new_from_icon_name("help-browser-symbolic"); gtk_widget_set_tooltip_text(help, _("How to use Kavrune")); click(help, [this] { g_action_group_activate_action(G_ACTION_GROUP(app_), "nav-help", nullptr); }); adw_header_bar_pack_end(ADW_HEADER_BAR(header), help); append(outer, header);
        split_ = adw_overlay_split_view_new(); gtk_widget_set_vexpand(split_, TRUE); append(outer, split_);
        adw_overlay_split_view_set_min_sidebar_width(ADW_OVERLAY_SPLIT_VIEW(split_), 240); adw_overlay_split_view_set_max_sidebar_width(ADW_OVERLAY_SPLIT_VIEW(split_), 240);
        auto* menu = gtk_button_new_from_icon_name("open-menu-symbolic"); gtk_widget_set_tooltip_text(menu, _("Show navigation"));
        click(menu, [this] { adw_overlay_split_view_set_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_), !adw_overlay_split_view_get_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_))); }); adw_header_bar_pack_start(ADW_HEADER_BAR(header), menu);
        g_object_bind_property(split_, "collapsed", menu, "visible", G_BINDING_SYNC_CREATE);
        auto* breakpoint = adw_breakpoint_new(adw_breakpoint_condition_new_length(ADW_BREAKPOINT_CONDITION_MAX_WIDTH, 840, ADW_LENGTH_UNIT_SP));
        GValue collapsed = G_VALUE_INIT; g_value_init(&collapsed, G_TYPE_BOOLEAN); g_value_set_boolean(&collapsed, TRUE);
        adw_breakpoint_add_setter(breakpoint, G_OBJECT(split_), "collapsed", &collapsed); g_value_unset(&collapsed);
        g_signal_connect(breakpoint, "apply", G_CALLBACK(+[](AdwBreakpoint*, gpointer data) { gtk_widget_add_css_class(static_cast<Desktop*>(data)->window_, "compact"); }), this);
        g_signal_connect(breakpoint, "unapply", G_CALLBACK(+[](AdwBreakpoint*, gpointer data) { gtk_widget_remove_css_class(static_cast<Desktop*>(data)->window_, "compact"); }), this);
        adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window_), breakpoint);
        sidebar_ = box(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_add_css_class(sidebar_, "app-sidebar");
        auto* sidebar_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll), sidebar_);
        adw_overlay_split_view_set_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_), sidebar_scroll);
        auto* brand = box(GTK_ORIENTATION_VERTICAL, 8); gtk_widget_add_css_class(brand, "brand");
        auto* logo = gtk_image_new_from_resource("/org/kavrune/Kavrune/icon.svg"); gtk_image_set_pixel_size(GTK_IMAGE(logo), 44); gtk_widget_set_halign(logo, GTK_ALIGN_START); append(brand, logo);
        append(brand, label("Kavrune", "brand-name")); append(brand, label(_("Your USB, step by step"), "brand-subtitle")); append(sidebar_, brand);
        auto* navigation = gtk_list_box_new(); gtk_widget_add_css_class(navigation, "sidebar-list"); gtk_list_box_set_selection_mode(GTK_LIST_BOX(navigation), GTK_SELECTION_SINGLE); append(sidebar_, navigation);
        navigation_ = navigation;
        struct Item { const char* name; const char* title; const char* description; const char* icon; };
        const Item items[] = {{"create", _("Create USB"), _("Try or install a system"), "media-playback-start-symbolic"}, {"format", _("Format USB"), _("Make space for files"), "drive-removable-media-symbolic"}, {"backup", _("Back up USB"), _("Save a complete copy"), "document-save-symbolic"}, {"check", _("Check download"), _("Verify your download"), "emblem-ok-symbolic"}, {"scan", _("Check USB"), _("Read for errors; keeps files"), "edit-find-symbolic"}, {"activity", _("Activity"), _("Results and details"), "view-list-symbolic"}, {"help", _("How to use"), _("Start here and get help"), "help-browser-symbolic"}};
        unsigned shortcut_index = 1;
        for (const auto& item : items) {
            auto* r = gtk_list_box_row_new(); auto* contents = box(GTK_ORIENTATION_HORIZONTAL, 13); gtk_widget_add_css_class(contents, "nav-item");
            append(contents, gtk_image_new_from_icon_name(item.icon)); auto* words = box(GTK_ORIENTATION_VERTICAL, 3); append(words, label(item.title, "nav-label")); append(words, label(item.description, "nav-description")); append(contents, words); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), contents);
            g_object_set_data_full(G_OBJECT(r), "page", g_strdup(item.name), g_free); gtk_list_box_append(GTK_LIST_BOX(navigation), r);
            // Real application actions also make all pages keyboard accessible.
            const std::string action_name = std::string("nav-") + item.name;
            auto* action = g_simple_action_new(action_name.c_str(), nullptr);
            auto* callback = new Fn([this, navigation, r] {
                if (!alive_) return;
                gtk_list_box_select_row(GTK_LIST_BOX(navigation), GTK_LIST_BOX_ROW(r));
                if (adw_overlay_split_view_get_collapsed(ADW_OVERLAY_SPLIT_VIEW(split_))) adw_overlay_split_view_set_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(split_), FALSE);
            });
            g_signal_connect_data(action, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) { (*static_cast<Fn*>(data))(); }),
                callback, +[](gpointer data, GClosure*) { delete static_cast<Fn*>(data); }, G_CONNECT_DEFAULT);
            g_action_map_add_action(G_ACTION_MAP(app_), G_ACTION(action)); g_object_unref(action);
            const std::string accelerator = "<Primary>" + std::to_string(shortcut_index++);
            const char* accelerators[] = {accelerator.c_str(), nullptr};
            gtk_application_set_accels_for_action(GTK_APPLICATION(app_), ("app." + action_name).c_str(), accelerators);
        }
        auto* filler = box(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_vexpand(filler, TRUE); append(sidebar_, filler);
        device_status_ = label(_("Checking connected drives…"), "sidebar-footer"); append(sidebar_, device_status_);
        auto* main = box(GTK_ORIENTATION_VERTICAL, 0); gtk_widget_set_hexpand(main, TRUE); adw_overlay_split_view_set_content(ADW_OVERLAY_SPLIT_VIEW(split_), main);
        stack_ = gtk_stack_new(); gtk_stack_set_transition_type(GTK_STACK(stack_), GTK_STACK_TRANSITION_TYPE_CROSSFADE); gtk_stack_set_transition_duration(GTK_STACK(stack_), 150); gtk_widget_set_vexpand(stack_, TRUE); append(main, stack_);
        device_model_ = gtk_string_list_new(nullptr); build_create(); build_format(); build_backup(); build_check(); build_scan(); build_activity(); build_help();
        g_signal_connect(navigation, "row-selected", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer data) {
            if (row) {
                auto* self = static_cast<Desktop*>(data);
                gtk_stack_set_visible_child_name(GTK_STACK(self->stack_), static_cast<const char*>(g_object_get_data(G_OBJECT(row), "page")));
                if (adw_overlay_split_view_get_collapsed(ADW_OVERLAY_SPLIT_VIEW(self->split_))) adw_overlay_split_view_set_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(self->split_), FALSE);
            }
        }), this); gtk_list_box_select_row(GTK_LIST_BOX(navigation), gtk_list_box_get_row_at_index(GTK_LIST_BOX(navigation), 0));
        g_signal_connect(navigation, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow*, gpointer data) {
            auto* self = static_cast<Desktop*>(data);
            if (adw_overlay_split_view_get_collapsed(ADW_OVERLAY_SPLIT_VIEW(self->split_))) adw_overlay_split_view_set_show_sidebar(ADW_OVERLAY_SPLIT_VIEW(self->split_), FALSE);
        }), this);
        progress_card_ = box(GTK_ORIENTATION_VERTICAL, 10); gtk_widget_add_css_class(progress_card_, "progress-card"); auto* top = box(GTK_ORIENTATION_HORIZONTAL, 12);
        progress_title_ = label(_("Ready"), "progress-title"); gtk_widget_set_hexpand(progress_title_, TRUE); append(top, progress_title_);
        cancel_button_ = button(_("Cancel"), "secondary-button"); click(cancel_button_, [this] {
            if (busy_ && can_cancel_) { cancel_->store(true); gtk_widget_set_sensitive(cancel_button_, FALSE); report(_("Cancellation requested"), _("Waiting for buffered data to be safely handled…")); }
        }); append(top, cancel_button_); append(progress_card_, top); progress_bar_ = gtk_progress_bar_new(); append(progress_card_, progress_bar_); progress_detail_ = label("", "status-text"); gtk_label_set_selectable(GTK_LABEL(progress_detail_), TRUE); append(progress_card_, progress_detail_);
        progress_expander_ = gtk_expander_new(_("Technical details")); progress_technical_ = label("", "technical-text"); gtk_label_set_selectable(GTK_LABEL(progress_technical_), TRUE); gtk_expander_set_child(GTK_EXPANDER(progress_expander_), progress_technical_); append(progress_card_, progress_expander_);
        append(main, progress_card_); gtk_widget_set_visible(progress_card_, FALSE);
        auto* self_test_action = g_simple_action_new("self-test", nullptr);
        g_signal_connect(self_test_action, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) { auto* self = static_cast<Desktop*>(data); if (self->alive_) self->safe_test(); }), this);
        g_action_map_add_action(G_ACTION_MAP(app_), G_ACTION(self_test_action)); g_object_unref(self_test_action);
        adw_application_window_set_content(ADW_APPLICATION_WINDOW(window_), outer); sync_selection(); update_controls();
        record(_("Kavrune preview opened. No drive changes occur until an operation is explicitly confirmed."));
    }
public:
    explicit Desktop(AdwApplication* app) : app_(app) { build(); }
    ~Desktop() { alive_ = false; cancel_->store(true); if (timer_) g_source_remove(timer_); g_clear_object(&device_model_); }
    void start() {
        gtk_window_present(GTK_WINDOW(window_)); refresh_devices(); timer_ = g_timeout_add_seconds(4, +[](gpointer data) -> gboolean {
            auto* self = static_cast<Desktop*>(data);
            if (!self->alive_) { self->timer_ = 0; return G_SOURCE_REMOVE; }
            self->refresh_devices(); return G_SOURCE_CONTINUE;
        }, this);
    }
    void smoke_close() { if (alive_ && !busy_) { alive_ = false; gtk_window_destroy(GTK_WINDOW(window_)); } }
    void present() { if (alive_) gtk_window_present(GTK_WINDOW(window_)); }
};
struct Options { bool smoke = false, startup_failed = false; std::shared_ptr<Desktop> desktop; };
}

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") options.smoke = true;
        else if (std::string(argv[i]) == "--version") { puts("Kavrune 0.1.0 preview"); return 0; }
        else if (std::string(argv[i]) == "--help") { puts("Kavrune — Linux-native USB media utility\nUsage: kavrune [--smoke-test] [--version]\nRun as your ordinary desktop user. Authorization is requested per operation."); return 0; }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 2; }
    }
    if (geteuid() == 0) { fputs("Kavrune refuses to run its graphical interface as root. Launch it as your ordinary desktop user; UDisks2 and Polkit authorize individual operations.\n", stderr); return 1; }
    setlocale(LC_ALL, "");
#ifdef KAVRUNE_LOCALE_DIR
    bindtextdomain("kavrune", KAVRUNE_LOCALE_DIR);
#endif
    bind_textdomain_codeset("kavrune", "UTF-8"); textdomain("kavrune");
    const auto flags = options.smoke ? G_APPLICATION_NON_UNIQUE : G_APPLICATION_DEFAULT_FLAGS;
    auto* application = adw_application_new("org.kavrune.Kavrune", flags);
    g_signal_connect(application, "activate", G_CALLBACK(+[](GApplication* app, gpointer data) {
        auto* options = static_cast<Options*>(data);
        if (!options->desktop) {
            auto* css = gtk_css_provider_new(); gtk_css_provider_load_from_resource(css, "/org/kavrune/Kavrune/style.css");
            gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION); g_object_unref(css);
            try { options->desktop = std::make_shared<Desktop>(ADW_APPLICATION(app)); options->desktop->start(); }
            catch (const std::exception& error) { options->startup_failed = true; fprintf(stderr, "Kavrune could not start: %s\n", error.what()); g_application_quit(app); return; }
            if (options->smoke) g_timeout_add(2000, +[](gpointer p) -> gboolean { static_cast<Options*>(p)->desktop->smoke_close(); return G_SOURCE_REMOVE; }, options);
        } else options->desktop->present();
    }), &options);
    // Private options were handled above; GTK receives only the executable name.
    int run_argc = 1; const int status = g_application_run(G_APPLICATION(application), run_argc, argv);
    options.desktop.reset(); g_object_unref(application); return options.startup_failed ? 1 : status;
}
