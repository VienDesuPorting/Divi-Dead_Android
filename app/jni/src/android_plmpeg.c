/*
 * android_plmpeg.c — MPEG-1 video player using pl_mpeg + SDL_Audio.
 *
 * Replaces android_video.c (which used Android MediaPlayer via JNI).
 * MediaPlayer is unreliable across devices: MPEG-1 support depends on
 * the SoC vendor, and on some devices (e.g. Nothing Phone 3A with
 * Adreno) prepare() fails with error (-2147483648). PlmPEG is a pure
 * C decoder with no platform dependencies, so it works everywhere.
 *
 * Audio is decoded to float samples by pl_mpeg and queued directly
 * to SDL_Audio via SDL_QueueAudio (the simple push API, no callback).
 * Video frames are converted from YUV planes to RGBA on the CPU via
 * plm_frame_to_rgba, blitted onto the engine's screen surface, and
 * uploaded to the GL texture through android_gl_render.
 *
 * Tap to skip is supported by polling SDL events in the decode loop.
 *
 * The pl_mpeg author's reference implementation
 * (pl_mpeg_player_sdl.c) uses exactly this approach: SDL_QueueAudio
 * for audio output and plm_set_audio_lead_time set to
 * (SDL_AudioSpec.samples / samplerate) so pl_mpeg decodes audio ~93 ms
 * ahead of video, matching the SDL audio buffer size.
 */

#include "shared.h"

#ifdef __ANDROID__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#define PL_MPEG_IMPLEMENTATION
#include "plmpeg/pl_mpeg.h"

#define TAG "DiviDead"
/* printf is redirected to logcat via android_log.c */
#define LOGI(...) printf("PLMPEG: " __VA_ARGS__)
#define LOGE(...) printf("PLMPEG ERROR: " __VA_ARGS__)

/* ----- Video decode state ----- */

static plm_t *g_plm = NULL;
static SDL_AudioDeviceID g_audio_dev = 0;
static uint8_t *g_frame_rgba = NULL;
static int g_video_width = 0;
static int g_video_height = 0;

/* Forward decls — android_gl_render is in android_gl_render.c,
 * g_sdl_window is declared in sdl12_compat.h (and defined in main.c),
 * screen is declared in main.h (and defined in main.c). */
extern void android_gl_render(SDL_Window *window, SDL_Surface *surface);

/* ---------- Video callback ---------- */

/* Convert a pl_mpeg YUV frame to the engine's screen surface format.
 *
 * plm_frame_to_rgba() outputs RGBA8888 (R,G,B,A bytes in memory).
 * The screen surface is SDL_PIXELFORMAT_RGBX8888 on Android (R,G,B,X
 * bytes — X is padding, ignored). SDL_BlitScaled between these two
 * formats silently fails on Android SDL2 builds (the stretch-blitter
 * has known issues with cross-format scaling on ARM), so we explicitly
 * convert the video surface to the screen's format first.
 */

