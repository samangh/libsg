#pragma once

#include <imgui.h>
#include <implot.h>

#include <functional>
#include <string>

namespace sg::imgui {

class IImGuiWrapper {
  public:
    typedef std::function<void(void)> on_start_t;
    typedef std::function<void(void)> on_end_t;
    typedef std::function<void(bool &done)> on_iteration_t;

    IImGuiWrapper(on_start_t, on_end_t, on_iteration_t);
    virtual void start(const std::string &title) = 0;

  protected:
    void initalise();
    void iterate(bool &done);
    void cleanup();

  private:
    const on_start_t m_on_start;
    const on_end_t m_on_end;
    const on_iteration_t m_on_iteration;
};

} // namespace sg::imgui
