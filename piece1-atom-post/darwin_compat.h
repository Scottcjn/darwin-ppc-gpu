#ifndef DARWIN_COMPAT_H
#define DARWIN_COMPAT_H
/* Off-target shim proving the ATOM interpreter builds free of Linux/DRM.
   Real KEXT: kmalloc->IOMalloc, printk->IOLog, udelay->IODelay, mutex->IOLock,
   jiffies->mach_absolute_time(ms), and the card_info reg/mc/pll callbacks do MMIO
   via OSRead/WriteLittleInt32 (the only big-endian sites in piece 1).
   Tri-brain hardened (Codex+Grok): scnprintf/strscpy/jiffies now match the
   Linux primitive contracts; unused kmalloc_array removed. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
#define GFP_KERNEL 0
#define kmalloc(sz,fl)         malloc(sz)
#define kzalloc(sz,fl)         calloc(1,(sz))
#define kcalloc(n,sz,fl)       calloc((n),(sz))
#define kzalloc_obj(type)      ((type*)calloc(1,sizeof(type)))
#define kfree(p)               free((void*)(p))
#define kvfree(p)              free((void*)(p))
#define vzalloc(sz)            calloc(1,(sz))
#define vfree(p)               free((void*)(p))
struct mutex { int _u; };
#define mutex_init(m)   ((void)(m))
#define mutex_lock(m)   ((void)(m))
#define mutex_unlock(m) ((void)(m))
#define msleep(x) ((void)0)
#define udelay(x) ((void)0)
#define mdelay(x) ((void)0)
#define KERN_DEBUG ""
#define KERN_INFO  ""
#define KERN_ERR   ""
#define KERN_WARNING ""
#define printk(...)   printf(__VA_ARGS__)
#define pr_err(...)   printf(__VA_ARGS__)
#define pr_info(...)  printf(__VA_ARGS__)
#define pr_warn(...)  printf(__VA_ARGS__)
#define pr_debug(...) printf(__VA_ARGS__)
#define DRM_ERROR(...) printf(__VA_ARGS__)
#define DRM_INFO(...)  printf(__VA_ARGS__)
#define DRM_DEBUG(...) printf(__VA_ARGS__)
#define drm_info(dev,...) printf(__VA_ARGS__)
#define drm_can_sleep() (1)
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#define str_yes_no(b) ((b)?"yes":"no")
#define EXPORT_SYMBOL(x)
#define __init
#define __exit
#ifndef likely
#define likely(x)   (x)
#define unlikely(x) (x)
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif
#ifndef __counted_by
#define __counted_by(x)
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#define __maybe_unused __attribute__((unused))
#define lower_32_bits(x) ((uint32_t)((x) & 0xffffffffUL))
#define upper_32_bits(x) ((uint32_t)(((uint64_t)(x)) >> 32))
#define do_div(n,base) ({ uint64_t __n=(uint64_t)(n); uint32_t __b=(uint32_t)(base); \
                          uint32_t __r=(uint32_t)(__n % __b); (n)=(__n/__b); __r; })

/* jiffies = monotonic milliseconds; jiffies_to_msecs is then identity.
   Real KEXT: __ms_now() -> mach_absolute_time() scaled to ms. */
static inline unsigned long __ms_now(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (unsigned long)((unsigned long)ts.tv_sec*1000UL + (unsigned long)ts.tv_nsec/1000000UL);
}
#define jiffies (__ms_now())
#define time_after(a,b) ((long)((unsigned long)(b)-(unsigned long)(a)) < 0)
#define jiffies_to_msecs(j) ((unsigned int)(j))
#define OPTIMIZER_HIDE_VAR(x) ((void)(x))

/* scnprintf: return chars ACTUALLY stored, capped at size-1 (Linux contract),
   NOT snprintf's would-be length -- callers do `off += scnprintf(...)`. */
static inline int scnprintf(char *buf, size_t size, const char *fmt, ...){
    va_list ap; va_start(ap, fmt);
    int i = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    if (size == 0) return 0;
    if (i < 0) return 0;
    if ((size_t)i >= size) return (int)size - 1;
    return i;
}
/* strscpy: NUL-terminated copy; returns length, or -E2BIG on truncation. */
static inline long strscpy(char *d, const char *s, unsigned long n){
    unsigned long i = 0;
    if (!n) return -E2BIG;
    for (; i < n - 1 && s[i]; ++i)
        d[i] = s[i];
    d[i] = 0;
    return s[i] ? -E2BIG : (long)i;
}
/* byte-assembled LE accessors = endian-agnostic (BE-safe; corroborated by Grok) */
static inline uint32_t get_unaligned_le32(const void *p){
    const unsigned char *b=(const unsigned char*)p;
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static inline uint16_t get_unaligned_le16(const void *p){
    const unsigned char *b=(const unsigned char*)p; return (uint16_t)(b[0]|(b[1]<<8));}
static inline uint32_t cpu_to_le32(uint32_t v){
    unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};
    uint32_t r; memcpy(&r,b,4); return r;}
#endif
