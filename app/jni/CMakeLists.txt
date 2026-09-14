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
# Disable all optional image formats to avoid cloning heavy submodules
# (libjxl, dav1d, libavif, libtiff, libwebp, etc.). We only need JPG
# (for splash) and PNG (for icons/credits). The submodule clone of
# libjxl → zlib was failing intermittently with TLS errors on GitHub.
set(SDLIMAGE_AVIF  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_BMP   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_GIF   ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_LBM   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PCX   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNM   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TGA   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XCF   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XPM   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XV    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_QOI   OFF CACHE BOOL "" FORCE)
# Build SDL_image as a static library (no .so to package)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
    # Only clone the submodules we actually need for JPG/PNG support.
    # Skipping libjxl/dav1d/libavif/libtiff/libwebp avoids the TLS errors
    # we were hitting on madler/zlib (pulled in transitively by libjxl).
    GIT_SUBMODULES "external/jpeg" "external/libpng" "external/zlib"
)
FetchContent_MakeAvailable(SDL_image)

# SDL_mixer
# Only OGG Vorbis is needed for music. Disable all other codecs to
# avoid cloning heavy submodules (libmodplug, libxmp, opusfile, flac,
# mpg123, fluidsynth, etc.).
set(SDL_MIXER_OPUS      OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_MOD       OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_MIDI      OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_FLAC      OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_MP3       OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_WAV       ON  CACHE BOOL "" FORCE)
set(SDL_MIXER_OGG       ON  CACHE BOOL "" FORCE)
set(SDL_MIXER_VORBIS    ON  CACHE BOOL "" FORCE)  # libvorbis (vendored)
set(SDL_MIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_INSTALL   OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_TESTS     OFF CACHE BOOL "" FORCE)
set(SDL_MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
    # Only clone what's needed for OGG Vorbis playback. Skip the rest
    # (mpg123, opusfile, libmodplug, libxmp, flac, fluidsynth) — they
    # were causing TLS errors during submodule clone.
    GIT_SUBMODULES "external/ogg" "external/vorbis"
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
    src/src/android_plmpeg.c
    src/src/android_gl_render.c
    src/src/lz_decompress_arm.c
    src/src/movie.c
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

add_library(dividead SHARED ${DIVIDEAD_SOURCES})

# C11 needed for <stdatomic.h> (used in android_plmpeg.c)
set_target_properties(dividead PROPERTIES C_STANDARD 11)

target_include_directories(dividead PRIVATE
    src/src
    src/RES
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
