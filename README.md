# Divi-Dead Android Port

Native Android port of the Divi-Dead visual novel engine, built with SDL2.
SDL2 is included as git submodules — clone with `--recursive`.

## Quick start

### 1. Clone this repo (with submodules)

```bash
git clone --recursive https://github.com/christopher-vn/Divi-dead_android.git
cd Divi-dead_android
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### 2. Add game assets

```bash
./populate_assets.sh /path/to/your/dividead-folder
```

Copies `SG.DL1`, `WV.DL1`, `LANG/ENGLISH.TXT`, `OGG/`, `CS_ROGO.MPG`
into `app/src/main/assets/`.

### 3. Build

Open in Android Studio → Run, or:
```bash
./gradlew assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

## Features

- **Full game engine** with UTF-8 rendering (Cyrillic support)
- **Touch gesture controls**:
  - Tap → Select/Confirm
  - Swipe up → Open menu
  - Swipe down → Gallery
  - Swipe left/right → Navigate
  - Long press → Cancel
- **640×480 fixed resolution** (gothic frame renders correctly)
- **OGG music + ROQ video** support
- **Adaptive launcher icons** (all densities, Android 8+)
- **Java Activity** (DiviDeadActivity) + SDL2 Java backend

## Project structure

```
├── app/
│   ├── build.gradle                    # AGP 8.7 config
│   ├── proguard-rules.pro
│   ├── jni/
│   │   ├── CMakeLists.txt              # NDK CMake build
│   │   ├── SDL/                        # SDL2 submodules (auto-cloned)
│   │   │   ├── SDL/                    # SDL2 core
│   │   │   ├── SDL_image/              # Image loading
│   │   │   ├── SDL_mixer/              # Audio mixing
│   │   │   └── SDL_ttf/                # TrueType font rendering
│   │   └── src/
│   │       ├── src/                    # Engine source (patched)
│   │       │   ├── main.c              # + SDL_main.h for Android
│   │       │   ├── touch_input.c       # Touch gesture handling
│   │       │   ├── platform.h          # 640×480 + HOME_DIRECTORY
│   │       │   └── ...
│   │       └── RES/                    # Compiled resources
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/
│       │   ├── org/libsdl/app/         # SDL2 Java backend
│       │   └── com/dividead/android/
│       │       └── DiviDeadActivity.java
│       ├── res/                        # Launcher icons
│       └── assets/                     # Game data
├── fonts/                              # Cyrillic-capable fonts
├── tools/                              # Translation tools
├── gradle/wrapper/                     # Gradle 8.11 wrapper
├── build.gradle
├── settings.gradle
└── gradlew
```

## SDL2 submodules

SDL2 is included as 4 git submodules in `app/jni/SDL/`:

| Submodule | Branch | Purpose |
|-----------|--------|---------|
| `SDL` | `SDL2` | SDL2 core library |
| `SDL_image` | `release-2.8.x` | Image loading (BMP, PNG, JPG) |
| `SDL_mixer` | `release-2.8.x` | Audio mixing (OGG, MIDI, WAV) |
| `SDL_ttf` | `release-2.24.x` | TrueType font rendering |

These are built from source alongside the engine via CMake.

## Build requirements

- Android Studio Narwhal (2025.1.1)+ or JDK 17 + SDK + NDK
- Android Gradle Plugin 8.7.2 (in build.gradle)
- Gradle 8.11.1 (wrapper included)
- NDK r25+
- CMake 3.22.1+ (bundled with Android SDK)

## Engine patches

| File | Patch |
|------|-------|
| `text.c` | `TTF_RenderUTF8_Shaded` + `TTF_SizeUTF8` |
| `main.c` | `TTF_RenderUTF8_Shaded` + `SDL_main.h` + touch events |
| `credit.c` | `TTF_RenderUTF8_Shaded` |
| `platform.h` | `__ANDROID__` block + 640×480 + `HOME_DIRECTORY` |
| `main.h` | Forward declaration for `text_at()` |
| `sjis_table.c` | `#include <stdlib.h>` |
| `touch_input.c` | **NEW** — touch gesture handling |

## Translation tools

In `tools/`:
- `ab_translator.py` — extract/verify/patch `.AB` script files
- `repack_sg.py` — repack patched `.AB` back into `SG.DL1`

```bash
python tools/ab_translator.py extract AASTART.AB -o aastart.patch
# Edit aastart.patch — fill in translations after >
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

## Troubleshooting

### "SDL2 not found!" CMake error
Run `git submodule update --init --recursive` to clone SDL2.

### Gradle sync fails
Make sure you're using Gradle 8.11 (the wrapper handles this).
If Android Studio prompts to upgrade AGP, you can accept — it should be
backwards compatible.

### App crashes on launch
Check logcat: `adb logcat -s SDL DiviDead`
Common issues:
- Missing `SG.DL1` or `WV.DL1` in assets/
- Out of memory (WV.DL1 is 315 MB; `largeHeap=true` is set)

## License

Engine: gameblabla's fork of soywiz's Divi-Dead interpreter.
SDL2: zlib license.
For personal use only.
