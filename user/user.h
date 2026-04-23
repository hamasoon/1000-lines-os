#ifndef USER_H
#define USER_H

#include "common.h"

long syscall(long sysno, long arg0, long arg1, long arg2);
void putchar(char ch);
int getchar(void);
NORETURN void exit(void);
long read(const char *filename, char *buf, long len);
long write(const char *filename, const char *buf, long len);

#endif /* USER_H */
