// SPDX-License-Identifier: MIT
// Real GTK widgets, synthetic drive labels, no windows or storage access.
#include "kavrune/device_selection.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
constexpr const char* placeholder = "Choose a USB drive…";
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Selectors {
    GtkStringList* model = gtk_string_list_new(nullptr);
    std::array<GtkDropDown*, 4> widgets{};
    std::size_t count = 0;
    std::optional<std::size_t> selected;
    bool syncing = false;
    unsigned selection_events = 0, model_events = 0;
    bool model_was_never_empty = true;

    Selectors() {
        kavrune::ui::replace_device_choices(model, placeholder, {});
        g_signal_connect(model, "items-changed", G_CALLBACK(+[](GListModel* list, guint, guint, guint, gpointer data) {
            auto* self = static_cast<Selectors*>(data);
            ++self->model_events;
            self->model_was_never_empty = self->model_was_never_empty &&
                g_list_model_get_n_items(list) > 0 &&
                std::string(gtk_string_list_get_string(self->model, 0)) == placeholder;
        }), this);
        for (auto& widget : widgets) {
            widget = GTK_DROP_DOWN(g_object_ref_sink(gtk_drop_down_new(G_LIST_MODEL(g_object_ref(model)), nullptr)));
            g_signal_connect(widget, "notify::selected", G_CALLBACK(+[](GObject* object, GParamSpec*, gpointer data) {
                auto* self = static_cast<Selectors*>(data);
                if (self->syncing) return;
                ++self->selection_events;
                self->selected = kavrune::ui::selection_index(gtk_drop_down_get_selected(GTK_DROP_DOWN(object)), self->count);
                self->sync();
            }), this);
        }
        sync();
    }
    ~Selectors() {
        for (auto* widget : widgets) {
            g_signal_handlers_disconnect_by_data(widget, this);
            g_object_unref(widget);
        }
        g_object_unref(model);
    }
    void sync() {
        syncing = true;
        for (auto* widget : widgets)
            gtk_drop_down_set_selected(widget, kavrune::ui::selection_row(selected, count));
        syncing = false;
    }
    void rebuild(const std::vector<std::string>& labels, std::optional<std::size_t> preserved) {
        syncing = true;
        count = labels.size(); selected = preserved;
        kavrune::ui::replace_device_choices(model, placeholder, labels);
        syncing = false;
        sync();
    }
    void displays(guint row, const std::string& text) {
        for (auto* widget : widgets) {
            check(gtk_drop_down_get_selected(widget) == row, "all task selectors show the same selected row");
            auto* item = gtk_drop_down_get_selected_item(widget);
            check(item && text == gtk_string_object_get_string(GTK_STRING_OBJECT(item)), "displayed drive matches the selected row");
            check(kavrune::ui::selection_index(gtk_drop_down_get_selected(widget), count) == selected,
                  "visible choice and application device index agree");
        }
    }
};

void mapping_tests() {
    using namespace kavrune::ui;
    check(!selection_index(0, 1), "placeholder never names a physical USB");
    check(!selection_index(GTK_INVALID_LIST_POSITION, 1), "GTK invalid selection never names a USB");
    check(!selection_index(1, 0), "an empty device list cannot name a USB");
    check(!selection_index(2, 1), "a stale selection outside the new list cannot name a USB");
    check(selection_index(1, 1) == 0, "the first and only USB is selectable");
    check(selection_index(2, 2) == 1, "the second USB maps without an offset error");
    check(selection_row(std::nullopt, 1) == 0, "no physical selection displays the real placeholder");
    check(selection_row(0, 1) == 1, "the first device has a distinct selectable row");
    check(selection_row(1, 1) == 0, "an out-of-date index displays the placeholder");
    check(selection_row(std::numeric_limits<std::size_t>::max(), 1) == 0, "an overflowing index cannot wrap into a device row");
}

void widget_tests() {
    Selectors selectors;
    selectors.displays(0, placeholder);
    const std::string first = "First USB · 32 GiB · /dev/sdb";
    const std::string second = "Second USB · 16 GiB · /dev/sdc";
    selectors.rebuild({first}, std::nullopt);
    selectors.displays(0, placeholder);
    check(!selectors.selected, "discovering the only USB never selects it automatically");
    const auto before = selectors.selection_events;
    gtk_drop_down_set_selected(selectors.widgets[0], 1);
    check(selectors.selection_events == before + 1, "choosing the first USB emits a real change rather than remaining on an apparent default");
    check(selectors.selected == 0, "choosing the first USB records the actual target");
    selectors.displays(1, first);

    // Discovery is polled every four seconds. Unchanged refreshes must not turn
    // an explicit selection back into a prompt, or an unselected prompt into USB.
    for (unsigned refresh = 0; refresh < 12; ++refresh) {
        selectors.rebuild({first}, 0);
        selectors.displays(1, first);
    }
    selectors.rebuild({first, second}, 0);
    selectors.displays(1, first);
    // Storage identity matching remaps the selected physical drive after order
    // changes; the shared GTK mapping must keep that remapped drive visible.
    selectors.rebuild({second, first}, 1);
    selectors.displays(2, first);
    gtk_drop_down_set_selected(selectors.widgets[2], 1);
    check(selectors.selected == 0, "a choice on another task updates the common device selection");
    selectors.displays(1, second);
    gtk_drop_down_set_selected(selectors.widgets[1], 0);
    check(!selectors.selected, "choosing the placeholder clears every task's target");
    selectors.displays(0, placeholder);

    // A disconnected drive's replacement can have the same row or /dev path.
    // The caller invalidates its identity; GTK must display that invalidation.
    selectors.rebuild({first}, std::nullopt);
    selectors.displays(0, placeholder);
    gtk_drop_down_set_selected(selectors.widgets[3], 1);
    selectors.displays(1, first);
    selectors.rebuild({"Replacement USB · 32 GiB · /dev/sdb"}, std::nullopt);
    selectors.displays(0, placeholder);
    selectors.rebuild({}, std::nullopt);
    selectors.displays(0, placeholder);
    selectors.rebuild({second}, std::nullopt);
    selectors.displays(0, placeholder);
    gtk_drop_down_set_selected(selectors.widgets[0], 1);
    selectors.displays(1, second);
    const auto events = selectors.model_events;
    selectors.rebuild({second}, 0);
    check(selectors.model_events == events + 1, "refresh replaces the model in one atomic splice");
    check(selectors.model_was_never_empty, "model changes always contain the explicit placeholder");
    selectors.displays(1, second);
}
} // namespace

int main() {
    if (!gtk_init_check()) {
        std::cout << "GTK display unavailable; run under xvfb-run to execute selection regressions.\n";
        return 77;
    }
    try {
        mapping_tests(); widget_tests();
        std::cout << checks << " GTK selection checks passed; no window or drive was opened.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "GTK selection regression: " << error.what() << '\n';
        return 1;
    }
}
