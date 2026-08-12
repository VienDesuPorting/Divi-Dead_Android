/*
 * SDL 1.2 to SDL 2.0 compatibility shim for Divi-Dead engine.
 */

#ifndef SDL12_COMPAT_SHIM_H
#define SDL12_COMPAT_SHIM_H

#include <SDL2/SDL.h>

/* ---- Surface flags (removed in SDL 2.0) ---- */
#define SDL_HWSURFACE   0
#define SDL_SWSURFACE   0
#define SDL_ASYNCBLIT   0
#define SDL_HWPALETTE   0
#define SDL_DOUBLEBUF   0
#define SDL_FULLSCREEN  SDL_WINDOW_FULLSCREEN
#define SDL_OPENGLBLIT  0
#define SDL_RESIZABLE   SDL_WINDOW_RESIZABLE
#define SDL_NOFRAME     SDL_WINDOW_BORDERLESS
#define SDL_SRCALPHA    0
#define SDL_SRCCOLORKEY 0

/* ---- Global window pointer (set by SDL_SetVideoMode) ---- */
extern SDL_Window *g_sdl_window;

/* ---- Removed functions ---- */

#define SDL_WM_SetCaption(t, i) ((void)0)
#define SDL_EnableKeyRepeat(delay, interval) ((void)0)
#define SDL_SetAlpha(surface, flags, alpha) ((void)0)

/* SDL_Flip -> update the window surface */
#define SDL_Flip(surface) \
    ((g_sdl_window) ? SDL_UpdateWindowSurface(g_sdl_window) : 0)

/* SDL_UpdateRect -> update the window surface */
#define SDL_UpdateRect(surface, x, y, w, h) \
    ((g_sdl_window) ? SDL_UpdateWindowSurface(g_sdl_window) : 0)

/* SDL_UpdateRects -> update the window surface rects */
#define SDL_UpdateRects(surface, numrects, rects) \
    ((g_sdl_window) ? SDL_UpdateWindowSurfaceRects(g_sdl_window, (rects), (numrects)) : 0)

/* SDL_DisplayFormat / SDL_DisplayFormatAlpha */
#define SDL_DisplayFormat(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_RGB888, 0)
#define SDL_DisplayFormatAlpha(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_ARGB8888, 0)

/* SDL_SetVideoMode -> create window + get surface */
extern SDL_Surface *screen_video;
static inline SDL_Surface *SDL_SetVideoMode_compat(int w, int h, int bpp, Uint32 flags) {
    (void)bpp;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    if (flags & SDL_FULLSCREEN) window_flags |= SDL_WINDOW_FULLSCREEN;
    g_sdl_window = SDL_CreateWindow("Divi-Dead",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        w, h, window_flags);
    if (!g_sdl_window) return NULL;
    return SDL_GetWindowSurface(g_sdl_window);
}
#define SDL_SetVideoMode(w, h, bpp, flags) SDL_SetVideoMode_compat((w), (h), (bpp), (flags))


/* ---- Android asset path fix ---- */
#ifdef __ANDROID__
/* On Android, SDL2 reads files from assets/ when given a relative path.
 * But the engine prepends "./" (from dirname()), which makes SDL2 try
 * the filesystem instead. Strip "./" prefix so assets work. */
static inline SDL_RWops *SDL_RWFromFile_android_fix(const char *file, const char *mode) {
    if (file == NULL) return NULL;
    while (file[0] == '.' && file[1] == '/') file += 2;
    return SDL_RWFromFile(file, mode);
}
#define SDL_RWFromFile(file, mode) SDL_RWFromFile_android_fix((file), (mode))
#endif

#endif /* SDL12_COMPAT_SHIM_H */
