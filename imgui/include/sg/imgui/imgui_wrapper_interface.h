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
    CREATE_CALLBACK(on_iteration_t, void(IImGuiWrapper&, bool &done));
    CREATE_CALLBACK(on_skipped_iteration_t, void(IImGuiWrapper&));

    struct Callbacks {
        on_start_t onStart {nullptr};
        on_end_t onEnd {nullptr};
        on_iteration_t onIteration {nullptr};
        /* Called instead of onIteration for each frame the loop skips because nothing can be
         * shown (main window minimised, or on Windows occluded, e.g. by a locked screen, and no
         * window dragged out of it is showing) - about every 10 ms while that lasts. Lets the
         * application keep up with work that must not wait for the window to come back, such as
         * taking in data arriving from another thread.
         *
         * It runs outside an ImGui frame: it must not call ImGui. */
        on_skipped_iteration_t onSkippedIteration {nullptr};
    };

    struct WindowPlacement {
        int x = 0, y = 0, width = 0, height = 0;
        bool maximised = false;
    };

    [[deprecated("Use Callbacks struct instead")]]
    IImGuiWrapper(on_start_t, on_end_t, on_iteration_t, ConfigFlags);
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

    void iterate(bool &done);
    void skipped_iteration();
    void cleanup();

    const ConfigFlags m_configflags;
    ImGuiConfigFlags to_imgui_configflags(ConfigFlags);

  private:
    const Callbacks m_callbacks;
};

} // namespace sg::imgui
