#ifndef _ORIC_VIDEO_H
#define _ORIC_VIDEO_H

#include "SDL.h"

typedef void (*GfxCallback)(SDL_Surface *,int);
typedef void (*QuitCallback)();

typedef struct VideoParams_
{
    const char *window_title;
    int double_size;   // 1=double video size
    int update_rate;   // in ms
    int full_screen;   // 1=full screen else windowed
    int mask_color;    // -1: no mask
    GfxCallback background_callback;
    GfxCallback foreground_callback;
    QuitCallback quit_callback;
    SDL_Palette *palette;
} VideoParams;

void reset_charset();
void video_update();
// pass -1 to cancel the limit
void set_mask_y_limit(int y);
void video_init(const VideoParams *vp);
void quit();
void fatal_error(const char *message);

const unsigned char *video_get_charset();

#endif
