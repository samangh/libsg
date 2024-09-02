# sets OUTPUT_VAR to TRUE if target is of a type that links to stuff. Otherwise sets OUTPUT_VAR to FALSE

function(does_target_use_linker TARGET OUTPUT_VAR)
  if(NOT TARGET)
    message(FATAL "Need to set TARGET for is_target_type_compiled function")
  endif()
  if(NOT OUTPUT_VAR)
    message(FATAL "Need to set OUTPUT_VAR for is_target_type_compiled function")
  endif()

  get_target_property(TARGET_TYPE ${TARGET} TYPE)
  foreach(_TYPE "MODULE_LIBRARY" "SHARED_LIBRARY" "EXECUTABLE")
    if (TARGET_TYPE STREQUAL ${_TYPE})
      set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
      return()
    endif()
  endforeach()

  # Set to false if no match
  set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
endfunction()
