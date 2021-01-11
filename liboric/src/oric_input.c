#include "SDL.h"

#include <strings.h>

static SDL_Joystick *tableJoysticks[4];


void joystick_init(int enabled)
{
    memset(tableJoysticks,0,sizeof(tableJoysticks));
    if (enabled)
    {
	int numJoystick = SDL_NumJoysticks();
	int i;
	if(numJoystick > 4)
	{
	    //printf("Seuls les quatre premiers joysticks seront pris en compte\n");
	    numJoystick = 4;
	}
	for(i=0 ; i < numJoystick ; i++)
	{
	    //printf("Nom du joystick %i : %s\n", i, SDL_JoystickName(i));
	tableJoysticks[i] = SDL_JoystickOpen(i);
    
	/*if(tableJoysticks[i])
	{
	    printf("Nombre d'axes : %d\n",     SDL_JoystickNumAxes   (tableJoysticks[i]));
	    printf("Nombre de boutons : %d\n", SDL_JoystickNumButtons(tableJoysticks[i]));
	    printf("... joystick ouvert\n");
	}
	else
	{
	    printf("... probleme d'ouverture avec ce joystick\n");
	    }*/
    
    }
    }
     

}
int joystick_get_button(int numjoy,int button)
{
    int rval = 0;
    SDL_Joystick *j = tableJoysticks[numjoy];
    if (j != NULL)
    {
	rval = SDL_JoystickGetButton(j, button);
    }
    return rval;
}
int joystick_get_axis(int numjoy,int axis)
{
    int rval = 0;
    SDL_Joystick *j = tableJoysticks[numjoy];
    if (j != NULL)
    {
	rval = SDL_JoystickGetAxis(j, axis);
    }
    return rval;
}

int is_key_pressed(SDLKey k)
{
    Uint8 *keystate = SDL_GetKeyState(NULL);
    return (keystate[k]);
}

const Uint8 *get_keyboard_state()
{
    return SDL_GetKeyState(NULL);
}


void flush_events()
{
    SDL_Event event;

    while (SDL_PollEvent(&event));
}
