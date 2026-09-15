# Divi-Dead — Порт на Android

Нативный порт визуальной новеллы **Divi-Dead** (Leaf, 1998) на Android. Поверх SDL2 с GPU-рендерером на OpenGL ES 2.0, собственным MPEG-1 видеоплеером на pl_mpeg и собственной системой сенсорных жестов. UI-строки движка зашиты в C как английские дефолты; позже их можно переопределить файлом перевода (`LANG/*.TXT`).

- **minSdk 24**, **targetSdk 35**
- ABI: `arm64-v8a`, `armeabi-v7a`
- Пакет: `su.viende.dividead`

---

## Содержание

- [Обзор](#обзор)
- [Установка](#установка)
- [Структура проекта](#структура-проекта)
- [Рендеринг](#рендеринг)
- [Воспроизведение видео](#воспроизведение-видео)
- [Сенсорные жесты](#сенсорные-жесты)
- [Распаковка ассетов](#распаковка-ассетов)
- [Патчи движка](#патчи-движка)
- [Локализация](#локализация)
- [Требования к сборке](#требования-к-сборке)
- [Инструменты перевода](#инструменты-перевода)
- [Устранение неполадок](#устранение-неполадок)
- [Авторство и лицензия](#авторство-и-лицензия)

---

## Обзор

Исходники движка — производная от форка gameblabla интерпретатора soywiz на SDL 1.2, переработанного под SDL2 и адаптированного под Android. Оригинальные PC-архивы (`SG.DL1` ~112 МБ и `WV.DL1` ~315 МБ, оба — LZ77-сжатые PAK-файлы) поставляются внутри APK и распаковываются во внутреннее хранилище при первом запуске.

| Компонент | Подход |
|-----------|--------|
| Окно / GL-контекст | `SDL_GL_CreateContext` + OpenGL ES 2.0 |
| Видео | pl_mpeg (чистый C, MPEG-1 + MP2) |
| Сенсорный ввод | Собственный детектор жестов → SDL-события клавиш |
| Аудио | `SDL_mixer` (OGG Vorbis) |
| Шрифты | `SDL_ttf` с `TTF_RenderUTF8_Shaded` |
| Сейвы | `/data/data/su.viende.dividead/files/.dividead/` |

---

## Установка

```bash
git clone https://github.com/christopher-vn/Divi-dead_android.git
cd Divi-dead_android
```

Флаг `--recursive` не нужен — SDL2, SDL_image, SDL_mixer, SDL_ttf загружаются автоматически через CMake `FetchContent`.

### Положить ассеты игры

Нужна оригинальная PC-версия Divi-Dead 1998 года. Скопируйте её файлы в папку `assets/` проекта вспомогательным скриптом:

```bash
./populate_assets.sh /путь/к/dividead-pc
```

Скопирует `SG.DL1`, `WV.DL1`, `OGG/*.OGG` и `CS_ROGO.MPG` в `app/src/main/assets/`. Если есть `OPEN.AVI` и установлен `ffmpeg`, скрипт также конвертирует его в `OPEN.MPG` (MPEG-1) — см. [Воспроизведение видео](#воспроизведение-видео). Скрипт также пытается скопировать `LANG/ENGLISH.TXT`, если он есть — но файл опциональный, см. [Локализация](#локализация).

#### Музыка: конвертация MIDI → OGG

Оригинальная PC-версия хранит музыку как `.MID` файлы. В `SDL_mixer` на Android нет MIDI-синтезатора, поэтому движок читает OGG. Ожидаемые имена файлов — `<оригинал>.MID.OGG`, то есть оригинальное имя MIDI-файла с добавленным `.OGG` (например, `OPENING.MID` → `OPENING.MID.OGG`, `BGM_1.MID` → `BGM_1.MID.OGG`).

Если у вас есть PSP-версия игры — просто скопируйте её уже сконвертированную папку `OGG/`. Если стартуете с PC-версии, конвертируйте MIDI через ffmpeg (нужен рабочий TiMidity + soundfont) или fluidsynth:

```bash
# ffmpeg — проще, но качество зависит от встроенного soundfont
cd /путь/к/dividead-pc/MIDI
for f in *.MID; do
    ffmpeg -i "$f" -c:a libvorbis -q:a 4 "${f}.OGG"
done
# → получит OPENING.MID.OGG, BGM_1.MID.OGG, ...
# Положите в OGG/ перед запуском populate_assets.sh
mkdir -p ../OGG && mv *.MID.OGG ../OGG/
```

```bash
# fluidsynth — качество выше, нужен soundfont (.sf2)
cd /путь/к/dividead-pc/MIDI
SF=/путь/к/GeneralUser.sf2
for f in *.MID; do
    fluidsynth -ni -g 0.5 "$SF" "$f" -F "${f}.OGG"
done
mkdir -p ../OGG && mv *.MID.OGG ../OGG/
```

В любом случае результат — папка `OGG/` с файлами `*.MID.OGG`, которую `populate_assets.sh` подхватит.

### Сборка

Откройте проект в Android Studio Narwhal (2025.1.1) или новее и нажмите Run, или:

```bash
./gradlew assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

> Первая сборка скачивает ~50 МБ исходников SDL2 через CMake `FetchContent`. Кеш хранится в `~/.gradle/cxx/` для последующих сборок.

---

## Структура проекта

```
Divi-dead_android/
├── app/
│   ├── build.gradle                          # конфиг AGP 8.7.2
│   ├── jni/
│   │   ├── CMakeLists.txt                    # FetchContent для SDL2-стека
│   │   ├── include/SDL/                      # Обёртка, маппящая SDL/ → SDL2/
│   │   │   ├── sdl12_compat.h                # Шим SDL 1.2 → 2.0
│   │   │   └── SDL_*.h                       # Оригинальные SDL 1.2 заголовки
│   │   └── src/
│   │       ├── main.c                        # Главный цикл + Android-вход
│   │       ├── text.c                        # Рендеринг UTF-8 текста
│   │       ├── menus.c                       # Заголовок / опции / сохранения
│   │       ├── script.c                      # Внутриигровая script VM
│   │       ├── vfs.c                         # VFS над DL1-архивами
│   │       ├── images.c                      # LZ-декодер изображений + кеш
│   │       ├── audio.c                       # Музыка / SFX / озвучка
│   │       ├── touch_input.c                 # Детектор сенсорных жестов
│   │       ├── android_gl_render.c           # Рендерер OpenGL ES 2.0
│   │       ├── android_plmpeg.c              # MPEG-1 видеоплеер (pl_mpeg + SDL_Audio)
│   │       ├── android_asset_extract.c       # Распаковщик ассетов при первом запуске
│   │       ├── android_log.c                 # stdout/stderr → logcat
│   │       ├── lz_decompress_arm.c           # Оптимизированный под ARM LZ77
│   │       ├── movie.c                       # Диспетчер MOVIE_PLAY
│   │       ├── plmpeg/pl_mpeg.h              # библиотека pl_mpeg (header-only)
│   │       ├── RES/                          # Вкомпилированные ресурсы (.c blobs)
│   │       └── ...
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/
│       │   ├── su/viende/dividead/
│       │   │   ├── SplashActivity.java       # Сплэш + соц. кнопки
│       │   │   └── DiviDeadActivity.java     # Подкласс SDLActivity
│       │   └── org/libsdl/app/               # SDL2 Java-бэкенд
│       ├── res/                              # Иконки запуска, drawables
│       └── assets/                           # Игровые данные (SG.DL1, WV.DL1, ...)
├── fonts/                                    # TTF-шрифты с поддержкой кириллицы
├── tools/
│   ├── ab_translator.py                      # Экстрактор / патчер .AB скриптов
│   └── repack_sg.py                          # Перепаковка .AB в SG.DL1
└── gradlew
```

---

## Рендеринг

Порт полностью обходит `SDL_Renderer`. `SDL_Renderer` ненадёжно работает на Android с OpenGL ES-бэкендом на GPU разных вендоров (Adreno, Mali, PowerVR), поэтому вместо него используется сырой OpenGL ES 2.0 через `SDL_GL_*`.

**Файл:** `app/jni/src/android_gl_render.c`

### Замечание про ANGLE (Android 15+, Adreno 800-series)

Начиная с Android 15, Google поставляет [ANGLE](https://developer.android.com/games/develop/vulkan/overview) как стандартный OpenGL ES драйвер на некоторых устройствах — в первую очередь на GPU Adreno 800-series (Snapdragon 7s Gen 3 / 8 Elite / 8s Gen 4). ANGLE транслирует OpenGL ES вызовы в Vulkan под капотом и строже старого нативного GL-драйвера: не терпит использования невалидного GL-контекста, а `surface->pixels` может быть невалидным до `SDL_LockSurface`.

Поэтому рендерер проверяет return value `SDL_GL_MakeCurrent` перед каждым GL-вызовом. При ошибке он удаляет и переинициализирует GL-контекст (перекомпиляция шейдеров, перевыделение текстуры). На старых устройствах это no-op; на Adreno 800-series с ANGLE это предотвращает SIGSEGV, который иначе возникал бы после воспроизведения видео.

Ссылки:
- [Use Vulkan for graphics — Android developer docs](https://developer.android.com/games/develop/vulkan/overview)
- [RetroArch issue #19462 — тот же баг ANGLE с чёрным экраном на Adreno 825 / Android 16](https://github.com/libretro/RetroArch/issues/19462)

### Инициализация

```c
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
gl_context = SDL_GL_CreateContext(window);
```

Текстура 640×480 RGBA создаётся один раз. `SDL_Surface` движка (640×480, 32 bpp) загружается в неё каждый кадр.

### Шейдеры

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

Swizzle `.bgra` во фрагментном шейдере нужен, потому что `GL_BGRA_EXT` не поддерживается на GPU Adreno и Mali. Читаем как `.rgba` и свиззлим в шейдере.

### Покадровый рендер

1. `SDL_LockSurface(screen)` — приостановить доступ движка к пиксельному буферу.
2. `glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, screen->pixels)` — инкрементальная загрузка.
3. Вычислить letterbox-вьюпорт:
   ```c
   double scale = min(win_w / 640.0, win_h / 480.0);
   double qw = (640.0 * scale) / win_w;
   double qh = (480.0 * scale) / win_h;
   ```
4. Один `GL_TRIANGLE_STRIP` (4 вершины), покрывающий letterboxed-квад.
5. `SDL_GL_SwapWindow(window)`.

Один GL-вызов на кадр. Без depth, без stencil, без blending — движок компонует всё на CPU-стороне `SDL_Surface` перед загрузкой.

### Частичные обновления

Для обновления грязных прямоугольников во время переходов `android_gl_render_rect()` загружает только изменившийся под-прямоугольник. Строки копируются в смежный временный буфер (т.к. `SDL_Surface->pitch` может содержать padding) перед вызовом `glTexSubImage2D` с `(rect->x, rect->y, rect->w, rect->h)`. Избегает полной загрузки 640×480 на каждом шаге перехода.

---

## Воспроизведение видео

PC-версия Divi-Dead поставляется с двумя роликами: `CS_ROGO.MPG` (MPEG-1, логотип студии) и `OPEN.AVI` (вступительная катсцена).

Оригинальный движок поддерживал три видеобекенда — SMPEG (C++), Dreamcast ROQ и мост к Java `MediaPlayer` — все они имели проблемы на Android. SMPEG и ROQ были удалены из порта ещё на раннем этапе. Подход с Java `MediaPlayer` оказался ненадёжным между устройствами: на некоторых SoC (например, Nothing Phone 3A с Adreno) `MediaPlayer.prepare()` падает с `error (1, -2147483648)` для MPEG-1, а на Motorola Moto G60s видео просто не воспроизводится.

Этот порт использует [pl_mpeg](https://github.com/phoboslab/pl_mpeg) — чистый C-декодер MPEG-1 видео + MP2 аудио, без платформенных зависимостей. Работает одинаково на всех Android-устройствах, потому что декодирование полностью происходит в процессе приложения.

**Файлы:**
- `app/jni/src/plmpeg/pl_mpeg.h` — библиотека pl_mpeg (header-only, MIT-лицензия)
- `app/jni/src/android_plmpeg.c` — реализация `android_play_video()`
- `app/jni/src/movie.c` — диспетчер `MOVIE_PLAY()`

### Как это работает

`android_play_video(path, skip)` в `android_plmpeg.c`:

1. Открывает файл через `plm_create_with_filename()`.
2. Выделяет RGBA-буфер под нативные размеры видео.
3. Устанавливает видео- и аудиоколбэки на инстанс pl_mpeg.
4. Открывает `SDL_AudioDevice` на частоте файла (обычно 44100 Гц, стерео, S16).
5. Входит в цикл вызовов `plm_decode(plm, delta_time)` с wall-clock дельтами. pl_mpeg сам решает, какие видеокадры и аудиочанки отдать, и вызывает колбэки.
6. **Видеоколбэк** — `plm_frame_to_rgba()` конвертирует плоскости Y/Cb/Cr в RGBA на CPU, оборачивает буфер во временный `SDL_Surface` и stretch-blit-ит в экранную поверхность движка 640×480. Затем экран загружается в GL-текстуру через `android_gl_render()`.
7. **Аудиоколбэк** — pl_mpeg отдаёт 1152-семпловые float-кадры (interleaved L/R, диапазон −1.0…1.0). Они добавляются в lock-free ring buffer. Колбэк SDL_Audio читает из кольца, конвертирует float → S16 (`sample * 32767`) и копирует в поток SDL.
8. **Тап-пропуск** — между шагами декодирования вызывается `SDL_PollEvent()`. Событие `FINGERUP`, `MOUSEBUTTONUP` или `KEYDOWN` (когда `skip=1`) прерывает цикл, и функция возвращает `2`.

### Коды возврата

| Значение | Значение |
|----------|---------|
| `1` | Видео воспроизведено до конца |
| `2` | Пользователь пропустил тапом |
| `0` | Воспроизведение не удалось (нет файла, ошибка декодера) |

### Ограничения формата

pl_mpeg декодирует **только MPEG-1 Program Stream** контейнеры (`.mpg` / `.mpeg`). AVI, MP4, MKV и другие контейнеры не поддерживаются.

- `CS_ROGO.MPG` — уже MPEG-1 в PC-релизе, используется как есть.
- `OPEN.AVI` — нужно сконвертировать в `OPEN.MPG` перед сборкой. Оригинал — 480×264 @ 15 fps с моно-PCM 22050 Гц, и оба параметра вне того, что принимает MPEG-1/MP2:
  - **15 fps** нет в таблице framerate-ов MPEG-1 — форсируем 30 fps (дублирует кадры, движение выглядит плавно, т.к. источник медленный).
  - **MP2 на 22050 Гц моно** не позволяет 192 kbps — апсемплим аудио в 44100 Гц стерео.

  `populate_assets.sh` делает это автоматически, если установлен `ffmpeg`:
  ```bash
  ffmpeg -i OPEN.AVI -r 30 \
      -c:v mpeg1video -q:v 4 \
      -c:a mp2 -b:a 192k -ar 44100 -ac 2 \
      OPEN.MPG
  ```

Если конвертацию пропустить, вступительное видео не воспроизведётся — движок выведет `OPEN.MPG not found or playback failed` и продолжит к титульному экрану.

---

## Сенсорные жесты

Собственный детектор жестов преобразует сенсорные события в уже существующую в движке систему клавишных событий. Движок хранит битмаск `keys` (`K_A`, `K_B`, `K_L`, `K_R`, `K_UP`, `K_DOWN`, ...), а `touch_input.c` синтезирует соответствующие биты из событий `SDL_FINGERDOWN` / `SDL_FINGERMOTION` / `SDL_FINGERUP`.

**Файл:** `app/jni/src/touch_input.c`

### Пороги

```c
#define SWIPE_DISTANCE   0.12f   /* 12% экрана = свайп */
#define SWIPE_MAX_TIME   400     /* должен завершиться за 400 мс */
#define TAP_MAX_TIME     250     /* быстрый тап = < 250 мс */
#define LONG_PRESS_TIME  600     /* удержание 600 мс = долгий тап */
#define TAP_DISTANCE     0.04f   /* макс. движение для тапа = 4% */
```

### Чтение (внутриигровой текст)

| Жест | Действие | Клавиша |
|------|----------|---------|
| Тап в любом месте | Продвинуть текст | `K_A` |
| Свайп вправо → | Открыть внутриигровое меню | `K_L` |
| Свайп влево ← | Назад / отмена | `K_B` |
| Свайп вверх ↑ | Навигация вверх | `K_UP` |
| Свайп вниз ↓ | Навигация вниз | `K_DOWN` |
| Долгое нажатие (600 мс) | Открыть галерею / доп. меню | `K_R` |

### Меню (заголовок / опции)

| Жест | Действие | Клавиша |
|------|----------|---------|
| Тап по пункту меню | Прямой выбор этого пункта | — |
| Тап в другом месте | Выбор подсвеченного пункта | `K_A` |
| Свайп вверх / вниз | Навигация по пунктам | `K_UP` / `K_DOWN` |
| Свайп влево ← | Отмена / назад | `K_B` |
| Свайп вправо → | Подтвердить | `K_A` |
| Долгое нажатие | Открыть галерею | `K_R` |

### Выборы (внутриигровой множественный выбор)

| Жест | Действие | Клавиша |
|------|----------|---------|
| Тап по выбору | Выбрать этот вариант | — |
| Свайп вверх / вниз | Навигация по выборам | `K_UP` / `K_DOWN` |
| Свайп вправо → | Подтвердить | `K_A` |
| Свайп влево ← | Отмена | `K_B` |

### Тап-выбор (letterbox-aware)

`TOUCH_GET_MENU_ITEM(touch_x, touch_y, screen_w, screen_h)` преобразует нормализованные сенсорные координаты (0.0–1.0) в индекс пункта меню. Преобразование учитывает letterbox-смещение, т.к. движок работает на фиксированном 640×480, тогда как фактическое окно может быть любого размера:

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

Геометрия меню публикуется движком через `TOUCH_SET_MENU_GEOMETRY(x, y, item_h, count)` при показе меню и сбрасывается через `TOUCH_CLEAR_MENU_GEOMETRY()` при закрытии.

---

## Распаковка ассетов

Android `AssetManager` медленный для крупных PAK-файлов, потому что каждый вызов `SDL_RWFromFile` идёт через JNI. Чтобы обойти это, порт извлекает все ассеты во внутреннее хранилище приложения (`/data/data/su.viende.dividead/files/`) при первом запуске и затем читает с файловой системы.

**Файл:** `app/jni/src/android_asset_extract.c`

### Логика распаковки

1. Получить `AssetManager` через `SDL_AndroidGetActivity()` → `Activity.getAssets()` → `AAssetManager_fromJava()`.
2. Создать подкаталоги `LANG/` и `OGG/` под корнем внутреннего хранилища.
3. Итерировать список файлов:
   ```
   SG.DL1, WV.DL1, CS_ROGO.MPG, OPEN.MPG, CLICK.WAV, ICMP.DAT,
   LANG/ENGLISH.TXT,
   OGG/OPENING.MID.OGG, OGG/BGM_1.MID.OGG ... OGG/OUTSIDE.MID.OGG
   ```
   (`LANG/ENGLISH.TXT` опциональный — см. [Локализация](#локализация). Файлы `OGG/*.MID.OGG` предварительно сконвертированы из оригинальных MIDI — см. [Музыка: конвертация MIDI → OGG](#музыка-конвертация-midi--ogg). `OPEN.MPG` — сконвертированное вступительное видео, см. [Воспроизведение видео](#воспроизведение-видео).)
4. Для каждого файла: если уже существует во внутреннем хранилище — пропустить. Иначе открыть через `AAssetManager_open(..., AASSET_MODE_STREAMING)` и стрим-копировать в назначение с буфером 64 КБ.
5. Записать маркер `.extracted` по завершении. Последующие запуски коротко замыкают распаковку.

### Разрешение путей

Движок вызывает `android_get_data_path("SG.DL1")` вместо прямого открытия ассета. Функция возвращает полный путь во внутреннем хранилище (например, `/data/data/su.viende.dividead/files/SG.DL1`) или `NULL`, если файла ещё нет.

---

## Патчи движка

Производная от движка gameblabla/soywiz на SDL 1.2. Изменения, адаптирующие его под SDL2 + Android:

| Файл | Патч |
|------|------|
| `text.c` | `TTF_RenderText_Shaded` → `TTF_RenderUTF8_Shaded` (кириллица рендерится корректно) |
| `text.c` | `TTF_SizeText` → `TTF_SizeUTF8` |
| `main.c` | Вход `SDL_main` + интеграция Android event loop |
| `main.c` | Маршрутизация сенсорных событий в `TOUCH_HANDLE_EVENT` |
| `credit.c` | `TTF_RenderUTF8_Shaded` для титров |
| `platform.h` | Блок `__ANDROID__`: 640×480 фиксированное разрешение + `HOME_DIRECTORY` + `GAME_HOME_DIRECTORY` |
| `main.h` | Forward-декларация для `text_at()` |
| `main.h` | `LANGUAGE_DEFAULT "ENGLISH"` (мультиязычное меню удалено) |
| `sjis_table.c` | `#include <stdlib.h>` (отсутствует на современном NDK) |
| `sdl12_compat.h` | **новый** — шим-макросы для API SDL 1.2, удалённого в SDL 2.0 |
| `touch_input.c` | **новый** — детектор жестов |
| `android_gl_render.c` | **новый** — рендерер OpenGL ES 2.0 |
| `android_plmpeg.c` | **новый** — MPEG-1 видеоплеер на pl_mpeg (замена Java MediaPlayer) |
| `android_asset_extract.c` | **новый** — распаковщик при первом запуске |
| `android_log.c` | **новый** — перенаправляет `stdout` / `stderr` в logcat |
| `lz_decompress_arm.c` | **новый** — оптимизированный под ARM LZ77-декомпрессор |
| `movie.c` | `MOVIE_PLAY` диспетчеризует в `android_play_video()` на Android |
| `menus.c` | Удалены не-английские пункты из `main_menu_langs[]` (поставляется только ENGLISH.TXT) |

### Шим SDL 1.2 → 2.0

`sdl12_compat.h` предоставляет макрос-алиасы для вызовов API SDL 1.2, удалённых или переименованных в SDL 2.0:

- `SDL_GetKeyState` → `SDL_GetKeyboardState` + трансляция индексов клавиш
- `SDL_VideoModeOK`, `SDL_SetVideoMode` → no-ops (использовать `SDL_CreateWindow`)
- `SDL_WM_SetCaption`, `SDL_WM_GrabInput` → no-ops
- `SDL_GetAppState` → синтезируется из событий фокуса

Код движка продолжает вызывать старые имена 1.2; шим перенаправляет их в эквиваленты 2.0.

### LZ77-декомпрессор

Оригинальный декомпрессор в `vfs.c` обрабатывал по одному байту за раз в цикле `while` по 8 управляющим битам. Новый `lz_decompress_arm.c` сохраняет тот же формат LZ77 (ring buffer 4096 байт, начальная позиция записи 0xFEE, 12-бит position + 4-бит length+3 match-encoding), но добавляет быстрый путь:

```c
if (len <= 8 &&
    pos + len <= 0x1000 && rinp + len <= 0x1000 &&
    dist >= len) {
    // Пакетное копирование — без wrap, без overlap
    for (uint32_t i = 0; i < len; i++) {
        output[i] = lz_ring[pos + i];
        lz_ring[rinp + i] = lz_ring[pos + i];
    }
} else {
    // Медленный путь — обработка wrap и overlap
    while (len--) { ... }
}
```

Быстрый путь покрывает ~80% совпадений в типичных данных сцен и пропускает проверку wrap ring-индекса на каждый байт.

---

## Локализация

12 UI-строк движка (START, SAVE, LOAD, OPTIONS, EXIT, формат процента галереи, подпись screenshot и т.д.) зашиты как английские дефолты прямо в `main.c`:

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

При старте `lang_init()` пытается открыть `LANG/<LANGUAGE>.TXT` (где `LANGUAGE` по умолчанию `"ENGLISH"`). Если файл есть — он построчно переопределяет эти 12 дефолтов. Если файла нет — движок использует зашитые в C-коде дефолты, так что файл **опциональный**.

Пайплайн рендеринга текста полностью UTF-8 (`TTF_SizeUTF8` для измерения, `TTF_RenderUTF8_Shaded` для растеризации), поэтому перевод с кириллицей можно положить в `LANG/ENGLISH.TXT` без изменений в коде. Используйте Unix line endings (`\n`, не `\r\n`); движок срезает `\r`, но не перекодирует кодировки.

Внутриигровые диалоги и строки скриптов лежат внутри `SG.DL1` как `.AB` файлы — не в `LANG/*.TXT`. Для их перевода нужны инструменты из `tools/` (см. [Инструменты перевода](#инструменты-перевода)).

Не-английские пункты (`JAPANESE`, `GERMAN`, `FRENCH`, `SPANISH`, `ITALIAN`) удалены из меню языков в `menus.c`. Движок по умолчанию использует `LANGUAGE_DEFAULT "ENGLISH"` (определено в `main.h`), поэтому `LANG/ENGLISH.TXT` — единственный файл переопределения, который движок будет искать.

Чтобы добавить другой язык: положите его `.TXT` в `assets/LANG/`, добавьте имя языка обратно в таблицу `main_menu_langs[]` в `menus.c` и установите `LANGUAGE_DEFAULT` в `main.h` в это имя.

---

## Требования к сборке

| Компонент | Версия |
|-----------|--------|
| Android Studio | Narwhal 2025.1.1+ (или JDK 17 + SDK + NDK на CLI) |
| JDK | 17 |
| Android Gradle Plugin | 8.7.2 (объявлен в `app/build.gradle`) |
| Gradle | 8.11.1 (wrapper включён) |
| Android NDK | r25 или новее (тестировалось с r27) |
| CMake | 3.22.1+ (вкомпилирован в Android SDK) |
| `compileSdk` / `targetSdk` | 35 |
| `minSdk` | 24 |

**Почему minSdk 24?** 32-битная сборка `armeabi-v7a` использует `ftello` / `fseeko` для стримингового ring buffer в `ringread.c`. Эти функции доступны в bionic libc только начиная с API level 24.

### ABI

- `arm64-v8a` — основная цель, современные 64-битные устройства
- `armeabi-v7a` — устаревшие 32-битные устройства (Android 7.0+)

x86 и x86-64 не поддерживаются.

---

## Инструменты перевода

Для внутриигровых диалогов и строк скриптов (которые живут внутри `SG.DL1` как `.AB` файлы, а не в `LANG/*.TXT`) в `tools/` лежат два Python-помощника:

```bash
# Извлечь строки из .AB-скрипта
python tools/ab_translator.py extract AASTART.AB -o aastart.patch

# Отредактировать aastart.patch — вписать русские переводы после каждой строки >

# Запатчить .AB-файл переведёнными строками
python tools/ab_translator.py patch AASTART.AB aastart.patch -o AASTART.RU.AB

# Перепаковать пропатченный .AB обратно в SG.DL1
python tools/repack_sg.py SG.DL1 AASTART.AB AASTART.RU.AB -o SG.RU.DL1
```

Оба скрипта — чистый Python 3 без сторонних зависимостей.

---

## Устранение неполадок

### CMake-ошибка "SDL2 not found!"

Не должна возникать — CMake `FetchContent` скачивает SDL2 автоматически. Если возникает:

- Проверьте доступ к сети во время шага configure.
- Проверьте `~/.gradle/cxx/` — здесь лежит кешированная загрузка.
- Как фолбэк вручную склонируйте SDL2-стек:
  ```bash
  cd app/jni
  mkdir -p SDL && cd SDL
  git clone --branch SDL2 https://github.com/libsdl-org/SDL.git
  git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_image.git
  git clone --branch release-2.8.x https://github.com/libsdl-org/SDL_mixer.git
  git clone --branch release-2.24.x https://github.com/libsdl-org/SDL_ttf.git
  ```

### Gradle sync не удаётся

- Подтвердите Gradle 8.11.1 (wrapper форсирует это — `./gradlew --version`).
- Если Android Studio предлагает обновить AGP — согласитесь.
- Если видите ошибки дублирующихся Kotlin-stdlib классов, блоки `constraints` и `resolutionStrategy` в `app/build.gradle` уже форсят Kotlin 1.8.22 — не удаляйте их.

### Приложение падает при запуске

```bash
adb logcat -s SDL DiviDead
```

Частые причины:

- **Нет ассетов** — `SG.DL1` или `WV.DL1` отсутствуют в `app/src/main/assets/`. Перезапустите `./populate_assets.sh`.
- **Нехватка памяти** — `WV.DL1` весит 315 МБ и распаковывается во внутреннее хранилище при первом запуске. В манифесте установлен `largeHeap="true"`; если устройство всё равно OOM-ит, освободите внутреннюю память и повторите.
- **GL-контекст не создаётся** — ищите `"GL: context failed"` в logcat. Обычно означает сломанный OpenGL ES 2.0 драйвер на устройстве.

### Первый запуск идёт долго

Нормально — приложение распаковывает ~430 МБ ассетов из APK во внутреннее хранилище. Последующие запуски быстрые (маркер `.extracted` коротко замыкает распаковщик).

### Русский текст отображается квадратиками

Убедитесь, что переведённый `LANG/ENGLISH.TXT` закодирован в UTF-8 (не Windows-1251) и использует Unix line endings (`\n`, не `\r\n`). Движок срезает `\r` с конца каждой строки, но не перекодирует кодировки.

---

## Авторство и лицензия

- **Исходники движка:** форк gameblabla интерпретатора soywiz Divi-Dead — выпущен только для персонального использования.
- **SDL2, SDL_image, SDL_mixer, SDL_ttf:** zlib-лицензия.
- **Android-порт:** © VienDesu! Porting Team.

Этот проект — **только для персонального использования**. Оригинальные игровые данные Divi-Dead (`SG.DL1`, `WV.DL1`, видео, музыка) защищены авторским правом Leaf/AQUAPLUS и **не** включены в этот репозиторий — нужно использовать свою легально полученную копию.

Репозиторий содержит только исходный код движка (с патчами и новыми модулями, описанными выше), систему сборки, инструменты перевода и иконки запуска. Никаких игровых ассетов, никаких защищённых авторским правом диалогов, никакой защищённой графики.
