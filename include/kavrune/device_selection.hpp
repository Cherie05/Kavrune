// SPDX-License-Identifier: MIT
#pragma once
#include <gtk/gtk.h>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace kavrune::ui {
// GtkDropDown may automatically display its first item even when an application
// tries to clear selection. A real placeholder at row zero keeps that displayed
// choice separate from every physical device, including the first/only device.
inline std::optional<std::size_t> selection_index(guint row, std::size_t device_count) {
    if (row == 0 || row == GTK_INVALID_LIST_POSITION) return std::nullopt;
    const auto index = static_cast<std::size_t>(row - 1);
    return index < device_count ? std::optional<std::size_t>(index) : std::nullopt;
}

inline guint selection_row(std::optional<std::size_t> index, std::size_t device_count) {
    if (!index || *index >= device_count || *index >= std::numeric_limits<guint>::max() - 1U) return 0;
    return static_cast<guint>(*index + 1);
}

inline void replace_device_choices(GtkStringList* model, const char* placeholder,
                                   const std::vector<std::string>& labels) {
    if (labels.size() >= std::numeric_limits<guint>::max() - 1U)
        throw std::length_error("Too many drive choices");
    std::vector<const char*> rows;
    rows.reserve(labels.size() + 2);
    rows.push_back(placeholder);
    for (const auto& text : labels) rows.push_back(text.c_str());
    rows.push_back(nullptr);
    // Never expose an empty intermediate model to any of the shared selectors.
    gtk_string_list_splice(model, 0, g_list_model_get_n_items(G_LIST_MODEL(model)), rows.data());
}
} // namespace kavrune::ui
