set(BUILD_TESTING OFF)
set(NONET ON)
set(ASAN OFF)
set(UBSAN OFF)

set(USE_SDL1 ON)
set(SDL1_VIDEO_MODE_BPP 8)
set(SDL1_VIDEO_MODE_FLAGS SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_HWSURFACE|SDL_HWPALETTE)
set(DEFAULT_WIDTH 640)
set(DEFAULT_HEIGHT 480)
set(DEVILUTIONX_GAMEPAD_TYPE Nintendo)

set(NOSOUND ON)
set(DEVILUTIONX_STATIC_ZLIB ON)
set(UNPACKED_SAVES ON)
set(DEVILUTIONX_SYSTEM_LIBFMT OFF)
set(DEVILUTIONX_STATIC_LUA ON)
set(DEVILUTIONX_DISABLE_STRIP ON)

set(DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/data/")
set(BUILD_ASSETS_MPQ OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/threads-stub")
list(APPEND DEVILUTIONX_PLATFORM_COMPILE_DEFINITIONS __DREAMCAST__)
add_compile_options(-fpermissive)

#SDL Joystick hat mapping (D-pad)
set(JOY_HAT_DPAD_UP_HAT 0)
set(JOY_HAT_DPAD_RIGHT_HAT 0)
set(JOY_HAT_DPAD_DOWN_HAT 0)
set(JOY_HAT_DPAD_LEFT_HAT 0)
set(JOY_HAT_DPAD_UP 1)
set(JOY_HAT_DPAD_RIGHT 2)
set(JOY_HAT_DPAD_DOWN 4)
set(JOY_HAT_DPAD_LEFT 8)

#SDL Joystick button mapping
set(JOY_BUTTON_A 2)
set(JOY_BUTTON_B 1)
set(JOY_BUTTON_X 5)
set(JOY_BUTTON_Y 6)

set(JOY_BUTTON_START 3)

#GPF SDL files
set(SDL_INCLUDE_DIR /usr/include/SDL/)
set(SDL_LIBRARY /usr/lib/libSDL.a)

add_compile_options(-flto=none)
