/*
 * SDL 1.2 to SDL 2.0 compatibility shim for Divi-Dead engine.
 *
 * The engine was written for SDL 1.2. SDL 2.0 removed/renamed many
 * functions and macros. This header provides shims so the engine
 * compiles against SDL 2.0 without extensive source changes.
 *
 * Include this AFTER all other SDL headers.
 */

#ifndef SDL12_COMPAT_SHIM_H
#define SDL12_COMPAT_SHIM_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

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

/* ---- Removed functions ---- */

/* SDL_WM_SetCaption -> SDL_SetWindowTitle */
static inline void SDL_WM_SetCaption_compat(const char *title, const char *icon) {
    (void)icon;
    /* SDLActivity creates the window; we can't easily get it here.
     * The title is set via AndroidManifest android:label instead. */
}
#define SDL_WM_SetCaption(t, i) SDL_WM_SetCaption_compat((t), (i))

/* SDL_EnableKeyRepeat -> no-op (key repeat is default in SDL2) */
#define SDL_EnableKeyRepeat(delay, interval) ((void)0)

/* SDL_SetAlpha -> no-op (per-surface alpha handled differently in SDL2) */
#define SDL_SetAlpha(surface, flags, alpha) ((void)0)

/* SDL_Flip -> SDL_UpdateWindowSurface */
#define SDL_Flip(surface) SDL_UpdateWindowSurface(SDL_GetWindowFromSurface(surface))

/* SDL_UpdateRect -> SDL_UpdateWindowSurface */
#define SDL_UpdateRect(surface, x, y, w, h) \
    SDL_UpdateWindowSurface(SDL_GetWindowFromSurface(surface))

/* SDL_UpdateRects -> SDL_UpdateWindowSurfaceRects */
#define SDL_UpdateRects(surface, numrects, rects) \
    SDL_UpdateWindowSurfaceRects(SDL_GetWindowFromSurface(surface), (rects), (numrects))

/* SDL_DisplayFormat / SDL_DisplayFormatAlpha */
#define SDL_DisplayFormat(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_RGB888, 0)
#define SDL_DisplayFormatAlpha(surface) \
    SDL_ConvertSurfaceFormat((surface), SDL_PIXELFORMAT_ARGB8888, 0)

/* SDL_SetVideoMode -> create window + get surface
 * screen_video is a global declared in the engine. */
extern SDL_Surface *screen_video;
static inline SDL_Surface *SDL_SetVideoMode_compat(int w, int h, int bpp, Uint32 flags) {
    (void)bpp;
    (void)flags;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    if (flags & SDL_FULLSCREEN) window_flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *win = SDL_CreateWindow("Divi-Dead",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        w, h, window_flags);
    if (!win) return NULL;
    return SDL_GetWindowSurface(win);
}
#define SDL_SetVideoMode(w, h, bpp, flags) SDL_SetVideoMode_compat((w), (h), (bpp), (flags))

#endif /* SDL12_COMPAT_SHIM_H */
