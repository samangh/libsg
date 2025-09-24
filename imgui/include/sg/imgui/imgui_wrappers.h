#pragma once

#include "imgui_wrapper_sdl2_opengl3.h"
#include "imgui_wrapper_win32_directx12.h"

namespace sg::imgui {

#if defined(LIBSG_IMGUI_OPENGL) && defined(LIBSG_IMGUI_DIRECTX)
typedef ImGuiWrapper_Win32_DirectX12 ImGuiWrapper;
#elif defined(LIBSG_IMGUI_OPENGL)
typedef ImGuiWrapper_Sdl2_OpenGl3 ImGuiWrapper;
#else
    #error "No backend specified for ImGUI"
#endif

} // namespace sg::imgui
