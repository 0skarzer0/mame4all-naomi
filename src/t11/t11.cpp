/*** t11: Portable DEC T-11 emulator ******************************************

	Copyright (C) Aaron Giles 1998

	System dependencies:	long must be at least 32 bits
	                        word must be 16 bit unsigned int
							byte must be 8 bit unsigned int
							long must be more than 16 bits
							arrays up to 65536 bytes must be supported
							machine must be twos complement

*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "t11.h"
#include "driver.h"


static t11_Regs state;

/* public globals */
int	t11_ICount=50000;

/* bits for the pending interrupts */
#define T11_IRQ0_BIT    0x0001
#define T11_IRQ1_BIT    0x0002
#define T11_IRQ2_BIT    0x0004
#define T11_IRQ3_BIT    0x0008
#define T11_WAIT        0x8000      /* Wait is pending */

/* register definitions and shortcuts */
#define REGD(x) state.reg[x].d
#define REGW(x) state.reg[x].w.l
#define REGB(x) state.reg[x].b.l
#define SP REGW(6)
#define PC REGW(7)
#define SPD REGD(6)
#define PCD REGD(7)
#define PSW state.psw.b.l

/* shortcuts for reading opcodes */
INLINE int ROPCODE (void)
{
	int pc = PCD;
	PC += 2;
	return READ_WORD (&state.bank[pc >> 13][pc & 0x1fff]);
}

#if defined(NAOMI) && defined(NAOMI_SSPRINT1)
/* Atari System 2 fast paths: RAM (0x0000-0x0fff), the banked ROM windows
   (0x4000-0x7fff) and the fixed ROM (0x8200-0xffff) are read straight from
   the bank pointers, RAM written straight; the rest (I/O, palette, video RAM,
   the slapstic at 0x8000-0x81ff and the spin-skip byte at 0x54c) still goes
   through MAME's memory handlers. */
#ifdef NAOMI_T11STAT
unsigned t11_stat_ins, t11_stat_slow;
unsigned long long t11_stat_slow_us;
#include <arch/timer.h>
#define SLOW(x) ({ uint64_t _t = timer_us_gettime64(); t11_stat_slow++; __typeof__(x) _r = (x); t11_stat_slow_us += timer_us_gettime64() - _t; _r; })
#define SLOWV(x) do { uint64_t _t = timer_us_gettime64(); t11_stat_slow++; x; t11_stat_slow_us += timer_us_gettime64() - _t; } while (0)
#else
#define SLOW(x) (x)
#define SLOWV(x) x
#endif
INLINE int t11_fast_read(int a)
{
	return (a < 0x1000 && (a & 0xfffe) != 0x54c) || (a >= 0x4000 && (a < 0x8000 || a >= 0x8200));
}
INLINE int RBYTE (int addr)
{
	addr &= 0xffff;
	if (t11_fast_read(addr))
		return state.bank[addr >> 13][addr & 0x1fff];
	return SLOW(T11_RDMEM (addr));
}
INLINE void WBYTE (int addr, int data)
{
	addr &= 0xffff;
	if (addr < 0x1000) {
		state.bank[0][addr] = data;
		return;
	}
	SLOWV(T11_WRMEM (addr, data));
}
INLINE int RWORD (int addr)
{
	addr &= 0xffff;
	if (!(addr & 1) && t11_fast_read(addr))
		return READ_WORD (&state.bank[addr >> 13][addr & 0x1fff]);
	return SLOW(T11_RDMEM_WORD (addr));
}
INLINE void WWORD (int addr, int data)
{
	addr &= 0xffff;
	if (!(addr & 1) && addr < 0x1000) {
		WRITE_WORD (&state.bank[0][addr], data);
		return;
	}
	SLOWV(T11_WRMEM_WORD (addr, data));
}
#else
/* shortcuts for reading/writing memory bytes */
#define RBYTE(addr)      T11_RDMEM (addr)
#define WBYTE(addr,data) T11_WRMEM ((addr), (data))

/* shortcuts for reading/writing memory words */
INLINE int RWORD (int addr)
{
	return T11_RDMEM_WORD (addr);
}

INLINE void WWORD (int addr, int data)
{
	T11_WRMEM_WORD (addr, data);
}
#endif

/* pushes/pops a value from the stack */
INLINE void PUSH (int val)
{
	SP -= 2;
	WWORD (SPD, val);
}

INLINE int POP (void)
{
	int result = RWORD (SPD);
	SP += 2;
	return result;
}

/* flag definitions */
#define CFLAG 1
#define VFLAG 2
#define ZFLAG 4
#define NFLAG 8

