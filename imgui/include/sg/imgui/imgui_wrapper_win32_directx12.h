#pragma once
#ifdef LIBSG_IMGUI_DIRECTX

#include "imgui_wrapper_interface.h"

namespace sg::imgui {

class ImGuiWrapper_Win32_DirectX12 : public IImGuiWrapper {
  public:
    ImGuiWrapper_Win32_DirectX12(on_start_t, on_end_t, on_iteration_t, ConfigFlags = ConfigFlags::None);
    void start(const std::string &title);
};
} // namespace sg::imgui

#endif