static void video_decode_cb(plm_t *plm, plm_frame_t *frame, void *user) {
    (void)plm; (void)user;

    /* YUV -> RGBA into g_frame_rgba (4 bytes per pixel) */
    plm_frame_to_rgba(frame, g_frame_rgba, g_video_width * 4);

    if (!screen || !screen->pixels || !g_sdl_window) {
        LOGE("video_decode_cb: missing state (screen=%p pixels=%p window=%p)\n",
             (void*)screen, screen ? (void*)screen->pixels : NULL, (void*)g_sdl_window);
        return;
    }

    /* Wrap the RGBA buffer in a throwaway surface, then convert to the
     * screen's pixel format. After conversion, SDL_BlitScaled degenerates
     * to a same-format stretch copy (memcpy + nearest-neighbor scale). */
    SDL_Surface *video_surf = SDL_CreateRGBSurfaceWithFormatFrom(
        g_frame_rgba,
        g_video_width, g_video_height, 32,
        g_video_width * 4,
        SDL_PIXELFORMAT_RGBA32);
    if (!video_surf) {
        LOGE("video_decode_cb: can't create surface: %s\n", SDL_GetError());
        return;
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(video_surf, screen->format->format, 0);
    SDL_FreeSurface(video_surf);
    if (!converted) {
        LOGE("video_decode_cb: SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
        return;
    }

    SDL_FillRect(screen, NULL, 0);
    SDL_Rect dst = {0, 0, screen->w, screen->h};
    if (SDL_BlitScaled(converted, NULL, screen, &dst) < 0) {
        LOGE("video_decode_cb: SDL_BlitScaled failed: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(converted);

    android_gl_render(g_sdl_window, screen);
}

/* ---------- Audio callback (pl_mpeg → SDL_QueueAudio) ---------- */

/* Convert float samples [-1.0, 1.0] to S16 and push to SDL's audio
 * queue. SDL handles buffering, underrun silence, and scheduling —
 * we just feed it decoded chunks as plm_decode emits them. */
static void audio_decode_cb(plm_t *plm, plm_samples_t *samples, void *user) {
    (void)plm; (void)user;
    if (!g_audio_dev) return;

    /* pl_mpeg emits interleaved stereo float (2 × count samples).
     * Convert to S16 in a stack buffer and queue. */
    static Sint16 pcm[PLM_AUDIO_SAMPLES_PER_FRAME * 2];
    unsigned int n = samples->count * 2;
    for (unsigned int i = 0; i < n; i++) {
        float s = samples->interleaved[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        pcm[i] = (Sint16)(s * 32767.0f);
    }
    SDL_QueueAudio(g_audio_dev, pcm, n * sizeof(Sint16));
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

    /* Suspend SDL_mixer during video playback.
     *
     * SDL_mixer holds its own audio device open for the entire engine
     * lifetime. On Android, opening a SECOND audio device (for our
     * video decoder) while SDL_mixer's device is still active causes
     * intermittent crackling on the second and subsequent videos —
     * AudioFlinger leaves the mixer's track in a half-suspended state
     * that produces audible glitches.
     *
     * Solution: explicitly pause and halt SDL_mixer before opening
     * our video audio device, then resume it after the video ends.
     * This gives us exclusive access to the audio subsystem during
     * video playback and avoids the cross-device contention. */
    int mixer_was_playing = (Mix_PlayingMusic() || Mix_Playing(-1));
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    Mix_Pause(-1);
    SDL_Delay(20);  /* Give AudioFlinger a moment to actually release */

    /* Open SDL_Audio device using the queue API (no callback).
     * samples=4096 gives ~93 ms buffer at 44.1 kHz. This is also
     * passed to plm_set_audio_lead_time below so plm_decode keeps
     * the audio queue filled with the same amount of headroom. */
    SDL_AudioSpec want = {0};
    want.freq = sample_rate > 0 ? sample_rate : 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 4096;
    /* No callback — we'll use SDL_QueueAudio to push samples.
     * SDL_AUDIO_ALLOW_FREQUENCY_CHANGE not needed; we want exactly
     * this spec so the lead_time calculation is exact. */
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio_dev == 0) {
        LOGE("SDL_OpenAudioDevice failed: %s — continuing without audio\n",
             SDL_GetError());
    }

    /* Tell pl_mpeg to decode audio ~93 ms ahead of video, matching
     * the SDL_AudioSpec.samples buffer size. Without this, plm_decode
     * only decodes audio up to the current video time, leaving the
     * SDL audio queue at risk of underrun whenever the decode loop
     * sleeps briefly. With lead_time set, audio is always decoded
     * one buffer ahead, giving SDL a steady supply.
     *
     * This matches the official pl_mpeg_player_sdl.c reference. */
    if (g_audio_dev && sample_rate > 0) {
        plm_set_audio_lead_time(plm, (double)want.samples / (double)sample_rate);
    }

    SDL_PauseAudioDevice(g_audio_dev, 0);

    /* Main decode loop with self-pacing based on SDL audio queue level.
     *
     * The naive approach (plm_decode(wall_clock_dt) + sleep) fails on
     * Android because SDL_Delay granularity is 20-50 ms, not the
     * requested 16 ms. This causes dt to accumulate, and plm_decode
     * then emits several video frames + audio chunks in a burst,
     * producing stutters in both video and audio.
     *
     * Instead, we monitor SDL_GetQueuedAudioSize and decode aggressively
     * when the queue is running low, sleep briefly when it's well
     * filled. This decouples the decode cadence from wall-clock jitter.
     *
     * Threshold rationale:
     *   - want.samples = 4096 (~93 ms at 44.1 kHz)
     *   - plm_set_audio_lead_time = 93 ms above
     *   - LOW_WATER = 2048 samples (~46 ms) — half the SDL buffer
     *   - HIGH_WATER = 16384 samples (~372 ms) — let consumer drain
     *
     * SDL_AudioSpec.samples is in frames (per channel), so the actual
     * byte size of one buffer is samples * 2 channels * 2 bytes = 16384.
     */
    const Uint32 AUDIO_LOW_WATER_BYTES = 2048 * 2 * 2;   /* ~46 ms */
    const Uint32 AUDIO_HIGH_WATER_BYTES = 16384 * 2 * 2; /* ~372 ms */

    Uint32 last_ticks = SDL_GetTicks();
    int skipped = 0;
    SDL_Event ev;

    while (!plm_has_ended(plm)) {
        Uint32 queued = g_audio_dev ? SDL_GetQueuedAudioSize(g_audio_dev) : 0;

        if (queued < AUDIO_LOW_WATER_BYTES) {
            /* Audio queue running low — decode in a tight loop without
             * sleeping until it recovers above LOW_WATER. Cap dt at
             * 1/30 s per call so plm_decode doesn't burst-emit. */
            do {
                if (plm_has_ended(plm)) goto done;
                Uint32 now = SDL_GetTicks();
                double dt = (now - last_ticks) / 1000.0;
                last_ticks = now;
                if (dt > 1.0 / 30.0) dt = 1.0 / 30.0;
                plm_decode(plm, dt);
                queued = g_audio_dev ? SDL_GetQueuedAudioSize(g_audio_dev) : 0;
            } while (queued < AUDIO_LOW_WATER_BYTES);
        } else if (queued < AUDIO_HIGH_WATER_BYTES) {
            /* Queue is healthy — normal decode pass + brief yield */
            Uint32 now = SDL_GetTicks();
            double dt = (now - last_ticks) / 1000.0;
            last_ticks = now;
            if (dt > 1.0 / 30.0) dt = 1.0 / 30.0;
            plm_decode(plm, dt);
            SDL_Delay(4);  /* brief yield to avoid hogging CPU */
        } else {
            /* Queue is full — let the consumer drain it */
            last_ticks = SDL_GetTicks();
            SDL_Delay(8);
        }

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
    }

done:
    if (g_audio_dev) {
        SDL_PauseAudioDevice(g_audio_dev, 1);
        SDL_ClearQueuedAudio(g_audio_dev);
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }

    /* Resume SDL_mixer.
     *
     * We paused Mix_HaltMusic + Mix_Pause before the video; now resume
     * so the engine's GAME_MUSIC_PLAY() call works normally. */
    Mix_Resume(-1);
    if (mixer_was_playing) {
        Mix_ResumeMusic();
    }

    free(g_frame_rgba);
    g_frame_rgba = NULL;

    plm_destroy(plm);
    g_plm = NULL;

    LOGI("done (skipped=%d)\n", skipped);
    return skipped ? 2 : 1;
}

#endif /* __ANDROID__ */
