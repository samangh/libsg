set(SG_TARGET_NANE "imgui")
set(TARGET "sg_${SG_TARGET_NANE}")

CPMAddPackage(
  NAME imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0-docking.zip
  # GITHUB_REPOSITORY ocornut/imgui
  # VERSION 1.91.9b-docking
  # GIT_SHALLOW
)
CPMAddPackage(
  NAME implot
  URL https://github.com/epezent/implot/archive/refs/tags/v1.1.zip
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

## ImGui and ImPlot are compiled into sg_imgui, which exports their API
## (see sg/imgui/config.h). sg_imgui can therefore be a shared library, as
## long as nothing else in the process compiles its own copy of ImGui/ImPlot:
## each copy would have its own global context.
##
## ImGui does not guarantee ABI compatibility between versions, so anything
## linking to a shared sg_imgui must be rebuilt when ImGui/ImPlot is updated.
##
## Performance of a shared sg_imgui (ELF, i.e. Linux/BSD)
##
## In a shared library, calls to exported functions normally go through the
## PLT (Procedure Linkage Table): a small stub that jumps via a pointer the
## dynamic loader fills in at load time. This is because, in principle,
## another library loaded first could provide (interpose) the same function.
## Each such call costs an extra indirect jump and, worse, the compiler can't
## inline or optimise around it. Since IMGUI_API exports nearly all of ImGui,
## this would also apply to ImGui's many calls to itself.
##
## Nothing is meant to interpose ImGui's functions, so:
##
##  - -fno-semantic-interposition lets the compiler call/inline exported
##    functions directly within the same source file;
##  - -Wl,-Bsymbolic-functions makes the linker bind calls between sg_imgui's
##    own source files directly, rather than via the PLT.
##
## Together, calls inside sg_imgui behave as in a static build. Calls from
## consumers into sg_imgui still go via the PLT, which is unavoidable for a
## shared library and cheap (~1ns per call). Windows DLLs always bind their
## own calls directly, so neither flag is needed there. Neither are they
## needed on macOS, where the linker binds calls within a dylib directly (the
## "two-level namespace"); Apple's linker doesn't accept -Bsymbolic-functions.
##

setup_library(
  TARGET
    sg_imgui
  NAMESPACE
    ${NAMESPACE}
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
    sg::common
    portable_file_dialogs
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:SDL2::SDL2main>
  LINK_PRIVATE
    $<$<BOOL:${LIBSG_IMGUI_DIRECTX}>:D3D12.lib DXGI.lib Dwmapi.lib d3dcompiler.lib>
    $<$<BOOL:${LIBSG_IMGUI_OPENGL}>:OpenGL::GL>
    "$<$<BOOL:${APPLE}>:-framework CoreFoundation>"
    $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:${CMAKE_DL_LIBS}>

    #math library, needed by sg::imgui::units
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:m>
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
  COMPILE_OPTIONS_PRIVATE
    # Avoid PLT calls within sg_imgui, see comment at the top
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-semantic-interposition>
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
  LINK_OPTIONS_INTERFACE
  LINK_OPTIONS_PUBLIC
  LINK_OPTIONS_PRIVATE
    # Avoid PLT calls within sg_imgui, see comment at the top
    $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:-Wl,-Bsymbolic-functions>

  ## Options
  GENERATE_EXPORT_HEADER
  VISIBILITY_HIDDEN
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

# For legacy users
add_library(SG::imgui ALIAS sg_imgui)
