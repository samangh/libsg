#include "sg/imgui/utils.h"

#include <imgui_internal.h>

#include <numbers>

#include "karla-font.h"

namespace sg::imgui {

int InputTextCallback(ImGuiInputTextCallbackData *data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        // Resize string callback
        std::string *str = (std::string *)data->UserData;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char *)str->c_str();
    }

    return 0;
}

bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(
        label, (char *)str.c_str(), str.capacity() + 1, flags, InputTextCallback, (void *)&str);
}

bool InputUInt32(const char *label, uint32_t &v, ImGuiInputTextFlags flags) {
    return ImGui::InputScalar(
        label, ImGuiDataType_U32, static_cast<void *>(&v), NULL, NULL, "%u", flags);
}

void disable_item(bool visible, std::function<void()> func) {
    if (visible)
        func();
    else {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        func();
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    };
}

void set_karla_font() {
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
        Karla_compressed_data, karla_compressed_size, 14);
}

void centre_next_control_hor(const ImVec2 &vec, const int no_of_items) {
    // obtain width of window
    float width = ImGui::GetWindowSize().x;

    // figure out where we need to move the controls to
    float centre_position_for_button =
        (width - vec.x - ImGui::GetStyle().ItemSpacing.x * (no_of_items - 1)) / 2;

    // tell Dear ImGui to render the controls at the current y pos, but with the new x pos
    ImGui::SetCursorPosX(centre_position_for_button);
}

void centre_next_window(ImGuiCond cond) {
    auto pos = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(pos, cond, ImVec2(0.5f, 0.5f));
}

ImVec2 dimensions_of_button(const char *msg) {
    // See: https://github.com/ocornut/imgui/issues/3714#issuecomment-759319268
    auto style = ImGui::GetStyle();
    auto textSize = ImGui::CalcTextSize(msg);

    auto x = textSize.x + style.FramePadding.x * 2.0f;
    auto y = textSize.y + style.FramePadding.y * 2.0f;
    return ImVec2(x, y);
}

ImVec2 draw_line_angle(ImDrawList *const drawlist,
                       const ImVec2 &from,
                       float length,
                       float angle,
                       ImU32 color,
                       float thickness) {
    drawlist->PathLineTo(from);
    drawlist->PathLineTo(from + ImVec2(length, 0));

    sg::imgui::rotate_path(drawlist->_Path, from, angle);
    auto res = drawlist->_Path.back();

    drawlist->PathStroke(color, ImDrawFlags_None, thickness);

    return res;
}

ImVec2 draw_arrow(ImDrawList *const drawlist,
                  const ImVec2 &from,
                  float length,
                  float deg,
                  ImU32 color,
                  float thickness,
                  float arrow_triangle_width) {
    auto arrow_line_width = length - arrow_triangle_width;

    auto arrow_triangle_bottom = from + ImVec2(arrow_line_width, 0.5f * arrow_triangle_width);
    auto arrow_bottom_line_end = from + ImVec2(arrow_line_width, 0.5f * thickness);
    auto arrow_bottom_line_start = from + ImVec2(0, 0.5f * thickness);
    auto arrow_top_line_start = from + ImVec2(0, -0.5f * thickness);
    auto arrow_top_line_end = from + ImVec2(arrow_line_width, -0.5f * thickness);
    auto arrow_triangle_top = from + ImVec2(arrow_line_width, -0.5f * arrow_triangle_width);
    auto arrow_triangle_endpoint = from + ImVec2(arrow_line_width + arrow_triangle_width, 0);

    drawlist->PathLineTo(arrow_triangle_endpoint);
    drawlist->PathLineTo(arrow_triangle_bottom);
    drawlist->PathLineTo(arrow_bottom_line_end);
    drawlist->PathLineTo(arrow_bottom_line_start);
    drawlist->PathLineTo(arrow_top_line_start);
    drawlist->PathLineTo(arrow_top_line_end);
    drawlist->PathLineTo(arrow_triangle_top);
    drawlist->PathLineTo(arrow_triangle_endpoint);

    sg::imgui::rotate_path(drawlist->_Path, from, deg);
    auto res = drawlist->_Path.back();

    drawlist->PathFillConvex(color);
    // drawlist->PathStroke(color, ImDrawFlags_None, thickness);

    return res;
}

ImVec2 draw_arrow(ImDrawList *const drawlist,
                  const ImVec2 &from,
                  float length,
                  float deg,
                  ImU32 color,
                  float thickness) {
    return draw_arrow(drawlist, from, length, deg, color, thickness, 4 * thickness);
}

ImVec2 draw_arrow_middle(ImDrawList *const drawlist,
                         const ImVec2 &from,
                         float length,
                         float deg,
                         ImU32 color,
                         float thickness,
                         float arrow_triangle_width) {
    draw_arrow(drawlist,
               from,
               0.5f * (length + arrow_triangle_width),
               deg,
               color,
               thickness,
               arrow_triangle_width);
    return draw_line_angle(drawlist, from, length, deg, color, thickness);
}

ImVec2 draw_arrow_middle(ImDrawList *const drawlist,
                         const ImVec2 &from,
                         float length,
                         float deg,
                         ImU32 color,
                         float thickness) {
    return draw_arrow_middle(drawlist, from, length, deg, color, thickness, 4 * thickness);
}

ImVec2 draw_arrow(ImDrawList *const drawlist,
                  const ImVec2 &from,
                  const ImVec2 &to,
                  ImU32 color,
                  float thickness,
                  float arrow_triangle_width) {
    auto vector = to - from;
    float total_length = ImSqrt(vector.x * vector.x + vector.y * vector.y);
    float angle = std::asin(vector.y / total_length) * 180.0f / static_cast<float>(std::numbers::pi);

    if (angle >= 0 && vector.x < 0)
        angle = 180.0f - angle;
    else if (angle < 0 && vector.x < 0)
        angle = -180.0f - angle;

    return draw_arrow(drawlist, from, total_length, angle, color, thickness, arrow_triangle_width);
}

ImVec2 draw_arrow(ImDrawList *const drawlist,
                  const ImVec2 &from,
                  const ImVec2 &to,
                  ImU32 color,
                  float thickness) {
    return draw_arrow(drawlist, from, to, color, thickness, 4 * thickness);
}

void rotate_path(ImVector<ImVec2> &vec, ImVec2 reference, float angle_deg) {
    float sin = static_cast<float>(std::sin(std::numbers::pi / 180.0f * angle_deg));
    float cos =  static_cast<float>(std::cos(std::numbers::pi / 180.0f * angle_deg));

    for (auto &p : vec) {
        auto x = p.x - reference.x;
        auto y = p.y - reference.y;
        p = ImVec2(reference.x + x * cos - y * sin, reference.y + x * sin + y * cos);
    }
}
}  // namespace sg::imgui
