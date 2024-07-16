macro(prioritise_static_libraries)
  if(MSYS OR MINGW)
    # MSYS and MINGW also use .a for static
    list(PREPEND CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  elseif(UNIX)
    # This matches ofr all Unix like OS includig Linux, macOS and CygWin
    list(PREPEND CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif()
endmacro()
