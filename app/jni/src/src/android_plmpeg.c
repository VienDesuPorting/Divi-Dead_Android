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
 * variable-size chunks. We use a 16-frame ring (≈ 425 ms at 44.1 kHz)
 * with prebuffering to absorb the bursty decode pattern and avoid
 * underruns that manifest as crackling on long videos. */

#define AUDIO_RING_FRAMES 16
#define AUDIO_RING_SAMPLES (PLM_AUDIO_SAMPLES_PER_FRAME * 2 * AUDIO_RING_FRAMES)
/* Prebuffer threshold: start SDL_Audio once the ring is at least this
 * full. 8 frames ≈ 210 ms — generous initial cushion to absorb GC
 * pauses and scheduler hiccups during the first few seconds. */
#define AUDIO_PREBUFFER_FRAMES 8
#define AUDIO_PREBUFFER_SAMPLES (PLM_AUDIO_SAMPLES_PER_FRAME * 2 * AUDIO_PREBUFFER_FRAMES)
/* Low-water mark: when ring drops below this, decode aggressively
 * (no sleep) until it recovers. Set to ~6 frames (~160 ms) — well
 * above the SDL_Audio buffer size (4096 samples ≈ 93 ms) so we have
 * at least one buffer's worth of headroom. */
#define AUDIO_LOW_WATER_FRAMES 6
#define AUDIO_LOW_WATER_SAMPLES (PLM_AUDIO_SAMPLES_PER_FRAME * 2 * AUDIO_LOW_WATER_FRAMES)

static float audio_ring[AUDIO_RING_SAMPLES];
static atomic_size_t audio_read_pos;
static atomic_size_t audio_write_pos;

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

/* ---------- Audio callback (pl_mpeg → ring buffer) ---------- */

static void audio_decode_cb(plm_t *plm, plm_samples_t *samples, void *user) {
    (void)plm; (void)user;

    /* Drop new samples if the ring is already full (decoder overran
     * the consumer). This happens when SDL_Audio stalls briefly —
     * better to drop the oldest undecoded audio than to overwrite
     * unplayed samples and produce a click. */
    size_t wpos = atomic_load(&audio_write_pos);
    size_t rpos = atomic_load(&audio_read_pos);
    size_t available = wpos - rpos;
    if (available + samples->count * 2 >= AUDIO_RING_SAMPLES) {
        /* Ring would overflow — bump read pointer forward to make room,
         * dropping the oldest samples. This is preferable to overwriting
         * unplayed data and produces a less noticeable glitch. */
        size_t drop = (available + samples->count * 2) - AUDIO_RING_SAMPLES + 1;
        atomic_store(&audio_read_pos, rpos + drop);
    }

    for (unsigned int i = 0; i < samples->count * 2; i++) {
        audio_ring[wpos % AUDIO_RING_SAMPLES] = samples->interleaved[i];
        wpos++;
    }
    atomic_store(&audio_write_pos, wpos);
}

/* ---------- SDL_Audio callback (ring buffer → S16 stream) ---------- */

/* Fade-in counter to avoid a click on the very first audio chunk.
 * Reset to FADE_LEN at prebuffer end, decremented per sample until 0. */
#define AUDIO_FADE_LEN 256
static atomic_int g_audio_fade;

/* Last sample written to the output, used for zero-order hold when
 * the ring underruns. Repeating the last sample produces a much less
 * audible glitch than inserting silence (which creates a discontinuity
 * at both the start and end of the gap). */
