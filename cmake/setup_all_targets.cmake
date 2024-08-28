include(GetAllTargets)

# This finction does the common config required for all targets
macro(setup_all_targets)
  # Set all targets to use static runtime
  if (USE_STATIC_RUNTIME)

    get_all_targets(_TARGETS)
    foreach(TARGET ${_TARGETS})
      # Link against statically to runtime in Windows if enabled
      if(MSVC)
        set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      else()
        target_link_options(${TARGET} PRIVATE
          # -static-libstdc++ for CXX GCC/Clang/AppleClang
          $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>>:-static-lib${STANDARD_LIBRARY}>
          # -static-libgcc for C or CXX, when using GCC/Clang (but not AppleClang)
          $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-static-libgcc>)
      endif()
    endforeach()
    unset(_TARGETS)

  endif()
endmacro()
