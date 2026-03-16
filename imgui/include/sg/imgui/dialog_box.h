#pragma once

#include "utils.h"

#include <imgui.h>

namespace sg::imgui {

/* results of a dialog box */
enum class dialog_box_result { NoResult, OK, Cancel, Yes, No, Abort };

/* buttons for a dialog box */
enum class dialog_box_buttons { Ok, YesNo, YesNoCancel, OkAbort };

void show_button(dialog_box_result& resultStore, dialog_box_result resultIfClicked, const char* text);

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
 * @return result of the dialog. This will be `DialogResult::NoResult` if the user has not pressed a
 * button.
 */
[[nodiscard]] dialog_box_result dialog_box(const char *imgui_id,
                        const std::string &message,
                             const dialog_box_buttons buttons = dialog_box_buttons::Ok,
                        const int flags = ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoSavedSettings);

} // namespace sg::imgui
