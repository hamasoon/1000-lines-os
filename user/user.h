#ifndef USER_H
#define USER_H

#include "common.h"

int syscall(int sysno, int arg0, int arg1, int arg2);
void putchar(char ch);
int getchar(void);
NORETURN void exit(void);
int read(const char *filename, char *buf, int len);
int write(const char *filename, const char *buf, int len);

#endif /* USER_H */