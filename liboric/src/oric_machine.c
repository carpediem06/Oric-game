#include "oric_machine.h"
#include "oric_video.h"
#include <strings.h>
#include <stdio.h>

#define NB_ITEMS_IN_STACK 0x100

unsigned char data_stack[NB_ITEMS_IN_STACK];

unsigned char *data_stack_pointer;

int trace_level = 0;


typedef struct
{
    int address;
    int size;
} MemWatch;

#define MAX_MEMWATCH 50

int memwatch_read_index = 0;
int memwatch_write_index = 0;
MemWatch memwatch_read_array[MAX_MEMWATCH];
MemWatch memwatch_write_array[MAX_MEMWATCH];


#ifndef NDEBUG
static void cpu_memwatch_lookup(const MemWatch *mw,int nb_mw,int offset, int kind, unsigned char value_written);
#endif




void cpu_init()
{
    memset(&data_stack,0,sizeof(data_stack));

    data_stack_pointer = data_stack;

    memset(&r,0,sizeof(r));
    r.s = 0xFF;

}



#ifndef NDEBUG

void cpu_dump_status()
{
    if (trace_level > 1)
    {
    printf("A=%02X, X=%02X, Y=%02X, S=1%02X, N=%d, V=%d, B=%d, D=%d, I=%d, Z=%d, C=%d\n",
	   r.a,r.x,r.y,r.s,r.n,r.v,r.b,r.d,r.i,r.z,r.c);
    }
}
#endif


#ifndef NDEBUG
static void cpu_memwatch_reached(int address, int kind, unsigned char value)
{
    if (kind == MEMWATCH_READ)
    {
	printf("memory address $%04X is being read\n",address);
    }
    else
    {
	printf("memory address $%04X is being written with value %x\n",address,value);
    }
    fflush(stdout);
}
static void cpu_memwatch_lookup(const MemWatch *mw,int nb_mw,int offset, int kind, unsigned char value_written)
{
    int i;
    for (i = 0; i < nb_mw; i++)
    {

	if ((mw[i].address <= offset) &&
	    (mw[i].address + mw[i].size) > offset)
	{
	    cpu_memwatch_reached(offset,kind,value_written);
	}
    }

}
#endif

void cpu_memcopy(int source, int dest, int count)
{
    check_in_ram(source);
    check_in_ram(source + count - 1);
    check_in_ram(dest);
    check_in_ram(dest + count - 1);


    memcpy(m+dest,m+source,count);

}
void cpu_memset(int dest, int count, unsigned char value)
{
    check_in_ram(dest);
    check_in_ram(dest + count - 1);


    memset(m+dest,value,count);

}


#define UPDATE_CARRY_FROM_LEFT(v) r.c = (v & 0x80) != 0;
#define UPDATE_CARRY_FROM_RIGHT(v) r.c = v & 0x1;

#define ASL(v) UPDATE_CARRY_FROM_LEFT(v); v = (v << 1) & 0xff;cpu_update_nz(v)
#define LSR(v) UPDATE_CARRY_FROM_RIGHT(v); v >>= 1;cpu_update_nz(v)
#define ROR(v) int old_c = r.c; UPDATE_CARRY_FROM_RIGHT(v); v = (v >> 1) + (old_c << 7); cpu_update_nz(v)
#define ROL(v) int old_c = r.c; UPDATE_CARRY_FROM_LEFT(v); v = ((v << 1) + old_c) & 0xFF; cpu_update_nz(v)

void cpu_asl_a()
{
    ASL(r.a);
}

void cpu_lsr_a()
{
    LSR(r.a);
}

void cpu_ror_a()
{
    ROR(r.a);
}
void cpu_rol_a()
{
    ROL(r.a);
}

void cpu_asl(int address)
{
    check_in_ram(address);
    my_assert(!is_in_io(address));

    ASL(m[address]);
}

void cpu_lsr(int address)
{
    check_in_ram(address);
    my_assert(!is_in_io(address));

    LSR(m[address]);
}

void cpu_ror(int address)
{
    check_in_ram(address);
    my_assert(!is_in_io(address));

    ROR(m[address]);

}

void cpu_rol(int address)
{
    check_in_ram(address);
    my_assert(!is_in_io(address));

    ROL(m[address]);
}

void cpu_asl_x(int address)
{
    cpu_asl(compute_indexed_address(address,r.x));
}

void cpu_lsr_x(int address)
{
    cpu_lsr(compute_indexed_address(address,r.x));
}
void cpu_ror_x(int address)
{
    cpu_ror(compute_indexed_address(address,r.x));
}

void cpu_rol_x(int address)
{
    cpu_rol(compute_indexed_address(address,r.x));
}


void cpu_cmp_ind_y(int zp_address)
{
    cpu_cmp(r.a,cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}
void cpu_cmp(unsigned char reg, unsigned char v)
{
    /*     LDA #$01 ;  1 (signed),   1 (unsigned)
    CMP #$FF ; -1 (signed), 255 (unsigned)

A = $01, C = 0, N = 0 (the subtraction result is $01 - $FF = $02), and Z = 0.
    */
    int subs = (int)reg - (int)v;

    r.z = (subs == 0);
    r.c = (subs >= 0);

    if (r.z)
    {
	r.n = 0;
    }
    else
    {
	r.n = (subs & 0x80) >> 7; // most significant bit of the result
    }
}



void cpu_lda_ind_x(unsigned char zp_address)
{
    cpu_set_a(cpu_read_mem(ADDRESS_16(zp_address + r.x)));
}

void cpu_lda_ind_y(unsigned char zp_address)
{
    cpu_set_a(cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}
void cpu_sta_ind_x(unsigned char zp_address)
{
    int address = ADDRESS_16(zp_address + r.x);

    cpu_write_mem(r.a,address);
}

void cpu_sta_ind_y(unsigned char zp_address)
{
    int address = ADDRESS_16(zp_address) + r.y;
    cpu_write_mem(r.a,address);
}

void cpu_illegal()
{
    printf("Illegal instruction\n");
    exit(1);
}



unsigned char *cpu_get_mem()
{
    return m;
}


int cycle_counter = 0;
int cycle_period = 0x100;

void cpu_add_cycles(int ncycles)
{
    cycle_counter += ncycles;
    if (cycle_counter > cycle_period)
    {
	cycle_counter -= cycle_period;
    }
}


void cpu_bit(unsigned char v)
{
    r.n = (v & 0x80) >> 7;
    r.v = (v & 0x40) >> 6;

    int res = v & r.a;

    r.z = res == 0;
}







void cpu_trace(const char *format)
{
    if (trace_level)
    {
	printf(format);fflush(stdout);
    }
}

#ifndef NDEBUG

void cpu_add_memwatch(int address, int size, int kind)
{
    MemWatch *mw = 0;

    switch (kind)
    {
      case MEMWATCH_READ:
	mw = memwatch_read_array + memwatch_read_index;
	memwatch_read_index++;
	break;
      case MEMWATCH_WRITE:
	mw = memwatch_write_array + memwatch_write_index;
	memwatch_write_index++;
	break;
    }

    mw->address = address;
    mw->size = size;
}

void cpu_set_trace(int p_trace_level)
{
    trace_level = p_trace_level;
}


void cpu_dump_memory(const char *dump_name)
{
    FILE *f;

    f = fopen(dump_name,"wb");

    if (f != 0)
    {
	fwrite(m,sizeof(m),1,f);
	fclose(f);
    }
}
#endif
