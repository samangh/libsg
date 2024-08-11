function(setup_target_options)
  # Usual CMake function boiler plate
  set(options "")
  set(multiValueArgs "")
  set(oneValueArgs TARGET)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  target_compile_options(${ARG_TARGET}
    PRIVATE
      #Warnings
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
      $<$<CXX_COMPILER_ID:MSVC>:/permissive->

      # Set arch to native (i.e. use all processor flags)
      $<$<AND:$<BOOL:${ARCH_NATIVE}>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-march=native>)

  target_link_libraries(${ARG_TARGET}
    PRIVATE
      # Enable SSE if
      $<$<AND:$<BOOL:${USE_SSE}>,$<BOOL:${SSE_SSE42_FOUND}>>:SSE::SSE42>
      $<$<AND:$<BOOL:${USE_SSE}>,$<BOOL:${SSE_AVX2_FOUND}>>:SSE::AVX2>
    PUBLIC
      $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.1>>:${STANDARD_LIBRARY}fs>
      $<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:${STANDARD_LIBRARY}fs>)


  # Link against statically to runtime in Windows if enabled
  if (USE_STATIC_RUNTIME)
    if(MSVC)
      set_property(TARGET ${ARG_TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
      target_link_options(${ARG_TARGET} PRIVATE
        # -static-libstdc++ for CXX GCC/Clang/AppleClang
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>>:-static-lib${STANDARD_LIBRARY}>
        # -static-libgcc for C or CXX, when using GCC/Clang (but not AppleClang)
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-static-libgcc>)
    endif()
  endif()

  add_sanitizers(${ARG_TARGET})
endfunction()
