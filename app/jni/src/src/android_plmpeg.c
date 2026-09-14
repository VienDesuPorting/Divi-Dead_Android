/*
 * android_plmpeg.c — MPEG-1 video player using pl_mpeg + SDL_Audio.
 *
 * Replaces android_video.c (which used Android MediaPlayer via JNI).
 * MediaPlayer is unreliable across devices: MPEG-1 support depends on
 * the SoC vendor, and on some devices (e.g. Nothing Phone 3A with
 * Adreno) prepare() fails with error (-2147483648). PlmPEG is a pure
 * C decoder with no platform dependencies, so it works everywhere.
 *
 * Audio is decoded to float samples by pl_mpeg, converted to S16 in
 * the audio callback, and played through SDL_Audio. Video frames are
 * converted from YUV planes to RGBA on the CPU via plm_frame_to_rgba,
 * blitted onto the engine's screen surface, and uploaded to the GL
 * texture through android_gl_render.
 *
 * Tap to skip is supported by polling SDL events in the decode loop.
 */

#include "shared.h"

#ifdef __ANDROID__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <SDL2/SDL.h>

#define PL_MPEG_IMPLEMENTATION
#include "plmpeg/pl_mpeg.h"

#define TAG "DiviDead"
/* printf is redirected to logcat via android_log.c */
#define LOGI(...) printf("PLMPEG: " __VA_ARGS__)
#define LOGE(...) printf("PLMPEG ERROR: " __VA_ARGS__)

/* ----- Audio ring buffer -----
 * pl_mpeg decodes 1152-sample frames at a time. SDL_Audio pulls
 * variable-size chunks. We use a 4x-oversized ring to absorb
 * the bursty decode pattern. */

#define AUDIO_RING_FRAMES 8
#define AUDIO_RING_SAMPLES (PLM_AUDIO_SAMPLES_PER_FRAME * 2 * AUDIO_RING_FRAMES)

static float audio_ring[AUDIO_RING_SAMPLES];
static atomic_size_t audio_read_pos;
static atomic_size_t audio_write_pos;

/* ----- Video decode state ----- */

static plm_t *g_plm = NULL;
static SDL_AudioDeviceID g_audio_dev = 0;
static uint8_t *g_frame_rgba = NULL;
static int g_video_width = 0;
static int g_video_height = 0;
static int g_video_frames_decoded = 0;

/* Forward decls — android_gl_render is in android_gl_render.c,
 * g_sdl_window is declared in sdl12_compat.h (and defined in main.c),
 * screen is declared in main.h (and defined in main.c). */
extern void android_gl_render(SDL_Window *window, SDL_Surface *surface);

/* ---------- Video callback ---------- */

