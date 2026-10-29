#pragma once

#include "ConfigFlags.h"
#include "sg/callback.h"

#include <imgui.h>

#include <memory>
#include <optional>
#include <string>

namespace sg::imgui {

class IImGuiWrapper {
  public:
    CREATE_CALLBACK(on_start_t, void(IImGuiWrapper&));
    CREATE_CALLBACK(on_end_t, void(IImGuiWrapper&));
    CREATE_CALLBACK(on_iteration_t, void(IImGuiWrapper&, bool &done));

    struct Callbacks {
        on_start_t onStart {nullptr};
        on_end_t onEnd {nullptr};
        on_iteration_t onIteration {nullptr};
    };

    [[deprecated("Use Callbacks struct instead")]]
    IImGuiWrapper(on_start_t, on_end_t, on_iteration_t, ConfigFlags);
    IImGuiWrapper(Callbacks, ConfigFlags);

    virtual ~IImGuiWrapper();

    virtual void start(const std::string &title) = 0;
    virtual void changeWindowTitle(const std::string &title) = 0;

    /* The main window's restored (not maximised) rectangle, in the platform's desktop
     * coordinates, and whether it is maximised - see ConfigFlags::RememberWindowPlacement. */
    struct window_placement {
        int x = 0, y = 0, width = 0, height = 0;
        bool maximised = false;
    };

  protected:
    /* sets up ImGUI IO */
    void setup_io(ImGuiIO&);

    /* this is called after the platform/renderer backends have been
     * intialised*/
    void initalise();

    void iterate(bool &done);
    void cleanup();

    const ConfigFlags m_configflags;
    ImGuiConfigFlags to_imgui_configflags(ConfigFlags);

    /* ConfigFlags::RememberWindowPlacement, kept in imgui.ini as [SgMainWindow][Main]. A wrapper
     * creates its main window hidden, calls this after setup_io(), applies what it returns - if
     * anything, and only while it is on a connected display - and only then shows the window, so
     * it doesn't appear in one place and jump to another. Reads imgui.ini now, not on the first
     * frame as ImGui would. nullopt without the flag, with NoIni, or with nothing saved. */
    std::optional<window_placement> load_window_placement();

    /* What to save: asked whenever ImGui writes imgui.ini - last when the ImGui context is
     * destroyed, which the wrappers do while the window still exists. nullopt keeps what was
     * loaded. */
    virtual std::optional<window_placement> current_window_placement() const { return std::nullopt; }

  private:
    const Callbacks m_callbacks;

    /* The imgui.ini handler's state (see load_window_placement()), type private to the .cpp */
    std::shared_ptr<void> m_placement_settings;
};

} // namespace sg::imgui
