#include "stdio.h"

#if SERIAL
#define DO_KBD (ioctrl & IO_KEYBOARD)
#define DO_SIO (ioctrl & IO_SERIAL)
#else
#define DO_KBD (1)
#define DO_SIO (0)
#endif

#if SERIAL
static int comspeed = SIOSPD;
static uint8_t ioctrl = IO_KEYBOARD;
#endif

static void bios_putchar(int c);
static void xputchar(int);
static int bios_haschar();
static int bios_getchar(int);
static int xgetchar(int);

static
void
bios_putchar(int c)
{
	v86.addr = 0x10;
	v86.eax = 0xe00 | (c & 0xff);
	v86.ebx = 0x7;
	v86int();
}

static
void
xputchar(int c) {
	if (DO_SIO)
		sio_putc(c);
	if (DO_KBD) 
		bios_putchar(c);
}

void
putchar(int c)
{
	if (c == '\n')
		xputchar('\r');
	xputchar(c);
}

void
printf(const char *fmt,...)
{
	va_list ap;
	static char buf[10];
	char *s;
	unsigned u;
	int c;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				putchar(va_arg(ap, int));
				continue;
			case 's':
				for (s = va_arg(ap, char *); *s; s++)
					putchar(*s);
				continue;
			case 'u':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = '0' + u % 10U;
				while (u /= 10U);
				while (--s >= buf)
					putchar(*s);
				continue;
			}
		}
		putchar(c);
	}
	va_end(ap);
	return;
}

static
int
bios_haschar()
{
	v86.addr = 0x16;
	v86.eax = 1;
	v86int();
	
	return !V86_ZR(v86.efl);
}

static
int
bios_getchar(int blocking)
{
	v86.addr = 0x16;
	v86.eax = 0;
	v86int();
	
	return v86.eax & 0xff;
}

static
int
xgetchar(int blocking)
{
	for (;;) {
		if (DO_KBD && bios_haschar())
			return (blocking ? bios_getchar(1) : 1);
		if (DO_SIO && sio_ischar())
			return (blocking ? sio_getc() : 1);
		if (!blocking)
			return (0);
	}
}

int
getchar()
{
	if (OPT_CHECK(RBX_NOINTR))
		return (0);
	
	return xgetchar(1);
}

int
keyhit(unsigned ticks)
{
	uint32_t t0, t1;

	if (OPT_CHECK(RBX_NOINTR))
		return (0);
	t0 = 0;
	for (;;) {
		if (xgetchar(1))
			return (1);
		t1 = *(uint32_t *)PTOV(0x46c);
		if (!t0)
			t0 = t1;
		if ((uint32_t)(t1 - t0) >= ticks)
			return (0);
	}
}

char*
gets(char* p)
{
	char* s = p;
	int c;

	for (;;) {
		switch (c = getchar()) {
		case 0:
			break;
		case '\177':
		case '\b':
			if (s > p) {
				s--;
				printf("\b \b");
			}
			break;
		case '\n':
		case '\r':
			*s = 0;
			break;
		default:
			*s++ = c;
			putchar(c);
		}
	}
	
	return p;
}