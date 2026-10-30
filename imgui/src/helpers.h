#pragma once

#include <imgui.h>

/**
 * Whether any secondary platform window (a window dragged out of the main one, with
 * ImGuiConfigFlags_ViewportsEnable) is showing.
 */
inline bool is_secondary_window_shown() {
    /* Viewports[0] is always the main viewport. Those windows are drawn by this same loop, so it
     * must keep running for them even while the main  window is minimised. */
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    for (int i = 1; i < platform_io.Viewports.Size; ++i)
    {
        ImGuiViewport* viewport = platform_io.Viewports[i];

        /* no platform window yet - nothing is on screen for this viewport */
        if (!viewport->PlatformWindowCreated)
            continue;

        /* ask the backend rather than reading ImGuiViewportFlags_IsMinimized: that flag is only
         * refreshed by UpdatePlatformWindows(), which the caller skips while it is parked on this
         * function returning false, so a cached flag could never change and the loop would never
         * wake up again. A backend without the query is treated as showing, as parking the loop on
         * a window we cannot ask about risks freezing it. */
        if (platform_io.Platform_GetWindowMinimized && platform_io.Platform_GetWindowMinimized(viewport))
            continue;

        return true;
    }
    return false;
}
