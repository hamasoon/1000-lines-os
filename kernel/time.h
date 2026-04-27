#ifndef TIME_H
#define TIME_H

#include "common.h"

/* QEMU virt machine drives mtime at 10 MHz, so one tick == 100 ns.
 * 10 ms preemption slice => 100_000 ticks. */
#define TIMER_INTERVAL  100000ULL

uint64_t read_time(void);

#endif /* TIME_H */
