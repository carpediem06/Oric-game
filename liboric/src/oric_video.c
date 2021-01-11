/***************************************************************************

	vidhrdw/oric.c
	
	All graphic effects are supported including mid-line changes.
	There may be some small bugs.

	TODO:
	- speed up this code a bit?
	
***************************************************************************/

  

#include "oric_machine.h"
#include "oric_video.h"
#include "SDL.h"
#include "charset.h"
#include <stdio.h>
#include "my_assert.h"

#define INLINE inline

#ifdef _WIN32
#include <windows.h>
#endif

#include <string.h>

SDL_Surface *screen;
SDL_Rect dest_rect;

VideoParams vp;

SDL_TimerID refresh_timer_id = 0;
SDL_TimerID flash_timer_id = 0;

typedef unsigned char UINT8;

/* current state of the display */
/* some attributes persist until they are turned off.
This structure holds this persistant information */
struct oric_vh_state
{
	/* foreground and background colour used for rendering */
	/* if flash attribute is set, these two will both be equal
	to background colour */
	int active_foreground_colour;
	int active_background_colour;
	/* current foreground and background colour */
	int foreground_colour;
	int background_colour;
	int mode;
	/* text attributes */
	int text_attributes;

	unsigned long read_addr;

	/* current addr to fetch data */
	unsigned char *char_data;
	/* base of char data */
	unsigned char *char_base;
	
	/* if (1<<3), display graphics, if 0, hide graphics */
	int flash_state;
	/* current count */
	int flash_count;
};


static struct oric_vh_state vh_state;
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 224
#define HIRES_LIMIT 200
#define SCREEN_DEPTH 32

int mask_y_limit = SCREEN_HEIGHT;

static UINT8 *oric_ram;


static int oric_rgb_pen[8] = {
    0x000000,  /* 0: black */
    0xFF0000,  /* 1: red */
    0x00FF00,  /* 2: green */
    0xFFFF00,  /* 3: yellow */
    0x0000FF,  /* 4: blue */
    0xFF00FF,  /* 5: magenta */
    0x00FFFF,  /* 6: cyan */
    0xFFFFFF   /* 7: white */
};
static int oric_sdl_pen[8];

static INLINE int is_hires_active()
{
  int rval = vh_state.mode & (1<<2);

  
  return rval;
}

