##
## Windows
##

if(WIN32)
  set(IVI_DIR "C:\\Program Files (x86)\\IVI Foundation\\VISA\\WinNT")

  # Use 32-bit visa in x86 and 64-bit in x64 by looking at pointer size
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VISA_LIB_NAME visa64)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(VISA_LIB_NAME visa32)
  endif()

  find_path(VISA_INCLUDE_DIRS
    NAMES visa.h
    HINTS ${VISA_DIR}/include ${IVI_DIR}/include)

  find_library(VISA_LIBRARIES
    NAMES ${VISA_LIB_NAME}
    HINTS ${VISA_DIR}/lib ${VISA_DIR}/lib/msc ${VISA_DIR}/Lib_x64/msc ${IVI_DIR}/lib/msc ${IVI_DIR}/Lib_x64/msc)
endif()

##
## macOS
##

if(APPLE)
  set(VISA_FRAMEWORK "/Library/Frameworks/VISA.framework")
  set(RSVISA_FRAMEWORK "/Library/Frameworks/RsVisa.framework")

  # The CMake searching for Frameworks is broken, so use it as a last
  # resort. Instead give direct paths to the library and header folders.
  #
  # See https://cmake.org/pipermail/cmake/2014-April/057397.html
  set(CMAKE_FIND_FRAMEWORK LAST)

  find_path(VISA_INCLUDE_DIRS
    NAMES visa.h
    HINTS ${VISA_DIR}/include ${VISA_FRAMEWORK}/Headers ${RSVISA_FRAMEWORK}/Headers)

  find_library(VISA_LIBRARIES
    NAMES rsvisa visa
    HINTS ${VISA_DIR}/lib ${VISA_FRAMEWORK}/Versions/Current/VISA ${RSVISA_FRAMEWORK}/Versions/Current/RsVisa)
endif()

##
## Unix/Linux
##

if (UNIX AND NOT APPLE)
  find_path(VISA_INCLUDE_DIRS
    NAMES visa.h
    HINTS ${VISA_DIR}/include "/usr/include/ni-visa" "/usr/include/rsvisa")

  find_library(VISA_LIBRARIES
    NAMES visa ivivisa rsvisa
    HINTS
      ${VISA_DIR}/lib

      # On openSUSE, NI mistakenly puts it's libraries at
      # /usr/lib/x86_64-linux-gnu instead of /usr/lib64, so add those
      "/usr/lib/i386-linux-gnu" "/usr/lib/x86_64-linux-gnu")
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VISA DEFAULT_MSG VISA_LIBRARIES VISA_INCLUDE_DIRS)

if(VISA_FOUND AND NOT TARGET VISA)
  add_library(VISA UNKNOWN IMPORTED)
  set_target_properties(VISA PROPERTIES
	IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
	IMPORTED_LOCATION "${VISA_LIBRARIES}"
	INTERFACE_INCLUDE_DIRECTORIES "${VISA_INCLUDE_DIRS}")
endif()
