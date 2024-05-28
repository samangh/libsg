##
## Checks if a CPU supports a feature, and optionally also checks if the compiler does too
##

function(check_cpu_flag)
  set(options "")
  set(multiValueArgs "")
  set(oneValueArgs CPU_FLAG COMPILER_FLAG OUTPUT_VARIABLE)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Change case to lower when it comes to CPU flags
  # This is because linux uses lower case, whilst Apple uses upper case
  string(TOLOWER "${ARG_CPU_FLAG}" ARG_CPU_FLAG)

  # Check CPU flag
  if(CMAKE_SYSTEM_NAME MATCHES "Linux" OR
      CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR
      CMAKE_SYSTEM_NAME MATCHES "Darwin")

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
      execute_process(COMMAND cat "/proc/cpuinfo" OUTPUT_VARIABLE CPUINFO)
    endif()
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
      execute_process(COMMAND /usr/sbin/sysctl "-n" "machdep.cpu.features" "machdep.cpu.leaf7_features" OUTPUT_VARIABLE CPUINFO)
    endif()
    if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      execute_process(COMMAND cat "/var/run/dmesg.boot" COMMAND grep "Features"
        OUTPUT_VARIABLE CPUINFO)
    endif()

    string(REGEX REPLACE "^.*(${ARG_CPU_FLAG}).*$" "\\1" _SSE_THERE ${CPUINFO})
    string(COMPARE EQUAL "${ARG_CPU_FLAG}" "${_SSE_THERE}" _CPU_SUPPORTS)
  endif()

  # Check compile flag, if required
  if(${ARG_COMPILER_FLAG})
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("${ARG_COMPILER_FLAG}" _COMPILER_SUPPORTS)
  else()
    set(_COMPILER_SUPPORTS true)
  endif()

  if(_CPU_SUPPORTS AND _COMPILER_SUPPORTS)
    set(${ARG_OUTPUT_VARIABLE} TRUE PARENT_SCOPE)
  endif()
endfunction()
