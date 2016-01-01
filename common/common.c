/* Minimal bare metal arm program for use without a bootloader.
 * Parts are specific to ARM vexpress-a9 emulation w/ QEMU
 *
 * Michael Davidsaver <mdavidsaver@gmail.com>
 */

#include "common.h"
#include "bsp.h"

#ifdef DEF_RAM_SIZE
uint32_t RamSize = DEF_RAM_SIZE;
#else
uint32_t RamSize;
#endif

void memcpy(void *dst, const void *src, size_t count)
{
    char *cdst = dst;
    const char *csrc = src;
    while(count--)
        *cdst++ = *csrc++;
}

void memmove(void *dst, const void *src, size_t count)
{
    char *cdst = dst;
    const char *csrc = src;
    if(cdst==csrc) return;
    else if(cdst>csrc) {
        /* copy from back to front */
        cdst += count;
        csrc += count;
        while(count--)
            *--cdst = *--csrc;
    } else { /* cdst<csrc */
        /* copy from front to back */
        while(count--)
            *cdst++ = *csrc++;
    }
}

void memset(void *dst, uint8_t val, size_t count)
{
    char *cdst = dst;
    while(count--)
        *cdst++ = val;
}

/* floor(log(v, 2))
 *  log2(31) -> 5
 *  log2(32) -> 6
 *  log2(33) -> 6
 */
unsigned log2_floor(uint32_t v)
{
    unsigned r=0;
    while(v) {
        v>>=1;
        r++;
    }
    return r;
}

/* ceil(log(v, 2))
 *  log2_ceil(31) -> 6
 *  log2_ceil(32) -> 6
 *  log2_ceil(33) -> 7
 */
unsigned log2_ceil(uint32_t v)
{
    unsigned r=0, c=0;
    while(v) {
        c += v&1;
        v >>= 1;
        r++;
    }
    if(c>1) r++;
    return r;
}

void _assert_fail(const char *cond,
                  const char *file,
                  unsigned int line)
{
    printk("%s:%u assertion fails: %s\n",
           file, line, cond);
    halt();
}