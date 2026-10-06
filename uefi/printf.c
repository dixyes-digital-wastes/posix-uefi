/*
 * printf.c
 *
 * Copyright (C) 2021 bzt (bztsrc@gitlab.com)
 * Copyright (C) 2005-2020 Rich Felker, et al. (musl libc)
 * Copyright (C) 2026 Yun Dou (dixyes@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This file is part of the POSIX-UEFI package.
 * @brief The conversion engine behind the printf family
 *
 */

#include <uefi.h>

/* The conversion loop is modelled on musl's printf_core (MIT licensed),
 * trimmed for this library: no floating point, no positional arguments, no
 * %n, no wide output and no locale aware grouping. A floating point
 * conversion consumes no argument and emits nothing. %D is a POSIX-UEFI
 * extension that dumps memory.
 *
 * Only vsnprintf() is defined here, the rest of the family in stdio.c goes
 * through it. Nothing in this file touches the firmware, so it also builds
 * and runs on the host. */

#define FMT_LEFT  0x01
#define FMT_PLUS  0x02
#define FMT_SPACE 0x04
#define FMT_ALT   0x08
#define FMT_ZERO  0x10

enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_Z, LEN_T, LEN_J };

typedef struct {
    char_t *dst;
    char_t *end;
    size_t count;
    char_t prev;
} printf_out_t;

/* Raw output. The last character is remembered so fmt_chr() can avoid a
 * double CR, and the length is kept even past end so the return value of
 * vsnprintf() matches what snprintf() would have produced. */
static void fmt_raw(printf_out_t *o, char_t c)
{
    if(o->dst < o->end) { *o->dst = c; o->dst++; }
    o->count++;
    o->prev = c;
}

/* Console text needs CRLF, the old formatter translated it here too. */
static void fmt_chr(printf_out_t *o, char_t c)
{
    if(c == CL('\n') && o->prev != CL('\r')) fmt_raw(o, CL('\r'));
    fmt_raw(o, c);
}

static void fmt_fill(printf_out_t *o, char_t c, int n)
{
    while(n-- > 0) fmt_raw(o, c);
}

/* Writes the digits of x backwards from end and returns the first one. */
static char *fmt_digits(uint64_t x, unsigned base, int upper, char *end)
{
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = end;
    do { *--p = d[x % base]; x /= base; } while(x != 0);
    return p;
}

