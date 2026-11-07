#pragma once
#ifdef LIBSG_IMGUI_DIRECTX

#include "imgui_wrapper_interface.h"
#include "sg/pimpl.h"

namespace sg::imgui {

class ImGuiWrapper_Win32_DirectX12 : public IImGuiWrapper {
    struct impl;
    pimpl<impl> m_pimpl;

  public:
    ImGuiWrapper_Win32_DirectX12(Callbacks, ConfigFlags = ConfigFlags::None);
    ~ImGuiWrapper_Win32_DirectX12() override;

    void start(const std::string &title) override;
    void changeWindowTitle(const std::string& title) override;
    [[nodiscard]] WindowPlacement windowPlacement() const override;
    void setWindowPlacement(const WindowPlacement& placement) override;
};
} // namespace sg::imgui

#endif
