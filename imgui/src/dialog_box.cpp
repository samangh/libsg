# include <sg/imgui/dialog_box.h>

namespace sg::imgui {

void show_button(dialog_box_result& resultStore, dialog_box_result resultIfClicked,
                 const char* text) {
    auto size = sg::imgui::dimensions_of_button(text);
    if (ImGui::Button(text, size)) {
        resultStore = resultIfClicked;
        ImGui::CloseCurrentPopup();
    }
}

dialog_box_result dialog_box(const char* imgui_id, const std::string& message,
                             dialog_box_buttons buttons, int flags) {
    dialog_box_result result = dialog_box_result::NoResult;

    if (ImGui::BeginPopupModal(imgui_id, NULL, flags)) {
        ImGui::Spacing();
        ImGui::Text("%s", message.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto size = ImVec2(0, 0);

        switch (buttons) {
        case dialog_box_buttons::Ok:
            size += sg::imgui::dimensions_of_button("OK");

            centre_next_control_hor(size);
            show_button(result, dialog_box_result::OK, "OK");
            break;
        case dialog_box_buttons::OkAbort:
            size += sg::imgui::dimensions_of_button("OK");
            size += sg::imgui::dimensions_of_button("Abort");

            centre_next_control_hor(size, 2);
            show_button(result, dialog_box_result::OK, "OK");
            ImGui::SameLine();
            show_button(result, dialog_box_result::Abort, "Abort");
            break;
        case dialog_box_buttons::YesNo:
            size += sg::imgui::dimensions_of_button("Yes");
            size += sg::imgui::dimensions_of_button("No");

            centre_next_control_hor(size, 2);
            show_button(result, dialog_box_result::Yes, "Yes");
            ImGui::SameLine();
            show_button(result, dialog_box_result::No, "No");
            break;
        case dialog_box_buttons::YesNoCancel:
            size += sg::imgui::dimensions_of_button("Yes");
            size += sg::imgui::dimensions_of_button("No");
            size += sg::imgui::dimensions_of_button("Cancel");

            centre_next_control_hor(size, 3);
            show_button(result, dialog_box_result::Yes, "Yes");
            ImGui::SameLine();
            show_button(result, dialog_box_result::No, "No");
            ImGui::SameLine();
            show_button(result, dialog_box_result::Cancel, "Cancel");
            break;
        default:
            break;
        }

        if (result!=dialog_box_result::NoResult)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    return result;
}
} // namespace sg::imgui
