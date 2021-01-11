#ifndef _ORIC_INPUT
#define _ORIC_INPUT

#include "SDL.h"

void joystick_init(int enabled);
int joystick_get_axis(int numjoy,int axis);
int joystick_get_button(int numjoy,int button);

int is_key_pressed(SDLKey k);
const Uint8 *get_keyboard_state();
void flush_events();

#endif
