#include "oric_video.h"
#include "oric_machine.h"
#include "oric_sound.h"
#include "oric_basic.h"
#include "oric_input.h"
#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct _ModeParams
{
  T_INT start_address;
  T_INT cursor_x;
  T_INT cursor_y;
  T_INT paper;
  T_INT ink;
  T_INT nb_lines;
  T_INT text_y_start;
} ModeParams;

typedef struct
{
  int invisible_cursor;
  int disable_output;
  int lower_case;
  int double_line_printing;
  int key_click;
  int escape;
}
PrintParams;
  
static PrintParams print_params;
static ModeParams text_params;
static ModeParams hires_params;
static ModeParams hires_text_params;

static T_INT hires_mode = -1;  // invalid so if basic_init() is not called we see it when calling TEXT or HIRES
static T_INT hires_pattern = 0;
static T_INT current_hires_pattern = 0;

static int escape=0;

static void update_paper_and_ink();

void basic_init()
{
  hires_mode = 0;
  // text screen
  text_params.start_address = 0xBB80;
  text_params.cursor_x = 2;
  text_params.cursor_y = 1;
  text_params.text_y_start = 1;
  text_params.paper = 7;
  text_params.ink = 0;
  text_params.nb_lines = 28;
  
  // hires screen
  hires_params.start_address = 0xA000;
  hires_params.cursor_x = 0;
  hires_params.cursor_y = 0;
  hires_params.text_y_start = 0;
  hires_params.paper = 0;
  hires_params.ink = 7;
  hires_params.nb_lines = 200;

  // 3 text lines (hires status)
  hires_text_params.start_address = 0xBF68;
  hires_text_params.cursor_x = 2;
  hires_text_params.cursor_y = 1;
  hires_text_params.text_y_start = 0;
  hires_text_params.paper = 7;
  hires_text_params.ink = 0;
  hires_text_params.nb_lines = 3;

}
static void wait_for_keys_up()
{
  while(key$()[0]!=0) {wait(5);}
}

static ModeParams *get_text_mode_params()
{
  return hires_mode == 0 ? &text_params : &hires_text_params;
}
static ModeParams *get_mode_params()
{
  return hires_mode == 0 ? &text_params : &hires_params;
}


static inline T_BYTE *get_start(const ModeParams *p)
{
    return cpu_get_mem() + p->start_address; // (hires_mode == 0 ? 0xBB80 : );
}
inline T_BYTE *get_text_start()
{
  return get_start(&text_params);
}

T_BYTE scrn(T_INT x,T_INT y)
{
   const T_BYTE *ts = get_start(&text_params);
   return ts[x+y*40];
}

static inline T_BYTE *get_current_char_pos(const ModeParams *p)
{
   return get_start(p)+p->cursor_x+p->cursor_y*40;
}

static void carriage_return(ModeParams *p)
{
    p->cursor_x=2;
}

static void linefeed(ModeParams *p)
{
  

  p->cursor_y++;
  if (p->cursor_y==p->nb_lines)
    {
      T_BYTE *ts = get_start(p)+40*p->text_y_start;
      T_INT block_size = 40*(p->nb_lines-p->text_y_start);
      p->cursor_y--;
      // scroll
      memmove(ts - 40 , ts, block_size);
      memset(ts+block_size-38,32,38); // preserve attributes
    }

}
static void next_char(ModeParams *p)
{
  p->cursor_x++;
  if (p->cursor_x==40)
    {
      crlf(p);
    }
}
static void prev_char(ModeParams *p)
{
  p->cursor_x--;
  if (p->cursor_x==-1)
    {
      p->cursor_y--;
      p->cursor_x=39;
    }
}