static Sint16 g_last_sample = 0;
static atomic_int g_underrun_count;

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

    int fade = atomic_load(&g_audio_fade);
    for (size_t i = 0; i < to_read; i++) {
        float s = audio_ring[rpos % AUDIO_RING_SAMPLES];
        /* Clamp and convert [-1.0, 1.0] → [-32768, 32767] */
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        Sint16 sample = (Sint16)(s * 32767.0f);
        /* Linear fade-in over the first FADE_LEN samples to avoid click. */
        if (fade > 0) {
            float gain = 1.0f - (float)fade / AUDIO_FADE_LEN;
            sample = (Sint16)(sample * gain);
            fade--;
        }
        out[i] = sample;
        g_last_sample = sample;
        rpos++;
    }
    if (fade > 0) atomic_store(&g_audio_fade, fade);

    /* If we didn't have enough, fill the rest with the last sample
     * (zero-order hold) instead of silence. This produces a flat line
     * at the last sample's value, which is much less audible than
     * a jump to silence and back. Count underruns for diagnostics. */
    if (to_read < (size_t)samples_wanted) {
        atomic_fetch_add(&g_underrun_count, 1);
        for (size_t i = to_read; i < (size_t)samples_wanted; i++) {
            out[i] = g_last_sample;
        }
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

    /* Open SDL_Audio device.
     * samples=4096 gives ~93 ms buffer at 44.1 kHz — large enough to
     * absorb Android scheduler jitter (which can be 20-50 ms per
     * SDL_Delay call) without underrunning on long videos. */
    SDL_AudioSpec want = {0}, got = {0};
    want.freq = sample_rate > 0 ? sample_rate : 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 4096;
    want.callback = sdl_audio_cb;
    want.userdata = NULL;
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (g_audio_dev == 0) {
        LOGE("SDL_OpenAudioDevice failed: %s — continuing without audio\n",
             SDL_GetError());
    }

    /* Prebuffer: decode frames until the ring is at least half full,
     * THEN start playback. This eliminates the startup underrun that
     * was causing the initial crackle. */
    LOGI("prebuffering audio (target=%d samples)...\n", AUDIO_PREBUFFER_SAMPLES);
    Uint32 prebuffer_start = SDL_GetTicks();
    while (1) {
        Uint32 now = SDL_GetTicks();
        double dt = (now - prebuffer_start) / 1000.0;
        prebuffer_start = now;
        plm_decode(plm, dt);

        size_t rpos = atomic_load(&audio_read_pos);
        size_t wpos = atomic_load(&audio_write_pos);
        if (wpos - rpos >= AUDIO_PREBUFFER_SAMPLES) break;

        if (plm_has_ended(plm)) break;

        SDL_Delay(2);
    }
    LOGI("prebuffered %zu samples\n",
         atomic_load(&audio_write_pos) - atomic_load(&audio_read_pos));

    /* Reset fade-in counter — will be consumed by the SDL_Audio callback
     * over the first AUDIO_FADE_LEN samples (≈ 5 ms at 44.1 kHz) to
     * eliminate startup click. */
    atomic_store(&g_audio_fade, AUDIO_FADE_LEN);

    if (g_audio_dev) {
        SDL_PauseAudioDevice(g_audio_dev, 0);
    }

    /* Reset underrun counter for this video */
    atomic_store(&g_underrun_count, 0);

    /* Main decode loop with self-pacing.
     *
     * Instead of blindly feeding wall-clock dt to plm_decode on a fixed
     * schedule, we check the ring buffer level and decode aggressively
     * when it's running low. This prevents underruns on long videos
     * where scheduler jitter (SDL_Delay can sleep 20-50 ms on Android
     * instead of the requested 16 ms) would otherwise drain the ring.
     *
     * Strategy (three tiers based on ring fill level):
     *   - Below LOW_WATER (~160 ms / 6 frames) → decode immediately
     *     with no sleep, repeat until above LOW_WATER
     *   - Below 75% full → normal decode pass + brief 4 ms yield
     *   - Above 75% full → skip decode, sleep 8 ms to let consumer drain
     *
     * The aggressive tier is critical for absorbing GC pauses: when the
     * runtime pauses for 50 ms, the consumer drains ~5 ms of audio per
     * 93 ms SDL buffer, and we need to catch up fast.
     */
    Uint32 last_ticks = SDL_GetTicks();
    int skipped = 0;
    SDL_Event ev;
    Uint32 log_timer = SDL_GetTicks();

    while (!plm_has_ended(plm)) {
        size_t rpos = atomic_load(&audio_read_pos);
        size_t wpos = atomic_load(&audio_write_pos);
        size_t available = wpos - rpos;

        if (available < AUDIO_LOW_WATER_SAMPLES) {
            /* Buffer is running low — decode in a tight loop without
             * sleeping until the ring recovers above LOW_WATER.
             * Cap dt at 50 ms per plm_decode call to avoid decoding
             * too far ahead of the video timestamp. */
            do {
                if (plm_has_ended(plm)) goto done;
                Uint32 now = SDL_GetTicks();
                double dt = (now - last_ticks) / 1000.0;
                last_ticks = now;
                if (dt > 0.050) dt = 0.050;
                plm_decode(plm, dt);
                rpos = atomic_load(&audio_read_pos);
                wpos = atomic_load(&audio_write_pos);
                available = wpos - rpos;
            } while (available < AUDIO_LOW_WATER_SAMPLES);
        } else if (available < (AUDIO_RING_SAMPLES * 3) / 4) {
            /* Buffer is healthy but not full — normal decode pass */
            Uint32 now = SDL_GetTicks();
            double dt = (now - last_ticks) / 1000.0;
            last_ticks = now;
            plm_decode(plm, dt);
            SDL_Delay(4);  /* brief yield to avoid hogging CPU */
        } else {
            /* Buffer is well-filled — let the consumer drain it */
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

        /* Log underrun count every 5 seconds for diagnostics */
        if (SDL_GetTicks() - log_timer > 5000) {
            int underruns = atomic_load(&g_underrun_count);
            if (underruns > 0) {
                LOGI("underruns so far: %d (ring available=%zu/%d)\n",
                     underruns, available, AUDIO_RING_SAMPLES);
            }
            log_timer = SDL_GetTicks();
        }
    }

done:
    if (g_audio_dev) {
        SDL_PauseAudioDevice(g_audio_dev, 1);
        SDL_CloseAudioDevice(g_audio_dev);
        g_audio_dev = 0;
    }

    atomic_store(&g_audio_fade, 0);
    g_last_sample = 0;

    /* Log total underruns for this video — helps diagnose remaining
     * audio issues. 0 underruns = perfectly clean playback. */
    int total_underruns = atomic_load(&g_underrun_count);
    if (total_underruns > 0) {
        LOGI("audio underruns during playback: %d\n", total_underruns);
    }
    atomic_store(&g_underrun_count, 0);

    /* Resume SDL_mixer — close and reopen its audio device to get a
     * clean state, since the underlying Android audio track may have
     * been disturbed by our temporary second device. The engine's
     * audio.c will restart music on the next GAME_MUSIC_PLAY() call. */
    Mix_Resume(-1);
    if (mixer_was_playing) {
        Mix_ResumeMusic();
    }

    free(g_frame_rgba);
    g_frame_rgba = NULL;

    plm_destroy(plm);
    g_plm = NULL;

    atomic_store(&audio_read_pos, 0);
    atomic_store(&audio_write_pos, 0);

    LOGI("done (skipped=%d)\n", skipped);
    return skipped ? 2 : 1;
}

#endif /* __ANDROID__ */