int vsnprintf(char_t *dst, size_t maxlen, const char_t *fmt, __builtin_va_list args)
{
    printf_out_t o;
    const char_t *s;
    char tmp[24];

    if(dst == NULL || fmt == NULL || maxlen == 0) return 0;

    o.dst = dst;
    o.end = dst + maxlen - 1;
    o.count = 0;
    o.prev = 0;

    for(s = fmt; *s != 0; ) {
        int flags, width, prec, len;
        char_t conv;

        if(*s != CL('%')) { fmt_chr(&o, *s++); continue; }
        s++;

        for(flags = 0; ; s++) {
            if(*s == CL('-'))      flags |= FMT_LEFT;
            else if(*s == CL('+')) flags |= FMT_PLUS;
            else if(*s == CL(' ')) flags |= FMT_SPACE;
            else if(*s == CL('#')) flags |= FMT_ALT;
            else if(*s == CL('0')) flags |= FMT_ZERO;
            else break;
        }

        if(*s == CL('*')) {
            width = (int)__builtin_va_arg(args, int);
            s++;
            if(width < 0) { flags |= FMT_LEFT; width = -width; }
        } else {
            for(width = 0; *s >= CL('0') && *s <= CL('9'); s++)
                width = width * 10 + (int)(*s - CL('0'));
        }

        if(*s == CL('.')) {
            s++;
            if(*s == CL('*')) {
                prec = (int)__builtin_va_arg(args, int);
                s++;
                if(prec < 0) prec = -1;
            } else {
                for(prec = 0; *s >= CL('0') && *s <= CL('9'); s++)
                    prec = prec * 10 + (int)(*s - CL('0'));
            }
        } else {
            prec = -1;
        }

        switch(*s) {
        case CL('h'): s++; if(*s == CL('h')) { len = LEN_HH; s++; } else len = LEN_H; break;
        case CL('l'): s++; if(*s == CL('l')) { len = LEN_LL; s++; } else len = LEN_L; break;
        case CL('z'): len = LEN_Z; s++; break;
        case CL('t'): len = LEN_T; s++; break;
        case CL('j'): len = LEN_J; s++; break;
        default:      len = LEN_NONE; break;
        }

        conv = *s;
        if(conv == 0) break;
        s++;

        if(flags & FMT_LEFT) flags &= ~FMT_ZERO;

        if(conv == CL('%')) { fmt_raw(&o, CL('%')); continue; }

        if(conv == CL('c')) {
            char_t c = (char_t)__builtin_va_arg(args, int);
            if(!(flags & FMT_LEFT)) fmt_fill(&o, CL(' '), width - 1);
            fmt_raw(&o, c);
            if(flags & FMT_LEFT) fmt_fill(&o, CL(' '), width - 1);
            continue;
        }

        if(conv == CL('s')) {
            const char_t *sp = __builtin_va_arg(args, const char_t *);
            int l = 0;
            if(sp == NULL) sp = CL("(null)");
            while(sp[l] != 0 && (prec < 0 || l < prec)) l++;
            if(!(flags & FMT_LEFT)) fmt_fill(&o, CL(' '), width - l);
            for(int i = 0; i < l; i++) fmt_chr(&o, sp[i]);
            if(flags & FMT_LEFT) fmt_fill(&o, CL(' '), width - l);
            continue;
        }

        if(conv == CL('D')) {
            uint64_t m = (uint64_t)__builtin_va_arg(args, efi_physical_address_t);
            int rows = width < 1 ? 1 : (width > 16 ? 16 : width);
            for(int row = 0; row < rows; row++) {
                for(int i = 44; i >= 0; i -= 4)
                    fmt_raw(&o, (char_t)"0123456789ABCDEF"[(m >> i) & 15]);
                fmt_raw(&o, CL(':')); fmt_raw(&o, CL(' '));
                for(int i = 0; i < 16; i++) {
                    uint8_t v = ((const uint8_t *)(uintptr_t)m)[i];
                    fmt_raw(&o, (char_t)"0123456789ABCDEF"[v >> 4]);
                    fmt_raw(&o, (char_t)"0123456789ABCDEF"[v & 15]);
                    fmt_raw(&o, CL(' '));
                }
                fmt_raw(&o, CL(' '));
                for(int i = 0; i < 16; i++) {
                    uint8_t v = ((const uint8_t *)(uintptr_t)m)[i];
                    fmt_raw(&o, (v < 32 || v >= 127) ? CL('.') : (char_t)v);
                }
                fmt_chr(&o, CL('\n'));
                m += 16;
            }
            continue;
        }

        /* Integer and pointer conversions, they all end up as an optional sign
         * or radix prefix followed by digits. */
        {
            char *p;
            const char *pfx = "";
            uint64_t x = 0;
            int base = 10, upper = 0, neg = 0, pl = 0;
            int nd, digits, total, padn, zeropad;

            if(conv == CL('d') || conv == CL('i')) {
                int64_t sv;
                switch(len) {
                case LEN_HH: sv = (int8_t)__builtin_va_arg(args, int); break;
                case LEN_H:  sv = (int16_t)__builtin_va_arg(args, int); break;
                case LEN_L:  sv = __builtin_va_arg(args, long); break;
                case LEN_LL: sv = __builtin_va_arg(args, long long); break;
                case LEN_T:  sv = __builtin_va_arg(args, __PTRDIFF_TYPE__); break;
                case LEN_J:  sv = __builtin_va_arg(args, intmax_t); break;
                case LEN_Z:  sv = (__PTRDIFF_TYPE__)__builtin_va_arg(args, size_t); break;
                default:     sv = __builtin_va_arg(args, int); break;
                }
                if(sv < 0) { neg = 1; x = (uint64_t)0 - (uint64_t)sv; } else x = (uint64_t)sv;
            } else if(conv == CL('u') || conv == CL('x') || conv == CL('X') || conv == CL('o')) {
                switch(len) {
                case LEN_HH: x = (uint8_t)__builtin_va_arg(args, unsigned int); break;
                case LEN_H:  x = (uint16_t)__builtin_va_arg(args, unsigned int); break;
                case LEN_L:  x = __builtin_va_arg(args, unsigned long); break;
                case LEN_LL: x = __builtin_va_arg(args, unsigned long long); break;
                case LEN_T:  x = (uint64_t)__builtin_va_arg(args, __PTRDIFF_TYPE__); break;
                case LEN_J:  x = __builtin_va_arg(args, uintmax_t); break;
                case LEN_Z:  x = __builtin_va_arg(args, size_t); break;
                default:     x = __builtin_va_arg(args, unsigned int); break;
                }
                if(conv == CL('x') || conv == CL('X')) { base = 16; upper = (conv == CL('X')); }
                else if(conv == CL('o')) base = 8;
            } else if(conv == CL('p')) {
                x = (uint64_t)(uintptr_t)__builtin_va_arg(args, void *);
                base = 16;
                flags |= FMT_ALT;
                if(prec < 0) prec = 16;
            } else {
                /* An unknown conversion, which includes every floating point
                 * one: emit nothing and consume no argument. */
                continue;
            }

            if(neg) { pfx = "-"; pl = 1; }
            else if(conv == CL('d') || conv == CL('i')) {
                if(flags & FMT_PLUS) { pfx = "+"; pl = 1; }
                else if(flags & FMT_SPACE) { pfx = " "; pl = 1; }
            }

            p = fmt_digits(x, (unsigned)base, upper, tmp + sizeof(tmp));
            nd = (int)(tmp + sizeof(tmp) - p);
            if(x == 0 && prec == 0) nd = 0;

            if(base == 8 && (flags & FMT_ALT) && prec < nd + 1) prec = nd + 1;
            if(base == 16 && (flags & FMT_ALT) && (x != 0 || conv == CL('p'))) {
                pfx = upper ? "0X" : "0x";
                pl = 2;
            }

            digits = nd > prec ? nd : prec;
            total = pl + digits;
            padn = width - total;
            if(padn < 0) padn = 0;
            zeropad = (flags & FMT_ZERO) && prec < 0;

            if(!(flags & FMT_LEFT)) {
                if(zeropad) {
                    for(int i = 0; i < pl; i++) fmt_raw(&o, (char_t)pfx[i]);
                    fmt_fill(&o, CL('0'), padn);
                    for(int i = 0; i < nd; i++) fmt_raw(&o, (char_t)p[i]);
                } else {
                    fmt_fill(&o, CL(' '), padn);
                    for(int i = 0; i < pl; i++) fmt_raw(&o, (char_t)pfx[i]);
                    fmt_fill(&o, CL('0'), digits - nd);
                    for(int i = 0; i < nd; i++) fmt_raw(&o, (char_t)p[i]);
                }
            } else {
                for(int i = 0; i < pl; i++) fmt_raw(&o, (char_t)pfx[i]);
                fmt_fill(&o, CL('0'), digits - nd);
                for(int i = 0; i < nd; i++) fmt_raw(&o, (char_t)p[i]);
                fmt_fill(&o, CL(' '), padn);
            }
        }
    }

    *o.dst = 0;
    return (int)o.count;
}
