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
# (for splash) and PNG (for icons/credits).
#
# NOTE: SDL_image 2.8.x uses SDL2IMAGE_* option names (not SDLIMAGE_*).
# Wrong names silently don't apply, and the build still tries to clone
# the submodules — manifesting as TLS errors on madler/zlib.
#
# Using SDL2IMAGE_BACKEND_STB=ON (the default) lets SDL_image decode
# JPG and PNG through the bundled stb_image — no external submodules
# needed at all. We still list the submodules we'd want as a fallback
# in case stb_image is disabled in the future.
set(SDL2IMAGE_VENDORED    OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_WIC OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_STRICT      OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF  OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JXL   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP  OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG   ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG   ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_BMP   ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_GIF   ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_LBM   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PCX   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNM   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TGA   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XCF   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XPM   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XV    OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_QOI   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SVG   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TESTS   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
    # With SDL2IMAGE_BACKEND_STB=ON and SDL2IMAGE_VENDORED=OFF, no
    # submodule clone is needed — stb_image is bundled in SDL_image's
    # own source tree. Empty GIT_SUBMODULES guards against any future
    # submodule additions breaking the build.
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(SDL_image)

# SDL_mixer
# Only OGG Vorbis is needed for music. Disable all other codecs to
# avoid cloning heavy submodules (libmodplug, libxmp, opusfile, flac,
# mpg123, fluidsynth, etc.).
#
# NOTE: SDL_mixer 2.8.x uses SDL2MIXER_* option names (not SDL_MIXER_*).
# If the names are wrong, the options silently don't apply and the
# build tries to find the submodules anyway — manifesting as a cmake
# error like "No cmake project for opus found in external/opus".
set(SDL2MIXER_VENDORED    OFF CACHE BOOL "" FORCE)  # don't use vendored deps
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS        OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD         OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI        OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC        OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3         OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME         OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVE        ON  CACHE BOOL "" FORCE)
# Use the STB vorbis decoder that's bundled inside SDL_mixer itself
# (no external submodule needed). Alternatives are "VORBISFILE"
# (needs external/libvorbis) and "TREMOR" (needs external/tremor).
set(SDL2MIXER_VORBIS      "STB" CACHE STRING "" FORCE)
set(SDL2MIXER_INSTALL     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD         OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-2.8.x
    GIT_SHALLOW TRUE
    # With SDL2MIXER_VENDORED=OFF and SDL2MIXER_VORBIS=STB, no
    # submodule clone is needed — STB vorbis is bundled in SDL_mixer's
    # own source tree. The empty GIT_SUBMODULES is a belt-and-suspenders
    # guard against any future submodule additions.
    GIT_SUBMODULES ""
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
