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

  private:
    const Callbacks m_callbacks;
};

} // namespace sg::imgui
