set(SG_TARGET_NANE "imgui")
set(TARGET "sg_${SG_TARGET_NANE}")

CPMAddPackage(
  NAME imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.6-docking.zip
  # GITHUB_REPOSITORY ocornut/imgui
  # VERSION 1.91.9b-docking
  # GIT_SHALLOW
)
CPMAddPackage(
  NAME implot
  URL https://github.com/epezent/implot/archive/81b8b1951392767cf458508385fa025fd087a252.zip
  # GITHUB_REPOSITORY epezent/implot
  # GIT_TAG 3da8bd34299965d3b0ab124df743fe3e076fa222
  # GIT_SHALLOW
)

set(PORTABLE_FILE_DIALOGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/portable-file-dialogs)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
add_subdirectory(${PORTABLE_FILE_DIALOGS_DIR} ../external/portable-file-dialogs EXCLUDE_FROM_ALL)

##
## Source files
##

## ImGui source files
##
file(GLOB SRC_FILES
  ${imgui_SOURCE_DIR}/*.cpp
  ${implot_SOURCE_DIR}/*.cpp)

#list(REMOVE_ITEM IMPLOT_SRC_FILES ${IMPLOT_DIR}/implot_demo.cpp)

## From ImPlot FAQ
##
## It is recommended that you compile and link ImPlot as a static
## library or directly as a part of your sources. However, if you must
## and are compiling ImPlot and ImGui as separate DLLs, make sure you
## set the current ImGui context with
## ImPlot::SetImGuiContext(ImGuiContext* ctx). This ensures that global
## ImGui variables are correctly shared across the DLL boundary.
##

setup_library(
  TARGET
    sg_imgui
  NAMESPACE
    SG
  NAMESPACE_TARGET
    imgui
  DIRECTORY
     ${CMAKE_CURRENT_SOURCE_DIR}

  SOURCES_PRIVATE
    # ImGUi files
    ${SRC_FILES}

    src/utils.cpp
    src/dialog_box.cpp
    src/imgui_wrapper.cpp

    # OpenGL
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:
      src/imgui_wrapper_sdl2_opengl3.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp>

    # DirectX
    $<$<BOOL:${LIBSG_IMGUI_DIRECTX}>:
      src/imgui_wrapper_win32_directx12.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp>

  INCLUDE_INTERFACE
  INCLUDE_PUBLIC
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
    $<BUILD_INTERFACE:${implot_SOURCE_DIR}>
    $<BUILD_INTERFACE:${IMGUI_FILE_BROWSER_DIR}>
  INCLUDE_PRIVATE
  LINK_INTERFACE
  LINK_PUBLIC
    SG::common
    portable_file_dialogs
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:SDL2::SDL2main>
  LINK_PRIVATE
    $<$<BOOL:${LIBSG_IMGUI_DIRECTX}>:D3D12.lib DXGI.lib>
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:OpenGL::GL>
    "$<$<BOOL:${APPLE}>:-framework CoreFoundation>"
    $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:${CMAKE_DL_LIBS}>

    #math library, needed by sg::imgui::units
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:m>
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
  COMPILE_OPTIONS_PRIVATE
  COMPILE_FEATURES_INTERFACE
  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_FEATURES_PRIVATE
  COMPILE_DEFINITIONS_INTERFACE
  COMPILE_DEFINITIONS_PUBLIC
    IMGUI_DISABLE_INCLUDE_IMCONFIG_H
    IMGUI_USER_CONFIG="sg/imgui/config.h"
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:LIBSG_IMGUI_OPENGL>
    $<$<BOOL:${LIBSG_IMGUI_DIRECTX}>:LIBSG_IMGUI_DIRECTX>
  COMPILE_DEFINITIONS_PRIVATE

  ## Options
  GENERATE_EXPORT_HEADER
  STATIC
)


##
## SDL
##

if(LIBSG_IMGUI_OPENGL)
  if(TARGET SDL2::SDL2main)
    if(TARGET SDL2::SDL2-static)
      if (NOT BUILD_SHARED_LIBS OR NOT TARGET SDL2::SDL2)
        set(USE_STATIC_SDL ON)
      endif()
    endif()
    target_link_libraries(${TARGET} PUBLIC
      $<IF:$<BOOL:${USE_STATIC_SDL}>,SDL2::SDL2-static,SDL2::SDL2>
      SDL2::SDL2main)
  else()
    target_include_directories(${TARGET} PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${TARGET} PUBLIC ${SDL2_LIBRARIES})
  endif()
endif()
