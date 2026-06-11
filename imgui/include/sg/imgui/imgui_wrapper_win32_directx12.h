#pragma once
#ifdef LIBSG_IMGUI_DIRECTX

#include "imgui_wrapper_interface.h"
#include "sg/pimpl.h"

namespace sg::imgui {

class ImGuiWrapper_Win32_DirectX12 : public IImGuiWrapper {
    struct impl;
    pimpl<impl> m_pimpl;
  public:
    ImGuiWrapper_Win32_DirectX12(on_start_t, on_end_t, on_iteration_t, ConfigFlags = ConfigFlags::None);
    void start(const std::string &title) override;
    void changeWindowTitle(const std::string& title) override;

    /* that the destructor must be in the implementation, as the destructor needs to know the size
     * of impl */
    ~ImGuiWrapper_Win32_DirectX12() override;
};
} // namespace sg::imgui

#endif
