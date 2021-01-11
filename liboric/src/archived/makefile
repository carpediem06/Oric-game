LIB = ../lib/liboric_rtlib.a
all: $(LIB)

CFLAGS = -O2 -DNDEBUG
#CFLAGS = -pg -g -DNDEBUG
#CFLAGS = -O2 -DUSE_SDL_MIX
GCC = gcc $(CFLAGS) -Wall -I/mingw/sdl/include/SDL -I/mingw/sdl/include

OBJS = oric_machine.o oric_sound.o oric_video.o oric_input.o oric_timer.o oric_basic.o my_assert.o

$(LIB): $(OBJS)
	ar r $(LIB) $(OBJS)

#6522via.o: 6522via.c 6522via.h
#	$(GCC) -c 6522via.c
my_assert.o : my_assert.h my_assert.c
	$(GCC) -c my_assert.c
oric_input.o : oric_input.h my_assert.h oric_input.c
	$(GCC) -c oric_input.c
oric_sound.o : oric_sound.h my_assert.h oric_sound.c
	$(GCC) -c oric_sound.c
oric_machine.o : oric_machine.h my_assert.h oric_machine.c
	$(GCC) -c oric_machine.c
oric_video.o : oric_machine.h oric_video.c
	$(GCC) -c oric_video.c
oric_basic.o : oric_basic.h oric_basic.c oric_machine.h
	$(GCC) -c oric_basic.c
oric_timer.o : oric_timer.h oric_timer.c my_assert.h
	$(GCC) -c oric_timer.c

clean:;
	rm -f *.[oa]
