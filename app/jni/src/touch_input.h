#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include "SDL/SDL.h"
#include "main.h"

/* Handle a touch event, returns key mask to OR into `keys` */
extern uint32_t TOUCH_HANDLE_EVENT(SDL_Event *event);

/* Check for long press (call periodically) */
extern uint32_t TOUCH_CHECK_LONG_PRESS(void);

/* Set/clear menu geometry for tap-to-select */
extern void TOUCH_SET_MENU_GEOMETRY(int x, int y, int item_h, int count);
extern void TOUCH_CLEAR_MENU_GEOMETRY(void);

/* Get menu item at touch coords, or -1 if not on a menu item */
extern int TOUCH_GET_MENU_ITEM(float touch_x, float touch_y, int screen_w, int screen_h);

#endif