static INLINE void draw_pixel(SDL_Surface *screen, int px, int py,
		Uint32 color)
{
  int x = px;
  int y = py;
  int screen_pitch = y*screen->pitch;
  
  if (vp.double_size)
  {
      x = px*2;
      y = py*2;
      screen_pitch*=2;
  }
  
  switch (screen->format->BytesPerPixel)
  {
    case 1: // Assuming 8-bpp
      {
        Uint8 *bufp;
        bufp = (Uint8 *)screen->pixels + screen_pitch + x;
        *bufp = color;
	if (vp.double_size)
	{
	    *(bufp+1) = color;
	}
      }
      break;
    case 2: // Probably 15-bpp or 16-bpp
      {
        Uint16 *bufp;
        bufp = (Uint16 *)screen->pixels + screen_pitch/2 + x;
        *bufp = color;
	if (vp.double_size)
	{
	    *(bufp+1) = color;
	}
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      {
        Uint8 *bufp;
        bufp = (Uint8 *)screen->pixels + screen_pitch + x * 3;
        if(SDL_BYTEORDER == SDL_LIL_ENDIAN)
        {
          bufp[0] = color;
          bufp[1] = color >> 8;
          bufp[2] = color >> 16;
        } else {
          bufp[2] = color;
          bufp[1] = color >> 8;
          bufp[0] = color >> 16;
        }
	if (vp.double_size)
	{
	    *(bufp+3) = bufp[0];
	    *(bufp+4) = bufp[1];
	    *(bufp+5) = bufp[2];
	}
      }
      break;
      case 4: // Probably 32-bpp
      {
        Uint32 *bufp;
        bufp = (Uint32 *)screen->pixels + screen_pitch/4 + x;
        *bufp = color;

	if (vp.double_size)
	{
	    *(bufp+1) = color;
	    
	    bufp += screen->pitch/4;
	    *(bufp++) = color;
	    *bufp = color;
	}
      }
      
      break;
      }
}


static inline void draw_6_pixels(SDL_Surface *screen, int px, int py, int sdl_pen_array[])
{
  int x = px;
  int y = py;
  int screen_pitch = y*screen->pitch;
  int i;
  int color = sdl_pen_array[0];
  
  if (vp.double_size)
  {
      x = px*2;
      y = py*2;
      screen_pitch*=2;
  }
  
  switch (screen->format->BytesPerPixel)
  {
    case 1: // Assuming 8-bpp
      {
	  if (vp.double_size)
	  {
	      Uint8 *bufp_s = (Uint8 *)screen->pixels + screen_pitch + x;
	      for (i=0;i<6;i++)
	      {
		  Uint8 *bufp = bufp_s;
		  int color = sdl_pen_array[i];
		  *(bufp) = color;
		  
		  *(bufp+1) = color;
		  bufp += screen->pitch;
		  *(bufp++) = color;
		  *bufp = color;

		  bufp_s+=2;
	      }
	  }
	  else
	  {
	      Uint8 *bufp = (Uint8 *)screen->pixels + screen_pitch + x;
	      for (i=0;i<6;i++)
	      {
		  *(bufp++) = sdl_pen_array[i];
	      }
	  }
      }
      break;
    case 2: // Probably 15-bpp or 16-bpp
      {
        Uint16 *bufp;
        bufp = (Uint16 *)screen->pixels + screen_pitch/2 + x;
        *bufp = color;
	if (vp.double_size)
	{
	    *(bufp+1) = color;
	}
      }
      break;
    case 3: // Slow 24-bpp mode, usually not used
      {
        Uint8 *bufp;
        bufp = (Uint8 *)screen->pixels + screen_pitch + x * 3;
        if(SDL_BYTEORDER == SDL_LIL_ENDIAN)
        {
          bufp[0] = color;
          bufp[1] = color >> 8;
          bufp[2] = color >> 16;
        } else {
          bufp[2] = color;
          bufp[1] = color >> 8;
          bufp[0] = color >> 16;
        }
	if (vp.double_size)
	{
	    *(bufp+3) = bufp[0];
	    *(bufp+4) = bufp[1];
	    *(bufp+5) = bufp[2];
	}
      }
      break;
      case 4: // Probably 32-bpp
      {
	  // optimized to avoid too many computations
	  
	  int sp4 = screen_pitch/4;
	  
	  if (vp.double_size)
	    {
	      int spp4 = screen->pitch/4;
	      Uint32 *bufp_s = (Uint32 *)screen->pixels + sp4 + x;
	      for (i = 0; i < 6; i++)
	      {
		  Uint32 *bufp = bufp_s;
		  color = sdl_pen_array[i];
		  if (color == -1)
		    {
		      bufp+=1+spp4;
		    }
		  else
		    {
		      *bufp = color;
		      *(bufp+1) = color;
		
		      bufp += spp4;
		      *(bufp++) = color;
		      *bufp = color;
		    }
		  bufp_s += 2;			
		}
	  }
	  else
	  {
	      Uint32 *bufp = (Uint32 *)screen->pixels + sp4 + x;
	      
	      for (i = 0; i < 6; i++)
		{
		  if (color == -1)
		    {
		      bufp++;
		    }
		  else
		    {
		      color = sdl_pen_array[i];
		      *(bufp++) = color;
		    }
	      }
	  }
      }
      
      break;
  }
}


static inline void draw_6_pixels_old(SDL_Surface *screen, int x, int y, int sdl_pen_array[])
{
    int i;
    for (i = 0; i < 6; i++)
    {
	int p = sdl_pen_array[i];
	if (p != -1)
	{
	    draw_pixel(screen,x+i,y,p);
	}
    }
}

void screen_lock()
{
  if ( SDL_MUSTLOCK(screen) )
  {
    if ( SDL_LockSurface(screen) < 0 )
    {
      return;
    }
  }
}

void screen_unlock()
{
  if ( SDL_MUSTLOCK(screen) )
  {
    SDL_UnlockSurface(screen);
  }
}
void set_mask_y_limit(int y)
{
    mask_y_limit = y >=0 ? y : SCREEN_HEIGHT;
}

int is_mask_active(int y)
{
    return (y < mask_y_limit);
}

static inline void plot_box(int x, int y, int w, int h, int pen)
{
    if ((pen != vp.mask_color) || !is_mask_active(y))

    {
	if (vp.double_size)
	{
	    dest_rect.x = x*2;
	    dest_rect.y = y*2;
	    dest_rect.w = w*2;
	    dest_rect.h = h*2;
	}
	else
	{
	    dest_rect.x = x;
	    dest_rect.y = y;
	    dest_rect.w = w;
	    dest_rect.h = h;

	}
	SDL_FillRect(screen, &dest_rect, pen);
    }
}




static void oric_vh_update_flash(void);
static void oric_vh_update_attribute(int c);
static void oric_refresh_charset(void);


static void oric_vh_update_flash(void)
{
    /* flash active? */
    if (vh_state.text_attributes & (1<<2))
    {
	/* yes */

	/* show or hide text? */
	if (vh_state.flash_state)
	{
	    /* hide */
	    /* set foreground and background to be the same */
	    vh_state.active_foreground_colour = vh_state.background_colour;
	    vh_state.active_background_colour = vh_state.background_colour;
	    return;
	}
    }
	

    /* show */
    vh_state.active_foreground_colour = vh_state.foreground_colour;
    vh_state.active_background_colour = vh_state.background_colour;
}

/* the alternate charset follows from the standard charset.
   Each charset holds 128 chars with 8 bytes for each char.

   The start address for the standard charset is dependant on the video mode */
static void oric_refresh_charset(void)
{
    /* alternate char set? */
    if ((vh_state.text_attributes & (1<<0))==0)
    {
	/* no */
	vh_state.char_data = vh_state.char_base;
    }
    else
    {
	/* yes */
	vh_state.char_data = vh_state.char_base + (128*8);
    }
}

/* update video hardware state depending on the new attribute */
static void oric_vh_update_attribute(int c)
{
    /* attribute */
    int attribute = c & 0x03f;

    switch ((attribute>>3) & 0x03)
    {
      case 0:
      {
	  /* set foreground colour */
	  vh_state.foreground_colour = attribute & 0x07;
	  oric_vh_update_flash();
      }
      break;

      case 1:
      {
	  vh_state.text_attributes = attribute & 0x07;

	  oric_refresh_charset();

	  /* text attributes */
	  oric_vh_update_flash();
      }
      break;

      case 2:
      {
	  /* set background colour */
	  vh_state.background_colour = attribute & 0x07;
	  oric_vh_update_flash();
      }
      break;

      case 3:
      {
	  /* set video mode */
	  vh_state.mode = attribute & 0x07;

	  /* a different charset base is used depending on the video mode */
	  /* hires takes all the data from 0x0a000 through to about 0x0bf68,
	     so the charset is moved to 0x09800 */
	  /* text mode starts at 0x0bb80 and so the charset is in a different location */
	  if (is_hires_active())
	    {
	      // HIRES
	      /* set screen memory base and standard charset location for this mode */
	      vh_state.read_addr = 0x0a000;
				
	      vh_state.char_base = oric_ram + (unsigned long)0x09800; 
				
	      /* changing the mode also changes the position of the standard charset
		 and alternative charset */
	      oric_refresh_charset();
	  }
	  else
	    {
	      // TEXT
	      /* set screen memory base and standard charset location for this mode */
	      vh_state.read_addr = 0x0bb80;
				
	      vh_state.char_base = oric_ram + (unsigned long)0x0b400;
			
	      /* changing the mode also changes the position of the standard charset
		 and alternative charset */
	      oric_refresh_charset();
	  }
      }
      break;

      default:
	break;
    }
}

/* render 6-pixels using foreground and background colours specified */
/* used in hires and text mode */
static INLINE void oric_vh_render_6pixels(int x,int y, int fg, int bg,int data, int invert_flag)
{
    int mask_active = is_mask_active(y);
    int i;
    int sdl_pens[2];
    int rgb_pens[2];
    int px;
    int sdl_pen_array[6];
    
    /* invert? */
    if (invert_flag)
    {
	fg ^=0x07;
	bg ^=0x07;
    }
    
    if (data != 0)
    {
	sdl_pens[1] = oric_sdl_pen[fg];
	sdl_pens[0] = oric_sdl_pen[bg];
	rgb_pens[1] = oric_rgb_pen[fg];
	rgb_pens[0] = oric_rgb_pen[bg];
	    
	px = x+6;
	int ii = 6;
	// process 6 bits
	for (i=0; i<6; i++)
	{
	    int d = data & 0x1;
	    
	    px--;
	    ii--;
	    if (!mask_active || (rgb_pens[d] != vp.mask_color))
	    {
		sdl_pen_array[ii] = sdl_pens[d];
	    }
	    else
	    {
	      // mask active and RGB = mask color
		sdl_pen_array[ii] = -1;  // don't draw anything, leave background as-is
	    }
	    data = data>>1;
	}
	 switch (screen->format->BytesPerPixel)
	 {
	   case 4:
	   case 1:
	     draw_6_pixels(screen,x,y,sdl_pen_array);
	     break;
	   default:
	     // compatibility mode
	     draw_6_pixels_old(screen,x,y,sdl_pen_array);
	     break;
	 }
	     
    }
    else
    {
	/* often occurs that there is an empty data to plot:
	   this is much faster */
	plot_box(x,y,6,1,oric_rgb_pen[bg]);
    }
}



const unsigned char *video_get_charset()
{
    return charset;
}


static Uint32 oric_vh_timer_callback(Uint32 interval, void *param)
{
  (void)param;
	/* update flash count */
	//vh_state.flash_count++;
	//if (vh_state.flash_count == 30)
	{	
		vh_state.flash_count = 0;
		vh_state.flash_state ^=(1<<3);
		oric_vh_update_flash();
	}

	return interval;
}


static Uint32 refresh_timer_callback(Uint32 interval, void *param)
{
   (void)param;
   video_update();

    return interval;
}

#ifdef _NDS

#include <nds.h>

void video_init(const VideoParams *pvp)
{
    vp = *pvp;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
	char buf[200];
	sprintf(buf, "Unable to initialize SDL: %s", SDL_GetError());
	fatal_error(buf);
    }
    
    int vm = SDL_HWSURFACE|SDL_FULLSCREEN;
    
	// image blit won't work if screenmode is not standard
    screen = SDL_SetVideoMode(320,200,8,vm);
 
     
    consoleDemoInit();
    iprintf("Xenon 1 DS: screen OK\n");
    if (screen == 0)
    {
	char buf[200];
	sprintf(buf, "Unable to open SDL screen: %s\n", SDL_GetError());
	fatal_error(buf);
    }

      
  reset_charset();
    int i;
    
    for (i=0; i<8; i++)
    {
	Uint32 rgb_pen = oric_rgb_pen[i];
	Uint8 R = rgb_pen >> 16;
	Uint8 G = (rgb_pen >> 8) & 0xff;
	Uint8 B = rgb_pen & 0xff;
	
	oric_sdl_pen[i] = SDL_MapRGB(screen->format, R, G, B);
    }
    /* initialise flash timer */
    vh_state.flash_count = 0;
    vh_state.flash_state = 0;
    
    /* mode */
    oric_vh_update_attribute((1<<3)|(1<<4));

    if (vp.palette)
    {
	SDL_Palette *p = vp.palette;
	
	
	SDL_SetColors(screen, p->colors, 0, p->ncolors);
    
    }
    /* initialise refresh timer */

    refresh_timer_id = SDL_AddTimer(vp.update_rate,refresh_timer_callback,0);

    flash_timer_id = 0;//SDL_AddTimer(50*10, oric_vh_timer_callback, 0);
 
    my_assert(refresh_timer_id != 0);

}
#else
void video_init(const VideoParams *pvp)
{
    vp = *pvp;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
	char buf[200];
	sprintf(buf, "Unable to initialize SDL: %s", SDL_GetError());
	fatal_error(buf);
    }
    int sz = vp.double_size ? 2 : 1;
    
    int vm = SDL_HWSURFACE|SDL_DOUBLEBUF;
    if (vp.full_screen)
    {
	vm |= SDL_FULLSCREEN;
	// image blit won't work if screenmode is not standard
	screen = SDL_SetVideoMode(640,480,SCREEN_DEPTH,vm);
    }
    else
    {
	screen = SDL_SetVideoMode(SCREEN_WIDTH*sz,SCREEN_HEIGHT*sz,SCREEN_DEPTH,vm);
    }
     

    if (screen == 0)
    {
	char buf[200];
	sprintf(buf, "Unable to open SDL screen: %s\n", SDL_GetError());
	fatal_error(buf);
    }

    SDL_WM_SetCaption(vp.window_title,vp.window_title);
   