/* extracts flags */
#define GET_C (PSW & CFLAG)
#define GET_V (PSW & VFLAG)
#define GET_Z (PSW & ZFLAG)
#define GET_N (PSW & NFLAG)

/* clears flags */
#define CLR_C (PSW &= ~CFLAG)
#define CLR_V (PSW &= ~VFLAG)
#define CLR_Z (PSW &= ~ZFLAG)
#define CLR_N (PSW &= ~NFLAG)

/* sets flags */
#define SET_C (PSW |= CFLAG)
#define SET_V (PSW |= VFLAG)
#define SET_Z (PSW |= ZFLAG)
#define SET_N (PSW |= NFLAG)

/* includes the static function prototypes and the master opcode table */
#include "t11table.h"

/* includes the actual opcode implementations */
#include "t11ops.h"


/****************************************************************************/
/* Set all registers to given values                                        */
/****************************************************************************/
void t11_SetRegs(t11_Regs *Regs)
{
	state = *Regs;
}

/****************************************************************************/
/* Get all registers in given buffer                                        */
/****************************************************************************/
void t11_GetRegs(t11_Regs *Regs)
{
	*Regs = state;
}

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
unsigned t11_GetPC(void)
{
	return PCD;
}

/****************************************************************************/
/* Sets the banking                                                         */
/****************************************************************************/
void t11_SetBank(int offset, unsigned char *base)
{
	state.bank[offset >> 13] = base;
}


void t11_reset(void)
{
	int i;
	extern unsigned char *RAM;

	fast_memset (&state, 0, sizeof (state));
	SP = 0x0400;
	PC = 0x8000;
	PSW = 0xe0;

	for (i = 0; i < 8; i++)
		state.bank[i] = &RAM[i * 0x2000];

	t11_Clear_Pending_Interrupts();
}


void t11_Cause_Interrupt(int type)
{
	if (type >= 0 && type <= 3)
	{
		state.pending_interrupts |= 1 << type;
		state.pending_interrupts &= ~T11_WAIT;
	}
}


void t11_Clear_Pending_Interrupts(void)
{
	state.pending_interrupts &= ~(T11_IRQ3_BIT | T11_IRQ2_BIT | T11_IRQ1_BIT | T11_IRQ0_BIT);
}


/* Generate interrupts - I don't really know how this works, but this is how Paperboy uses them */
static void Interrupt(void)
{
	int level = (PSW >> 5) & 3;

	if (state.pending_interrupts & T11_IRQ3_BIT)
	{
		state.pending_interrupts &= ~T11_IRQ3_BIT;
		PUSH (PSW);
		PUSH (PC);
		PCD = RWORD (0x60);
		PSW = RWORD (0x62);
	}
	else if ((state.pending_interrupts & T11_IRQ2_BIT) && level < 3)
	{
		state.pending_interrupts &= ~T11_IRQ2_BIT;
		PUSH (PSW);
		PUSH (PC);
		PCD = RWORD (0x50);
		PSW = RWORD (0x52);
	}
	else if ((state.pending_interrupts & T11_IRQ1_BIT) && level < 2)
	{
		state.pending_interrupts &= ~T11_IRQ1_BIT;
		PUSH (PSW);
		PUSH (PC);
		PCD = RWORD (0x40);
		PSW = RWORD (0x42);
	}
	else if ((state.pending_interrupts & T11_IRQ0_BIT) && level < 1)
	{
		state.pending_interrupts &= ~T11_IRQ0_BIT;
		PUSH (PSW);
		PUSH (PC);
		PCD = RWORD (0x38);
		PSW = RWORD (0x3a);
	}
}



/* execute instructions on this CPU until icount expires */
#ifdef NAOMI_T11_RECOMP
/* an interrupt the interpreter would take before the next instruction */
static int t11_irq_ready (void) __attribute__((noinline));
static int t11_irq_ready (void)
{
	int p = state.pending_interrupts;
	int level;
	if (!p)
		return 0;
	if (p & (T11_WAIT | T11_IRQ3_BIT))
		return 1;
	level = (PSW >> 5) & 3;
	return ((p & T11_IRQ2_BIT) && level < 3) || ((p & T11_IRQ1_BIT) && level < 2) ||
	       ((p & T11_IRQ0_BIT) && level < 1);
}
#ifdef NAOMI_T11RC_MISSES
unsigned t11_rc_ins;
#define RC_INS t11_rc_ins++
#else
#define RC_INS
#endif
/* out-of-line memory access for the recompiled blocks (small code) */
static int rc_rbyte (int a) __attribute__((noinline));
static int rc_rbyte (int a) { return RBYTE (a); }
static void rc_wbyte (int a, int d) __attribute__((noinline));
static void rc_wbyte (int a, int d) { WBYTE (a, d); }
static int rc_rword (int a) __attribute__((noinline));
static int rc_rword (int a) { return RWORD (a); }
static void rc_wword (int a, int d) __attribute__((noinline));
static void rc_wword (int a, int d) { WWORD (a, d); }
static void rc_push (int v) __attribute__((noinline));
static void rc_push (int v) { PUSH (v); }
static int rc_pop (void) __attribute__((noinline));
static int rc_pop (void) { return POP (); }
/* Statically recompiled blocks of the game's fixed program (0x8000-0xffff),
   generated by naomi/tools/t11recomp.py from the user's ROMs (not committed). */
