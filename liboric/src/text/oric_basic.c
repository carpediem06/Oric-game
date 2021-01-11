#include "oric_machine.h"
#include "oric_basic.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void print(const T_STRING_PTR first_arg,...)
{

  if (first_arg==0)
    {
      print_string("\n");
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
	      case (int)PRINT_COMMA_SEPARATOR: printf("\t");break;
	      case (int)PRINT_TAB_SEPARATOR: break;
	      default:
		print_string(na);
	    }
	}
      va_end(ap);
    }
}

void print_string(const T_STRING_PTR s)
{
  int l = strlen(s),i;
  for (i=0;i<l;i++)
    {
      char c = s[i] & 0x7F; // discard inverse video
      if ((c<32) && (c!=10))
	{
	  // avoid oric control chars
	}
      else
	{
	  fwrite(&c,1,1,stdout);
	}
    }

  fflush(stdout);
}

void input(T_STRING txt)
{
   gets(txt);
}


