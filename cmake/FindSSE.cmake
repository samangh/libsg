# Checks for the following CPU flags (and support by compiler)
#
# Components: SSE2 SSE3 SSSE3 SSE41 SSE42 AVX AVX2 AVX512

include(CheckCXXCompilerFlag)
include(check_cpu_flag)

##
## _SSE_set_target
##

function(_SSE_set_target)
  set(options "")
  set(multiValueArgs "")
  set(oneValueArgs FEATURE GCC_FLAG CLANG_FLAG MSVC_FLAG)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(SSE_${ARG_FEATURE}_FOUND AND
      NOT TARGET SSE_${ARG_FEATURE} AND
      NOT TARGET SSE::${ARG_FEATURE})

    add_library(SSE_${ARG_FEATURE} INTERFACE)
    target_compile_options(SSE_${ARG_FEATURE} INTERFACE
      $<$<CXX_COMPILER_ID:GNU>:${ARG_GCC_FLAG}>
      $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${ARG_CLANG_FLAG}>
      $<$<CXX_COMPILER_ID:MSVC>:${ARG_MSVC_FLAG}>)
    target_compile_definitions(SSE_${ARG_FEATURE} INTERFACE CPU_SUPPORTS_${ARG_FEATURE})

    add_library(SSE::${ARG_FEATURE} ALIAS SSE_${ARG_FEATURE})
  endif()
endfunction()

##
## Start
##

if(CMAKE_SYSTEM_NAME MATCHES "Linux" OR
    CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR
    (CMAKE_SYSTEM_NAME MATCHES "Darwin" AND
      NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm"))
  check_cpu_flag(CPU_FLAG sse COMPILER_FLAG -msse OUTPUT_VARIABLE _SSE_SUPPORTED)
  check_cpu_flag(CPU_FLAG sse2 COMPILER_FLAG -msse2 OUTPUT_VARIABLE _SSE2_SUPPORTED)
  check_cpu_flag(CPU_FLAG pni COMPILER_FLAG -msse3 OUTPUT_VARIABLE _SSE3_SUPPORTED)
  check_cpu_flag(CPU_FLAG sse4.1 COMPILER_FLAG -msse4.1 OUTPUT_VARIABLE _SSE41_SUPPORTED)
  check_cpu_flag(CPU_FLAG sse4.2 COMPILER_FLAG -msse4.2 OUTPUT_VARIABLE _SSE42_SUPPORTED)
  check_cpu_flag(CPU_FLAG ssse3 COMPILER_FLAG -msse3 OUTPUT_VARIABLE _SSSE3_SUPPORTED)
  check_cpu_flag(CPU_FLAG avx COMPILER_FLAG -mavx OUTPUT_VARIABLE _AVX_SUPPORTED)
  check_cpu_flag(CPU_FLAG avx2 COMPILER_FLAG -mavx2 OUTPUT_VARIABLE _AVX2_SUPPORTED)
  check_cpu_flag(CPU_FLAG pclmulqdq COMPILER_FLAG -mpclmul OUTPUT_VARIABLE _CLMUL_SUPPORTED)
  check_cxx_compiler_flag("-mcrc32" _CRC32_SUPPORTED)

  # SSE3 is known as the Prescott New Instructions (PNI) on Linux
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    check_cpu_flag(CPU_FLAG pni COMPILER_FLAG -msse3 OUTPUT_VARIABLE _SSE3_SUPPORTED)
  else()
    check_cpu_flag(CPU_FLAG sse3 COMPILER_FLAG -msse3 OUTPUT_VARIABLE _SSE3_SUPPORTED)
  endif()
endif()

if(MSVC)
  # Don't know how to check in Windows
  check_cxx_compiler_flag("/arch:AVX" _AVX_SUPPORTED)
  check_cxx_compiler_flag("/arch:AVX2" _AVX2_SUPPORTED)
  check_cxx_compiler_flag("/arch:AVX512" _AVX512_SUPPORTED)
  check_cxx_compiler_flag("/d2archSSE42" _SSE42_SUPPORTED)

  # Set everything else true
  foreach(c SSE SSE2 SSE3 SSE41 SSSE3 CLMUL CRC32)
    set(_${c}_SUPPORTED true)
  endforeach()
endif()

if(MSYS OR MINGW)
  check_cxx_compiler_flag("-msse" _SSE_SUPPORTED)
  check_cxx_compiler_flag("-msse2" _SSE2_SUPPORTED)
  check_cxx_compiler_flag("-msse3" _SSE3_SUPPORTED)
  check_cxx_compiler_flag("-msse4.1" _SSE41_SUPPORTED)
  check_cxx_compiler_flag("-msse4.2" _SSE42_SUPPORTED)
  check_cxx_compiler_flag("-msse3" _SSSE3_SUPPORTED)
  check_cxx_compiler_flag("-mavx" _AVX_SUPPORTED)
  check_cxx_compiler_flag("-mavx2" _AVX2_SUPPORTED)
  check_cxx_compiler_flag("-mpclmul" _CLMUL_SUPPORTED)
  check_cxx_compiler_flag("-mcrc32" _CRC32_SUPPORTED)
  check_cxx_compiler_flag("-msse3" _SSE3_SUPPORTED)
endif()

##
## Find components
##

# A component, say SSE41, is found if _SSE41_SUPPORTED is set
foreach(comp ${SSE_FIND_COMPONENTS})
  if (_${comp}_SUPPORTED)
    set(SSE_${comp}_FOUND _${comp}_SUPPORTED)
  endif()
endforeach()

##
## Find package
##
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SSE HANDLE_COMPONENTS)

##
## Set alias libraries
##

_sse_set_target(FEATURE SSE
  GCC_FLAG "-msse"
  CLANG_FLAG "-msse")

_sse_set_target(FEATURE SSE2
  GCC_FLAG "-msse2"
  CLANG_FLAG "-msse2")

_sse_set_target(FEATURE SSE3
  GCC_FLAG "-msse3"
  CLANG_FLAG "-msse3")

_sse_set_target(FEATURE SSE3
  GCC_FLAG "-msse4.1"
  CLANG_FLAG "-msse4.1")

_sse_set_target(FEATURE SSE42
  GCC_FLAG "-msse4.2"
  CLANG_FLAG "-msse4.2"
  MSVC_FLAG "/d2archSSE42")

_sse_set_target(FEATURE AVX
  GCC_FLAG "-mavx"
  CLANG_FLAG "-mavx"
  MSVC_FLAG "/arch:AVX")

_sse_set_target(FEATURE AVX2
  GCC_FLAG "-mavx2"
  CLANG_FLAG "-mavx2"
  MSVC_FLAG "/arch:AVX2")

_sse_set_target(FEATURE AVX512
  MSVC_FLAG "/arch:AVX512")

_SSE_set_target(FEATURE CRC32
  GCC_FLAG "-mcrc32"
  CLANG_FLAG "-mcrc32")

_SSE_set_target(FEATURE CLMUL
  GCC_FLAG "-mpclmul"
  CLANG_FLAG "-mpclmul")

mark_as_advanced(SSE_FOUND)
