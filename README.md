# Divi-Dead Android Port

Native Android port of the Divi-Dead visual novel engine, built with SDL2.
Includes touch gesture controls, UTF-8 rendering (Cyrillic support), and
all necessary launcher icons.

## Features

- **Full game engine** — from [gameblabla/divi-dead](https://github.com/gameblabla/divi-dead) with patches:
  - UTF-8 rendering fix (`TTF_RenderUTF8_Shaded` instead of `TTF_RenderText_Shaded`)
  - Fixed 640×480 resolution (matches original game design, frame renders correctly)
  - `HOME_DIRECTORY` + `GAME_HOME_DIRECTORY` for proper file path handling
  - OGG music + ROQ video enabled
- **Touch gesture controls** (`src/touch_input.c`):
  - **Tap** → Select/Confirm (K_A)
  - **Swipe up** → Open main menu (K_L)
  - **Swipe down** → Open gallery menu (K_R)
  - **Swipe left/right** → Navigate (K_LEFT/K_RIGHT)
  - **Long press (500ms)** → Cancel/Back (K_B)
- **Cyrillic font** — DejaVu Sans Bold subset (28 KB) in `fonts/`
- **Launcher icons** — all densities + adaptive icons (Android 8+)

## Build requirements

1. **Android Studio** 4.0+ with **NDK** r25+
2. **CMake** 3.10.2+ (bundled with Android SDK)
3. **SDL2** source libraries (see setup below)

## Setup

### 1. Clone SDL2 libraries

```bash
cd app/jni
mkdir -p SDL && cd SDL
git clone --depth 1 --branch SDL2 https://github.com/libsdl-org/SDL.git
git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
git clone --depth 1 --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
```

### 2. Copy SDLActivity.java

```bash
mkdir -p app/src/main/java/org/libsdl/app
cp app/jni/SDL/SDL/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java \
   app/src/main/java/org/libsdl/app/
```

### 3. Add game assets

```bash
./populate_assets.sh /path/to/your/dividead-folder
```

This copies `SG.DL1`, `WV.DL1`, `LANG/ENGLISH.TXT`, `OGG/`, `CS_ROGO.MPG`
into `app/src/main/assets/`.

### 4. Build

Open in Android Studio → Run, or:
```bash
./gradlew assembleDebug
```

## Source patches applied

All patches are in `app/jni/src/src/`:

| File | Patch |
|------|-------|
| `text.c` | `TTF_RenderText_Shaded` → `TTF_RenderUTF8_Shaded`, `TTF_SizeText` → `TTF_SizeUTF8` |
| `main.c` | `TTF_RenderText_Shaded` → `TTF_RenderUTF8_Shaded` + touch event handling |
| `credit.c` | `TTF_RenderText_Shaded` → `TTF_RenderUTF8_Shaded` |
| `platform.h` | `__ANDROID__` block + 640×480 fixed resolution + `HOME_DIRECTORY` |
| `main.h` | Forward declaration for `text_at()` |
| `sjis_table.c` | `#include <stdlib.h>` for `bsearch()` |
| `touch_input.c` | **NEW** — touch gesture handling (tap/swipe/long press) |

## Translation tools

In `tools/`:

- `ab_translator.py` — extract/verify/patch `.AB` script files
- `repack_sg.py` — repack patched `.AB` back into `SG.DL1`

```bash
# Extract strings from a script:
python tools/ab_translator.py extract AASTART.AB -o aastart.patch

# Edit aastart.patch — fill in translations after >

# Verify:
python tools/ab_translator.py verify AASTART.AB aastart.patch

# Apply:
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB

# Repack into SG.DL1:
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

## Project structure

```
├── app/
│   ├── build.gradle              # Gradle config
│   ├── jni/
│   │   ├── CMakeLists.txt        # NDK CMake build
│   │   └── src/
│   │       ├── src/              # Engine source (patched)
│   │       │   ├── touch_input.c # Touch gestures
│   │       │   ├── platform.h    # Android config (640×480)
│   │       │   └── ...
│   │       └── RES/              # Compiled resources (.c files)
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── res/                  # Launcher icons (all densities)
│       └── assets/               # Game data (populated by script)
├── fonts/                        # Cyrillic-capable fonts
├── tools/                        # Translation tools
├── populate_assets.sh            # Asset copier script
├── build.gradle
├── settings.gradle
└── gradle.properties
```

## License

Engine source: gameblabla's fork of soywiz's Divi-Dead interpreter.
Copyright status unknown (original game by C's ware, 1998).
SDL2 libraries under their respective licenses (zlib, LGPL).
For personal use only.
