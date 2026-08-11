#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include "SDL/SDL.h"
#include "main.h"

/* Handle a touch event, returns key mask to OR into `keys` */
extern uint32_t TOUCH_HANDLE_EVENT(SDL_Event *event);

/* Check for long press (call periodically) */
extern uint32_t TOUCH_CHECK_LONG_PRESS(void);

#endif
