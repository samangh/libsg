#pragma once

#include "ConfigFlags.h"
#include "sg/callback.h"

#include <imgui.h>

#include <string>

namespace sg::imgui {

class IImGuiWrapper {
  public:
    CREATE_CALLBACK(on_start_t, void(IImGuiWrapper&));
    CREATE_CALLBACK(on_end_t, void(IImGuiWrapper&));
    CREATE_CALLBACK(on_every_iteration_t, void(IImGuiWrapper&, bool &done));
    CREATE_CALLBACK(on_skipped_iteration_t, void(IImGuiWrapper&, bool &done));
    CREATE_CALLBACK(on_visible_iteration_t, void(IImGuiWrapper&, bool &done));

    struct Callbacks {
        on_start_t onStart {nullptr};
        on_end_t onEnd {nullptr};
        on_every_iteration_t onEveryIteration {nullptr};
        on_skipped_iteration_t onSkippedIteration {nullptr};
        on_visible_iteration_t onVisibleIteration {nullptr};
    };

    struct WindowPlacement {
        int x = 0, y = 0, width = 0, height = 0;
        bool maximised = false;
    };

    [[deprecated("Use Callbacks struct instead")]]
    IImGuiWrapper(on_start_t, on_end_t, on_visible_iteration_t, ConfigFlags);
    IImGuiWrapper(Callbacks, ConfigFlags);

    virtual ~IImGuiWrapper();

    virtual void start(const std::string &title) = 0;
    virtual void changeWindowTitle(const std::string &title) = 0;

    /* Get/set the main window's placement.
     *
     * Only available from onStart to onEnd: otherwise windowPlacement() returns an empty placement
     * and setWindowPlacement() does nothing. A placement that is empty, or not on any connected
     * display, won't leave the window off screen. */
    [[nodiscard]] virtual WindowPlacement windowPlacement() const = 0;
    virtual void setWindowPlacement(const WindowPlacement &placement) = 0;

  protected:
    /* sets up ImGUI IO */
    void setup_io(ImGuiIO&);

    /* this is called after the platform/renderer backends have been
     * intialised*/
    void initalise();

    void on_every_iteration(bool &done);
    void on_skipped_iteration(bool& done);
    void on_visible_iteration(bool& done);

    void cleanup();

    const ConfigFlags m_configflags;
    ImGuiConfigFlags to_imgui_configflags(ConfigFlags);

  private:
    const Callbacks m_callbacks;
};

} // namespace sg::imgui
