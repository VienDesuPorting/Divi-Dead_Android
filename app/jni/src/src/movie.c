/*
 * Video playback - uses Android MediaPlayer on Android, stub on other platforms.
 * Replaces the ROQ decoder which only supports .ROQ format.
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
    
    /* Play using Android MediaPlayer */
    return android_play_video(name, skip);
}
#else
/* Non-Android: use the ROQ decoder from sdl-dreamroq.c */
/* MOVIE_PLAY is defined in roq/sdl-dreamroq.c */
#endif

void MOVIE_START(void) {
    printf("MOVIE_START();\n");
}

void MOVIE_END(void) {
    printf("MOVIE_END();\n");
}