void print(const T_STRING_PTR first_arg,...)
{

  if (first_arg==0)
    {
      crlf();
    }
  else
    {
      va_list ap;
      va_start(ap,first_arg);
      print_string(first_arg);
      while(1)
	{
	  T_STRING_PTR na = va_arg(ap,T_STRING_PTR);
	  if(na==0)break;
	  switch ((int)na)
	    {
	      case (int)PRINT_LINEFEED: 
		crlf();
		break;
	      case (int)PRINT_COMMA_SEPARATOR: 
		do
		  {
		    print_string(" ");
		  }
		  while(text_params.cursor_x % 5 != 0);
		  break;
	      case (int)PRINT_TAB_SEPARATOR: break;
	      default:
		print_string(na);
	    }
	}
      va_end(ap);
    }
}

void print_char(ModeParams *p,const char t)
{
  T_BYTE *ts = get_current_char_pos(p);
  if (!print_params.disable_output)
    {
  *ts = t;
  if (print_params.double_line_printing)
    {
      int last_line = (p->cursor_y==p->nb_lines-1);
      linefeed(p);
      ts = get_current_char_pos(p);
      *ts = t;
      if (!last_line)
	{
	  p->cursor_y--;
	}
      
    }
  }
}

void print_char_and_advance(ModeParams *p,const char t)
{
  /*
  0 Null
1 Copy CTRL-A
2
3 Break CTRL-C
4 Double line printing CTRL-D
5
6 Keyclick CTRL-F
7 Bell (PING) CTRL-G
8 Backspace (Cursor left) CTRL-H
9 Cursor right CTRL-1
10 Line feed (Cursor down) CTRL-J
11 Cursor up CTRL-K
12 Clear screen CTRL-L
13 RETURN CTRL-M
14 Clear line CTRL-N
15 Disable screen CTRL-0
16
17 Cursor CTRL-Q
18
19 Screen CTRL-S
20 Caps (upper case) CTRL-T
24 Cancel line CTRL-X*/
  
  switch(t)
    {
      case 10:
	linefeed(p);
	break;
      case 13:
	carriage_return(p);
	break;
      case 12:
	cls();
	break;
	case 14:
	break; // clear line
      case 17:
	print_params.invisible_cursor ^= 1;
	break;
      case 19:
	print_params.disable_output ^= 1;
	break;
      case 20:
	print_params.lower_case ^= 1;
	memset(get_text_start()+35,7,print_params.lower_case ? 0 : 7);

	
	break;
      case 4:
	print_params.double_line_printing ^= 1;
	break;
      case 6:
	print_params.key_click ^= 1;
	break;
      case 7:
	ping();
	break;
      case 8:
	break; // backspace
      case 27:
	print_params.escape=1;
	break;
      default:
	if (t>31)
	  {
	    if (print_params.escape)
	      {
		print_params.escape=0;
		print_char(p, t-64);
	      }
	    else
	      {
		print_char(p, t);
	      }
	  }
	
    }
  if (!print_params.escape)
    {
      next_char(p);
    }
}


void crlf()
{
  ModeParams *p = get_text_mode_params();
  carriage_return(p);
  linefeed(p);
}

void print_string(const T_STRING_PTR t)
{
  int lt = strlen(t);
  int i;
  ModeParams *p = get_text_mode_params();

  for (i=0;i<lt;i++)
    {
      print_char_and_advance(p,t[i]);
    }
}

T_INT pos(T_INT m)
{
  if (m!=0)
    {
      error(ILLEGAL_QUANTITY,"POS",0);
    }
  return get_text_mode_params()->cursor_x;
}

