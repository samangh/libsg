set(SG_TARGET_NANE "imgui")
set(TARGET "sg_${SG_TARGET_NANE}")
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)
set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/implot)
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
  ${IMGUI_DIR}/*.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp

  ${IMPLOT_DIR}/*.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

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
  SRC_FILES
     ${SRC_FILES}
  INCLUDE_INTERFACE
  INCLUDE_PUBLIC
    $<BUILD_INTERFACE:${IMGUI_DIR}>
    $<BUILD_INTERFACE:${IMPLOT_DIR}>
    $<BUILD_INTERFACE:${IMGUI_FILE_BROWSER_DIR}>
  INCLUDE_PRIVATE
    Boost::Boost
  LINK_INTERFACE
  LINK_PUBLIC
    SG::common
    portable_file_dialogs

    $<TARGET_NAME_IF_EXISTS:bar>
    SDL2::SDL2main
  LINK_PRIVATE
    OpenGL::GL
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
  COMPILE_DEFINITIONS_PRIVATE

  ## Options
  GENERATE_EXPORT_HEADER
  STATIC
)


##
## SDL
##

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
