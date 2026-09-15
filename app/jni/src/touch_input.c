/*
 * touch_input.c - Touch gesture handling v2 for Android
 *
 * Redesigned gesture system for visual novel gameplay:
 *
 * READING (in-game text):
 *   Tap anywhere         → Advance text (K_A)
 *   Swipe right          → Open in-game menu (K_L)
 *   Swipe left           → Back/cancel (K_B)
 *   Swipe up             → Navigate up (K_UP)
 *   Swipe down           → Navigate down (K_DOWN)
 *   Long press (600ms)   → Gallery/extra menu (K_R)
 *
 * MENU (title/options/language):
 *   Tap on menu item     → Select that item directly
 *   Tap elsewhere        → Select highlighted item (K_A)
 *   Swipe up/down        → Navigate items (K_UP/K_DOWN)
 *   Swipe left           → Cancel/back (K_B)
 *   Swipe right          → Confirm (K_A)
 *   Long press           → Gallery (K_R)
 *
 * CHOICES (in-game multiple choice):
 *   Tap on choice        → Select that choice
 *   Swipe up/down        → Navigate choices
 *   Swipe right          → Confirm (K_A)
 *   Swipe left           → Cancel (K_B)
 */

#include "SDL/SDL.h"
#include "main.h"

typedef struct {
    int active;
    float start_x, start_y;
    float last_x, last_y;
    Uint32 start_time;
    Uint32 last_time;
    int long_press_fired;
} TouchState;

static TouchState touch_state = {0};

/* Tunable thresholds */
#define SWIPE_DISTANCE   0.12f   /* 12% of screen = swipe */
#define SWIPE_MAX_TIME   400     /* must complete within 400ms */
#define TAP_MAX_TIME    250      /* quick tap = < 250ms */
#define LONG_PRESS_TIME 600      /* hold 600ms = long press */
#define TAP_DISTANCE    0.04f   /* max movement for tap = 4% */

uint32_t TOUCH_HANDLE_EVENT(SDL_Event *event) {
    uint32_t result = 0;

    switch (event->type) {
        case SDL_FINGERDOWN:
            touch_state.active = 1;
            touch_state.start_x = event->tfinger.x;
            touch_state.start_y = event->tfinger.y;
            touch_state.last_x = event->tfinger.x;
            touch_state.last_y = event->tfinger.y;
            touch_state.start_time = SDL_GetTicks();
            touch_state.last_time = touch_state.start_time;
            touch_state.long_press_fired = 0;
            break;

        case SDL_FINGERMOTION:
            if (touch_state.active) {
                touch_state.last_x = event->tfinger.x;
                touch_state.last_y = event->tfinger.y;
                touch_state.last_time = SDL_GetTicks();

                if (!touch_state.long_press_fired &&
                    (touch_state.last_time - touch_state.start_time) > LONG_PRESS_TIME) {
                    float dx = touch_state.last_x - touch_state.start_x;
                    float dy = touch_state.last_y - touch_state.start_y;
                    if ((dx*dx + dy*dy) < (TAP_DISTANCE * TAP_DISTANCE)) {
                        result |= K_R;  /* long press = gallery */
                        touch_state.long_press_fired = 1;
                    }
                }
            }
            break;

        case SDL_FINGERUP:
            if (touch_state.active) {
                touch_state.active = 0;
                Uint32 duration = touch_state.last_time - touch_state.start_time;
                float dx = touch_state.last_x - touch_state.start_x;
                float dy = touch_state.last_y - touch_state.start_y;
                float abs_dx = dx > 0 ? dx : -dx;
                float abs_dy = dy > 0 ? dy : -dy;

                if (touch_state.long_press_fired) break;

                if (abs_dx < TAP_DISTANCE && abs_dy < TAP_DISTANCE) {
                    /* TAP */
                    if (duration < TAP_MAX_TIME) {
                        result |= K_A;
                    } else if (duration < LONG_PRESS_TIME) {
                        /* Slow tap - also K_A (more forgiving) */
                        result |= K_A;
                    }
                } else if (duration < SWIPE_MAX_TIME) {
                    /* SWIPE - determine direction */
                    if (abs_dx > abs_dy) {
                        /* Horizontal swipe */
                        if (dx > 0) {
                            result |= K_L;  /* swipe right = open menu */
                        } else {
                            result |= K_B;  /* swipe left = cancel/back */
                        }
                    } else {
                        /* Vertical swipe */
                        if (dy < 0) {
                            result |= K_UP;  /* swipe up = navigate up */
                        } else {
                            result |= K_DOWN; /* swipe down = navigate down */
                        }
                    }
                }
                /* Long slow drag (>400ms, >tap distance) = ignore */
            }
            break;
    }
    return result;
}

uint32_t TOUCH_CHECK_LONG_PRESS(void) {
    if (touch_state.active && !touch_state.long_press_fired) {
        Uint32 now = SDL_GetTicks();
        if ((now - touch_state.start_time) > LONG_PRESS_TIME) {
            float dx = touch_state.last_x - touch_state.start_x;
            float dy = touch_state.last_y - touch_state.start_y;
            if ((dx*dx + dy*dy) < (TAP_DISTANCE * TAP_DISTANCE)) {
                touch_state.long_press_fired = 1;
                return K_R;
            }
        }
    }
    return 0;
}

/* ---- Menu geometry for tap-to-select ---- */
typedef struct {
    int active;
    int x, y;
    int item_h;
    int count;
} MenuGeometry;

static MenuGeometry menu_geom = {0};

void TOUCH_SET_MENU_GEOMETRY(int x, int y, int item_h, int count) {
    menu_geom.active = 1;
    menu_geom.x = x;
    menu_geom.y = y;
    menu_geom.item_h = item_h;
    menu_geom.count = count;
}

void TOUCH_CLEAR_MENU_GEOMETRY(void) {
    menu_geom.active = 0;
}

int TOUCH_GET_MENU_ITEM(float touch_x, float touch_y, int screen_w, int screen_h) {
    if (!menu_geom.active || menu_geom.count <= 0) return -1;

    /* Get real window size (screen_w/h may be 640x480 dummy surface) */
    extern SDL_Window *g_sdl_window;
    int real_w = screen_w, real_h = screen_h;
    if (g_sdl_window) {
        SDL_GetWindowSize(g_sdl_window, &real_w, &real_h);
    }

    int screen_x = (int)(touch_x * real_w);
    int screen_y = (int)(touch_y * real_h);

    double scale_x = (double)real_w / 640.0;
    double scale_y = (double)real_h / 480.0;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    int game_w = (int)(640 * scale);
    int game_h = (int)(480 * scale);
    int offset_x = (real_w - game_w) / 2;
    int offset_y = (real_h - game_h) / 2;

    if (screen_x < offset_x || screen_x >= offset_x + game_w) return -1;
    if (screen_y < offset_y || screen_y >= offset_y + game_h) return -1;

    int game_x = (int)((screen_x - offset_x) * 640.0 / game_w);
    int game_y = (int)((screen_y - offset_y) * 480.0 / game_h);

    if (game_y < menu_geom.y) return -1;

    int item = (game_y - menu_geom.y) / menu_geom.item_h;
    if (item < 0 || item >= menu_geom.count) return -1;

    return item;
}
