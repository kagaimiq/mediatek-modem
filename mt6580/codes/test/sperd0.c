#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <xprintf.h>


static inline uint32_t reg32_read(uint32_t addr) {
	return *(volatile uint32_t*)addr;
}
static inline void reg32_write(uint32_t addr, uint32_t val) {
	*(volatile uint32_t*)addr = val;
}
static inline void reg32_wsmask(uint32_t addr, int shift, uint32_t mask, uint32_t val) {
	*(volatile uint32_t*)addr =
		(*(volatile uint32_t*)addr & ~(mask << shift)) | ((val & mask) << shift);
}
static inline uint32_t reg32_rsmask(uint32_t addr, int shift, uint32_t mask) {
	return (*(volatile uint32_t*)addr >> shift) & mask;
}


void hexdump(void *ptr, int len) {
	for (int i = 0; i < len; i += 16) {
		xprintf("%08x: ", ptr + i);
		
		for (int j = 0; j < 16; j++) {
			if (i+j < len)
				xprintf("%02x ", *(uint8_t*)(ptr+i+j));
			else
				xputs("-- ");
		}
		
		xputs(" |");
		
		for (int j = 0; j < 16; j++) {
			uint8_t c = ' ';
			
			if (i+j < len) {
				c = *(uint8_t*)(ptr+i+j);
				if (c < 0x20 || c >= 0x7f) c = '.';
			}
			
			xputc(c);
		}
		
		xputs("|\n");
	}
	
	xputc('\n');
}


void kputc(int c) {
	/* address: AP_CCIF0 + 0x200 (512 byte SRAM) */
	volatile uint32_t *r = (void *)0xa000c200;
	
	/* put char */
	*r = c | (1<<31);
	
	/* wait until the host acknowledges */
	while (*r & (1<<31));
}


uint64_t micros(void) {
	return *(volatile uint32_t*)(0xa000c208);
}

void usleep(uint64_t us) {
	for (uint64_t t = micros() + us; micros() < t; );
}



/*
 * From https://github.com/wtarreau/mhz
 *
 * Note that this doesnt calculate the clock frequency right when ran in this enviroment.
 * Under Linux it calculates the correct frequency
 * while there it is ~3 times lower! (1.3 GHz ==> ~426 MHz)
 *
 * on cortex-r4 it is almost 2 times lower than rdtsc(pmccntr)
 *      ...or even 4.69 times???  @ 468 MHz
 */

#define microseconds() micros()

#define HAVE_RDTSC
//#define rdtsc() 0

static inline unsigned long long rdtsc()
{
	uint32_t tmp;
	asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r"(tmp));
	return tmp;
}


/* performs read-after-write operations that the CPU is not supposed to be able
 * to parallelize. The "asm" statements are here to prevent the compiler from
 * reordering this code.
 */
#define dont_move(var) do { asm volatile("" : "=r"(var) : "0" (var)); } while (0)

#define run1cycle_ae()   do { a ^= e; dont_move(a); } while (0)
#define run1cycle_ba()   do { b ^= a; dont_move(b); } while (0)
#define run1cycle_cb()   do { c ^= b; dont_move(c); } while (0)
#define run1cycle_dc()   do { d ^= c; dont_move(d); } while (0)
#define run1cycle_ed()   do { e ^= d; dont_move(e); } while (0)
#define run1cycle_eb()   do { e ^= b; dont_move(e); } while (0)

#define run5cycles()                                    \
	do {                                            \
		run1cycle_ae();				\
		run1cycle_ba();				\
		run1cycle_cb();				\
		run1cycle_dc();				\
		run1cycle_ed();				\
	} while (0)

#define run10cycles()          \
	do {                   \
		run5cycles();  \
		run5cycles();  \
	} while (0)

#define run100cycles()          \
	do {                    \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
		run10cycles();  \
	} while (0)


/* performs 50 operations in a loop, all dependant on each other, so that the
 * CPU cannot parallelize them, hoping to take 50 cycles per loop, plus the
 * loop counter overhead.
 */
static __attribute__((noinline)) void loop50(unsigned int n)
{
	unsigned int a = 0, b = 0, c = 0, d = 0, e = 0;

	do {
		run10cycles();
		run10cycles();
		run10cycles();
		run10cycles();
		run10cycles();
	} while (__builtin_expect(--n, 1));
}

/* performs 250 operations in a loop, all dependant on each other, so that the
 * CPU cannot parallelize them, hoping to take 250 cycles per loop, plus the
 * loop counter overhead. Do not increase this loop so that it fits in a small
 * 1 kB L1 cache on 32-bit instruction sets.
 */
static __attribute__((noinline)) void loop250(unsigned int n)
{
	unsigned int a = 0, b = 0, c = 0, d = 0, e = 0;

	do {
		run10cycles();
		run10cycles();
		run10cycles();
		run10cycles();
		run10cycles();
		run100cycles();
		run100cycles();
	} while (__builtin_expect(--n, 1));
}

void run_once(long count)
{
	long long tsc_begin, tsc_end50, tsc_end250;
	long long us_begin, us_end50, us_end250;

	/* now run the loop */
	us_begin   = microseconds();
	tsc_begin  = rdtsc();
	loop50(count);
	tsc_end50 = rdtsc() - tsc_begin;
	us_end50  = microseconds() - us_begin;

	/* now run the loop */
	us_begin   = microseconds();
	tsc_begin  = rdtsc();
	loop250(count);
	tsc_end250 = rdtsc() - tsc_begin;
	us_end250  = microseconds() - us_begin;

	xprintf("count=%ld us50=%lld us250=%lld diff=%lld cpu_MHz=%.3f",
	       count, us_end50, us_end250, us_end250 - us_end50,
	       count * 200.0 / (us_end250 - us_end50));
#ifdef HAVE_RDTSC
	xprintf(" tsc50=%lld tsc250=%lld diff=%lld rdtsc_MHz=%.3f",
	       tsc_end50, tsc_end250, (tsc_end250 - tsc_end50) / count,
	       (tsc_end250 - tsc_end50) / (float)(us_end250 - us_end50));
#endif
	xputc('\n');
}

