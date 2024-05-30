#include "sg/imgui/imgui_wrapper.h"

#include "sg/enumeration.h"


namespace sg::imgui {

IImGuiWrapper::IImGuiWrapper(IImGuiWrapper::on_start_t on_start_cb,
                             IImGuiWrapper::on_end_t on_end_cb,
                             IImGuiWrapper::on_iteration_t on_it_cb,
                             ConfigFlags flags)
    : m_configflags(flags),
      m_on_start(on_start_cb),
      m_on_end(on_end_cb),
      m_on_iteration(on_it_cb) {}

void IImGuiWrapper::initalise()
{
    if (sg::enumeration::contains(m_configflags, ConfigFlags::IncludeImPlot))
        ImPlot::CreateContext();
    m_on_start();
}

void IImGuiWrapper::iterate(bool& done)
{
    if (sg::enumeration::contains(m_configflags, ConfigFlags::Docking))
        ImGui::DockSpaceOverViewport();
    m_on_iteration(done);
}

void IImGuiWrapper::cleanup()
{
    on_end_t();    
    if (sg::enumeration::contains(m_configflags, ConfigFlags::IncludeImPlot))
        ImPlot::CreateContext();
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
