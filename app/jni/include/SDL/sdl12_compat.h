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
#define SDL_SRCALPHA    SDL_TRUE
#define SDL_SRCCOLORKEY SDL_TRUE

/* ---- Global window/renderer/texture (set by SDL_SetVideoMode) ---- */
extern SDL_Window *g_sdl_window;

/* ---- Removed functions ---- */

#define SDL_WM_SetCaption(t, i) ((void)0)
#define SDL_EnableKeyRepeat(delay, interval) ((void)0)
#define SDL_SetAlpha(surface, flags, alpha) ((void)0)

/* If GPU renderer is active, these are handled by GAME_SCREEN_UPDATE.
 * Otherwise fall back to window surface updates. */
#define SDL_Flip(surface) ((void)0)
#define SDL_UpdateRect(surface, x, y, w, h) ((void)0)
#define SDL_UpdateRects(surface, numrects, rects) ((void)0)

#define SDL_DisplayFormat(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_RGB888, 0)
#define SDL_DisplayFormatAlpha(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_ARGB8888, 0)

/* SDL_SetVideoMode -> create window + (renderer on Android) */
extern SDL_Surface *screen_video;
/* GPU renderer globals (created lazily on first screen update) */
extern SDL_Renderer *g_sdl_renderer;
extern SDL_Texture *g_sdl_texture;

static inline SDL_Surface *SDL_SetVideoMode_compat(int w, int h, int bpp, Uint32 flags) {
    (void)bpp;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
#ifdef __ANDROID__
    window_flags |= SDL_WINDOW_OPENGL;  /* needed for GPU renderer */
#endif
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
static inline SDL_RWops *SDL_RWFromFile_android_fix(const char *file, const char *mode) {
    if (file == NULL) return NULL;
    while (file[0] == '.' && file[1] == '/') file += 2;
    return SDL_RWFromFile(file, mode);
}
#define SDL_RWFromFile(file, mode) SDL_RWFromFile_android_fix((file), (mode))
#endif

#endif /* SDL12_COMPAT_SHIM_H */
