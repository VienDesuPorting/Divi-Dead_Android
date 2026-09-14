# Divi-Dead — Android Port

A native Android port of the **Divi-Dead** visual novel (Leaf, 1998). Built on SDL2 with an OpenGL ES 2.0 renderer, a self-contained MPEG-1 video player (pl_mpeg), and a custom touch gesture system. The engine's UI strings are hardcoded in C as English defaults; a translation file (`LANG/*.TXT`) can override them later if needed.

- **minSdk 24**, **targetSdk 35**
- ABIs: `arm64-v8a`, `armeabi-v7a`
- Package: `su.viende.dividead`

---

## Contents

- [Overview](#overview)
- [Installation](#installation)
- [Project structure](#project-structure)
- [Rendering](#rendering)
- [Video playback](#video-playback)
- [Touch gestures](#touch-gestures)
- [Asset extraction](#asset-extraction)
- [Engine patches](#engine-patches)
- [Localization](#localization)
- [Build requirements](#build-requirements)
- [Translation tools](#translation-tools)
- [Troubleshooting](#troubleshooting)
- [Credits & license](#credits--license)

---

## Overview

The engine is derived from gameblabla's fork of soywiz's SDL 1.2 interpreter, reworked for SDL2 and adapted for Android. The original PC archives (`SG.DL1` ~112 MB and `WV.DL1` ~315 MB, both LZ77-compressed PAK files) are bundled inside the APK and unpacked to internal storage on first launch.

| Component | Approach |
|-----------|----------|
| Window / GL context | `SDL_GL_CreateContext` + OpenGL ES 2.0 |
| Video | pl_mpeg (pure C, MPEG-1 + MP2) |
| Touch input | Custom gesture detector → SDL key events |
| Audio | `SDL_mixer` (OGG Vorbis) |
| Fonts | `SDL_ttf` with `TTF_RenderUTF8_Shaded` |
| Saves | `/data/data/su.viende.dividead/files/.dividead/` |

---

## Installation

```bash
git clone https://github.com/christopher-vn/Divi-dead_android.git
cd Divi-dead_android
```

No `--recursive` flag — SDL2, SDL_image, SDL_mixer, SDL_ttf are fetched automatically by CMake `FetchContent`.

### Drop in the game assets

You need the original 1998 PC version of Divi-Dead. Copy its files into the project's `assets/` folder with the included helper:

```bash
./populate_assets.sh /path/to/your/dividead-pc-install
```

This copies `SG.DL1`, `WV.DL1`, `OGG/*.OGG`, and `CS_ROGO.MPG` into `app/src/main/assets/`. If `OPEN.AVI` is present and `ffmpeg` is installed, the helper also converts it to `OPEN.MPG` (MPEG-1) — see [Video playback](#video-playback). The helper also tries to copy `LANG/ENGLISH.TXT` if present, but it's optional — see [Localization](#localization).

#### Music: MIDI → OGG conversion

The original PC version ships background music as `.MID` files. `SDL_mixer` on Android doesn't include a MIDI synthesizer, so the engine reads OGG files instead. The expected filenames are `<original>.MID.OGG` — that is, the original MIDI filename with `.OGG` appended (e.g. `OPENING.MID` → `OPENING.MID.OGG`, `BGM_1.MID` → `BGM_1.MID.OGG`).

If you already have the PSP version of the game, you can just copy its pre-converted `OGG/` folder. If you're starting from the PC version, convert the MIDIs with ffmpeg (requires a working TiMidity + soundfont setup) or fluidsynth:

```bash
# ffmpeg — simple, but quality depends on bundled soundfont
cd /path/to/your/dividead-pc-install/MIDI
for f in *.MID; do
    ffmpeg -i "$f" -c:a libvorbis -q:a 4 "${f}.OGG"
done
# → produces OPENING.MID.OGG, BGM_1.MID.OGG, ...
# Move them into OGG/ before running populate_assets.sh
mkdir -p ../OGG && mv *.MID.OGG ../OGG/
```

```bash
# fluidsynth — better quality, requires a soundfont (.sf2)
cd /path/to/your/dividead-pc-install/MIDI
SF=/path/to/GeneralUser.sf2
for f in *.MID; do
    fluidsynth -ni -g 0.5 "$SF" "$f" -F "${f}.OGG"
done
mkdir -p ../OGG && mv *.MID.OGG ../OGG/
```

Either way, the result should be an `OGG/` folder with files named `*.MID.OGG` — `populate_assets.sh` will pick it up.

### Build

Open in Android Studio Narwhal (2025.1.1) or later and press Run, or:

```bash
./gradlew assembleDebug
```

APK output: `app/build/outputs/apk/debug/app-debug.apk`

> First build downloads ~50 MB of SDL2 source via CMake `FetchContent`. Cached in `~/.gradle/cxx/` for subsequent builds.

---

## Project structure

```
Divi-dead_android/
├── app/
│   ├── build.gradle                          # AGP 8.7.2 config
│   ├── jni/
│   │   ├── CMakeLists.txt                    # FetchContent for SDL2 stack
│   │   └── src/
│   │       ├── src/                          # Engine C sources (patched)
│   │       │   ├── main.c                    # Main loop + Android entry
│   │       │   ├── text.c                    # UTF-8 text rendering
│   │       │   ├── menus.c                   # Title / options / save-load
│   │       │   ├── script.c                  # In-game script VM
│   │       │   ├── vfs.c                     # VFS layer over DL1 archives
│   │       │   ├── images.c                  # LZ image decoder + cache
│   │       │   ├── audio.c                   # Music / SFX / voice
│   │       │   ├── touch_input.c             # Touch gesture detector
│   │       │   ├── android_gl_render.c       # OpenGL ES 2.0 renderer
│   │       │   ├── android_plmpeg.c           # MPEG-1 video player (pl_mpeg + SDL_Audio)
│   │       │   ├── android_asset_extract.c   # First-launch asset unpacker
│   │       │   ├── android_log.c             # stdout/stderr → logcat
│   │       │   ├── lz_decompress_arm.c       # ARM-optimized LZ77
│   │       │   ├── movie.c                   # MOVIE_PLAY dispatcher
│   │       │   └── ...
│   │       ├── RES/                          # Compiled-in resources (.c blobs)
│   │       └── include/SDL/                  # Wrapper mapping SDL/ → SDL2/
│   │           ├── sdl12_compat.h            # SDL 1.2 → 2.0 shim
│   │           └── SDL_*.h                   # Original SDL 1.2 headers
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/
│       │   ├── su/viende/dividead/
│       │   │   ├── SplashActivity.java       # Splash + social buttons
│       │   │   └── DiviDeadActivity.java     # SDLActivity subclass
│       │   └── org/libsdl/app/               # SDL2 Java backend
│       ├── res/                              # Launcher icons, drawables
│       └── assets/                           # Game data (SG.DL1, WV.DL1, ...)
├── fonts/                                    # Cyrillic-capable TTFs
├── tools/
│   ├── ab_translator.py                      # .AB script extractor / patcher
│   └── repack_sg.py                          # Repack patched .AB into SG.DL1
└── gradlew
```

---

## Rendering

The port bypasses `SDL_Renderer` entirely. `SDL_Renderer` does not work reliably on Android with OpenGL ES backends across all GPU vendors (Adreno, Mali, PowerVR), so we use raw OpenGL ES 2.0 through `SDL_GL_*` instead.

**File:** `app/jni/src/src/android_gl_render.c`

### Init

```c
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
gl_context = SDL_GL_CreateContext(window);
```

A 640×480 RGBA texture is allocated once. The engine's `SDL_Surface` (640×480, 32 bpp) is uploaded into it each frame.

### Shaders

```glsl
// Vertex
attribute vec2 a_position;
attribute vec2 a_texcoord;
varying vec2 v_texcoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
}

// Fragment
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D u_texture;
void main() {
    gl_FragColor = texture2D(u_texture, v_texcoord).bgra;
}
```

The `.bgra` swizzle in the fragment shader is needed because `GL_BGRA_EXT` is not supported on Adreno and Mali GPUs. We read `.rgba` and swizzle in-shader instead.

### Per-frame

1. `SDL_LockSurface(screen)` — pause engine access to the pixel buffer.
2. `glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, screen->pixels)` — incremental upload.
3. Compute letterbox viewport:
   ```c
   double scale = min(win_w / 640.0, win_h / 480.0);
   double qw = (640.0 * scale) / win_w;
   double qh = (480.0 * scale) / win_h;
   ```
4. One `GL_TRIANGLE_STRIP` (4 vertices) covering the letterboxed quad.
5. `SDL_GL_SwapWindow(window)`.

One GL draw call per frame. No depth, no stencil, no blending — the engine composites everything onto the CPU-side `SDL_Surface` before upload.

### Partial updates

For dirty-rect updates during scene transitions, `android_gl_render_rect()` uploads only the changed sub-rectangle. Rows are copied into a contiguous temp buffer (because `SDL_Surface->pitch` may include padding) before calling `glTexSubImage2D` with `(rect->x, rect->y, rect->w, rect->h)`. Avoids a full 640×480 upload per transition step.

---

## Video playback

Divi-Dead's PC version ships two videos: `CS_ROGO.MPG` (MPEG-1, opening studio logo) and `OPEN.AVI` (opening cinematic).

The original engine supported three video backends — SMPEG (C++), Dreamcast ROQ, and a Java `MediaPlayer` bridge — all of which had problems on Android. SMPEG and ROQ were removed from this port early on. The Java `MediaPlayer` approach was unreliable across devices: on some SoCs (e.g. Nothing Phone 3A with Adreno) `MediaPlayer.prepare()` fails with `error (1, -2147483648)` for MPEG-1, and on Motorola Moto G60s videos simply don't play.

This port uses [pl_mpeg](https://github.com/phoboslab/pl_mpeg) — a pure C MPEG-1 video + MP2 audio decoder with no platform dependencies. It works identically across all Android devices since the decoding happens entirely in-process.

**Files:**
- `app/jni/src/src/plmpeg/pl_mpeg.h` — pl_mpeg library (header-only, MIT license)
- `app/jni/src/src/android_plmpeg.c` — `android_play_video()` implementation
- `app/jni/src/src/movie.c` — `MOVIE_PLAY()` dispatcher

### How it works

`android_play_video(path, skip)` in `android_plmpeg.c`:

1. Opens the file with `plm_create_with_filename()`.
2. Allocates a RGBA buffer sized to the video's native dimensions.
3. Installs a video decode callback and an audio decode callback on the pl_mpeg instance.
4. Opens an `SDL_AudioDevice` at the file's sample rate (usually 44100 Hz, stereo, S16).
5. Enters a loop calling `plm_decode(plm, delta_time)` with wall-clock deltas. pl_mpeg internally decides which video frames and audio chunks to emit and invokes the callbacks.
6. **Video callback** — `plm_frame_to_rgba()` converts the Y/Cb/Cr planes to RGBA on the CPU, wraps the buffer in a temporary `SDL_Surface`, and stretch-blits it onto the engine's 640×480 screen surface. The screen is then uploaded to the GL texture via `android_gl_render()`.
7. **Audio callback** — pl_mpeg emits 1152-sample float frames (interleaved L/R, range −1.0 to 1.0). These are appended to a lock-free ring buffer. The SDL_Audio callback pulls from the ring, converts float → S16 (`sample * 32767`), and copies into SDL's stream.
8. **Tap-to-skip** — `SDL_PollEvent()` is called between decode steps. A `FINGERUP`, `MOUSEBUTTONUP`, or `KEYDOWN` event (when `skip=1`) breaks the loop and the function returns `2`.

### Return codes

| Value | Meaning |
|-------|---------|
| `1` | Video played to completion |
| `2` | User skipped by tapping |
| `0` | Playback failed (file not found, decode error) |

### Format constraints

pl_mpeg decodes **MPEG-1 Program Stream** containers only (`.mpg` / `.mpeg`). It does not support AVI, MP4, MKV, or any other container.

- `CS_ROGO.MPG` — already MPEG-1 in the PC release, used as-is.
- `OPEN.AVI` — must be converted to `OPEN.MPG` before building. `populate_assets.sh` does this automatically if `ffmpeg` is installed:
  ```bash
  ffmpeg -i OPEN.AVI -c:v mpeg1video -q:v 4 -c:a mp2 -b:a 192k OPEN.MPG
  ```

If you skip the conversion, the opening video will not play — the engine logs `OPEN.MPG not found or playback failed` and continues to the title screen.

---

## Touch gestures

A custom gesture detector maps touch events to the engine's existing key-event system. The engine keeps its `keys` bitmask (`K_A`, `K_B`, `K_L`, `K_R`, `K_UP`, `K_DOWN`, ...), and `touch_input.c` synthesizes the appropriate bits from `SDL_FINGERDOWN` / `SDL_FINGERMOTION` / `SDL_FINGERUP` events.

**File:** `app/jni/src/src/touch_input.c`

### Thresholds

```c
#define SWIPE_DISTANCE   0.12f   /* 12% of screen = swipe */
#define SWIPE_MAX_TIME   400     /* must complete within 400ms */
#define TAP_MAX_TIME     250     /* quick tap = < 250ms */
#define LONG_PRESS_TIME  600     /* hold 600ms = long press */
#define TAP_DISTANCE     0.04f   /* max movement for tap = 4% */
```

### Reading (in-game text)

| Gesture | Action | Key |
|---------|--------|-----|
| Tap anywhere | Advance text | `K_A` |
| Swipe right → | Open in-game menu | `K_L` |
| Swipe left ← | Back / cancel | `K_B` |
| Swipe up ↑ | Navigate up | `K_UP` |
| Swipe down ↓ | Navigate down | `K_DOWN` |
| Long press (600 ms) | Open gallery / extra menu | `K_R` |

### Menu (title / options)

| Gesture | Action | Key |
|---------|--------|-----|
| Tap on menu item | Select that item directly | — |
| Tap elsewhere | Select highlighted item | `K_A` |
| Swipe up / down | Navigate items | `K_UP` / `K_DOWN` |
| Swipe left ← | Cancel / back | `K_B` |
| Swipe right → | Confirm | `K_A` |
| Long press | Open gallery | `K_R` |

### Choices (in-game multiple choice)

| Gesture | Action | Key |
|---------|--------|-----|
| Tap on choice | Select that choice | — |
| Swipe up / down | Navigate choices | `K_UP` / `K_DOWN` |
| Swipe right → | Confirm | `K_A` |
| Swipe left ← | Cancel | `K_B` |

### Tap-to-select (letterbox-aware)

`TOUCH_GET_MENU_ITEM(touch_x, touch_y, screen_w, screen_h)` converts normalized touch coordinates (0.0–1.0) to a menu item index. The conversion accounts for letterbox offset because the engine runs at 640×480 while the actual window can be any size:

```c
SDL_GetWindowSize(g_sdl_window, &real_w, &real_h);
double scale = min(real_w / 640.0, real_h / 480.0);
int game_w = (int)(640 * scale);
int game_h = (int)(480 * scale);
int offset_x = (real_w - game_w) / 2;
int offset_y = (real_h - game_h) / 2;

int game_x = (screen_x - offset_x) * 640 / game_w;
int game_y = (screen_y - offset_y) * 480 / game_h;
return (game_y - menu_geom.y) / menu_geom.item_h;
```

The menu's layout geometry is published by the engine via `TOUCH_SET_MENU_GEOMETRY(x, y, item_h, count)` when a menu is shown, and cleared via `TOUCH_CLEAR_MENU_GEOMETRY()` when it closes.

---

## Asset extraction

Android's `AssetManager` is slow for large PAK files because every `SDL_RWFromFile` call goes through JNI. To work around this, the port extracts all assets to the app's internal storage (`/data/data/su.viende.dividead/files/`) on first launch and reads from the filesystem thereafter.

**File:** `app/jni/src/src/android_asset_extract.c`

### Extraction logic

1. Get the `AssetManager` via `SDL_AndroidGetActivity()` → `Activity.getAssets()` → `AAssetManager_fromJava()`.
2. Create subdirectories `LANG/` and `OGG/` under the internal storage root.
3. Iterate the file list:
   ```
   SG.DL1, WV.DL1, CS_ROGO.MPG, OPEN.MPG, CLICK.WAV, ICMP.DAT,
   LANG/ENGLISH.TXT,
   OGG/OPENING.MID.OGG, OGG/BGM_1.MID.OGG ... OGG/OUTSIDE.MID.OGG
   ```
   (`LANG/ENGLISH.TXT` is optional — see [Localization](#localization). The `OGG/*.MID.OGG` files are pre-converted from the original MIDI — see [Music: MIDI → OGG conversion](#music-midi--ogg-conversion). `OPEN.MPG` is the converted opening video — see [Video playback](#video-playback).)
4. For each file: if it already exists in internal storage, skip it. Otherwise open it via `AAssetManager_open(..., AASSET_MODE_STREAMING)` and stream-copy to the destination with a 64 KB buffer.
5. Write a `.extracted` marker file when done. Subsequent launches short-circuit.

### Path resolution

The engine calls `android_get_data_path("SG.DL1")` instead of opening the asset directly. The function returns the full path in internal storage (e.g. `/data/data/su.viende.dividead/files/SG.DL1`), or `NULL` if the file isn't there yet.

---

## Engine patches

Derived from gameblabla/soywiz SDL 1.2 engine. Changes that adapt it for SDL2 + Android:

| File | Patch |
|------|-------|
| `text.c` | `TTF_RenderText_Shaded` → `TTF_RenderUTF8_Shaded` (Cyrillic glyphs render correctly) |
| `text.c` | `TTF_SizeText` → `TTF_SizeUTF8` |
| `main.c` | `SDL_main` entry + Android event loop integration |
| `main.c` | Touch event routing to `TOUCH_HANDLE_EVENT` |
| `credit.c` | `TTF_RenderUTF8_Shaded` for the credits roll |
| `platform.h` | `__ANDROID__` block: 640×480 fixed resolution + `HOME_DIRECTORY` + `GAME_HOME_DIRECTORY` |
| `main.h` | Forward declaration for `text_at()` |
| `main.h` | `LANGUAGE_DEFAULT "ENGLISH"` (multi-language menu removed) |
| `sjis_table.c` | `#include <stdlib.h>` (missing on modern NDK) |
| `sdl12_compat.h` | **new** — shim macros for SDL 1.2 API removed in SDL 2.0 |
| `touch_input.c` | **new** — gesture detector |
| `android_gl_render.c` | **new** — OpenGL ES 2.0 renderer |
| `android_plmpeg.c` | **new** — MPEG-1 video player using pl_mpeg (replaces Java MediaPlayer) |
| `android_asset_extract.c` | **new** — first-launch unpacker |
| `android_log.c` | **new** — redirects `stdout` / `stderr` to logcat |
| `lz_decompress_arm.c` | **new** — ARM-optimized LZ77 decompressor |
| `movie.c` | `MOVIE_PLAY` dispatches to `android_play_video()` on Android |
| `menus.c` | Removed non-English entries from `main_menu_langs[]` (only ENGLISH.TXT is shipped) |

### SDL 1.2 → 2.0 shim

`sdl12_compat.h` provides macro aliases for SDL 1.2 API calls removed or renamed in SDL 2.0:

- `SDL_GetKeyState` → `SDL_GetKeyboardState` + key-index translation
- `SDL_VideoModeOK`, `SDL_SetVideoMode` → no-ops (use `SDL_CreateWindow`)
- `SDL_WM_SetCaption`, `SDL_WM_GrabInput` → no-ops
- `SDL_GetAppState` → synthesized from focus events

The engine code keeps calling the old 1.2 names; the shim redirects them to the 2.0 equivalents.

### LZ77 decompressor

The original `vfs.c` decompressor processed one byte at a time with a `while` loop over the 8 control bits. The new `lz_decompress_arm.c` keeps the same LZ77 format (4096-byte ring buffer, initial write position 0xFEE, 12-bit position + 4-bit length+3 match encoding) but adds a fast path:

```c
if (len <= 8 &&
    pos + len <= 0x1000 && rinp + len <= 0x1000 &&
    dist >= len) {
    // Batch copy — no wrap, no overlap
    for (uint32_t i = 0; i < len; i++) {
        output[i] = lz_ring[pos + i];
        lz_ring[rinp + i] = lz_ring[pos + i];
    }
} else {
    // Slow path — handle wrap and overlap
    while (len--) { ... }
}
```

The fast path covers ~80% of matches in typical scene data and skips the per-byte ring-index wrap check.

---

## Localization

The engine's 12 UI strings (START, SAVE, LOAD, OPTIONS, EXIT, gallery-percentage format, screenshot label, etc.) are hardcoded as English defaults in `main.c`:

```c
char lang_texts[12][0x30] = {
    "ENGLISH.DL1",
    "START", "SAVE", "LOAD", "EXIT",
    "%.1f%% GALLERY",
    "SAVE IMAGE",
    "Start new game?",
    "OPTIONS",
    "voice", "music", "No data"
};
```

At startup, `lang_init()` tries to open `LANG/<LANGUAGE>.TXT` (where `LANGUAGE` defaults to `"ENGLISH"`). If the file is present, it overrides those 12 defaults line-by-line. If the file is missing, the engine keeps the C-source defaults — so the file is **optional**.

The text-rendering pipeline is UTF-8 end to end (`TTF_SizeUTF8` for measurement, `TTF_RenderUTF8_Shaded` for rasterization), so dropping in a translated `LANG/ENGLISH.TXT` with Cyrillic content works without code changes. Use Unix line endings (`\n`, not `\r\n`); the engine strips `\r` but does not transcode encodings.

In-game dialogue and script strings live inside `SG.DL1` as `.AB` files — not in `LANG/*.TXT`. Those need the tools in `tools/` to translate (see [Translation tools](#translation-tools)).

The non-English entries (`JAPANESE`, `GERMAN`, `FRENCH`, `SPANISH`, `ITALIAN`) have been removed from the language menu in `menus.c`. The engine defaults to `LANGUAGE_DEFAULT "ENGLISH"` (defined in `main.h`), so `LANG/ENGLISH.TXT` is the only override file the engine will look for.

To add another language: drop its `.TXT` into `assets/LANG/`, add the language name back to the `main_menu_langs[]` table in `menus.c`, and set `LANGUAGE_DEFAULT` in `main.h` to that name.

---

## Build requirements

| Component | Version |
|-----------|---------|
| Android Studio | Narwhal 2025.1.1+ (or JDK 17 + SDK + NDK on CLI) |
| JDK | 17 |
| Android Gradle Plugin | 8.7.2 (declared in `app/build.gradle`) |
| Gradle | 8.11.1 (wrapper included) |
| Android NDK | r25 or newer (tested with r27) |
| CMake | 3.22.1+ (bundled with Android SDK) |
| `compileSdk` / `targetSdk` | 35 |
| `minSdk` | 24 |

**Why minSdk 24?** The 32-bit `armeabi-v7a` build uses `ftello` / `fseeko` for the streaming ring buffer in `ringread.c`. These functions are only available in bionic libc starting from API level 24.

### ABIs

- `arm64-v8a` — primary target, modern 64-bit devices
- `armeabi-v7a` — legacy 32-bit devices (Android 7.0+)

x86 and x86-64 are not supported.

---

## Translation tools

For in-game dialogue and script strings (which live inside `SG.DL1` as `.AB` files, not in `LANG/*.TXT`), the project ships two Python helpers in `tools/`:

```bash
# Extract strings from a .AB script file
python tools/ab_translator.py extract AASTART.AB -o aastart.patch

# Edit aastart.patch — fill in Russian translations after each > line

# Patch the .AB file with the translated strings
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB

# Repack the patched .AB back into SG.DL1
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

Both scripts are pure Python 3 with no third-party dependencies.

---

## Troubleshooting

### "SDL2 not found!" CMake error

Should not happen — CMake `FetchContent` downloads SDL2 automatically. If you see this:

- Verify network access during the configure step.
- Check `~/.gradle/cxx/` — the cached download lives here.
- Manually clone the SDL2 stack as a fallback:
  ```bash
  cd app/jni
  mkdir -p SDL && cd SDL
  git clone --branch SDL2 https://github.com/libsdl-org/SDL.git
  git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
  git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
  git clone --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
  ```

### Gradle sync fails

- Confirm Gradle 8.11.1 (the wrapper enforces this — `./gradlew --version`).
- If Android Studio offers to upgrade AGP, accept.
- If you see duplicate Kotlin stdlib class errors, the `constraints` and `resolutionStrategy` blocks in `app/build.gradle` already force Kotlin 1.8.22 — don't remove them.

### App crashes on launch

```bash
adb logcat -s SDL DiviDead
```

Common causes:

- **Missing assets** — `SG.DL1` or `WV.DL1` not in `app/src/main/assets/`. Re-run `./populate_assets.sh`.
- **Out of memory** — `WV.DL1` is 315 MB and gets unpacked to internal storage on first launch. `largeHeap="true"` is set in the manifest; if the device still OOMs, free up internal storage and retry.
- **GL context creation failed** — look for `"GL: context failed"` in logcat. Usually means the device's OpenGL ES 2.0 driver is broken.

### First launch takes a long time

Normal — the app is unpacking ~430 MB of assets from the APK into internal storage. Subsequent launches are fast (the `.extracted` marker short-circuits the unpacker).

### Russian text renders as boxes

Make sure your translated `LANG/ENGLISH.TXT` is encoded as UTF-8 (not Windows-1251) and uses Unix line endings (`\n`, not `\r\n`). The engine strips `\r` from the end of each line but doesn't transcode encodings.

---

## Credits & license

- **Engine source:** gameblabla's fork of soywiz's Divi-Dead interpreter — released for personal use only.
- **SDL2, SDL_image, SDL_mixer, SDL_ttf:** zlib license.
- **Android port:** © VienDesu! Porting Team.

This project is for **personal use only**. The original Divi-Dead game data (`SG.DL1`, `WV.DL1`, videos, music) is copyrighted by Leaf/AQUAPLUS and is **not** included in this repository — you must supply your own legally-obtained copy.

The repository contains only the engine source code (with the patches and new modules described above), the build system, translation tooling, and launcher icons. No game assets, no copyrighted dialogue, no copyrighted artwork.
