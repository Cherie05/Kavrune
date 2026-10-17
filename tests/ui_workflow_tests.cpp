// SPDX-License-Identifier: MIT
// Exercise the actual desktop control logic with synthetic data. The window is
// constructed but never presented; no discovery, disk operation or dialog runs.
#define KAVRUNE_DESKTOP_TEST
#include "../src/desktop.cpp"
#include <iostream>

namespace {
unsigned workflow_checks = 0;
void workflow_check(bool condition, const char* message) {
    ++workflow_checks;
    if (!condition) throw std::runtime_error(message);
}
bool enabled(GtkWidget* widget) { return gtk_widget_get_sensitive(widget); }
std::string text(GtkWidget* widget) { return gtk_label_get_text(GTK_LABEL(widget)); }

struct DesktopUiTest {
    static void progress_regressions(Desktop& ui) {
        const uint64_t bytes = 8 * 1024 * 1024;
        ui.busy_ = true;
        ui.progress_usb_creation_ = true;
        ui.progress_started_ = std::chrono::steady_clock::now();
        ui.apply_progress({"Checking source", bytes, bytes, 0, ""});
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "finishing the source check never presents the complete operation as finished");
        ui.apply_progress({"Writing", bytes, bytes, 0, ""});
        const double written_fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_));
        workflow_check(written_fraction > 0 && written_fraction < 1,
                       "finishing the write reserves visible progress for saving and verification");
        ui.apply_progress({"Synchronizing", bytes, bytes, 0,
                           "Cancellation waits until buffered writes have been synchronized."});
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "the synchronization stage does not display a completed progress bar");
        workflow_check(gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)),
                       "synchronization visibly animates while the storage worker is waiting");
        workflow_check(ui.progress_timer_ != 0,
                       "synchronization owns a main-thread heartbeat independent of worker callbacks");
        workflow_check(text(ui.progress_title_).find("still working") != std::string::npos,
                       "the synchronization heading explicitly says the application is still working");
        workflow_check(text(ui.progress_detail_).find("connected") != std::string::npos &&
                       text(ui.progress_detail_).find("100%") == std::string::npos,
                       "saving guidance asks users to keep the USB connected without claiming completion");
        workflow_check(std::string(gtk_progress_bar_get_text(GTK_PROGRESS_BAR(ui.progress_bar_))).find("Step") != std::string::npos,
                       "USB creation describes progress as workflow steps rather than a misleading overall percentage");

        const auto detail_before_tick = text(ui.progress_detail_);
        const auto heartbeat = ui.progress_timer_;
        ui.progress_started_ -= std::chrono::seconds(75);
        auto* source = g_main_context_find_source_by_id(nullptr, heartbeat);
        workflow_check(source != nullptr, "the progress heartbeat is attached to the GTK main context");
        g_source_set_ready_time(source, 0);
        for (unsigned i = 0; i < 16 && g_main_context_pending(nullptr); ++i)
            g_main_context_iteration(nullptr, FALSE);
        workflow_check(text(ui.progress_detail_) != detail_before_tick,
                       "the synchronization status updates elapsed time without another worker event");
        workflow_check(ui.progress_timer_ == heartbeat,
                       "a heartbeat update reuses the same source rather than creating duplicate timers");

        ui.apply_progress({"Verifying", bytes / 2, bytes, 0, ""});
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) >= written_fraction &&
                       gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "readback verification advances overall progress while keeping completion pending");
        ui.apply_progress({"Verifying", bytes, bytes, 0, ""});
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "a full verification callback alone cannot claim final operation success");
        ui.apply_progress({"Complete", bytes, bytes, 0, ""});
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1 &&
                       text(ui.progress_title_).find("please wait") != std::string::npos,
                       "the writer's last callback still waits for final release before claiming success");
        ui.cancel_->store(true);
        ui.apply_progress({"Synchronizing", bytes, bytes, 0, ""});
        workflow_check(text(ui.progress_title_).find("Stopping safely") != std::string::npos &&
                       text(ui.progress_detail_).find("Cancellation") != std::string::npos,
                       "cancelling while saving explains that the drive must first stop safely");
        workflow_check(ui.progress_timer_ == heartbeat && gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)),
                       "a cancellation request keeps activity visible until the blocked save finishes");
        ui.busy_ = false;
        ui.finish_progress(false);
        workflow_check(ui.progress_timer_ == 0 && !gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)),
                       "a failed or cancelled operation releases its heartbeat and stops the spinner");
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "a failed or cancelled operation never leaves a completed progress bar");

        ui.busy_ = true;
        ui.cancel_->store(false);
        ui.progress_usb_creation_ = false;
        ui.progress_started_ = std::chrono::steady_clock::now();
        ui.apply_progress({"Synchronizing", bytes, bytes, 0, ""});
        workflow_check(text(ui.progress_detail_).find("source and destination") != std::string::npos &&
                       text(ui.progress_detail_).find("Verification comes next") == std::string::npos,
                       "backup synchronization describes its source and destination without promising USB creation steps");
        ui.apply_progress({"Creating partition table", 0, 2, 0, ""});
        workflow_check(text(ui.progress_detail_).find("B /") == std::string::npos &&
                       gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)),
                       "formatting's numbered steps stay animated without displaying step counts as byte sizes");
        ui.apply_progress({"Synchronizing", bytes, bytes, 0, ""}, true);
        workflow_check(gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)) && ui.progress_timer_ != 0,
                       "a later safe test restarts activity after a previous operation has stopped");
        workflow_check(text(ui.progress_detail_).find("No USB") != std::string::npos,
                       "the safe test explicitly identifies temporary files instead of a physical USB");
        ui.busy_ = false;
        ui.finish_progress(true);
        workflow_check(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) == 1,
                       "only explicit successful completion fills the complete progress bar");
        workflow_check(ui.progress_timer_ == 0 && !gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)),
                       "successful completion releases animation just as failure does");
    }

    static void run(AdwApplication* app) {
        auto desktop = std::make_shared<Desktop>(app);
        struct WindowCleanup {
            Desktop* desktop;
            ~WindowCleanup() { gtk_window_destroy(GTK_WINDOW(desktop->window_)); }
        } cleanup{desktop.get()};
        auto& ui = *desktop;
        workflow_check(!gtk_widget_get_visible(ui.window_), "test window remains hidden throughout the workflow");
        workflow_check(ui.dropdowns_.size() == 4, "create, format, backup and scan expose a shared drive selector");
        workflow_check(!ui.selected_device(), "startup has no physical drive target");
        workflow_check(!enabled(ui.create_) && !enabled(ui.format_) && !enabled(ui.backup_) && !enabled(ui.scan_),
                       "all drive operations start disabled without a drive");
        workflow_check(!enabled(ui.check_), "checksum calculation requires a source file");
        workflow_check(!enabled(ui.copy_hash_), "copying a checksum is disabled before a checksum exists");

        Device drive;
        drive.path = "/dev/test-kavrune-not-a-device";
        drive.model = "Synthetic USB";
        drive.size = 1024ULL * 1024 * 1024;
        drive.serial = "TEST-ONLY";
        drive.connection = "usb";
        drive.removable = true;
        ui.devices_ = {drive};
        ui.syncing_selection_ = true;
        kavrune::ui::replace_device_choices(ui.device_model_, "Choose a USB drive…", {"Synthetic USB · 1 GiB"});
        ui.syncing_selection_ = false;
        ui.sync_selection(); ui.update_controls();
        for (auto* widget : ui.dropdowns_)
            workflow_check(gtk_drop_down_get_selected(GTK_DROP_DOWN(widget)) == 0,
                           "discovering a single USB displays a placeholder in every module");
        workflow_check(!ui.selected_device() && !enabled(ui.format_) && !enabled(ui.backup_) && !enabled(ui.scan_),
                       "discovering one USB does not silently select it");

        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.dropdowns_[0]), 1);
        workflow_check(ui.selected_device() && ui.selected_device()->model == drive.model,
                       "choosing the only USB records a real selected device");
        for (auto* widget : ui.dropdowns_)
            workflow_check(gtk_drop_down_get_selected(GTK_DROP_DOWN(widget)) == 1,
                           "choosing a USB synchronizes every module's visible selection");
        for (auto* widget : ui.device_details_)
            workflow_check(text(widget).find(drive.model) != std::string::npos,
                           "every module names the selected drive in its explanation");
        workflow_check(enabled(ui.format_) && enabled(ui.backup_) && enabled(ui.scan_),
                       "an explicitly selected eligible USB enables format, backup and read scan");
        workflow_check(!enabled(ui.create_), "create remains unavailable until an image is selected");

        Image image;
        image.path = "/test-only/synthetic.img";
        image.name = "synthetic.img";
        image.kind = "Raw disk image";
        image.size = 8 * 1024 * 1024;
        image.raw_writable = true; image.bootable = true;
        ui.image_ = image; ui.update_controls();
        workflow_check(enabled(ui.create_) && enabled(ui.check_), "a fitting USB-ready source enables creation and checksum calculation");
        ui.digest_ = std::string(64, 'a'); ui.update_controls();
        workflow_check(enabled(ui.copy_hash_), "a calculated checksum makes the copy action available");
        workflow_check(text(ui.review_text_).find(image.name) != std::string::npos &&
                       text(ui.review_text_).find(drive.model) != std::string::npos,
                       "the review names both the exact source and selected target");
        workflow_check(text(ui.create_hint_).find("select your USB") == std::string::npos,
                       "a selected USB is never followed by a contradictory choose-USB prompt");

        ui.checksum_failed_ = true; ui.update_controls();
        workflow_check(!enabled(ui.create_) && enabled(ui.format_) && enabled(ui.backup_) && enabled(ui.scan_),
                       "a publisher checksum mismatch blocks creation without disabling unrelated USB tasks");
        ui.checksum_failed_ = false; ui.image_.size = drive.size + 1; ui.update_controls();
        workflow_check(!enabled(ui.create_) && enabled(ui.check_), "an oversized image blocks creation but can still be checked");
        ui.image_ = image; ui.image_.raw_writable = false; ui.update_controls();
        workflow_check(!enabled(ui.create_) && enabled(ui.check_), "an unsupported boot layout remains available for checksum verification only");
        ui.image_ = image; ui.busy_ = true; ui.can_cancel_ = true; ui.update_controls();
        workflow_check(!enabled(ui.create_) && !enabled(ui.format_) && !enabled(ui.backup_) && !enabled(ui.scan_) && !enabled(ui.check_),
                       "every operation is disabled while another operation is running");
        workflow_check(text(ui.review_text_).find(image.name) != std::string::npos &&
                       text(ui.review_text_).find(drive.model) != std::string::npos,
                       "the exact image and USB remain visible while the operation is running");
        for (auto* widget : ui.dropdowns_)
            workflow_check(!enabled(widget), "target controls cannot change during an operation");
        workflow_check(enabled(ui.cancel_button_), "a cancellable operation offers cancellation");
        ui.can_cancel_ = false; ui.update_controls();
        workflow_check(!enabled(ui.cancel_button_), "a noncancellable operation cannot offer a false cancel action");
        ui.busy_ = false; ui.update_controls();

        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "BAD/LABEL");
        workflow_check(!enabled(ui.format_), "an invalid volume label is blocked before any erase confirmation");
        workflow_check(!text(ui.format_hint_).empty(), "invalid format settings explain what needs correcting");
        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "12345678901");
        workflow_check(enabled(ui.format_), "an eleven-character FAT32 name is accepted immediately");
        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "123456789012");
        workflow_check(!enabled(ui.format_), "a twelve-character FAT32 name is rejected immediately");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.filesystem_), 1);
        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "123456789012345");
        workflow_check(enabled(ui.format_), "a fifteen-character exFAT name is accepted immediately");
        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "1234567890123456");
        workflow_check(!enabled(ui.format_), "a sixteen-character exFAT name is rejected immediately");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.filesystem_), 2);
        workflow_check(enabled(ui.format_), "switching to ext4 immediately accepts a valid sixteen-character name");
        gtk_editable_set_text(GTK_EDITABLE(ui.volume_label_), "KAVRUNE");
        workflow_check(enabled(ui.format_), "correcting a volume label restores the formatting action");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.filesystem_), 0);

        // Returning to the placeholder on another module must clear the actual
        // destination everywhere, including the review on the creation page.
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.dropdowns_[2]), 0);
        workflow_check(!ui.selected_device(), "clearing selection from another module clears the common target");
        workflow_check(!enabled(ui.create_) && !enabled(ui.format_) && !enabled(ui.backup_) && !enabled(ui.scan_),
                       "clearing the USB disables all four device operations");
        for (auto* widget : ui.dropdowns_)
            workflow_check(gtk_drop_down_get_selected(GTK_DROP_DOWN(widget)) == 0,
                           "cleared selection is shown consistently in every task");
        workflow_check(enabled(ui.check_), "download checking does not need a selected USB");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(ui.dropdowns_[3]), 1);
        workflow_check(enabled(ui.create_) && enabled(ui.format_) && enabled(ui.backup_) && enabled(ui.scan_),
                       "selecting the USB on Check USB restores every valid operation");
        ui.reset_source("Choose another image"); ui.update_controls();
        workflow_check(!enabled(ui.create_) && !enabled(ui.check_) && enabled(ui.format_) && enabled(ui.backup_) && enabled(ui.scan_),
                       "resetting a source does not discard the selected physical USB");
        workflow_check(!enabled(ui.copy_hash_), "resetting a source removes the previous checksum copy action");
        progress_regressions(ui);
        workflow_check(!gtk_widget_get_visible(ui.window_), "functional regression never opens a visible app window");
    }
};
} // namespace

int main() {
    if (!gtk_init_check()) {
        std::cout << "GTK display unavailable; run under xvfb-run to execute workflow regressions.\n";
        return 77;
    }
    adw_init();
    GError* error = nullptr;
    char* state = g_dir_make_tmp("kavrune-ui-workflow-XXXXXX", &error);
    if (!state) { std::cerr << error->message << '\n'; g_clear_error(&error); return 1; }
    g_setenv("XDG_STATE_HOME", state, TRUE);
    auto* app = adw_application_new("org.kavrune.WorkflowTests", G_APPLICATION_NON_UNIQUE);
    int result = 0;
    try {
        if (!g_application_register(G_APPLICATION(app), nullptr, &error)) {
            std::string detail = error->message; g_clear_error(&error); throw std::runtime_error(detail);
        }
        DesktopUiTest::run(app);
        std::cout << workflow_checks << " actual desktop workflow checks passed; no drive was opened.\n";
    } catch (const std::exception& failure) {
        std::cerr << "Desktop workflow regression: " << failure.what() << '\n'; result = 1;
    }
    g_object_unref(app);
    std::error_code cleanup_error;
    std::filesystem::remove_all(state, cleanup_error);
    g_free(state);
    return result;
}
