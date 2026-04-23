#include "common.h"

void *memset(void *buf, char c, size_t n) {
    char *p = buf;
    for (size_t i = 0; i < n; i++) {
        p[i] = c;
    }
    return buf;
}

void *memcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void print_udec(unsigned long long value) {
    unsigned long long divisor = 1;
    while (value / divisor > 9)
        divisor *= 10;

    while (divisor > 0) {
        putchar('0' + (char) ((value / divisor) % 10));
        divisor /= 10;
    }
}

static void print_dec(long long value) {
    unsigned long long mag;
    if (value < 0) {
        putchar('-');
        /* -(value+1)+1 avoids UB when value == LLONG_MIN (plain -value overflows). */
        mag = (unsigned long long) (-(value + 1)) + 1;
    } else {
        mag = (unsigned long long) value;
    }
    print_udec(mag);
}

static void print_hex(unsigned long long value, int digits) {
    for (int i = digits - 1; i >= 0; i--) {
        unsigned nib = (value >> (i * 4)) & 0xf;
        putchar("0123456789abcdef"[nib]);
    }
}

/*
 * Supported specifiers:
 *   %s                  NUL-terminated string (NULL prints as "(null)")
 *   %c                  char
 *   %d  %ld  %lld       signed decimal
 *   %u  %lu  %llu       unsigned decimal
 *   %x  %lx  %llx       hex, leading-zero (8 / 16 / 16 digits)
 *   %p                  pointer, 0x + 16-digit hex
 *   %%                  literal '%'
 *
 * Length modifiers are branched strictly by C standard types even though
 * lp64 makes `long` and `long long` the same width -- keeps va_arg portable.
 */
void printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt++);
            continue;
        }

        fmt++;
        int lmod = 0;
        if (*fmt == 'l') {
            lmod = 1;
            fmt++;
            if (*fmt == 'l') {
                lmod = 2;
                fmt++;
            }
        }

        switch (*fmt) {
            case '\0':
                putchar('%');
                goto end;
            case '%':
                putchar('%');
                break;
            case 'c':
                putchar((char) va_arg(vargs, int));
                break;
            case 's': {
                const char *s = va_arg(vargs, const char *);
                if (!s)
                    s = "(null)";
                while (*s)
                    putchar(*s++);
                break;
            }
            case 'd': {
                long long v;
                if (lmod == 2)      v = va_arg(vargs, long long);
                else if (lmod == 1) v = va_arg(vargs, long);
                else                v = va_arg(vargs, int);
                print_dec(v);
                break;
            }
            case 'u': {
                unsigned long long v;
                if (lmod == 2)      v = va_arg(vargs, unsigned long long);
                else if (lmod == 1) v = va_arg(vargs, unsigned long);
                else                v = va_arg(vargs, unsigned int);
                print_udec(v);
                break;
            }
            case 'x': {
                unsigned long long v;
                int digits;
                if (lmod == 2) {
                    v = va_arg(vargs, unsigned long long);
                    digits = 16;
                } else if (lmod == 1) {
                    v = va_arg(vargs, unsigned long);
                    digits = 16;
                } else {
                    v = va_arg(vargs, unsigned int);
                    digits = 8;
                }
                print_hex(v, digits);
                break;
            }
            case 'p': {
                void *p = va_arg(vargs, void *);
                putchar('0');
                putchar('x');
                print_hex((unsigned long long) (uint64_t) p, 16);
                break;
            }
            default:
                putchar('%');
                if (lmod >= 1) putchar('l');
                if (lmod >= 2) putchar('l');
                putchar(*fmt);
                break;
        }

        fmt++;
    }

end:
    va_end(vargs);
}
