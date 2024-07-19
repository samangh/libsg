#pragma once

#include "imgui_wrapper.h"

// SDL header must be here, as it defines SDL_main which must be delcared
// in a running library (SDL renames main to SDL_main, to provide some wrappers)
#include <SDL2/SDL.h>

namespace sg::imgui {

class ImGuiWrapper_Sdl2_OpenGl3 : public IImGuiWrapper {
  public:
    ImGuiWrapper_Sdl2_OpenGl3(on_start_t, on_end_t, on_iteration_t, ConfigFlags = ConfigFlags::None);
    void start(const std::string &title);
};

} // namespace sg::imgui
