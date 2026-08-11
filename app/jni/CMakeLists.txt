cmake_minimum_required(VERSION 3.10.2)
project(DiviDead C)

set(ANDROID TRUE CACHE BOOL "" FORCE)
set(SDL_STATIC TRUE CACHE BOOL "" FORCE)

# SDL2 subprojects - must be cloned into app/jni/SDL/
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/SDL/CMakeLists.txt")
    add_subdirectory(SDL/SDL)
    add_subdirectory(SDL/SDL_image)
    add_subdirectory(SDL/SDL_mixer)
    add_subdirectory(SDL/SDL_ttf)
    set(SDL2_LIBS SDL2 SDL2_image SDL2_mixer SDL2_ttf)
    set(SDL2_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/SDL/SDL/include)
else()
    message(FATAL_ERROR "SDL2 not found! Clone SDL2 libraries into app/jni/SDL/:
  cd app/jni
  mkdir -p SDL && cd SDL
  git clone --depth 1 --branch SDL2 https://github.com/libsdl-org/SDL.git
  git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
  git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
  git clone --depth 1 --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
")
endif()

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
    src/src/roq/dreamroqlib.c
    src/src/roq/sdl-dreamroq.c
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

target_include_directories(dividead PRIVATE
    src/src
    src/RES
    src/src/roq
    ${SDL2_INCLUDES}
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
    z
)
