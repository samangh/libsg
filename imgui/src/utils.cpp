#include "sg/imgui/imgui_wrapper.h"
#include "sg/imgui/utils.h"

#include "karla-font.h"
#include <imgui_internal.h>

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

DialogResult create_modal_popup(const char *imgui_id, const std::string &message, const int flags)
{
    DialogResult result = DialogResult::NoResult;
    if (ImGui::BeginPopupModal(imgui_id, NULL, flags))
    {
        ImGui::Spacing();
        ImGui::Text("%s", message.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto size = ImVec2(80, 0);
        centre_next_control_hor(size);

        if (ImGui::Button("OK", size))
        {
            result = DialogResult::OK;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return result;
}

void centre_next_window(ImGuiCond cond)
{
    auto pos = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(pos, cond, ImVec2(0.5f,0.5f));

}

/* Calcultes the size of a button with the given text */
ImVec2 get_button_size(const char *msg)
{
    // See: https://github.com/ocornut/imgui/issues/3714#issuecomment-759319268
    auto style = ImGui::GetStyle();
    auto textSize= ImGui::CalcTextSize(msg);

    auto x= textSize.x + style.FramePadding.x * 2.0f;
    auto y= textSize.y + style.FramePadding.y * 2.0f;
    return ImVec2(x,y);
}

}
