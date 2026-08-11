/*
 * touch_input.c - Touch gesture handling for Android/mobile platforms
 *
 * Gesture to key mapping:
 *   - Tap (quick touch)       -> K_A (select/confirm)
 *   - Swipe up                 -> K_L (open main menu)
 *   - Swipe down               -> K_R (open gallery/extra menu)
 *   - Swipe left               -> K_LEFT
 *   - Swipe right              -> K_RIGHT
 *   - Long press (hold 500ms)  -> K_B (cancel/back)
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
                    if (duration < TAP_MAX_TIME) result |= K_A;
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
