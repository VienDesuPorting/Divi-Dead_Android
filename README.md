# Divi-Dead Android Port

Native Android port of the Divi-Dead visual novel engine, built with SDL2.
SDL2 is **automatically downloaded** during CMake configure via FetchContent.

## Quick start

### 1. Clone this repo

```bash
git clone https://github.com/christopher-vn/Divi-dead_android.git
cd Divi-dead_android
```

No `--recursive` flag needed — SDL2 is fetched automatically by CMake.

### 2. Add game assets

```bash
./populate_assets.sh /path/to/your/dividead-folder
```

Copies `SG.DL1`, `WV.DL1`, `LANG/ENGLISH.TXT`, `OGG/`, `CS_ROGO.MPG`
into `app/src/main/assets/`.

### 3. Build

Open in Android Studio → Sync → Run, or:
```bash
./gradlew assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

**Note:** The first build will take longer because CMake downloads SDL2,
SDL_image, SDL_mixer, and SDL_ttf from GitHub. Subsequent builds use the
cached downloads.

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
│   │   ├── CMakeLists.txt              # NDK CMake build (FetchContent for SDL2)
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

## How SDL2 is handled

SDL2, SDL_image, SDL_mixer, and SDL_ttf are **automatically downloaded**
by CMake using `FetchContent` during the configure step. No git submodules,
no manual cloning — just build and CMake handles the rest.

The first build downloads ~50 MB of SDL2 source code (cached in
`~/.gradle/cxx/` for subsequent builds).

## Build requirements

- Android Studio Narwhal (2025.1.1)+ or JDK 17 + SDK + NDK
- Android Gradle Plugin 8.7.2 (in build.gradle)
- Gradle 8.11.1 (wrapper included)
- NDK r25+ (you have r27 — fine)
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
This should not happen anymore — CMake downloads SDL2 automatically.
If you see this error, make sure you have network access during the
configure step. CMake caches the download in `~/.gradle/cxx/`.

### Gradle sync fails
Make sure you're using Gradle 8.11 (the wrapper handles this).
If Android Studio prompts to upgrade AGP, accept.

### CMake FetchContent download fails
If the download fails (network issues), you can manually clone SDL2:
```bash
cd app/jni
mkdir -p SDL && cd SDL
git clone --branch SDL2 https://github.com/libsdl-org/SDL.git
git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
git clone --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
```
Then CMake will use the local copies instead of downloading.

### App crashes on launch
Check logcat: `adb logcat -s SDL DiviDead`
Common issues:
- Missing `SG.DL1` or `WV.DL1` in assets/
- Out of memory (WV.DL1 is 315 MB; `largeHeap=true` is set)

## License

Engine: gameblabla's fork of soywiz's Divi-Dead interpreter.
SDL2: zlib license.
For personal use only.
