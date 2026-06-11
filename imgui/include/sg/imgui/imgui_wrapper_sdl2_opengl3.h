#pragma once
#ifdef LIBSG_IMGUI_OPENGL

#include "imgui_wrapper_interface.h"
#include "sg/pimpl.h"

// SDL header must be here, as it defines SDL_main which must be delcared
// in a running library (SDL renames main to SDL_main, to provide some wrappers)
#include <SDL2/SDL.h>

namespace sg::imgui {

class ImGuiWrapper_Sdl2_OpenGl3 : public IImGuiWrapper {
    struct impl;
    pimpl<impl> m_pimpl;

  public:
    [[deprecated("Use Callbacks struct instead")]]
    ImGuiWrapper_Sdl2_OpenGl3(on_start_t, on_end_t, on_iteration_t,
                              ConfigFlags = ConfigFlags::None);
    ImGuiWrapper_Sdl2_OpenGl3(Callbacks, ConfigFlags = ConfigFlags::None);
    ~ImGuiWrapper_Sdl2_OpenGl3() override;

    void start(const std::string &title) override;
    void changeWindowTitle(const std::string& title) override;
};
} // namespace sg::imgui

#endif
