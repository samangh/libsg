set(SG_TARGET_NANE "common")
set(TARGET "sg_${SG_TARGET_NANE}")

set_property(GLOBAL APPEND PROPERTY ${PROJECT_NAME}_TARGETS ${TARGET})

find_package(Threads REQUIRED)
find_package(LibUV REQUIRED)
find_package(fmt REQUIRED)
find_package(zstd REQUIRED)

# Needed hardware-assited CRC32C checksum, optional as the SSE package
# will set pre-processor header definitions and the code will check for
# that
find_package(SSE OPTIONAL_COMPONENTS COMPONENTS SSE42 CRC32 CLMUL)

# Boost is required for other CRC checksums
find_package(Boost REQUIRED CONFIG)
include(setup_boost)

##
## Source files
##

file(GLOB_RECURSE SRC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${TARGET} ${SRC_FILES})
add_library(SG::${SG_TARGET_NANE} ALIAS ${TARGET})

##
## Includes
##

target_include_directories(${TARGET}
  INTERFACE
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  PRIVATE
    Boost::Boost
  )

target_link_libraries(${TARGET}
  INTERFACE
  PUBLIC
    LibUV::LibUV
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.1>>:${STANDARD_LIBRARY}fs>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:${STANDARD_LIBRARY}fs>
  PRIVATE
    $<$<BOOL:${SSE_SSE42_FOUND}>:SSE::SSE42> # For hardware accelerated CRC32
    $<$<BOOL:${SSE_CRC32_FOUND}>:SSE::CRC32> # For hardware accelerated CRC32
    $<$<BOOL:${SSE_CLMUL_FOUND}>:SSE::CLMUL> # For hardware accelerated CRC32
    Threads::Threads
    $<$<BOOL:${WIN32}>:Winmm>
    $<IF:$<BOOL:${USE_STATIC_LIBS}>,zstd::libzstd_static,zstd::libzstd_shared>
    fmt::fmt)

include (GenerateExportHeader)
generate_export_header(${TARGET}
             EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/include/sg/export/${TARGET}.h")


##
## Compiler flags
##

target_compile_features(${TARGET} PUBLIC cxx_std_20)
