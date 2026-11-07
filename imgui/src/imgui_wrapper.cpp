#include "sg/imgui/imgui_wrapper_interface.h"

#include "sg/enumeration.h"

#include <implot.h>

#include <utility>

namespace sg::imgui {

IImGuiWrapper::IImGuiWrapper(Callbacks callbacks, ConfigFlags flags)
    : m_configflags(flags),
      m_callbacks(std::move(callbacks)) {}

IImGuiWrapper::~IImGuiWrapper()  =default;

void IImGuiWrapper::initialise()
{
    if (sg::enumeration::contains(m_configflags, ConfigFlags::IncludeImPlot))
        ImPlot::CreateContext();
    if (m_callbacks.onStart)
        m_callbacks.onStart.invoke(*this);
}

void IImGuiWrapper::tick(bool& done, bool willRender)
{
    if (m_callbacks.onTick)
        m_callbacks.onTick.invoke(*this, done, willRender);
}

void IImGuiWrapper::draw(bool& done) {
    if (sg::enumeration::contains(m_configflags, ConfigFlags::Docking))
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
    if (m_callbacks.onDraw)
        m_callbacks.onDraw.invoke(*this, done);
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
