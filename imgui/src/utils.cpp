#include "sg/imgui/utils.h"

#include "karla-font.h"
#include <imgui_internal.h>

#include <numbers>

namespace {

struct int_sincos
{
   int_sincos(float deg)
   {
       #ifdef _WIN32
       sin = std::sin(deg * std::numbers::pi/180.0f);
       cos = std::cos(deg* std::numbers::pi/180.0f);
       #else
       sincosf(deg * std::numbers::pi/180.0f, &sin, &cos);
       #endif
   }

   float sin;
   float cos;
};

/* rotates given point round origin */
inline ImVec2 rotate(const ImVec2& vec, int_sincos trig)
{
    auto x=vec.x;
    auto y=vec.y;
    return ImVec2(x*trig.cos -y*trig.sin,
                  x*trig.sin +y*trig.cos);
}

/* rotates given point round origin */
inline ImVec2 rotate(const ImVec2& vec, float deg)
{
    return rotate(vec, int_sincos(deg));
}

}

namespace  sg::imgui{

int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        std::string* str = (std::string*)data->UserData;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }

    return 0;
}

bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(label, (char*)str.c_str(), str.capacity() + 1, flags, InputTextCallback, (void*)&str);
}

bool InputUInt32(const char *label, uint32_t& v, ImGuiInputTextFlags flags)
{
    return ImGui::InputScalar(label, ImGuiDataType_U32, static_cast<void*>(&v),  NULL, NULL, "%u", flags);
}

void disable_item(bool visible, std::function<void ()> func)
{
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

void set_karla_font(){
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
                Karla_compressed_data, karla_compressed_size, 14);
}

void centre_next_control_hor(const ImVec2 &vec, const int no_of_items)
{
    // obtain width of window
    float width = ImGui::GetWindowSize().x;

     // figure out where we need to move the controls to
     float centre_position_for_button = (width - vec.x - ImGui::GetStyle().ItemSpacing.x * (no_of_items-1)) / 2;

     // tell Dear ImGui to render the controls at the current y pos, but with the new x pos
     ImGui::SetCursorPosX(centre_position_for_button);
}


void centre_next_window(ImGuiCond cond)
{
    auto pos = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(pos, cond, ImVec2(0.5f,0.5f));
}

ImVec2 dimensions_of_button(const char *msg)
{
    // See: https://github.com/ocornut/imgui/issues/3714#issuecomment-759319268
    auto style = ImGui::GetStyle();
    auto textSize= ImGui::CalcTextSize(msg);

    auto x= textSize.x + style.FramePadding.x * 2.0f;
    auto y= textSize.y + style.FramePadding.y * 2.0f;
    return ImVec2(x,y);
}

ImVec2 draw_line_angle(ImDrawList * const drawlist, const ImVec2 &from, float length, float angle, ImU32 color, float thickness)
{
    ImVec2 to = from + rotate(ImVec2(length, 0), angle);
    drawlist->AddLine(from, to, color, thickness);
    return to;
}

ImVec2 draw_arrow(ImDrawList * const drawlist, const ImVec2 &from, float length, float deg, ImU32 color, float thickness)
{
    auto width=3*thickness;

    auto trig = int_sincos(deg);
    ImVec2 line_end = from + rotate(ImVec2(length-width, 0), trig);

    auto arrow_end =line_end + rotate(ImVec2(width, 0), trig);
    auto arrow_top =line_end + rotate(ImVec2(0, -0.5f*width), trig);
    auto arrow_bot =line_end + rotate(ImVec2(0, +0.5f*width), trig);

    drawlist->AddLine(from, line_end, color, thickness);
    drawlist->AddTriangleFilled(arrow_top, arrow_end, arrow_bot,color);

    return arrow_end;
}

ImVec2 draw_arrow_middle(ImDrawList * const drawlist, const ImVec2 &from, float length, float deg, ImU32 color, float thickness)
{
    auto width=3*thickness;

    auto trig = int_sincos(deg);
    ImVec2 line_end = from + rotate(ImVec2(length, 0), trig);

    ImVec2 pos = from + rotate(ImVec2(0.5f*(length - width), 0), trig);
    auto arrow_end =pos + rotate(ImVec2(width, 0), trig);
    auto arrow_top =pos + rotate(ImVec2(0, -0.5f*width), trig);
    auto arrow_bot =pos + rotate(ImVec2(0, +0.5f*width), trig);

    drawlist->AddLine(from, line_end, color, thickness);
    drawlist->AddTriangleFilled(arrow_top, arrow_end,arrow_bot, color);
    return line_end;
}

ImVec2 draw_arrow(ImDrawList * const drawlist, const ImVec2 &from, const ImVec2 &to, ImU32 color, float thickness)
{
    auto vector = to-from;
    float total_length = ImSqrt(vector.x*vector.x + vector.y*vector.y);
    float angle = std::asin(vector.y/total_length) * 180.0f / std::numbers::pi;

    if (angle >=0 && vector.x<0)
        angle=180.0f - angle;
    else if (angle <0 && vector.x<0)
        angle=-180.0f - angle;

    return draw_arrow(drawlist, from, total_length, angle, color, thickness);
}


}
