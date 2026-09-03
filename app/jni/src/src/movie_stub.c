/*
 * Video playback stub - skips video and returns immediately.
 * 
 * The original engine uses ROQ format for videos, but the PC/PSP versions
 * use .MPG and .AVI. Neither format is supported by the ROQ decoder.
 * 
 * This stub allows the game to start immediately without video playback.
 * Video can be added later via Android MediaPlayer JNI or FFmpeg.
 */

#include "shared.h"

/* Stub MOVIE_PLAY - just log and return */
uint_fast8_t MOVIE_PLAY(char *name, int skip) {
    (void)skip;
    printf("MOVIE_PLAY: skipping '%s'\n", name);
    return 0;
}