reset_charset();

    int i;
    
    for (i=0; i<8; i++)
    {
	Uint32 rgb_pen = oric_rgb_pen[i];
	Uint8 R = rgb_pen >> 16;
	Uint8 G = (rgb_pen >> 8) & 0xff;
	Uint8 B = rgb_pen & 0xff;
	
	oric_sdl_pen[i] = SDL_MapRGB(screen->format, R, G, B);
    }
    /* initialise flash timer */
    vh_state.flash_count = 0;
    vh_state.flash_state = 0;
    
    /* mode */
    oric_vh_update_attribute((1<<3)|(1<<4));

    if (vp.palette)
    {
	SDL_Palette *p = vp.palette;
	
	
	SDL_SetColors(screen, p->colors, 0, p->ncolors);
    
    }
    /* initialise refresh timer */

    refresh_timer_id = SDL_AddTimer(vp.update_rate,refresh_timer_callback,0);

    flash_timer_id = SDL_AddTimer(50*10, oric_vh_timer_callback, 0);
 
    my_assert(refresh_timer_id != 0 && flash_timer_id != 0);

}
#endif


			
void fatal_error(const char *message)
{
#ifdef _WIN32
    MessageBox(NULL, message, "Fatal Error", MB_OK);
#else
#ifdef _NDS
    iprintf("%s\n",message);
#else
    fprintf(stderr,"%s\n",message);
#endif
    SDL_Quit();
#endif
#ifndef _NDS
    exit(1);
#endif
}
void quit()
{
    if (refresh_timer_id != 0)
    {
	SDL_RemoveTimer(refresh_timer_id);
	refresh_timer_id = 0;
    }
    if (flash_timer_id != 0)
    {
	SDL_RemoveTimer(flash_timer_id);
	flash_timer_id = 0;
    }

    // wait till we're sure that the timers are removed (SDL bug?)
    SDL_Delay(100);
    
    if (vp.quit_callback)
    {
	vp.quit_callback();
    }
    SDL_Quit();
#ifndef _NDS
    exit(0);
#endif
}