void input(T_STRING_PTR first_arg,...)
{
  // first count nb params
  ModeParams *p = get_text_mode_params();
  int i,nb_params = 0;
  va_list ap;
  va_start(ap,first_arg);
  int ok=0;
  char current_key = 0;
  T_STRING s;
  T_STRING_PTR cs,output_string = first_arg;
  // compute the number of params
  while(1)
    {
      T_STRING_PTR na = va_arg(ap,T_STRING_PTR);
      if(na==0)break;
      nb_params++;
    }
  va_end(ap);

  // not a simple INPUT A$
  if (nb_params>0)
    {
      print_string(first_arg);
      va_start(ap,first_arg);
      for (i=0;i<nb_params-1;i++)
	{
	  T_STRING_PTR na = va_arg(ap,T_STRING_PTR);
	  print_string(na);
	}
      output_string = va_arg(ap,T_STRING_PTR);
      va_end(ap);
    }
  cs = output_string;
  print_string("? ");
  // wait for all keys up

  while(1)
    {
      
      int full = (cs-output_string) == (sizeof(T_STRING)-1);
      get(s);
      current_key = s[0];
      gc_strings();

      *cs = current_key;
      
      switch (current_key)
	{
	  case SDLK_RETURN:
	    if (cs>output_string)
	      {
		*cs = 0;
		ok = 1;
		crlf();
	      }
	    break;
	  case SDLK_BACKSPACE:
	    if (cs>output_string)
	      {
		*cs = 0;
		cs--;
		prev_char(p);
		    print_char(p,' ');
		  }
		break;
	      default:
		if (!full)
		  {
		    print_char(p,*cs);
		    next_char(p);
		    cs++;
		  }
		break;
	    }
	  
	  if (ok)
	    {break;}
	  
	  
	  
	}
    
    
}
static void handle_keypress(T_STRING_PTR s,int i,int shift_pressed)
{
    static const char *shifted= ")!@#$%^&*(";

  if (print_params.lower_case && !shift_pressed)
    {
      s[0] = i;
    }
  else
    {
      s[0]=toupper(i);
    }
	  
  if (shift_pressed)
    {
      if (i>='0' && i<='9')
	{
	  s[0] = shifted[i-'0'];
	}
    }
}

T_STRING_PTR key$()
{
  static const int special[] = { SDLK_BACKSPACE,SDLK_RETURN, SDLK_ESCAPE };
  
  int i;
  
  T_STRING_PTR s=alloc_string();

  const Uint8* ks;
  s[0]=s[1]=0;
  flush_events();
  ks = get_keyboard_state();
  for (i=32;i<128;i++)
    {

      if (ks[i])
	{
	  handle_keypress(s,i,ks[SDLK_RSHIFT] || ks[SDLK_LSHIFT]);

	  break;
	}
    }
  for (i=0;i<3;i++)
    {
      int si = special[i];
      if (ks[si])
	{
	  s[0] = si;
	  break;
	}
    }
  return s;
}


void get(T_STRING_PTR s)
{
  SDL_Event event;
  int ctrl_pressed = 0;
  int shift_pressed = 0;
  int real_key = 0;
  s[1] = 0;
  while (SDL_WaitEvent(&event))
    {
      flush_events();
	  
      switch(event.type)
	{
	  case SDL_QUIT:
	    quit();
	    break;
	  case SDL_KEYUP:	      
	    switch (event.key.keysym.sym)
	      {
		case SDLK_RSHIFT:
		case SDLK_LSHIFT:
		  shift_pressed = 0;
		  break;
		case SDLK_RCTRL:
		case SDLK_LCTRL:
		  ctrl_pressed = 0;
		  break;
		default:
		  break;
	      }
	    break;
	  case SDL_KEYDOWN:	
	    if (!ctrl_pressed)
	      {
		switch (event.key.keysym.sym)
		  {
		    case SDLK_RSHIFT:
		    case SDLK_LSHIFT:
		      shift_pressed = 1;
		      break;
		    case SDLK_RCTRL:
		    case SDLK_LCTRL:
		      ctrl_pressed = 1;
		      break;
		    default:
		      handle_keypress(s,event.key.keysym.sym,shift_pressed);
		      real_key = 1;
		      break;
		  }
	      }
	    break;
	  default:
	    break;
	}		    
      if (real_key)
	{
	  break;
	}
      
    }
}

void oldget(T_STRING_PTR s)
{
  wait_for_keys_up();
  
  while (1)
    {
      T_STRING_PTR r = key$();
      if (r[0] == 0)
	{
	  // do not overload the CPU as this is supposed to be a blocking call
	  gc_strings();
	  wait(5);
    
	}
      else
	{
	  s[0]=r[0];
	  break;
	}
    }
	      
}

