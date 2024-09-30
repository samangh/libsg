#pragma once

#include "utils.h"

#include <imgui.h>

namespace sg::imgui {

/* results of a dialog box */
enum class dialog_box_result { NoResult, OK, Cancel, Yes, No, Abort };

/* buttons for a dialog box */
enum class dialog_box_buttons { Ok, YesNo, YesNoCancel, OkAbort };

void show_button(dialog_box_result& resultStore, dialog_box_result resultIfClicked, const char* text) {
    auto size = sg::imgui::dimensions_of_button(text);
    if (ImGui::Button(text, size))
    {
        resultStore = resultIfClicked;
        ImGui::CloseCurrentPopup();
    }
}

/** Create a popup dialog
 *
 *  Note that you must have set the popup to open first by calling
 *  `ImGui::OpenPopup(imgui_id)` first
 *
 *  The popup title will be determined by the `imgui_id`.
 *
 * @param imgui_id
 * @param message   Message to show to the user
 * @param buttons   Buttons to show
 * @param flags     Addition ImGuiWindowFlags_* flags
 * @return result of the dialog. This will be `DialogResult::NoResult` if the user has not pressed a button.
 */
[[nodiscard]] dialog_box_result dialog_box(const char *imgui_id,
                        const std::string &message,
                             const dialog_box_buttons buttons = dialog_box_buttons::Ok,
                        const int flags = ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoSavedSettings) {
    dialog_box_result result = dialog_box_result::NoResult;

    if (ImGui::BeginPopupModal(imgui_id, NULL, flags))
    {
        ImGui::Spacing();
        ImGui::Text("%s", message.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto size = ImVec2(0, 0);

        switch (buttons) {
        case dialog_box_buttons::Ok:
            size+=sg::imgui::dimensions_of_button("OK");

            centre_next_control_hor(size);
            show_button(result, dialog_box_result::OK, "OK");
            break;
        case dialog_box_buttons::OkAbort:
            size+=sg::imgui::dimensions_of_button("OK");
            size+=sg::imgui::dimensions_of_button("Abort");

            centre_next_control_hor(size,2);
            show_button(result, dialog_box_result::OK, "OK");
            ImGui::SameLine();
            show_button(result, dialog_box_result::Abort, "Abort");
            break;
        case dialog_box_buttons::YesNo:
            size+=sg::imgui::dimensions_of_button("Yes");
            size+=sg::imgui::dimensions_of_button("No");

            centre_next_control_hor(size,2);
            show_button(result, dialog_box_result::Yes, "Yes");
            ImGui::SameLine();
            show_button(result, dialog_box_result::No, "No");
            break;
        case dialog_box_buttons::YesNoCancel:
            size+=sg::imgui::dimensions_of_button("Yes");
            size+=sg::imgui::dimensions_of_button("No");
            size+=sg::imgui::dimensions_of_button("Cancel");

            centre_next_control_hor(size,3);
            show_button(result, dialog_box_result::Yes, "Yes");
            ImGui::SameLine();
            show_button(result, dialog_box_result::No, "No");
            ImGui::SameLine();
            show_button(result, dialog_box_result::Cancel, "Cancel");
            break;
        default:
            break;
        }

        ImGui::EndPopup();
    }

    return result;
}

} // namespace sg::imgui
