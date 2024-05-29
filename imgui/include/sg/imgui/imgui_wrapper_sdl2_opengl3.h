#pragma once

#include "imgui_wrapper.h"

namespace sg::imgui {

class ImGuiWrapper_Sdl2_OpenGl3 : public IImGuiWrapper {
  public:
    ImGuiWrapper_Sdl2_OpenGl3(on_start_t, on_end_t, on_iteration_t, ConfigFlags = ConfigFlags::None);
    void start(const std::string &title);
};

} // namespace sg::imgui
