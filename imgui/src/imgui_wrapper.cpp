#include "sg/imgui/imgui_wrapper_interface.h"

#include "sg/enumeration.h"

#include <implot.h>

#include <utility>

namespace sg::imgui {

IImGuiWrapper::IImGuiWrapper(IImGuiWrapper::on_start_t on_start_cb,
                             IImGuiWrapper::on_end_t on_end_cb,
                             IImGuiWrapper::on_iteration_t on_it_cb,
                             ConfigFlags flags)
    : m_configflags(flags),
      m_callbacks(Callbacks{.onStart     = std::move(on_start_cb),
                            .onEnd       = std::move(on_end_cb),
                            .onIteration = std::move(on_it_cb)}) {}

IImGuiWrapper::IImGuiWrapper(Callbacks callbacks, ConfigFlags flags)
    : m_configflags(flags),
      m_callbacks(std::move(callbacks)) {}

IImGuiWrapper::~IImGuiWrapper()  =default;

void IImGuiWrapper::initalise()
{
    if (sg::enumeration::contains(m_configflags, ConfigFlags::IncludeImPlot))
        ImPlot::CreateContext();
    if (m_callbacks.onStart)
        m_callbacks.onStart.invoke(*this);
}

void IImGuiWrapper::iterate(bool& done)
{
    if (sg::enumeration::contains(m_configflags, ConfigFlags::Docking))
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
    if (m_callbacks.onIteration)
        m_callbacks.onIteration.invoke(*this, done);
}

void IImGuiWrapper::cleanup()
{
    if (m_callbacks.onEnd)
        m_callbacks.onEnd.invoke(*this);
    if (sg::enumeration::contains(m_configflags, ConfigFlags::IncludeImPlot))
        ImPlot::DestroyContext();
}

void IImGuiWrapper::setup_io(ImGuiIO& io) {
    io.ConfigFlags |= to_imgui_configflags(m_configflags);

    if (sg::enumeration::contains(m_configflags, ConfigFlags::NoIni))
        io.IniFilename = nullptr;
}

ImGuiConfigFlags IImGuiWrapper::to_imgui_configflags(ConfigFlags flags)
{
    ImGuiConfigFlags output = ImGuiConfigFlags_None;

    if (sg::enumeration::contains(flags, ConfigFlags::Docking))
        output|=ImGuiConfigFlags_DockingEnable;

    if (sg::enumeration::contains(flags, ConfigFlags::ViewPort))
        output|=ImGuiConfigFlags_ViewportsEnable;

    return output;
}


}
