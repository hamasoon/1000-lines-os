#include "time.h"

uint64_t read_time(void) {
    uint64_t t;
    __asm__ __volatile__("rdtime %0" : "=r"(t));
    return t;
}
