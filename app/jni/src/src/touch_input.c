/*
 * touch_input.c - Touch gesture handling for Android/mobile platforms
 *
 * Gestures:
 *   - Tap on menu item    -> Select that item (K_A)
 *   - Tap elsewhere        -> Advance text (K_A)
 *   - Swipe up             -> Open main menu (K_L)
 *   - Swipe down           -> Open gallery (K_R)
 *   - Swipe left/right     -> Navigate (K_LEFT/K_RIGHT)
 *   - Long press (500ms)   -> Cancel (K_B)
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

#define SWIPE_DISTANCE  0.15f
#define SWIPE_MAX_TIME  500
#define TAP_MAX_TIME    300
#define LONG_PRESS_TIME 500
#define TAP_DISTANCE    0.05f

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
                        result |= K_B;
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
                    if (duration < TAP_MAX_TIME) {
                        /* Tap - always send K_A (select/advance text) */
                        result |= K_A;
                    }
                } else if (duration < SWIPE_MAX_TIME) {
                    if (abs_dx > abs_dy) {
                        result |= (dx > 0) ? K_RIGHT : K_LEFT;
                    } else {
                        result |= (dy < 0) ? K_L : K_R;
                    }
                }
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
                return K_B;
            }
        }
    }
    return 0;
}

/* Menu geometry for tap-to-select */
typedef struct {
    int active;       /* Is a menu currently shown? */
    int x, y;         /* Top-left of menu area */
    int item_h;       /* Height of each menu item */
    int count;        /* Number of items */
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

/* Get the menu item index at the given touch coordinates (0.0-1.0).
 * Returns -1 if not tapping on a menu item.
 * screen_w/h are the actual window dimensions for coordinate conversion.
 */
int TOUCH_GET_MENU_ITEM(float touch_x, float touch_y, int screen_w, int screen_h) {
    if (!menu_geom.active || menu_geom.count <= 0) return -1;
    
    /* Convert normalized touch coords to screen pixels */
    int screen_x = (int)(touch_x * screen_w);
    int screen_y = (int)(touch_y * screen_h);
    
    /* Calculate the game's render area on screen (letterboxed 4:3) */
    double scale_x = (double)screen_w / 640.0;
    double scale_y = (double)screen_h / 480.0;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    int game_w = (int)(640 * scale);
    int game_h = (int)(480 * scale);
    int offset_x = (screen_w - game_w) / 2;
    int offset_y = (screen_h - game_h) / 2;
    
    /* Check if tap is within the game render area (not on black bars) */
    if (screen_x < offset_x || screen_x >= offset_x + game_w) return -1;
    if (screen_y < offset_y || screen_y >= offset_y + game_h) return -1;
    
    /* Convert screen pixels to 640x480 game coordinates */
    int game_x = (int)((screen_x - offset_x) * 640.0 / game_w);
    int game_y = (int)((screen_y - offset_y) * 480.0 / game_h);
    
    /* Find which menu item was tapped */
    if (game_y < menu_geom.y) return -1;
    
    int item = (game_y - menu_geom.y) / menu_geom.item_h;
    if (item < 0 || item >= menu_geom.count) return -1;
    
    return item;
}