void lores(T_BYTE mode)
{
  int i;
  T_BYTE *start = get_text_start();
  text();
  
  for (i=text_params.text_y_start;i<text_params.nb_lines;i++)
    {
      start[i*40] = 8+mode;
    }
  
}
	
void pattern(T_INT value)
{
  if (value<0 || value>255)
    {
      error(ILLEGAL_QUANTITY,"PATTERN",0);
    }
  hires_pattern = value;
}

void cls()
{
  ModeParams *p = get_text_mode_params();
  T_BYTE *start = get_start(p);
  memset(start, 32, p->nb_lines*40);
  p->cursor_x = 2;
  p->cursor_y = p->text_y_start;
  update_paper_and_ink(p);
  escape=0;
}

static inline T_INT pixel_address(T_INT x,T_INT y)
{
  return hires_params.start_address + x/6 + y*40;
}

T_INT point(T_INT nx, T_INT ny)
{
  if (nx>=0 && nx<240 && ny>=0 && ny<200)
    {
      T_INT pa = pixel_address(nx,ny);
      int remainder_bit = 1<<(5-(nx % 6));
      T_BYTE new_value = peek(pa);
      return new_value & remainder_bit ? ORIC_TRUE : ORIC_FALSE;
    }
  else
    {
      error(ILLEGAL_QUANTITY,"POINT",str$(nx),str$(ny),0);
      return ORIC_FALSE;
    }
  
      
}

void curset(T_INT nx, T_INT ny, T_INT fb)
{
  if (nx>=0 && nx<240 && ny>=0 && ny<200)
    {
      hires_params.cursor_x = nx;
      hires_params.cursor_y = ny;
      
      switch(fb)
	{
	  case 3:
	    break;
	  case 0:
	  case 2:
	  case 1:
	    {
		  // mark pixel
	      T_INT pa = pixel_address(nx,ny);
	      T_BYTE new_value = peek(pa);
	      if (new_value & 0x40)
		{
		  int remainder_bit = 1<<(5-(nx % 6));
		  if (fb==0) 
		    { 
		      new_value &= ((!remainder_bit) & 0x3F) | 0x40;
		    }
		  else if (fb==1)
		    {
		      new_value |= remainder_bit;
		    }
		  else if (fb==2)
		    {
		      new_value ^= remainder_bit;
		    }
		  poke(pa,new_value);
		}
	      
	      break;
	    }
	  default:
	    error("CURSET",": unsupported fb mode",str$(fb),0);
	    break;
	}
    }
  else
    {
      error("CURSET",": ",ILLEGAL_QUANTITY," ",str$(nx)," ",str$(ny),0);
    }

}

void curmov(T_INT x, T_INT y, T_INT fb)
{
  int nx = hires_params.cursor_x + x;
  int ny = hires_params.cursor_y + y;
  curset(nx,ny,fb);

}
static void init_current_pattern()
{
  current_hires_pattern = hires_pattern;
}
static void next_current_pattern()
{
  // ROL
  current_hires_pattern = ((current_hires_pattern<<1) & 0xFF) | ((current_hires_pattern>>7)&0x1);
}

