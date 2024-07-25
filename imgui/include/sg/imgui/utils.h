#pragma once

#include "DialogResult.h"

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>

namespace sg::imgui {

/* Create a version of ImGui::InputText for std::string */
bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags = 0);

/* Create a numeric input for UInt32 integers */
bool InputUInt32(const char *label, uint32_t &v, ImGuiInputTextFlags flags = 0);

/* Disables the ImGUI items defined in func if visibile is false */
void disable_item(bool visible, std::function<void(void)> func);

/* Sets the ImGUI font to Karla */
void set_karla_font();

/* Horizontally centres the next ImGui item(s) on the window
 *
 * If more than one control is present, they must be placed on teh
 * same lien using `ImGui::SameLine()`.
 *
 * @param vec           total size of the controls to be displated
 * @param no_of_items   number of controls to be cenrelised
 */
void centre_next_control_hor(const ImVec2 &vec, const int no_of_items = 1);

/* Create a modal popup.
 *
 * Note that you must have set the popup to open first by calling
 * `ImGui::OpenPopup(imgui_id)` first.
 *
 * @param imgui_id      ImGui ID of the popup
 * @param message       message to shown
 * @param flags         window flags
 */
DialogResult create_modal_popup(const char *imgui_id,
                                const std::string &message,
                                const int flags = ImGuiWindowFlags_AlwaysAutoResize |
                                                  ImGuiWindowFlags_NoSavedSettings |
                                                  ImGuiWindowFlags_NoMove);

/* Centre the new window both orizontally and vertically */
void centre_next_window(ImGuiCond cond = ImGuiCond_Always);

/* Calcultes the size of a button with the given text */
ImVec2 dimensions_of_button(const char *msg);

/** Draws a line, given starting point, length and angle.
 *
 * @param drawlist  ImGui drawlist, get this from ImGui::GetWindowDrawList();
 * @param color     Color, use IM_COL32(..). For example, IM_COL32(255,0,0,255) for red
 * @return          the position of the end of the line
 */
ImVec2 draw_line_angle(ImDrawList* const drawlist, const ImVec2& from, float length, float angle, ImU32 color, float thickness=1.0);

/** Draws an arrow, given starting point, length and angle
 *
 * @param drawlist  ImGui drawlist, get this from ImGui::GetWindowDrawList();
 * @param color     Color, use IM_COL32(..). For example, IM_COL32(255,0,0,255) for red
 * @return          The position of teh arrow tip
 */
ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, float length, float deg_angle, ImU32 color, float thickness=1);

ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness=1);

/** Draws line with with an arrow at the midpoint
 *
 * @param drawlist  ImGui drawlist, get this from ImGui::GetWindowDrawList();
 * @param color     Color, use IM_COL32(..). For example, IM_COL32(255,0,0,255) for red
 * @return          The position of teh arrow tip
 */
ImVec2 draw_arrow_middle(ImDrawList * const drawlist, const ImVec2 &from, float length, float deg, ImU32 color, float thickness=1);

} // namespace sg::imgui
