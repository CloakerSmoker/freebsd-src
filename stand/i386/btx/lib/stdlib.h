
#define BIO_BUFFER_SIZE 0x4000

#define NULL ((void*)0)

#include <stddef.h>

void __attribute__((weak)) *bio_alloc(size_t size);
void __attribute__((weak)) bio_free(void *ptr, size_t size);
void __attribute__((weak)) *malloc(size_t size);
void __attribute__((weak)) free(void *ptr, size_t size);

int __attribute__((weak)) bcmp(const void*, const void*, int);
void __attribute__((weak)) bzero(void*, int);