void draw(T_INT delta_x,T_INT delta_y,T_INT fb)
{
  T_INT saved_x = hires_params.cursor_x;
  T_INT saved_y = hires_params.cursor_y;
  T_INT to_x = saved_x+delta_x;
  T_INT to_y = saved_y+delta_y;
  int x_step = sgn(delta_x);
  int y_step = sgn(delta_y);
  int local_fb = fb;
  
  if (to_x<0) { to_x = 0; }
  else if (to_x>239) { to_x = 239; }
  if (to_y<0) { to_y = 0; }
  else if (to_y>199) { to_y = 199; }
  
  init_current_pattern();
  
  if (x_step==0)
    {
      // horizontal line
      int i=saved_y;
      while (i!=to_y)
	{
	  if ((fb!=3)&&(hires_pattern!=255))
	    {
	      next_current_pattern();
	      local_fb = current_hires_pattern&1 ? fb : 3;
	    }
	  
	  curset(saved_x,i,local_fb);
	  i+=y_step;

	}
    }
  else if (y_step==0)
    {
      // vertical line
      int i=saved_x;
      while (i!=to_x)
	{
	  if ((fb!=3)&&(hires_pattern!=255))
	    {
	      local_fb = current_hires_pattern&1 ? fb : 3;
	      next_current_pattern();
	    }
	  curset(i,saved_y,local_fb);
	  i+=x_step;
	}
    }
  else
    {
      // diagonal: x and y vary
      float x_variation = ((float)(to_x-saved_x))/(to_y-saved_y);
      int current_x,target_x;
      int current_y = saved_y;
      int previous_x = saved_x;
      
      while (current_y!=to_y)
	{

	  target_x = (int)(saved_x+(x_variation*(current_y-saved_y)));
	  for (current_x=previous_x;(x_step < 0) ? (current_x>=target_x) : (current_x<=target_x);current_x+=x_step)
	    {
	      if ((fb!=3)&&(hires_pattern!=255))
		{
		  local_fb = current_hires_pattern&1 ? fb : 3;
		  next_current_pattern();
		}
	      curset(current_x,current_y,local_fb);
	    }
	  previous_x = target_x;
	  
	  current_y += y_step;
	}
    }
  curset(to_x,to_y,3);
  
}
void hires_draw_char(T_INT c,T_INT charset,T_INT fb)
{
  T_BYTE *char_address = cpu_get_mem()+0x9800+(c+128*charset)*8;
  int i,j;
  T_INT saved_x=hires_params.cursor_x;
  T_INT saved_y=hires_params.cursor_y;
  
  for (i = 0;i<8;i++)
    {
      T_BYTE d = char_address[i];
      for (j = 0;j < 6;j++)
	{
	  if ((d>>j) & 1)
	    {
	      curset(saved_x+5-j,saved_y+i,fb);
	    }
	      
	}
      
    }
  curset(saved_x,saved_y,3);
  
}

void fill(T_INT nb_lines,T_INT nb_bytes,T_INT pattern)
{
  int j;
  T_BYTE *start_mem = cpu_get_mem()+hires_params.start_address+hires_params.cursor_x/6 + hires_params.cursor_y*40;
  if (hires_params.cursor_y+nb_lines<=hires_params.nb_lines && hires_params.cursor_x+nb_bytes*6 < 240 && nb_bytes>=0 && nb_lines>=0)
    {
      for (j=0;j<nb_lines;j++)
	{
	  memset(start_mem+j*40,pattern,nb_bytes);
	}
    }
  else
    {
      error(ILLEGAL_QUANTITY,"FILL"," nb lines= ",str$(nb_lines),
	    " nb bytes= ",str$(nb_bytes)," curx=",str$(hires_params.cursor_x),
	    "cury=",str$(hires_params.cursor_y),0);
      }
  
}

void circle(T_INT radius,T_INT fb)
{
  T_INT saved_x = hires_params.cursor_x;
  T_INT saved_y = hires_params.cursor_y;
  float step = (0.25*90)/radius; // empiric, if increase step the "macrame" oric effect appears
  float angle = 0;
  while (angle<180.0)
    {
      int delta_x = radius * cos(angle*M_PI/180.0);
      int delta_y = radius * sin(angle*M_PI/180.0);
      curset(saved_x+delta_x,saved_y+delta_y,fb);
      curset(saved_x+delta_x,saved_y-delta_y,fb);
      angle += step;
    }
  curset(saved_x,saved_y,3); // restore curset
  
  }

void update_paper_and_ink(const ModeParams *p)
{
  unsigned char *mem = cpu_get_mem();
  
  T_INT i;
  for (i = p->text_y_start; i < p->nb_lines;i++)
    {
      T_INT offset = p->start_address + i*40;
      mem[offset] = p->paper + 16;
      mem[offset+1] = p->ink;
    }
}
void paper(T_INT paper)
{
  ModeParams *p = get_mode_params();
  p->paper = paper;

  update_paper_and_ink(p);
  if (hires_mode)
    {
      update_paper_and_ink(&hires_text_params);
    }
}