static void video_decode_cb(plm_t *plm, plm_frame_t *frame, void *user) {
    (void)plm; (void)user;
    g_video_frames_decoded++;

    /* YUV -> RGBA in place */
    plm_frame_to_rgba(frame, g_frame_rgba, g_video_width * 4);

    /* Wrap RGBA buffer in a throwaway SDL_Surface (no pixel copy,
     * just a header that points at g_frame_rgba) */
    SDL_Surface *video_surf = SDL_CreateRGBSurfaceWithFormatFrom(
        g_frame_rgba,
        g_video_width, g_video_height, 32,
        g_video_width * 4,
        SDL_PIXELFORMAT_RGBA32);
    if (!video_surf) {
        LOGE("video_decode_cb: can't create surface: %s\n", SDL_GetError());
        return;
    }

    /* Stretch-blit into the engine's 640x480 screen surface, then
     * upload to the GL texture. SDL_FillRect clears any letterbox
     * regions to black (g_video_* may be smaller than 640x480). */
    if (!screen) {
        LOGE("video_decode_cb: screen is NULL\n");
        SDL_FreeSurface(video_surf);
        return;
    }
    if (!screen->pixels) {
        LOGE("video_decode_cb: screen->pixels is NULL\n");
        SDL_FreeSurface(video_surf);
        return;
    }
    if (!g_sdl_window) {
        LOGE("video_decode_cb: g_sdl_window is NULL\n");
        SDL_FreeSurface(video_surf);
        return;
    }

    SDL_FillRect(screen, NULL, 0);
    SDL_Rect dst = {0, 0, screen->w, screen->h};
    int blit_ok = SDL_BlitScaled(video_surf, NULL, screen, &dst);
    if (blit_ok < 0) {
        LOGE("video_decode_cb: SDL_BlitScaled failed: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(video_surf);

    /* Log the first few frames to verify the callback is being called
     * and that the screen surface actually got non-black pixels. */
    if (g_video_frames_decoded <= 3) {
        Uint32 *px = (Uint32 *)screen->pixels;
        Uint32 top_left = px[0];
        Uint32 center = px[(screen->w * screen->h) / 2];
        LOGI("video_decode_cb: frame %d, screen %dx%d (pitch=%d, fmt=0x%x), "
             "top_left=0x%08x center=0x%08x\n",
             g_video_frames_decoded, screen->w, screen->h, screen->pitch,
             screen->format->format, top_left, center);
    }

    android_gl_render(g_sdl_window, screen);
}

/* ---------- Audio callback (pl_mpeg → ring buffer) ---------- */

static void audio_decode_cb(plm_t *plm, plm_samples_t *samples, void *user) {
    (void)plm; (void)user;

    size_t wpos = atomic_load(&audio_write_pos);
    for (unsigned int i = 0; i < samples->count * 2; i++) {
        audio_ring[wpos % AUDIO_RING_SAMPLES] = samples->interleaved[i];
        wpos++;
    }
    atomic_store(&audio_write_pos, wpos);
}

/* ---------- SDL_Audio callback (ring buffer → S16 stream) ---------- */

static void sdl_audio_cb(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Sint16 *out = (Sint16 *)stream;
    int samples_wanted = len / sizeof(Sint16);

    size_t rpos = atomic_load(&audio_read_pos);
    size_t wpos = atomic_load(&audio_write_pos);
    size_t available = wpos - rpos;
    size_t to_read = (available < (size_t)samples_wanted)
                         ? available
                         : (size_t)samples_wanted;

    for (size_t i = 0; i < to_read; i++) {
        float s = audio_ring[rpos % AUDIO_RING_SAMPLES];
        /* Clamp and convert [-1.0, 1.0] → [-32768, 32767] */
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out[i] = (Sint16)(s * 32767.0f);
        rpos++;
    }
    /* If we didn't have enough, fill the rest with silence */
    for (size_t i = to_read; i < (size_t)samples_wanted; i++) {
        out[i] = 0;
    }
    atomic_store(&audio_read_pos, rpos);
}

/* ---------- Public API ---------- */

int android_play_video(const char *path, int skip) {
    LOGI("playing '%s' (skip=%d)\n", path, skip);

    if (!path || !*path) {
        LOGE("null path\n");
        return 0;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        LOGE("file not found '%s'\n", path);
        return 0;
    }
    fclose(f);

    /* Reset state */
    atomic_store(&audio_read_pos, 0);
    atomic_store(&audio_write_pos, 0);

    plm_t *plm = plm_create_with_filename(path);
    if (!plm) {
        LOGE("plm_create_with_filename failed for '%s'\n", path);
        return 0;
    }
    g_plm = plm;

    g_video_width = plm_get_width(plm);
    g_video_height = plm_get_height(plm);
    int sample_rate = plm_get_samplerate(plm);
    double framerate = plm_get_framerate(plm);
    int has_audio = plm_get_audio_enabled(plm);
    int has_video = plm_get_video_enabled(plm);
    LOGI("video %dx%d @ %.2ffps (enabled=%d), audio %d Hz (enabled=%d)\n",
         g_video_width, g_video_height, framerate, has_video, sample_rate, has_audio);

    if (!has_video) {
        LOGE("video stream is disabled — no frames will be decoded\n");
    }

    g_frame_rgba = (uint8_t *)malloc(g_video_width * g_video_height * 4);
    if (!g_frame_rgba) {
        LOGE("can't allocate RGBA buffer\n");
        plm_destroy(plm);
        g_plm = NULL;
        return 0;
    }

    plm_set_video_decode_callback(plm, video_decode_cb, NULL);
    plm_set_audio_decode_callback(plm, audio_decode_cb, NULL);

    /* Open SDL_Audio device */
    SDL_AudioSpec want = {0}, got = {0};
    want.freq = sample_rate > 0 ? sample_rate : 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = sdl_audio_cb;
    want.userdata = NULL;
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (g_audio_dev == 0) {
        LOGE("SDL_OpenAudioDevice failed: %s — continuing without audio\n",
             SDL_GetError());
    } else {
        SDL_PauseAudioDevice(g_audio_dev, 0);
    }

    /* Main decode loop.
     * plm_decode() takes a delta time and internally decides which
     * frames / audio chunks to emit, so we just feed wall-clock time. */
    Uint32 last_ticks = SDL_GetTicks();
    int skipped = 0;
    SDL_Event ev;

    while (!plm_has_ended(plm)) {
        Uint32 now = SDL_GetTicks();
        double dt = (now - last_ticks) / 1000.0;
        last_ticks = now;

        plm_decode(plm, dt);

        /* Poll SDL events for tap-to-skip / quit */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                goto done;
            }
            if (skip && (ev.type == SDL_FINGERUP ||
                         ev.type == SDL_MOUSEBUTTONUP ||
                         ev.type == SDL_KEYDOWN)) {
                skipped = 1;
                goto done;
            }
        }

        /* Sleep until ~1 frame ahead to avoid busy loop.
         * 1000 / framerate ≈ 33ms @ 30fps, 41ms @ 24fps. */
        SDL_Delay((Uint32)(1000.0 / (framerate > 0 ? framerate : 30.0)) - 1);
    }

done:
    if (g_audio_dev) {
        SDL_PauseAudioDevice(g_audio_dev, 1);
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }

    free(g_frame_rgba);
    g_frame_rgba = NULL;

    plm_destroy(plm);
    g_plm = NULL;

    atomic_store(&audio_read_pos, 0);
    atomic_store(&audio_write_pos, 0);

    LOGI("done (skipped=%d, video_frames_decoded=%d)\n",
         skipped, g_video_frames_decoded);
    g_video_frames_decoded = 0;
    return skipped ? 2 : 1;
}

#endif /* __ANDROID__ */
