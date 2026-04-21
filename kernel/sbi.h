#ifndef SBI_H
#define SBI_H

#include "common.h"

/**
 * @brief struct sbiret - return type for SBI calls
 *
 * @error: error code returned by the SBI call
 * @value: value returned by the SBI call (if any)
 */
typedef struct __sbiret {
    long error;
    long value;
} sbiret_t;

sbiret_t sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                  long arg5, long fid, long eid);
void putchar(char ch);
long getchar(void);

#endif /* SBI_H */
