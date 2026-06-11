#pragma once
#ifdef LIBSG_IMGUI_DIRECTX

#include "imgui_wrapper_interface.h"
#include "sg/pimpl.h"

namespace sg::imgui {

class ImGuiWrapper_Win32_DirectX12 : public IImGuiWrapper {
    struct impl;
    pimpl<impl> m_pimpl;

  public:
    [[deprecated("Use Callbacks struct instead")]]
    ImGuiWrapper_Win32_DirectX12(on_start_t, on_end_t, on_iteration_t,
                                 ConfigFlags = ConfigFlags::None);
    ImGuiWrapper_Win32_DirectX12(Callbacks, ConfigFlags = ConfigFlags::None);
    ~ImGuiWrapper_Win32_DirectX12() override;

    void start(const std::string &title) override;
    void changeWindowTitle(const std::string& title) override;
};
} // namespace sg::imgui

#endif
