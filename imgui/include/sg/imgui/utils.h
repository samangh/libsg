#pragma once

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>

namespace sg::imgui {

/* Create a version of ImGui::InputText for std::string */
bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags = 0);

template<typename T>
    requires(std::is_arithmetic_v<T>)
bool InputNumeric(const char* label, T& v, ImGuiInputTextFlags flags = ImGuiInputFlags_None) {
    ImGuiDataType_ imguiType = ImGuiDataType_S8;

    if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char>)
        imguiType = ImGuiDataType_S8;
    else if constexpr (std::is_same_v<T, unsigned char>)
        imguiType = ImGuiDataType_U8;
    else if constexpr (std::is_same_v<T, short>)
        imguiType = ImGuiDataType_S16;
    else if constexpr (std::is_same_v<T, unsigned short>)
        imguiType = ImGuiDataType_U16;
    else if constexpr (std::is_same_v<T, int>)
        imguiType = ImGuiDataType_S32;
    else if constexpr (std::is_same_v<T, unsigned int>)
        imguiType = ImGuiDataType_U32;
    else if constexpr (std::is_same_v<T, long>)
        imguiType = (sizeof(T) == 8) ? ImGuiDataType_S64 : ImGuiDataType_S32;
    else if constexpr (std::is_same_v<T, unsigned long>)
        imguiType = (sizeof(T) == 8) ? ImGuiDataType_U64 : ImGuiDataType_U32;
    else if constexpr (std::is_same_v<T, long long>)
        imguiType = ImGuiDataType_S64;
    else if constexpr (std::is_same_v<T, unsigned long long>)
        imguiType = ImGuiDataType_U64;
    else if constexpr (std::is_same_v<T, float>)
        imguiType = ImGuiDataType_Float;
    else if constexpr (std::is_same_v<T, double>)
        imguiType = ImGuiDataType_Double;
    else
        static_assert(false, "provided arithmetic type does not match to a ImGuiDataType_");

    return ImGui::InputScalar(label, imguiType, static_cast<void*>(&v), nullptr, nullptr, nullptr,
                              flags);
}

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

/* Centre the next window (over the whole screen) */
void centre_next_window(ImGuiCond cond = ImGuiCond_Appearing);

/* Centre the new window over the current window */
void centre_next_window_wrt_current_window(ImGuiCond cond = ImGuiCond_Appearing);

/** Maximises the next window
 *
 * You probably want to also pass the following flags to the window:
 *
 * @code ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
 *       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
 * @endcode
 *
 * @param useWorkArea if true, do not maximise over toolbar or menubar
 */
void maximise_next_window(bool useWorkArea);

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
 * @param arrow_triangle_width Width of the point of the triangle
 * @return          The position of teh arrow tip
 */
ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, float length, float deg_angle, ImU32 color, float thickness, float arrow_triangle_width);
ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, float length, float deg_angle, ImU32 color, float thickness = 1);

ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness, float arrow_triangle_width);
ImVec2 draw_arrow(ImDrawList* const drawlist, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness=1);

/** Draws line with with an arrow at the midpoint
 *
 * @param drawlist  ImGui drawlist, get this from ImGui::GetWindowDrawList();
 * @param color     Color, use IM_COL32(..). For example, IM_COL32(255,0,0,255) for red
 * @return          The position of teh arrow tip
 */
ImVec2 draw_arrow_middle(ImDrawList * const drawlist, const ImVec2 &from, float length, float deg, ImU32 color, float thickness, float arrow_triangle_width);
ImVec2 draw_arrow_middle(ImDrawList * const drawlist, const ImVec2 &from, float length, float deg, ImU32 color, float thickness=1);


/**
 * @brief Rotates the given set of co-oridates around the refeerence point
 * @details This is mostly used to rotate ImGui::GetWindowDrawList()->_Path
 * @param vec       List of co-ordiantes
 * @param reference Refernce point
 * @param angle     Angle in degrees
 */
void rotate_path(ImVector<ImVec2>& vec, ImVec2 reference, float angle_deg);

} // namespace sg::imgui
