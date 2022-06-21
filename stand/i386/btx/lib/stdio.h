#include "btxv86.h"

#include "rbx.h"

#include <stdarg.h>

/* Define to 0 to omit serial support */
#ifndef SERIAL
#define SERIAL 1
#endif

#ifndef BTX_SERIAL
#undef SERIAL
#define SERIAL 0
#endif

#define IO_KEYBOARD	1
#define IO_SERIAL	2

int sio_init(int) __attribute__((regparm (3)));
int sio_flush(void);
void sio_putc(int) __attribute__((regparm (3)));
int sio_getc(void);
int sio_ischar(void);

void putchar(int);
void printf(const char*, ...);
int getchar(void);
int keyhit(unsigned);
char* gets(char*);