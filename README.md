# Divi-Dead — Android Port

A native Android port of the **Divi-Dead** visual novel (Leaf, 1998). Built on SDL2 with an OpenGL ES 2.0 renderer, a self-contained MPEG-1 video player (pl_mpeg), and a custom touch gesture system.

- **minSdk 24**, **targetSdk 35**
- ABIs: `arm64-v8a`, `armeabi-v7a`
- Package: `su.viende.dividead`

## Links

- **Source engine:** [gameblabla/divi-dead](https://github.com/gameblabla/divi-dead) — fork of soywiz's SDL 1.2 Divi-Dead interpreter
- **Video decoder:** [phoboslab/pl_mpeg](https://github.com/phoboslab/pl_mpeg) — single-file MPEG-1 + MP2 decoder
- **VienDesu! Porting Team:**
  - Telegram (EN): https://t.me/visual_novels_android_eng
  - Telegram (RU): https://t.me/visual_novels_for_android
  - YouTube: https://www.youtube.com/@viendesu
  - Web: https://viende.su

---

## Installation

### 1. Clone

```bash
git clone https://github.com/VienDesuPorting/Divi-Dead_Android.git
cd Divi-Dead_Android
```

No `--recursive` flag — SDL2, SDL_image, SDL_mixer, SDL_ttf are fetched automatically by CMake `FetchContent`.

### 2. Add game assets

You need the original 1998 PC version of Divi-Dead. Copy its files into the project's `assets/` folder with the included helper:

```bash
./populate_assets.sh /path/to/your/dividead-pc-install
```

This copies `SG.DL1`, `WV.DL1`, `OGG/*.OGG`, and `CS_ROGO.MPG` into `app/src/main/assets/`. If `OPEN.AVI` is present and `ffmpeg` is installed, the helper also converts it to `OPEN.MPG` (MPEG-1) — see [Music: MIDI → OGG conversion](#music-midi--ogg-conversion) below for details.

### 3. Build

Open in Android Studio Narwhal (2025.1.1) or later and press Run, or:

```bash
./gradlew assembleDebug
```

APK output: `app/build/outputs/apk/debug/app-debug.apk`

> First build downloads ~50 MB of SDL2 source via CMake `FetchContent`. Cached in `~/.gradle/cxx/` for subsequent builds.

### Music: MIDI → OGG conversion

The original PC version ships background music as `.MID` files. `SDL_mixer` on Android doesn't include a MIDI synthesizer, so the engine reads OGG files instead. The expected filenames are `<original>.MID.OGG` (e.g. `OPENING.MID` → `OPENING.MID.OGG`).

If you already have the PSP version of the game, you can just copy its pre-converted `OGG/` folder. If you're starting from the PC version, convert the MIDIs with ffmpeg (requires a working TiMidity + soundfont setup) or fluidsynth:

```bash
# ffmpeg — simple, but quality depends on bundled soundfont
cd /path/to/your/dividead-pc-install/MIDI
for f in *.MID; do
    ffmpeg -i "$f" -c:a libvorbis -q:a 4 "${f}.OGG"
done
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

---

## Touch controls

| Gesture | Action |
|---------|--------|
| Tap | Advance text / confirm / select menu item |
| Swipe right → | Open menu / confirm |
| Swipe left ← | Back / cancel |
| Swipe up ↑ / down ↓ | Navigate menu items |
| Long press | Open gallery |

Tap-to-select works directly on menu items — tap the item you want, no need to navigate to it first.

---

## Build requirements

- Android Studio Narwhal (2025.1.1)+ or JDK 17 + SDK + NDK
- Android Gradle Plugin 8.7.2 (declared in `app/build.gradle`)
- Gradle 8.11.1 (wrapper included)
- Android NDK r25 or newer
- CMake 3.22.1+ (bundled with Android SDK)

**Why minSdk 24?** The 32-bit `armeabi-v7a` build uses `ftello` / `fseeko` for the streaming ring buffer in `ringread.c`. These functions are only available in bionic libc starting from API level 24.

---

## Translation tools

For in-game dialogue and script strings (which live inside `SG.DL1` as `.AB` files, not in `LANG/*.TXT`), the project ships two Python helpers in `tools/`:

```bash
# Extract strings from a .AB script file
python tools/ab_translator.py extract AASTART.AB -o aastart.patch

# Edit aastart.patch — fill in translations after each > line

# Patch the .AB file with the translated strings
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB

# Repack the patched .AB back into SG.DL1
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

Both scripts are pure Python 3 with no third-party dependencies.

---

## Troubleshooting

### App crashes on launch

```bash
adb logcat -s SDL DiviDead
```

Common causes:
- Missing `SG.DL1` or `WV.DL1` in `app/src/main/assets/`
- Out of memory — `WV.DL1` is 315 MB; `largeHeap="true"` is set in the manifest
- First launch is slow — the app is unpacking ~430 MB of assets to internal storage

### Russian text renders as boxes

Make sure your translated `LANG/ENGLISH.TXT` is encoded as UTF-8 (not Windows-1251) and uses Unix line endings (`\n`, not `\r\n`).

---

## Credits & license

- **Engine source:** [gameblabla/divi-dead](https://github.com/gameblabla/divi-dead) — fork of soywiz's Divi-Dead interpreter, released for personal use only.
- **Video decoder:** [phoboslab/pl_mpeg](https://github.com/phoboslab/pl_mpeg) — MIT license.
- **SDL2, SDL_image, SDL_mixer, SDL_ttf:** zlib license.
- **Android port:** © [VienDesu! Porting Team](https://viende.su).

This project is for **personal use only**. The original Divi-Dead game data (`SG.DL1`, `WV.DL1`, videos, music) is copyrighted by Leaf/AQUAPLUS and is **not** included in this repository — you must supply your own legally-obtained copy.

The repository contains only the engine source code (with the patches and new modules described above), the build system, translation tooling, and launcher icons. No game assets, no copyrighted dialogue, no copyrighted artwork.

For technical architecture details, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
