#include "sg/imgui/imgui_wrapper_interface.h"

#include "sg/enumeration.h"

#include <imgui_internal.h> // ImGuiSettingsHandler, AddSettingsHandler(), ImHashStr()
#include <implot.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

namespace {

/******************
 * ADDED BY ROBERT *
 ******************/
/* ConfigFlags::RememberWindowPlacement's imgui.ini section - see load_window_placement():
 *
 *     [SgMainWindow][Main]
 *     Pos=100,100
 *     Size=1280,800
 *     Maximized=0
 */
using window_placement = sg::imgui::IImGuiWrapper::window_placement;

struct placement_settings {
    std::optional<window_placement> loaded;
    std::function<std::optional<window_placement>()> current;
};

void* placement_read_open(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name) {
    if (std::strcmp(name, "Main") != 0)
        return nullptr;
    auto* settings   = static_cast<placement_settings*>(handler->UserData);
    settings->loaded = window_placement{};
    return &*settings->loaded;
}

void placement_read_line(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    auto* placement = static_cast<window_placement*>(entry);
    int a = 0, b = 0;
    if (std::sscanf(line, "Pos=%d,%d", &a, &b) == 2) {
        placement->x = a;
        placement->y = b;
    } else if (std::sscanf(line, "Size=%d,%d", &a, &b) == 2) {
        placement->width  = a;
        placement->height = b;
    } else if (std::sscanf(line, "Maximized=%d", &a) == 1) {
        placement->maximised = a != 0;
    }
}

void placement_write_all(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    const auto* settings = static_cast<const placement_settings*>(handler->UserData);
    std::optional<window_placement> placement = settings->current ? settings->current() : std::nullopt;
    if (!placement || placement->width <= 0 || placement->height <= 0)
        placement = settings->loaded; // the window couldn't say: keep what was read
    if (!placement)
        return;
    buf->appendf("[%s][Main]\nPos=%d,%d\nSize=%d,%d\nMaximized=%d\n\n", handler->TypeName, placement->x,
                 placement->y, placement->width, placement->height, placement->maximised ? 1 : 0);
}
/******************/

} // namespace

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

std::optional<IImGuiWrapper::window_placement> IImGuiWrapper::load_window_placement() {
    const ImGuiIO& io = ImGui::GetIO();
    if (!sg::enumeration::contains(m_configflags, ConfigFlags::RememberWindowPlacement) || !io.IniFilename)
        return std::nullopt;

    auto settings     = std::make_shared<placement_settings>();
    settings->current = [this] { return current_window_placement(); };
    m_placement_settings = settings;

    ImGuiSettingsHandler handler;
    handler.TypeName   = "SgMainWindow";
    handler.TypeHash   = ImHashStr("SgMainWindow");
    handler.ReadOpenFn = placement_read_open;
    handler.ReadLineFn = placement_read_line;
    handler.WriteAllFn = placement_write_all;
    handler.UserData   = settings.get();
    ImGui::AddSettingsHandler(&handler);

    /* Now rather than on the first frame, where ImGui would: the window is shown before then.
     * ImGui marks the settings loaded, so it doesn't read them again. */
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    if (settings->loaded && (settings->loaded->width <= 0 || settings->loaded->height <= 0))
        settings->loaded.reset();
    return settings->loaded;
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
