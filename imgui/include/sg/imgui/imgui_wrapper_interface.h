#pragma once

#include "ConfigFlags.h"
#include "sg/callback.h"

#include <imgui.h>

#include <string>

namespace sg::imgui {

class IImGuiWrapper {
  public:
    CREATE_CALLBACK(on_start_t, void(void));
    CREATE_CALLBACK(on_end_t, void(void));
    CREATE_CALLBACK(on_iteration_t, void(bool &done));

    IImGuiWrapper(on_start_t, on_end_t, on_iteration_t, ConfigFlags);
    virtual ~IImGuiWrapper();

    virtual void start(const std::string &title) = 0;

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
    const on_start_t m_on_start;
    const on_end_t m_on_end;
    const on_iteration_t m_on_iteration;
};

} // namespace sg::imgui
