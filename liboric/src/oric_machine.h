#ifndef _ORIC_MACHINE
#define _ORIC_MACHINE

#include "constants.h"
#include "my_assert.h"
#include <stdlib.h>

typedef struct {
    int a;
    int x;
    int y;
    int s;

    int c;
    int n;
    int v;
    int b;
    int d;
    int i;
    int z;
} Regs;

Regs r;
unsigned char m[0xC000];

#define cpu_update_nz(v)   r.z = (v == 0);   r.n = (v > 0x7F)

void cpu_dump_status();


#define MASK_N 0x80
#define MASK_V 0x40
#define MASK_B 0x10
#define MASK_D 0x08
#define MASK_I 0x04
#define MASK_Z 0x02
#define MASK_C 0x01

#define cpu_dec(xy)    xy--;   if (xy < 0) xy = 0xFF; cpu_update_nz(xy);
#define cpu_inc(xy)    xy++;   if (xy > 0xFF) xy = 0;  cpu_update_nz(xy)
#define check_in_ram(offset)   my_assert((offset >= 0) && (offset < 0xC000))
#define is_in_io(offset) ((offset >= 0x300) && (offset < 0x400))
#define ADDRESS_16(a) (m[(int)a] + (((int)m[(int)a+1]) << 8))

static inline void cpu_dec_memory(int address)
{
    check_in_ram(address);
    int v = m[address];
    cpu_dec(v);
    m[address] = v;
}

static inline void cpu_inc_memory(int address)
{
    check_in_ram(address);
    // assert io
    int v = m[address];
    cpu_inc(v);
    m[address] = v;
}


void cpu_init();

// set trace level. 0, 1, 2, 3 (max)

void cpu_set_trace(int p_trace_level);
void cpu_trace(const char *format);


// do not use v more than once

#define cpu_set_a(v) r.a = v; cpu_update_nz(r.a)
#define cpu_set_x(v) r.x = v; cpu_update_nz(r.x)
#define cpu_set_y(v) r.y = v; cpu_update_nz(r.y)


static inline void cpu_push_byte(unsigned char b)
{
    my_assert (r.s > 0);

    r.s--;
    m[0x100+r.s] = b;
}

static inline unsigned char cpu_pop_byte()
{
    unsigned char rval = 0;

    my_assert(r.s < 0x100);

    rval = m[0x100+r.s];

    r.s++;

    return rval;
}


static inline void cpu_php()
{
    unsigned char sr = (r.n << 7) + (r.v << 6) + (r.b << 4) +
    (r.d << 3) + (r.i << 2) + (r.z << 1) + r.c;

    cpu_push_byte(sr);
}

static inline void cpu_plp()
{
    unsigned char sr = cpu_pop_byte();

    r.n = (sr & MASK_N) >> 7;
    r.v = (sr & MASK_V) >> 6;
    r.b = (sr & MASK_B) >> 4;
    r.d = (sr & MASK_D) >> 3;
    r.i = (sr & MASK_I) >> 2;
    r.z = (sr & MASK_Z) >> 1;
    r.c = (sr & MASK_C);

}
void cpu_illegal();

void cpu_cmp(unsigned char reg, unsigned char v);


#define cpu_eor(v) r.a ^= v; cpu_update_nz(r.a);
#define cpu_and(v)   r.a &= v;   cpu_update_nz(r.a)
#define cpu_ora(v) r.a |= v;  cpu_update_nz(r.a)

static inline void cpu_adc(unsigned char a)
{
    my_assert (!r.d);

    int v = r.a + a + r.c;
    r.c = (v > 0xFF);
    r.a = v & 0xFF;
    cpu_update_nz(r.a);
}

static inline void cpu_sbc(unsigned char a)
{
    my_assert (!r.d);

    int v = r.a - a - (1 - r.c);
    r.c = (v >= 0);
    if (!r.c) { v += 0x100; }
    r.a = v;
    cpu_update_nz(r.a);
}

