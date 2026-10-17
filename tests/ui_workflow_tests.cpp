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
