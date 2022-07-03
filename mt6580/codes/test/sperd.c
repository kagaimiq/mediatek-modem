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



void kputc(int c) {
	/* address: MD_CCIF0 + 0x200 (256 byte SRAM) */
	volatile uint32_t *r = (void *)0xa000d200;
	
	/* put char */
	*r = c | (1<<31);
	
	/* wait until the host acknowledges */
	while (*r & (1<<31));
}


uint64_t micros(void) {
	return *(volatile uint32_t*)(0xa000d208);
}

void usleep(uint64_t us) {
	for (uint64_t t = micros() + us; micros() < t; );
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

#if 1
void emi_mpu_dump(void) {
	static const char *prots[] = {
		"none",   "r/w(s)", "r/w(s)+r", "r/w(s)+w",
		"r(s)+r", "forbidden",   "r(s)+r/w", "??7",
	};
	
	xputs(
		"№| start  |  end   |  group0  |  group1  |  group2  |  group3  |\n"
		"-|--------|--------|----------|----------|----------|----------|\n"
	);
	
	for (int i = 0; i < 8; i++) {
		uint32_t rstart = reg32_rsmask(0xa0205160 + (i*8), 16, 0xffff) << 16;
		uint32_t rend   = reg32_rsmask(0xa0205160 + (i*8),  0, 0xffff) << 16;
		uint16_t perm   = reg32_rsmask(0xa02051a0 + (i/2*8), (i%2)*16, 0xffff);
		
		if (rend == 0) rend = 0x10000;
		rend -= 1;
		
		xprintf("%d|%08x|%08x|%-10s|%-10s|%-10s|%-10s|\n",
			i, rstart+0x80000000, rend+0x80000000,
			prots[perm&7], prots[(perm>>3)&7], prots[(perm>>6)&7], prots[(perm>>9)&7]);
	}
	
	xputs("\n");
}
#else
size_t strlen(const char *str) {
	size_t len = 0;
	
	// UTF-8 aware strlen
	for (; *str; str++) {
		// either an ASCII character or an UTF-8 code first byte
		if (!(*str & 0x80) || ((*str & 0xE0) == 0xC0))
			len++;
	}
	
	return len;
}

void emi_mpu_dump(void) {
	static const char *prots[] = {
		"нет",   "ч/з(защ)", "ч/з(защ)+ч", "ч/з(защ)+з",
		"ч(защ)+ч", "запрещено",   "ч(защ)+ч/з", "??7",
	};
	
	xputs(
		"№|  нач.  |  кон.  |  гр.0    |  гр.1    |  гр.2    |  гр.3    |\n"
		"-|--------|--------|----------|----------|----------|----------|\n"
	);
	
	for (int i = 0; i < 8; i++) {
		uint32_t rstart = reg32_rsmask(0xa0205160 + (i*8), 16, 0xffff) << 16;
		uint32_t rend   = reg32_rsmask(0xa0205160 + (i*8),  0, 0xffff) << 16;
		uint16_t perm   = reg32_rsmask(0xa02051a0 + (i/2*8), (i%2)*16, 0xffff);
		
		if (rend == 0) rend = 0x10000;
		rend -= 1;
		
		xprintf("%d|%08x|%08x|%-10s|%-10s|%-10s|%-10s|\n",
			i, rstart+0x80000000, rend+0x80000000,
			prots[perm&7], prots[(perm>>3)&7], prots[(perm>>6)&7], prots[(perm>>9)&7]);
	}
	
	xputs("\n");
}
#endif


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
	
	reg32_write(0xa0001308, 0x01 | 0x02); // MAP DRAM 02000000~03ffffff into 40000000~41ffffff
	
	emi_mpu_dump();
	hexdump((void *)0x40000000, 0x40);
	
	reg32_write(0xa02051b8, 0x00000000); // grp4: permission: all allowed
	
	emi_mpu_dump();
	hexdump((void *)0x40000000, 0x40);
	
	kputc(0x47774777); // we are ending
	
	return 0;
}
