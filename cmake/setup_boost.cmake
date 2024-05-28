# Always include this before find_package(Boost ...)

# Use boost statially if making a static library
if(NOT BUILD_SHARED_LIBS)
  set(Boost_USE_STATIC_LIBS ON)
endif()

if (USE_STATIC_RUNTIME)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()

# Configure Boost in Windows
if(MSVC)
  # Target Windows 7 and higher
  add_compile_definitions(_WIN32_WINNT=_WIN32_WINNT_WIN7)

  # Boost tries to use auto linking (i.e. #pragma lib in headers) to tell
  # the compiler what to link to. This does not work properly on
  # Widnows/MSVC.
  add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

