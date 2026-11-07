#ifndef DOS_COMPAT_H
#define DOS_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __DJGPP__
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>
#include <sys/movedata.h>
#include <sys/nearptr.h>
#include <bios.h>
#include <conio.h>
#else
/* Host stub declarations for syntax checking if compiled outside DJGPP */
typedef union {
    struct {
        uint32_t edi, esi, ebp, res, ebx, edx, ecx, eax;
    } d;
    struct {
        uint16_t di, di_hi, si, si_hi, bp, bp_hi, res, res_hi;
        uint16_t bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi;
        uint16_t flags, es, ds, fs, gs, ip, cs, sp, ss;
    } x;
    struct {
        uint8_t edi[4], esi[4], ebp[4], res[4];
        uint8_t bl, bh, ebx_b2, ebx_b3;
        uint8_t dl, dh, edx_b2, edx_b3;
        uint8_t cl, ch, ecx_b2, ecx_b3;
        uint8_t al, ah, eax_b2, eax_b3;
    } h;
} __dpmi_regs;

static inline int __dpmi_int(int vec, __dpmi_regs *r) { (void)vec; (void)r; return 0; }
static inline void outportb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline uint8_t inportb(uint16_t port) { (void)port; return 8; }
static inline void dosmemput(const void *buf, size_t len, uint32_t addr) { (void)buf; (void)len; (void)addr; }
static inline int __dpmi_allocate_dos_memory(int paragraphs, int *ret_sel) { if (ret_sel) *ret_sel = 1; (void)paragraphs; return 0x1000; }
static inline int __dpmi_free_dos_memory(int sel) { (void)sel; return 0; }
static inline void sound(unsigned freq) { (void)freq; }
static inline void nosound(void) {}
static inline void delay(unsigned ms) { (void)ms; }
#endif

#endif /* DOS_COMPAT_H */
