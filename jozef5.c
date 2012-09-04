#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EXIT   0xfeff
#define PUTC   0xfefe
#define ALOP1  0xfefd
#define ALOP2  0xfefc
#define ALOR   0xfefb
#define ALSL   0xfefa
#define JMPNZ  0xfef9
#define JMPL   0xfef8
#define JMPH   0xfef7
#define PTRL   0xfef6
#define PTRH   0xfef5
#define PTRA   0xfef4
#define ALADD  0xfef3
#define ALNOTL 0xfef2
#define SPL    0xfef1
#define SPH    0xfef0
#define STACK  0xfeef
#define CALL   0xfeee

struct vmstate {
	unsigned char mem[65536];
	unsigned int pc;
	unsigned char a;
};

unsigned char mread(struct vmstate *, unsigned int);
void mwrite(struct vmstate *, unsigned int, unsigned char);

void load(struct vmstate *s, char *name) {
	s->pc = 0;
	memset(s->mem, 0, 65536);
	FILE *f = fopen(name, "rb");
	if (f==NULL) {
		perror("fopen");
		exit(-1);
	}
	int r = fread(s->mem, 1, 65536, f);
	if (r<2) {
		perror("fread");
		exit(-1);
	}
	fclose(f);
}

uint16_t mread16(struct vmstate *s, unsigned int addr) {
	return mread(s, addr + 1) | mread(s, addr) << 8;
}

void mwrite16(struct vmstate *s, unsigned int addr, uint16_t val) {
	mwrite(s, addr + 1, val & 0xff);
	mwrite(s, addr, val >> 8);
}

unsigned char mread(struct vmstate *s, unsigned int addr) {
	uint16_t sp;
	uint8_t tmp8;
	
	if (addr >= 0xff00)
		return addr & 0xff;
	switch(addr) {
		case ALOR:
			return mread(s, ALOP1) | mread(s, ALOP2);
		case ALSL:
			return mread(s, ALOP1) << mread(s, ALOP2);
		case PTRA:
			return mread(s, mread16(s, PTRH));
		case ALADD:
			return mread(s, ALOP1) + mread(s, ALOP2);
		case ALNOTL:
			return !mread(s, ALOP1);
		case STACK:
			sp = mread16(s, SPH);
			tmp8 = mread(s, sp++);
			//printf("pop %x %x\n", tmp8, sp-1);
			mwrite16(s, SPH, sp);
			return tmp8;
		default:
			return s->mem[addr%65536];
	}
}

void mwrite(struct vmstate *s, unsigned int addr, unsigned char c) {
	uint16_t tmp16;
	
	switch(addr) {
		case EXIT:
			exit(-c);
		case PUTC:
			putc(c, stdout);
			return;
		case JMPNZ:
			if (c) s->pc = mread16(s, JMPH);
			return;
		case PTRA:
			mwrite(s, mread16(s, PTRH), c);
			return;
		case STACK:
			tmp16 = mread16(s, SPH);
			//printf("push %2x %4x\n", c, tmp16);
			mwrite(s, --tmp16, c);
			mwrite16(s, SPH, tmp16);
			return;
		case CALL:
			tmp16 = s->pc;
			s->pc = mread16(s, JMPH);
			mwrite16(s, JMPH, tmp16);
			return;
		default:
			s->mem[addr%65536] = c;
	}
}

void run(struct vmstate *s) {
	unsigned int addr;
	while(1) {
		addr = mread16(s, s->pc);
		fprintf(stderr, "pc %4x addr %4x a %2x\n", s->pc, addr, s->a);
		if (s->pc % 4 == 0)
			s->a = mread(s, addr);
		else if (s->pc % 4 == 2)
			mwrite(s, addr, s->a);
		else {
			fprintf(stderr, "Unaligned pc at %x\n", s->pc);
			exit(-2);
		}
		s->pc = (s->pc + 2) % 65536;
	}
}

int main(int argc, char **argv) {
	struct vmstate *s = malloc(sizeof(struct vmstate));
	if (argc != 2) {
		fprintf(stderr, "Zly pocet parametrov\n");
		exit(-3);
	}
	load(s, argv[1]);
	run(s);
	free(s);
	return 0;
}
