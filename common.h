#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdarg.h>

void memset(void *addr, int val, uint32_t cnt);
int strcmp(const char *lhs, const char *rhs);

void printk(const char *fmt, ...) __attribute__((format(__printf__,1,2)));
void vprintk(const char *fmt, va_list) __attribute__((format(__printf__,1,0)));

void putc(char c);
void putc_escape(char c);
void puts(const char *s);
extern const char hexchars[];

#endif /* COMMON_H */