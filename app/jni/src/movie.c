/*
 * Video playback via pl_mpeg (pure C MPEG-1 decoder).
 *
 * Replaces the original ROQ / SMPEG / Android MediaPlayer approaches
 * with a single portable decoder. See android_plmpeg.c for details.
 */

#include "shared.h"

#ifdef __ANDROID__
extern int android_play_video(const char *path, int skip);

uint_fast8_t MOVIE_PLAY(char *name, int skip) {
    printf("MOVIE_PLAY: '%s' (skip=%d)\n", name, skip);

    if (!name || !*name) return 0;

    /* Check if file exists */
    if (!_file_exists(name)) {
        printf("MOVIE_PLAY: file not found '%s'\n", name);
        return 0;
    }

    return android_play_video(name, skip);
}
#else
/* Non-Android: MOVIE_PLAY is defined in roq/sdl-dreamroq.c */
#endif

/* MOVIE_START() and MOVIE_END() are #define macros in main.h (no-ops).
 * No function definitions needed - they expand to nothing. */
