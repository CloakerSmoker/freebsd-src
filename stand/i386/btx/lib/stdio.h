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

void __attribute__((weak)) putchar(int);
void __attribute__((weak)) printf(const char*, ...);
int __attribute__((weak)) getchar(void);
int __attribute__((weak)) keyhit(unsigned);
char* __attribute__((weak)) gets(char*);

#include "fs.h"

int __attribute__((weak)) open(const char*, int);
ssize_t __attribute__((weak)) read(int, void*, size_t);
void __attribute__((weak)) close(int);