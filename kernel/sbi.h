#ifndef SBI_H
#define SBI_H

#include "common.h"

typedef struct __sbiret {
    long error;
    long value;
} sbiret_t;

sbiret_t sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                  long arg5, long fid, long eid);
void putchar(char ch);
long getchar(void);

#endif /* SBI_H */
