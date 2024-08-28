include(GetAllTargets)
get_all_targets(_TARGETS)

##
## Set all targets to use static runtime
##

# Link against statically to runtime if enabled.
#
# It's better to use ${get_all_targets}, as that also pulls in any
# subprojects that we might be using (e.g. nanobench)

if (USE_STATIC_RUNTIME)
  foreach(TARGET ${_TARGETS})
    if(MSVC)
      set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
      target_link_options(${TARGET} PRIVATE
        # -static-libstdc++ for CXX GCC/Clang/AppleClang
        $<$<AND:$<COMPILE_LANGUAGE:CXX>, $<OR:$<CXX_COMPILER_ID:Clang>,
                                              $<CXX_COMPILER_ID:AppleClang>,
                                              $<CXX_COMPILER_ID:GNU>>>:-static-lib${STANDARD_LIBRARY}>

        # -static-libgcc for C or CXX, when using GCC/Clang (but not AppleClang)
        $<$<OR:$<CXX_COMPILER_ID:Clang>,
               $<CXX_COMPILER_ID:GNU>,
               $<C_COMPILER_ID:GNU>,
               $<C_COMPILER_ID:GNU>>:-static-libgcc>)
    endif()
  endforeach()
endif()

unset(_TARGETS)
