#include "common.h"

/**
 * memset - fill a block of memory with a specific character
 * @buf: pointer to the block of memory to fill
 * @c: the character to fill the memory with
 * @n: the number of bytes to fill
 */
void *memset(void *buf, char c, size_t n) {
    char *p = buf;
    for (size_t i = 0; i < n; i++) {
        p[i] = c;
    }
    return buf;
}

/**
 * memcpy - copy a block of memory from source to destination
 * @dst: pointer to the destination block of memory
 * @src: pointer to the source block of memory
 * @n: the number of bytes to copy
 */
void *memcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

/**
 * strcpy - copy a null-terminated string from source to destination
 * @dst: pointer to the destination buffer where the string will be copied
 * @src: pointer to the null-terminated source string to copy
 */
char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++))
        ;
    return ret;
}

/**
 * strcmp - compare two null-terminated strings
 * @s1: pointer to the first null-terminated string
 * @s2: pointer to the second null-terminated string
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/**
 * printf - a simple implementation of printf that supports %s, %d, %x, and %% format specifiers.
 * @fmt: the format string, which may contain format specifiers like %
 * ...: variable arguments corresponding to the format specifiers in fmt
 * 
 * This implementation of printf is designed to be simple and does not support all features of the standard printf.
 * It uses the putchar function to output characters one at a time, and it handles a limited set of format specifiers
 * for strings, decimal integers, hexadecimal integers, and literal percent signs.
 */
void printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++; // Skip '%'
            switch (*fmt) { // Read the next character
                case '\0': // '%' at the end of the format string
                    putchar('%');
                    goto end;
                case '%': // Print '%'
                    putchar('%');
                    break;
                case 's': { // Print a NULL-terminated string.
                    const char *s = va_arg(vargs, const char *);
                    while (*s) {
                        putchar(*s);
                        s++;
                    }
                    break;
                }
                case 'd': { // Print an integer in decimal.
                    int value = va_arg(vargs, int);
                    unsigned magnitude = value; // https://github.com/nuta/operating-system-in-1000-lines/issues/64
                    if (value < 0) {
                        putchar('-');
                        magnitude = -magnitude;
                    }

                    unsigned divisor = 1;
                    while (magnitude / divisor > 9)
                        divisor *= 10;

                    while (divisor > 0) {
                        putchar('0' + magnitude / divisor);
                        magnitude %= divisor;
                        divisor /= 10;
                    }

                    break;
                }
                case 'x': { // Print an integer in hexadecimal.
                    unsigned value = va_arg(vargs, unsigned);
                    for (int i = 7; i >= 0; i--) {
                        unsigned nibble = (value >> (i * 4)) & 0xf;
                        putchar("0123456789abcdef"[nibble]);
                    }
                }
            }
        } else {
            putchar(*fmt);
        }

        fmt++;
    }

end:
    va_end(vargs);
}