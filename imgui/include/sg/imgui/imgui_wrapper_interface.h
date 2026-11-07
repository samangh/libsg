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
    CREATE_CALLBACK(on_iteration_t, void(IImGuiWrapper&, bool &done, bool willRender));
    CREATE_CALLBACK(on_visible_iteration_t, void(IImGuiWrapper&, bool &done));

    /**
     * Callbacks:
     *
     *  - @c onIteration: called on every loop iteration, before the ImGui frame is started. @c
     *    willRender is false if rendering is being skipped this iteration (e.g. minimised)
     *  - @c onVisibleIteration: only called if the GUI is visible, after @c onIteration
     *
     *  You can only call ImGui functions in the @c onVisibleIteration callback. Setting
     *  @c done in @c onIteration still lets @c onVisibleIteration run for that iteration.
     */
    struct Callbacks {
        on_start_t onStart {nullptr};
        on_end_t onEnd {nullptr};
        on_iteration_t onIteration {nullptr};
        on_visible_iteration_t onVisibleIteration {nullptr};
    };

    struct WindowPlacement {
        int x = 0, y = 0, width = 0, height = 0;
        bool maximised = false;
    };

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

    /* this is called after the platform/renderer backends have been intialised*/
    void initialise();

    void on_iteration(bool &done, bool willRender);
    void on_visible_iteration(bool& done);

    void cleanup();

    const ConfigFlags m_configflags;
    ImGuiConfigFlags to_imgui_configflags(ConfigFlags);

  private:
    const Callbacks m_callbacks;
};

} // namespace sg::imgui