void ink(T_INT ink)
{
  ModeParams *p = get_mode_params();
  p->ink = ink;
  update_paper_and_ink(p);
  if (hires_mode)
    {
      update_paper_and_ink(&hires_text_params);
    }
}

void AP_F89B()
{
  reset_charset();
}

void AP_F8D0()
{
  reset_charset();
}

void dim(T_NUMERIC **ptr,T_INT size)
{
  if (*ptr==0)
    {
      *ptr = (T_NUMERIC*)malloc(size*sizeof(T_NUMERIC));
      memset(*ptr,0,size*sizeof(T_NUMERIC));
    }
  
}

void dim_int(T_INT **ptr,T_INT size)
{
  if (*ptr==0)
    {
      *ptr = (T_INT*)malloc(size*sizeof(T_INT));
      memset(*ptr,0,size*sizeof(T_INT));
    }
  
}

void dim_str(T_STRING_PTR *ptr,T_INT size)
{
  if (*ptr==0)
    {
      *ptr = (T_STRING_PTR)malloc(size*sizeof(T_STRING));
      memset(*ptr,0,size*sizeof(T_STRING));
    }
  
}
void free_pointers(void *array,int nb_items)
{
  int **p = (int **)array;
  int i;
  for (i = 0;i<nb_items;i++)
    {
      int *z = p[i];
      if (z!=0)
	{
	  free(z);
	  *z=0;
	}
    }
  }

void hires()
{
  unsigned char *mem = cpu_get_mem();
    if (hires_mode==-1)
    {
      error("basic_init() was not called",0);
    }

  if (!hires_mode)
    {
      // transfer charset
      memcpy(mem + 0x9800, mem + 0xB400, 0xB80-0x400);
      // reset pattern
      pattern(255);
    }
  memset(mem + 0xA000, 64, hires_params.nb_lines*40); // hires cls
  mem[0xBB80] = 0x1F;  // so VDU switches to hires

  hires_mode = 1;
  hires_text_params.ink = text_params.ink;
  hires_text_params.paper = text_params.paper;
  // wait for a refresh
  SDL_Delay(100);
  cls();
}

void text()
{

  unsigned char *mem = cpu_get_mem();
  if (hires_mode==-1)
    {
      error("basic_init() was not called",0);
    }
  
  if (hires_mode)
    {
      // Transfer charset back
      memcpy(mem + 0xB400, mem + 0x9800, 0xB80-0x400);
      // reset print params all to default
      memset(&print_params,0,sizeof(print_params));
      hires_mode = 0; // BEFORE cls!!
      cls();
    }
  else
    {
      hires_mode = 0;  
    }
  
  
  memcpy(get_text_start()+35,"\007CAPS",5);

  SDL_Delay(100);
}


void plot(T_INT x, T_INT y, const T_STRING_PTR txt)
{
    memcpy(get_text_start() + x + (y+1)*40,txt,strlen(txt));
}
void plot_char(T_INT x, T_INT y, const char c)
{
    get_text_start()[x + (y+1)*40] = c;
}

void sound(T_INT channel,T_INT pitch, T_INT volume)
{
  // TODO
  printf("SOUND %d,%d,%d\n",channel,pitch,volume);
}

void play(T_INT a,T_INT b, T_INT c, T_INT d)
{
  (void)b;
  (void)c;
  (void)d;
  
    if (a == 0)
    {
	T_INT i;
	for (i = 1; i < 4; i++)
	{
	    sound_stop_music(i);
	}
    }

}
void music(T_INT channel, T_INT octave, T_INT note, T_INT volume)
{
  (void)volume;
    sound_play_music(channel,octave,note);
}

void zap()
{
}

void ping()
{
}

void explode()
{
}
void shoot()
{
}
