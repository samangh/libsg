#pragma once
#ifdef LIBSG_IMGUI_OPENGL

#include "imgui_wrapper_interface.h"
#include "sg/pimpl.h"

// SDL header must be here, as it defines SDL_main which must be delcared
// in a running library (SDL renames main to SDL_main, to provide some wrappers)
#include <SDL2/SDL.h>

namespace sg::imgui {

class SG_IMGUI_EXPORT ImGuiWrapper_Sdl2_OpenGl3 : public IImGuiWrapper {
    struct impl;
    pimpl<impl> m_pimpl;

  public:
    ImGuiWrapper_Sdl2_OpenGl3(Callbacks, ConfigFlags = ConfigFlags::None);
    ~ImGuiWrapper_Sdl2_OpenGl3() override;

    void start(const std::string &title) override;
    void changeWindowTitle(const std::string& title) override;
    [[nodiscard]] WindowPlacement windowPlacement() const override;
    void setWindowPlacement(const WindowPlacement& placement) override;
};
} // namespace sg::imgui

#endif