void reset_charset()
{
  oric_ram = cpu_get_mem();
    
  memcpy(oric_ram+0xB500,charset,sizeof(charset));

}
/***************************************************************************
  oric_vh_screenrefresh
***************************************************************************/
void video_update()
{
    unsigned char *RAM;
    int byte_offset;
    int y;
    unsigned long read_addr_base;
    int hires_active;

    screen_lock();

    if (vp.background_callback != 0)
    {
	vp.background_callback(screen,vp.update_rate);
    }
    
    RAM = oric_ram;
    
    /* set initial base */
    read_addr_base = vh_state.read_addr;

    /* is hires active? */
    
  hires_active = is_hires_active();



    for (y = 0; y < SCREEN_HEIGHT; y++)
    {
	int x = 0;

	/* foreground colour white */
	oric_vh_update_attribute(7);
	/* background colour black */
	oric_vh_update_attribute((1<<3));

	oric_vh_update_attribute((1<<4));
		
	for (byte_offset=0; byte_offset<40; byte_offset++)
	{
	    int c;
	    unsigned long read_addr;

	    /* after line HIRES_LIMIT all rendering is done in text mode */
	    if (y<HIRES_LIMIT)
	    {
		/* calculate fetch address based on current line and
		   current mode */
		if (hires_active)
		{
		    read_addr = (unsigned long)read_addr_base + (unsigned long)byte_offset + (unsigned long)(y*40);
		}
		else
		{
		    int char_line;

		    char_line = (y>>3);
		    read_addr = (unsigned long)read_addr_base + (unsigned long)byte_offset + (unsigned long)(char_line*40);		
		}
	    }
	    else
	    {
		int char_line;

		char_line = ((y-HIRES_LIMIT)>>3);
		read_addr = (unsigned long)read_addr_base + (unsigned long)byte_offset + (unsigned long)(char_line*40);		
	    }

	    /* fetch data */
	    c = RAM[read_addr];

	    /* if bits 6 and 5 are zero, the byte contains a serial attribute */
	    if ((c & ((1<<6) | (1<<5)))==0)
	    {
		oric_vh_update_attribute(c);

		/* display background colour when attribute has been found */
		oric_vh_render_6pixels(x,y,vh_state.active_foreground_colour, vh_state.active_background_colour, 0,(c & 0x080));

		if (y<HIRES_LIMIT)
		{				
		    /* is hires active? */
		    hires_active = is_hires_active();
					
		    read_addr_base = vh_state.read_addr;
		}
	    }
	    else
	    {
		/* hires? */
		if (hires_active)
		{
		    int pixel_data = c & 0x03f;
		    /* plot hires pixels */
		    oric_vh_render_6pixels(x,y,vh_state.active_foreground_colour, vh_state.active_background_colour, pixel_data,(c & 0x080));
		}
		else
		{
		    int char_index;
		    int char_data;
		    int ch_line;

		    char_index = (c & 0x07f);

		    ch_line = y & 7;

		    /* is double height set? */
		    if (vh_state.text_attributes & (1<<1))
		    { 
			/* if char line is even, top half of character is displayed,
			   if char line is odd, bottom half of character is displayed */
			int double_height_flag = ((y>>3) & 0x01);

			/* calculate line to fetch */
			ch_line = (ch_line>>1) + (double_height_flag<<2);
		    }
					
		    /* fetch pixel data for this char line */
		    char_data = vh_state.char_data[(char_index<<3) | ch_line] & 0x03f;

		    /* draw! */
		    oric_vh_render_6pixels(x,y,
					   vh_state.active_foreground_colour, 
					   vh_state.active_background_colour, char_data, (c & 0x080));
		}

	    }
			
	    x+=6;	
	}
		
	/* after HIRES_LIMIT lines have been drawn, force a change of the read address */
	/* there are HIRES_LIMIT lines of hires/text mode, then 24 lines of text mode */
	/* the mode can't be changed in the last 24 lines. */
	if (y==HIRES_LIMIT-1)
	{
	    /* mode */
	    read_addr_base = (unsigned long)0x0bf68;
	    hires_active = 0;
	}
    }
    if (vp.foreground_callback != 0)
    {
	vp.foreground_callback(screen,vp.update_rate);
    }
    screen_unlock();
    SDL_Flip(screen);
}



