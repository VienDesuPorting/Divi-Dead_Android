# Divi-Dead Android Port

Native Android port of the Divi-Dead visual novel engine, built with SDL2.

## Features

- **Full game engine** with UTF-8 rendering (Cyrillic support)
- **Touch gesture controls** (tap, swipe, long press)
- **640×480 fixed resolution** (gothic frame renders correctly)
- **OGG music + ROQ video** support
- **Adaptive launcher icons** (all densities, Android 8+)
- **Java Activity** (DiviDeadActivity) + SDL2 Java backend

## Quick start

### 1. Clone SDL2 C libraries

```bash
cd app/jni
mkdir -p SDL && cd SDL
git clone --depth 1 --branch SDL2 https://github.com/libsdl-org/SDL.git
git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
git clone --depth 1 --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
git clone --depth 1 --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
```

### 2. Add game assets

```bash
./populate_assets.sh /path/to/your/dividead-folder
```

This copies `SG.DL1`, `WV.DL1`, `LANG/ENGLISH.TXT`, `OGG/`, `CS_ROGO.MPG`
into `app/src/main/assets/`.

### 3. Build

Open in Android Studio → Run, or from command line:
```bash
./gradlew assembleDebug
```

APK output: `app/build/outputs/apk/debug/app-debug.apk`

## Project structure

```
├── app/
│   ├── build.gradle                    # AGP 8.x config
│   ├── proguard-rules.pro
│   ├── jni/
│   │   ├── CMakeLists.txt              # NDK CMake build
│   │   └── src/
│   │       ├── src/                    # Engine source (patched)
│   │       │   ├── main.c              # + SDL_main.h for Android
│   │       │   ├── touch_input.c       # Touch gesture handling
│   │       │   ├── platform.h          # 640×480 + HOME_DIRECTORY
│   │       │   └── ...
│   │       └── RES/                    # Compiled resources
│   └── src/main/
│       ├── AndroidManifest.xml         # DiviDeadActivity entry
│       ├── java/
│       │   ├── org/libsdl/app/         # SDL2 Java backend (5 files)
│       │   │   ├── SDLActivity.java
│       │   │   ├── SDLAudioManager.java
│       │   │   ├── SDLControllerManager.java
│       │   │   ├── HIDDeviceManager.java
│       │   │   └── HIDDeviceBLESteamController.java
│       │   └── com/dividead/android/
│       │       └── DiviDeadActivity.java  # Main entry point
│       ├── res/                        # Launcher icons (all densities)
│       └── assets/                     # Game data (populated by script)
├── fonts/                              # Cyrillic-capable fonts
├── tools/                              # Translation tools
├── gradle/wrapper/                     # Gradle 8.2 wrapper
├── build.gradle                        # AGP 8.1.2
├── settings.gradle
├── gradle.properties
├── gradlew                             # Gradle wrapper script
└── populate_assets.sh
```

## Java side

### DiviDeadActivity.java

The main entry point. Extends `SDLActivity` (from SDL2) which handles:
- Loading `libdividead.so` (the native engine)
- Setting up SDL2 video/audio/input
- Calling the native `SDL_main()` function
- Android lifecycle (pause/resume/destroy)

DiviDeadActivity overrides:
- `getArguments()` → passes `["SG.DL1"]` as argv to native main()
- `getMainLibraryName()` → returns `"dividead"` (loads `libdividead.so`)

### SDL2 Java backend

The 5 files in `org/libsdl/app/` are from the SDL2 repository and provide
the Android platform glue. They're included directly (not as a dependency)
because SDL2 is built from source alongside the engine.

## Native side

### Touch gestures (`touch_input.c`)

| Gesture | Key | Action |
|---------|-----|--------|
| Tap | K_A | Select / Confirm / Advance text |
| Swipe up | K_L | Open main menu |
| Swipe down | K_R | Open gallery / extra menu |
| Swipe left | K_LEFT | Navigate left |
| Swipe right | K_RIGHT | Navigate right |
| Long press (500ms) | K_B | Cancel / Back |

### Engine patches

| File | Patch |
|------|-------|
| `text.c` | `TTF_RenderUTF8_Shaded` + `TTF_SizeUTF8` |
| `main.c` | `TTF_RenderUTF8_Shaded` + `SDL_main.h` + touch events |
| `credit.c` | `TTF_RenderUTF8_Shaded` |
| `platform.h` | `__ANDROID__` block + 640×480 + `HOME_DIRECTORY` |
| `main.h` | Forward declaration for `text_at()` |
| `sjis_table.c` | `#include <stdlib.h>` |
| `touch_input.c` | **NEW** — touch gesture handling |

## Build requirements

- Android Studio Hedgehog (2023.1.1)+ or just JDK 17 + SDK + NDK
- Android Gradle Plugin 8.1.2 (included in build.gradle)
- Gradle 8.2 (wrapper included)
- NDK r25+
- CMake 3.22.1+ (bundled with Android SDK)

## Translation tools

In `tools/`:
- `ab_translator.py` — extract/verify/patch `.AB` script files
- `repack_sg.py` — repack patched `.AB` back into `SG.DL1`

```bash
# Extract strings:
python tools/ab_translator.py extract AASTART.AB -o aastart.patch

# Edit aastart.patch — fill in translations after >

# Apply:
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB

# Repack:
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

## Troubleshooting

### "SDL2 not found!" CMake error
You forgot to clone SDL2. See step 1 above.

### Gradle sync fails with AGP version error
Make sure you're using Gradle 8.2 (the wrapper handles this automatically).
If Android Studio prompts to upgrade AGP, decline — 8.1.2 is what we need.

### Build fails with "cannot find SDL_main.h"
The `SDL_main.h` include is wrapped in `#ifdef __ANDROID__`. Make sure
`__ANDROID__` is defined in CMakeLists.txt (it is, by default).

### App crashes on launch
Check logcat: `adb logcat -s SDL DiviDead`
Common issues:
- Missing `SG.DL1` or `WV.DL1` in assets/
- Out of memory (WV.DL1 is 315 MB; `largeHeap=true` is set)

## License

Engine: gameblabla's fork of soywiz's Divi-Dead interpreter.
SDL2: zlib license.
For personal use only.
