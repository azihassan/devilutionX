include(functions/FetchContent_MakeAvailableExcludeFromAll)

if(NOT WIN32 AND NOT APPLE AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL FreeBSD)
  # Enable POSIX extensions such as `readlink` and `ftruncate`.
  add_definitions(-D_POSIX_C_SOURCE=200809L)
endif()

# Disable fmt/os.h functionality.
# We do not use it and it is not supported on some systems.
set(FMT_OS OFF)

if(DEVILUTIONX_STATIC_LIBFMT)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()
include(FetchContent)
FetchContent_Declare(libfmt
  URL https://github.com/fmtlib/fmt/archive/ff9ee0461a8dd02cbaf9458aa13301aa1099c0f7.tar.gz
  URL_HASH MD5=19ff2a3abbbbdf25eff6bbc7afe539e7
)
FetchContent_MakeAvailableExcludeFromAll(libfmt)

if(DEVILUTIONX_WINDOWS_NO_WCHAR)
  target_compile_definitions(fmt PUBLIC FMT_WINDOWS_NO_WCHAR)
endif()

target_compile_definitions(fmt PUBLIC FMT_BUILTIN_TYPES=0)
