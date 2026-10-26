// SPDX-License-Identifier: MIT
// Exercise the actual desktop control logic with synthetic data. The window is
// constructed but never presented; discovery is synthetic and no disk is opened.
#define KAVRUNE_DESKTOP_TEST
#include "../src/desktop.cpp"
#include <fstream>
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
    static void discovery_shutdown_regressions() {
        for (const bool fail_query : {false, true}) {
            struct Scenario {
                std::shared_ptr<Desktop> desktop;
                std::atomic_bool started{false}, release{false}, finished{false};
                bool closed = false, timed_out = false, fail_query = false;
                guint close_source = 0, release_source = 0, deadline_source = 0;
            } scenario;
            scenario.fail_query = fail_query;
            auto* application = adw_application_new("org.kavrune.DiscoveryShutdownTests", G_APPLICATION_NON_UNIQUE);
            g_signal_connect(application, "activate", G_CALLBACK(+[](GApplication* app, gpointer data) {
                auto* state = static_cast<Scenario*>(data);
                state->desktop = std::make_shared<Desktop>(ADW_APPLICATION(app));
                state->desktop->refresh_devices([state] {
                    state->started.store(true);
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                    while (!state->release.load() && std::chrono::steady_clock::now() < deadline) g_usleep(1000);
                    state->finished.store(true);
                    if (state->fail_query) throw std::runtime_error("Synthetic discovery failure");
                    return std::vector<Device>{};
                });
                state->close_source = g_timeout_add(1, +[](gpointer value) -> gboolean {
                    auto* pending = static_cast<Scenario*>(value);
                    if (!pending->started.load()) return G_SOURCE_CONTINUE;
                    pending->desktop->alive_ = false;
                    gtk_window_destroy(GTK_WINDOW(pending->desktop->window_));
                    pending->desktop->window_ = nullptr;
                    pending->closed = true; pending->close_source = 0;
                    // Only the discovery hold can keep the application loop
                    // alive until this timer allows the worker to finish.
                    pending->release_source = g_timeout_add(20, +[](gpointer item) -> gboolean {
                        auto* waiting = static_cast<Scenario*>(item);
                        waiting->release.store(true); waiting->release_source = 0;
                        return G_SOURCE_REMOVE;
                    }, pending);
                    return G_SOURCE_REMOVE;
                }, state);
                state->deadline_source = g_timeout_add(2500, +[](gpointer value) -> gboolean {
                    auto* pending = static_cast<Scenario*>(value);
                    pending->timed_out = true; pending->release.store(true); pending->deadline_source = 0;
                    g_application_quit(G_APPLICATION(pending->desktop->app_));
                    return G_SOURCE_REMOVE;
                }, state);
            }), &scenario);
            const int status = g_application_run(G_APPLICATION(application), 0, nullptr);
            const bool waited = scenario.closed && scenario.finished.load() && scenario.desktop && !scenario.desktop->refreshing_;
            // Keep failure cleanup safe too, so a missing hold fails an
            // assertion rather than leaving the synthetic worker behind.
            scenario.release.store(true);
            const auto cleanup_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (scenario.desktop && scenario.desktop->refreshing_ && std::chrono::steady_clock::now() < cleanup_deadline) {
                while (g_main_context_pending(nullptr)) g_main_context_iteration(nullptr, FALSE);
                g_usleep(1000);
            }
            for (const auto source : {scenario.close_source, scenario.release_source, scenario.deadline_source})
                if (source && g_main_context_find_source_by_id(nullptr, source)) g_source_remove(source);
            if (scenario.desktop && scenario.desktop->window_) {
                scenario.desktop->alive_ = false;
                gtk_window_destroy(GTK_WINDOW(scenario.desktop->window_)); scenario.desktop->window_ = nullptr;
            }
            scenario.desktop.reset(); g_object_unref(application);
            workflow_check(waited, "closing the final window waits for the synthetic discovery result, including failure");
            workflow_check(!scenario.timed_out, "discovery releases the application hold without a forced quit");
            workflow_check(status == 0, "the discovery shutdown path exits the actual application loop successfully");
        }
    }
    static std::string activity(Desktop& ui) {
        auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.activity_view_)); GtkTextIter first, last;
        gtk_text_buffer_get_bounds(buffer, &first, &last);
        gchar* shown = gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
        std::string result(shown); g_free(shown); return result;
    }
    static void standards_regressions(Desktop& ui) {
        workflow_check(ui.persistent_labels_.size() == 9,
                       "USB selectors, format settings and checksum fields have persistent labels");
        for (auto* title : ui.persistent_labels_) {
            auto* widget = gtk_label_get_mnemonic_widget(GTK_LABEL(title));
            workflow_check(widget && gtk_test_accessible_has_property(GTK_ACCESSIBLE(widget), GTK_ACCESSIBLE_PROPERTY_LABEL) &&
                           gtk_test_accessible_has_relation(GTK_ACCESSIBLE(widget), GTK_ACCESSIBLE_RELATION_LABELLED_BY),
                           "each persistent field label is programmatically associated with its native control");
        }
        workflow_check(gtk_test_accessible_has_relation(GTK_ACCESSIBLE(ui.progress_bar_), GTK_ACCESSIBLE_RELATION_DESCRIBED_BY),
                       "the native progress bar is associated with its detailed status");
        auto** shortcuts = gtk_application_get_accels_for_action(GTK_APPLICATION(ui.app_), "app.close");
        workflow_check(shortcuts && shortcuts[0] && shortcuts[1], "standard close shortcuts use the window's existing safe close path"); g_strfreev(shortcuts);

        gboolean previous_motion = TRUE;
        g_object_get(ui.settings_, "gtk-enable-animations", &previous_motion, nullptr);
        ui.busy_ = true; ui.cancel_->store(false); ui.progress_usb_creation_ = true;
        ui.progress_started_ = std::chrono::steady_clock::now();
        g_object_set(ui.settings_, "gtk-enable-animations", FALSE, nullptr);
        ui.apply_progress({"Synchronizing", 1024, 1024, 0, ""});
        workflow_check(!ui.animations_enabled_ && !gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)) &&
                       !gtk_widget_get_visible(ui.progress_spinner_), "reduced motion disables the custom progress spinner");
        workflow_check(gtk_stack_get_transition_type(GTK_STACK(ui.stack_)) == GTK_STACK_TRANSITION_TYPE_NONE,
                       "reduced motion disables page transition animation");
        workflow_check(ui.progress_timer_ && text(ui.progress_detail_).find("Still working") != std::string::npos &&
                       gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ui.progress_bar_)) < 1,
                       "reduced motion retains elapsed activity and truthful incomplete saving progress");
        g_object_set(ui.settings_, "gtk-enable-animations", TRUE, nullptr);
        workflow_check(gtk_spinner_get_spinning(GTK_SPINNER(ui.progress_spinner_)) &&
                       gtk_stack_get_transition_type(GTK_STACK(ui.stack_)) == GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                       "changing the system motion preference during an operation takes effect immediately");
        ui.finish_progress(false); ui.busy_ = false;
        workflow_check(!ui.inhibit_cookie_, "the terminal progress path releases any session inhibitor");
        g_object_set(ui.settings_, "gtk-enable-animations", previous_motion, nullptr);

        log_event("saved-diagnostic-regression-marker"); ui.load_activity();
        workflow_check(activity(ui).find("saved-diagnostic-regression-marker") != std::string::npos &&
                       activity(ui).find(KAVRUNE_VERSION) != std::string::npos,
                       "Activity reads persistent local diagnostics with the runtime application version");
        ui.busy_ = true; ui.update_controls(); const auto before = activity(ui);
        workflow_check(!enabled(ui.activity_reload_) && !enabled(ui.activity_export_) && !enabled(ui.activity_clear_),
                       "reload, export and clear are unavailable during active media operations");
        ui.clear_activity(); ui.load_activity();
        workflow_check(activity(ui) == before && ui.clear_activity_dialog() == nullptr,
                       "programmatic clear and reload cannot replace diagnostics while an operation is running");
        ui.busy_ = false; ui.update_controls();
        workflow_check(enabled(ui.activity_reload_) && enabled(ui.activity_export_) && enabled(ui.activity_clear_),
                       "saved diagnostics controls become available when an operation ends");
        auto* dialog = ui.clear_activity_dialog(); g_object_ref_sink(dialog);
        workflow_check(!gtk_widget_get_visible(dialog) &&
                       std::string(adw_message_dialog_get_default_response(ADW_MESSAGE_DIALOG(dialog))) == "cancel" &&
                       std::string(adw_message_dialog_get_close_response(ADW_MESSAGE_DIALOG(dialog))) == "cancel",
                       "clearing diagnostics uses a hidden-testable confirmation that defaults to keeping data");
        adw_message_dialog_response(ADW_MESSAGE_DIALOG(dialog), "cancel");
        workflow_check(read_diagnostics().find("saved-diagnostic-regression-marker") != std::string::npos,
                       "cancelling the clear confirmation preserves saved diagnostics"); g_object_unref(dialog);
        dialog = ui.clear_activity_dialog(); g_object_ref_sink(dialog);
        adw_message_dialog_response(ADW_MESSAGE_DIALOG(dialog), "clear"); g_object_unref(dialog);
        workflow_check(activity(ui).empty() && read_diagnostics().find("saved-diagnostic-regression-marker") == std::string::npos,
                       "explicit confirmation clears saved and displayed activity without recreating a log event");
        workflow_check(text(ui.activity_status_).find("Exported copies") != std::string::npos,
                       "clear explains that previously exported diagnostic copies are unaffected");
        const auto export_path = std::string(g_getenv("XDG_STATE_HOME")) + "/ui-diagnostics.txt";
        const auto wait_for_export = [&ui] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (ui.activity_saving_ && std::chrono::steady_clock::now() < deadline) {
                while (g_main_context_pending(nullptr)) g_main_context_iteration(nullptr, FALSE);
                g_usleep(1000);
            }
            workflow_check(!ui.activity_saving_, "the background diagnostics export returns its final state to GTK");
        };
        ui.export_activity(export_path);
        workflow_check(ui.activity_saving_ && !enabled(ui.activity_clear_) && !enabled(ui.activity_export_),
                       "an asynchronous diagnostics export cannot be duplicated or cleared while saving");
        workflow_check(!enabled(ui.create_) && !enabled(ui.format_) && !enabled(ui.backup_) && !enabled(ui.scan_) && !enabled(ui.check_),
                       "image and USB operations cannot start while diagnostics may be writing to their filesystem");
        const auto operation_before_export = ui.operation_id_;
        ui.work("Do not run during export", [](const ProgressFn&, const std::atomic_bool&) { return std::string("Unexpected concurrent job"); });
        workflow_check(ui.operation_id_ == operation_before_export && !ui.busy_,
                       "the worker entry point rejects new jobs until a diagnostics export completes");
        wait_for_export();
        std::ifstream exported(export_path); std::string exported_text((std::istreambuf_iterator<char>(exported)), std::istreambuf_iterator<char>());
        workflow_check(exported_text.find(KAVRUNE_VERSION) != std::string::npos &&
                       text(ui.activity_status_).find("review") != std::string::npos &&
                       text(ui.activity_status_).find("Nothing was uploaded") != std::string::npos,
                       "saved diagnostics include the real version and sharing feedback remains explicitly local");
        ui.export_activity(export_path); wait_for_export();
        workflow_check(text(ui.activity_status_).find("could not be saved") != std::string::npos && enabled(ui.activity_export_),
                       "an existing diagnostics filename produces recoverable feedback and restores the export action");
        auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.activity_view_));
        const std::string large(static_cast<size_t>(ui.activity_char_limit_) * 2, 'x');
        gtk_text_buffer_set_text(buffer, large.c_str(), -1); ui.trim_activity();
        workflow_check(gtk_text_buffer_get_char_count(buffer) <= ui.activity_char_limit_,
                       "large session activity is bounded instead of retaining an unlimited text history");
        ui.record(std::string("invalid-utf8-") + static_cast<char>(0xff));
        workflow_check(g_utf8_validate(activity(ui).c_str(), -1, nullptr),
                       "invalid bytes in a local error cannot corrupt GTK's UTF-8 activity buffer");
    }
    static void legal_regressions(Desktop& ui) {
        workflow_check(enabled(ui.legal_access_), "legal and privacy documents are available without starting an operation");
        const auto source = ui.image_.path;
        const auto selected = ui.selected_;
        const auto operation = ui.operation_id_;
        ui.busy_ = true; ui.update_controls();
        workflow_check(enabled(ui.legal_access_), "legal and privacy access remains available during another operation");
        auto* window = LegalViewer::create(GTK_WINDOW(ui.window_));
        struct LegalCleanup { GtkWidget* window; ~LegalCleanup() { if (window) gtk_window_destroy(GTK_WINDOW(window)); } } cleanup{window};
        auto* reader = static_cast<LegalViewer*>(g_object_get_data(G_OBJECT(window), "kavrune-legal-viewer"));
        workflow_check(reader && !gtk_widget_get_visible(window), "the real legal reader can be constructed without showing a window");
        workflow_check(!gtk_text_view_get_editable(GTK_TEXT_VIEW(reader->body)) &&
                       gtk_text_view_get_wrap_mode(GTK_TEXT_VIEW(reader->body)) == GTK_WRAP_WORD_CHAR,
                       "legal text is read-only, selectable, and wraps on a compact screen");
        auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(reader->body));
        for (unsigned i = 0; i < G_N_ELEMENTS(LegalViewer::filenames); ++i) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(reader->documents), i);
            const std::string path = std::string("/org/kavrune/Kavrune/legal/") + LegalViewer::filenames[i];
            GError* error = nullptr;
            GBytes* bytes = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
            workflow_check(bytes != nullptr && error == nullptr, "each complete legal document is embedded for offline access");
            gsize size = 0; const auto* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
            GtkTextIter first, last; gtk_text_buffer_get_bounds(buffer, &first, &last);
            gchar* shown = gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
            const bool same = std::string(shown) == std::string(data, size);
            g_free(shown); g_bytes_unref(bytes); g_clear_error(&error);
            workflow_check(same && size > 200, "switching documents displays the complete packaged source text");
        }
        gtk_drop_down_set_selected(GTK_DROP_DOWN(reader->documents), 3);
        gtk_editable_set_text(GTK_EDITABLE(reader->search), "sOfTwArE"); reader->find();
        GtkTextIter first_match, last_match;
        workflow_check(gtk_text_buffer_get_selection_bounds(buffer, &first_match, &last_match),
                       "document search selects a case-insensitive match in the MIT license");
        const int match_offset = gtk_text_iter_get_offset(&first_match);
        reader->find(false, true);
        workflow_check(gtk_text_buffer_get_selection_bounds(buffer, &first_match, &last_match) &&
                       gtk_text_iter_get_offset(&first_match) != match_offset,
                       "next-match search advances to another occurrence");
        reader->find(true, true);
        workflow_check(gtk_text_buffer_get_selection_bounds(buffer, &first_match, &last_match) &&
                       gtk_text_iter_get_offset(&first_match) == match_offset,
                       "previous-match search returns to the earlier occurrence");
        gtk_editable_set_text(GTK_EDITABLE(reader->search), "kavrune-query-that-is-not-in-any-document"); reader->find();
        workflow_check(!gtk_text_buffer_get_selection_bounds(buffer, &first_match, &last_match) &&
                       text(reader->status).find("No matching text") != std::string::npos,
                       "a missing search result has a clear visible explanation");
        gtk_editable_set_text(GTK_EDITABLE(reader->search), ""); reader->find();
        workflow_check(text(reader->status).find("offline") != std::string::npos,
                       "clearing search restores offline reading guidance");
        workflow_check(ui.busy_ && ui.image_.path == source && ui.selected_ == selected && ui.operation_id_ == operation,
                       "opening and reading legal documents never changes the active operation or selected media");
        gtk_editable_set_text(GTK_EDITABLE(reader->search), "software");
        g_signal_emit_by_name(reader->search, "stop-search");
        workflow_check(!*gtk_editable_get_text(GTK_EDITABLE(reader->search)), "Escape clears a document search before closing the reader");
        gtk_window_destroy(GTK_WINDOW(window)); cleanup.window = nullptr;
        workflow_check(ui.busy_ && ui.operation_id_ == operation && !ui.cancel_->load(),
                       "closing the legal reader leaves an active operation running");
        ui.busy_ = false; ui.update_controls();
    }
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
            ~WindowCleanup() { if (desktop->window_) gtk_window_destroy(GTK_WINDOW(desktop->window_)); }
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
        legal_regressions(ui);
        standards_regressions(ui);
        workflow_check(!gtk_widget_get_visible(ui.window_), "functional regression never opens a visible app window");
        const bool motion_before_close = ui.animations_enabled_;
        ui.alive_ = false; gtk_window_destroy(GTK_WINDOW(ui.window_)); ui.window_ = nullptr;
        g_object_set(ui.settings_, "gtk-enable-animations", !motion_before_close, nullptr);
        workflow_check(ui.animations_enabled_ == motion_before_close,
                       "a system settings change after window destruction cannot access released GTK widgets");
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
        DesktopUiTest::discovery_shutdown_regressions();
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