/* spend <delay> ms waiting for the CPU's frequency to raise. Will also stop
 * on backwards time jumps if any.
 */
void pre_heat(long delay)
{
	unsigned long long start = microseconds();

	while (microseconds() - start < (unsigned long long)delay)
		;
}

/* determines how long loop50() must be run to reach more than 20 milliseconds.
 * This will ensure that an integral number of clock ticks will have happened
 * on 100, 250, 1000 Hz systems.
 */
unsigned int calibrate()
{
	unsigned long long duration = 0;
	unsigned long long start;
	unsigned int count = 1000;

	while (duration < 20000) {
		count = count * 5 / 4;
		start = microseconds();
		loop50(count);
		duration = microseconds() - start;
	}
	return count;
}

void mhz(long runs, long preheat)
{
	unsigned int count;
	
	xprintf("---- MHZ runs=%ld, preheat=%ld ----\n", runs, preheat);
	
	/* enable pmccntr */ {
		uint32_t tmp;
		
		asm volatile("mrc p15, 0, %0, c9, c12, 0" :: "r"(tmp));
		tmp |= (1<<0)|(1<<2); // enable, reset
		tmp &= ~(1<<3); // 1 clock per tick
		asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(tmp));
		
		asm volatile("mrc p15, 0, %0, c9, c12, 1" :: "r"(tmp));
		tmp |= (1<<31); // another enable
		asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(tmp));
	}

	if (preheat > 0)
		pre_heat(preheat);

	count = calibrate();
	while (runs--)
		run_once(count);
	
	/* disable pmccntr */ {
		uint32_t tmp;
		
		asm volatile("mrc p15, 0, %0, c9, c12, 0" :: "r"(tmp));
		tmp &= ~(1<<0); // disable
		asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(tmp));
	}
}


int main(void) {
	xdev_out(kputc);
	xputs("\e[1;33;40;7m >>> hey kagamin! MT6580 MODEM 5G <<< \e[0m\n");
	
	/* peek the p15 regs */ {
		uint32_t tmp = 0xdeadbeef;
		
		asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(tmp));
		xprintf(">>>> MIDR=%08x\n", tmp);
		
		asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r"(tmp));
		xprintf(">>>> MPIDR=%08x\n", tmp);
	}
	
	#if 1
	/* woowoo magic --> Clock config & PLLs config?? */ {
		// bits 1 & 2 should be 1 to CPU to work after sw to 494/468 MHz
		reg32_write(0x801200ac, 0x003f);
		
		reg32_write(0x8012004c, 0x8300);
		reg32_write(0x8012004c, 0x0000);
		
		reg32_write(0x80120048, 0x0000);
		
		reg32_write(0x80120700, 0x0010);
		reg32_wsmask(0x80120100, 15, 1, 1);
		reg32_wsmask(0x80120140, 15, 1, 1);
		reg32_wsmask(0x801201c0, 15, 1, 1);
		reg32_wsmask(0x80120200, 15, 1, 1);
		for (volatile int i = 0; i < 20; i++);
		reg32_write(0x80120700, 0x0000);
		
		for (volatile int i = 0; i < 5; i++);
		
		reg32_write(0x80120514, 0x0076);
		reg32_write(0x80120510, 0xa455);
		for (volatile int i = 0; i < 80; i++);
		
		reg32_wsmask(0x80120100, 15, 1, 0);
		for (volatile int i = 0; i < 1; i++);
		
		reg32_wsmask(0x80120110, 0, 1, 0);
		for (volatile int i = 0; i < 1; i++);
		
		reg32_wsmask(0x80120100, 0, 0xf, 0x0);  // Maybe
		
		reg32_write(0x8012008c, 0xc100);
		
		reg32_wsmask(0x8000045c, 29, 1, 1);
		
		reg32_write(0x80120060, 0x2020); // CPU clock == 494 MHz!  [26 * 19]
		reg32_write(0x80120064, 0x2000);
		reg32_write(0x80120068, 0x2220);
		reg32_write(0x8012006c, 0x0113); // CPU clock drops to 468 MHz!  [26 * 18]
		
		reg32_wsmask(0x8012004c, 0, 0x0220, 0x0220);
		reg32_wsmask(0x80120048, 0, 1, 1);
		
		reg32_wsmask(0x80120200, 15, 1, 0);
		
		reg32_write(0x801200ac, 0x0000);
		
		// ====> CPU Clock ::: 468 MHz
	}
	#endif
	
	#if 0
	xputs("Entry\n");
	
	mhz(10, 1000000);
	
	xputs("Quit\n");
	#endif
	
	/* Instr TCM at 0x70000000 */
	asm volatile ("mcr p15, 0, %0, c9, c1, 0" :: "r"(0xc0000000|1));
		
	/* Data TCM at 0x70020000 */
	asm volatile ("mcr p15, 0, %0, c9, c1, 1" :: "r"(0xd0000000|1));
	
	#if 1
	reg32_write(0xa0001308, 0x00000109); // map!
	
	for (int i = 0; i < 16; i++) {
		hexdump((void*)(i<<28), 0x40);
	}
	#endif
	
	kputc(0x47774777); // we are ending
	
	return 0;
}