#include "t11_ssprint_rc.h"

#ifdef NAOMI_T11RC_MISSES
/* Debug: fixed-program addresses that ran in the interpreter, most frequent
   first, to add as entries (naomi/tools/t11recomp.py EXTRA_ENTRIES). */
#define RCM 128
static unsigned rcm_pc[RCM], rcm_n[RCM];
unsigned t11_rc_hits, t11_rc_misses;
static void rc_miss (unsigned pc)
{
	int i;
	t11_rc_misses++;
	for (i = 0; i < RCM; i++) {
		if (rcm_n[i] && rcm_pc[i] == pc) { rcm_n[i]++; return; }
		if (!rcm_n[i]) { rcm_pc[i] = pc; rcm_n[i] = 1; return; }
	}
}
void t11_rc_report (char *buf)
{
	/* hits/misses per frame are printed by the caller; top 4 missed addresses */
	int k, j;
	char *p = buf;
	for (k = 0; k < 4; k++) {
		int b = -1; unsigned bn = 0;
		for (j = 0; j < RCM; j++) if (rcm_n[j] > bn) { bn = rcm_n[j]; b = j; }
		if (b < 0) break;
		p += sprintf (p, "%X ", rcm_pc[b]);
		rcm_n[b] = 0;
	}
	*p = 0;
	for (j = 0; j < RCM; j++) rcm_n[j] = 0;
}
#endif
#endif

int t11_execute(int cycles)
{
	t11_ICount = cycles;

	if (state.pending_interrupts & T11_WAIT)
	{
		t11_ICount = 0;
		goto getout;
	}

change_pc (0xffff);
	do
	{
		if (state.pending_interrupts != 0)
			Interrupt();

#if 0
/* use this code to nail a bogus jump or opcode */
		{
			extern int DasmT11 (unsigned char *pBase, char *buffer, int pc);
			static unsigned int pclist[256];
			static unsigned char inst[256][8];
			static int pcindex = 0;
			int i;

			pclist[pcindex] = PCD;
			for (i = 0; i < 8; i++)
				inst[pcindex][i] = state.bank[(PCD + i) >> 13][(PCD + i) & 0x1fff];
			pcindex = (pcindex + 1) & 255;

			if (PCD < 0x4000)
			{
				char buffer[200];
				int i;

				#undef printf
				printf ("Error!\n");
				for (i = 255; i >= 0; i--)
				{
					unsigned char *temp;
					int index = (pcindex - i) & 255;

					temp = OP_ROM;
					OP_ROM = &inst[index][0] - pclist[index];
					DasmT11 (&inst[index][0], buffer, pclist[index]);
					OP_ROM = temp;

					printf ("%04X: %s\n", pclist[index], buffer);
				}
				printf ("$38 = %04X\n", RAM[0x38] + (RAM[0x39] << 8));
				printf ("$40 = %04X\n", RAM[0x40] + (RAM[0x41] << 8));
				printf ("$50 = %04X\n", RAM[0x50] + (RAM[0x51] << 8));
				printf ("$60 = %04X\n", RAM[0x60] + (RAM[0x61] << 8));
				gets (buffer);
			}
		}
#endif


#ifdef NAOMI_T11_RECOMP
		if (PCD >= 0x8000)
		{
			void (*f)(void) = t11_rc_table[(PCD - 0x8000) >> 1];
			if (f)
			{
#ifdef NAOMI_T11RC_MISSES
				t11_rc_hits++;
#endif
				f ();
				continue;
			}
#ifdef NAOMI_T11RC_MISSES
			rc_miss (PCD);
#endif
		}
#endif
		state.op = ROPCODE ();
		(*opcode_table[state.op >> 3])();

		t11_ICount -= 22;
#ifdef NAOMI_T11STAT
		t11_stat_ins++;
#endif

	} while (t11_ICount > 0);

getout:
	return cycles - t11_ICount;
}
