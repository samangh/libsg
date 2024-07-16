function(setup_target_options)
  # Usual CMake function boiler plate
  set(options "")
  set(multiValueArgs "")
  set(oneValueArgs TARGET)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  target_compile_options(${ARG_TARGET}
    PRIVATE
      #Warnings
      $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall -Wextra>
      $<$<CXX_COMPILER_ID:MSVC>:/permissive->

      # Set arch to native (i.e. use all processor flags)
      $<$<AND:$<BOOL:${ARCH_NATIVE}>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>>:-march=native>)

  target_link_libraries(${ARG_TARGET}
    PRIVATE
      # Enable SSE if
      $<$<AND:$<BOOL:${USE_SSE}>,$<BOOL:${SSE_SSE41_FOUND}>>:SSE::SSE42>
      $<$<AND:$<BOOL:${USE_SSE}>,$<BOOL:${SSE_AVX2_FOUND}>>:SSE::AVX2>)

  # Link against statically to runtime in Windows if enabled
  if (USE_STATIC_RUNTIME)
    if(MSVC)
      set_property(TARGET ${ARG_TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if (MSYS OR MINGW)
      target_compile_options(${ARG_TARGET} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-static-libstdc++>)
    endif()
  endif()

  add_sanitizers(${ARG_TARGET})
endfunction()