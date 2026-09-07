cmake_minimum_required(VERSION 3.10.2)
project(DiviDead C)

set(ANDROID TRUE CACHE BOOL "" FORCE)
set(SDL_STATIC TRUE CACHE BOOL "" FORCE)

# ----------------------------------------------------------------------------
# FetchContent: automatically download SDL2 and friends during CMake configure
# No need for git submodules or manual cloning.
# ----------------------------------------------------------------------------

include(FetchContent)

# SDL2 core
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG SDL2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL)

# SDL_image
FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL_image)

# SDL_mixer
FetchContent_Declare(
    SDL_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL_mixer)

# SDL_ttf
FetchContent_Declare(
    SDL_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-2.24.x
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL_ttf)

set(SDL2_LIBS SDL2 SDL2_image SDL2_mixer SDL2_ttf)
set(SDL2_INCLUDES ${sdl_SOURCE_DIR}/include)

# ----------------------------------------------------------------------------
# Divi-Dead engine sources
# ----------------------------------------------------------------------------

set(DIVIDEAD_SOURCES
    src/src/main.c
    src/src/text.c
    src/src/menus.c
    src/src/script.c
    src/src/vfs.c
    src/src/images.c
    src/src/audio.c
    src/src/rects.c
    src/src/ringread.c
    src/src/system.c
    src/src/system_unix.c
    src/src/credit.c
    src/src/input.c
    src/src/zoom.c
    src/src/sjis_table.c
    src/src/touch_input.c
    src/src/android_log.c
    src/src/android_asset_extract.c
    src/src/android_video.c
    src/src/android_gl_render.c
    src/src/movie.c
# ROQ sources added conditionally below
    src/RES/FONT_TTF.c
    src/RES/FONT2_TTF.c
    src/RES/CLICK_WAV.c
    src/RES/ICMP_DAT.c
    src/RES/SPLASH_JPG.c
    src/RES/END0_PNG.c
    src/RES/END1_PNG.c
    src/RES/ROLL0_PNG.c
    src/RES/ROLL1_PNG.c
    src/RES/ROLL2_PNG.c
    src/RES/ROLL_TXT.c
)
if(NOT ANDROID)
    list(APPEND DIVIDEAD_SOURCES
        src/src/roq/dreamroqlib.c
        src/src/roq/sdl-dreamroq.c
    )
endif()

add_library(dividead SHARED ${DIVIDEAD_SOURCES})

target_include_directories(dividead PRIVATE
    src/src
    src/RES
    src/src/roq
    include                      # Wrapper: SDL/ -> SDL2/
    ${SDL2_INCLUDES}
    ${SDL2_INCLUDES}/SDL2       # For direct SDL2/ includes
)

target_compile_definitions(dividead PRIVATE
    __ANDROID__
    HOME_DIRECTORY
    GAME_HOME_DIRECTORY
    ADJUST_FONT_SIZE_DYNAMIC
    ENABLE_VIDEO_ROQ
    SJIS_SUPPORT
)

target_link_libraries(dividead
    ${SDL2_LIBS}
    log
    android
    EGL
    GLESv1_CM
    GLESv2
    z
)