void cpu_asl_a();
void cpu_lsr_a();
void cpu_ror_a();
void cpu_rol_a();
void cpu_asl(int address);
void cpu_lsr(int address);
void cpu_ror(int address);
void cpu_rol(int address);
void cpu_asl_x(int address);
void cpu_lsr_x(int address);
void cpu_ror_x(int address);
void cpu_rol_x(int address);
void cpu_lda_ind_x(unsigned char zp_address);
void cpu_lda_ind_y(unsigned char zp_address);


unsigned char *cpu_get_mem();

void cpu_memcopy(int source, int dest, int count);
void cpu_memset(int dest, int count, unsigned char value);

static inline void cpu_write_mem(unsigned char v, int address)
{
  check_in_ram(address);
  #if 0
 #ifndef NDEBUG
     my_assert((address >= 0x200) || (address < 0x100));
    if (memwatch_write_index > 0)
     {
 	cpu_memwatch_lookup(memwatch_write_array,
 			    memwatch_write_index,address,
 			    MEMWATCH_WRITE,
			    v);
    }

#endif
  #endif
    m[address] = v;

}

static inline int compute_indexed_address(int address, int xy_register)
{
    int noffset = address + xy_register;

    if (noffset < 0x100)
    {
	// zero page
	noffset = noffset % 0x100;
    }

    return noffset;
}


static inline void cpu_write_mem_idx(unsigned char v, int address, int xy_register)
{
    cpu_write_mem(v,compute_indexed_address(address,xy_register));
}

void cpu_push_byte(unsigned char b);
unsigned char cpu_pop_byte();

void cpu_add_cycles(int ncycles);

#ifndef NDEBUG
#define MEMWATCH_WRITE 1
#define MEMWATCH_READ 2

void cpu_add_memwatch(int address, int size, int kind);

void cpu_dump_memory(const char *dump_name);
#endif


/* ----------------- */

static inline int cpu_random_8_bit()
{
    return (rand() * 0x100) / RAND_MAX;
}

static inline unsigned char cpu_read_mem(int address)
{
    unsigned char rval = m[address];
/* #ifndef NDEBUG */
/*     check_in_ram(address); */
/*     if (memwatch_read_index > 0) */
/*     { */
/* 	cpu_memwatch_lookup(memwatch_read_array, */
/* 			    memwatch_read_index,address,MEMWATCH_READ,0); */
/*     } */
/* #endif */

    if (is_in_io(address))
    {
	//printf("trying to read from %x\n",address);
	if ((address == VIA_T1CL) || (address == VIA_T2CL))
	{
	    // we consider this as random as we don't really count the cycles

	    rval = cpu_random_8_bit(); //cycle_counter;
	}
    }
    return rval;
}

static inline unsigned char cpu_read_mem_idx(int address, int xy_register)
{
    return cpu_read_mem(compute_indexed_address(address,xy_register));
}



static inline void cpu_sbc_ind_y(unsigned char zp_address)
{
    cpu_sbc(cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}
static inline void cpu_adc_ind_y(unsigned char zp_address)
{
    cpu_adc(cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}
static inline void cpu_ora_ind_x(int zp_address)
{
    cpu_ora(cpu_read_mem(ADDRESS_16(zp_address + r.x)));
}
static inline void cpu_ora_ind_y(int zp_address)
{
    cpu_ora(cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}

static inline void cpu_and_ind_x(int zp_address)
{
    cpu_and(cpu_read_mem(ADDRESS_16(zp_address + r.x)));
}
static inline void cpu_and_ind_y(int zp_address)
{
    cpu_and(cpu_read_mem(ADDRESS_16(zp_address) + r.y));
}
static inline void cpu_adc_ind_x(unsigned char zp_address)
{
    cpu_adc(cpu_read_mem(ADDRESS_16(zp_address + r.x)));
}

void cpu_sta_ind_x(unsigned char zp_address);

void cpu_sta_ind_y(unsigned char zp_address);

void cpu_bit(unsigned char v);

#endif
