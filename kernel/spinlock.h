#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "common.h"
#include "exception.h"

/* Forward-declare cpu_t to avoid a circular include with process.h
 * (Phase 4 will embed a spinlock_t inside ptable in process.c). C11 allows
 * redefining a typedef name to the same type, so process.h's full definition
 * is still legal. */
typedef struct __cpu cpu_t;

typedef struct __spinlock {
    volatile uint64_t locked;

    // for debugging
    const char *name;
    cpu_t      *owner;
} spinlock_t;

static inline void local_irq_save(uint64_t *flags) {
    *flags = READ_CSR(sstatus) & SSTATUS_SIE;
    CLEAR_CSR(sstatus, SSTATUS_SIE);
}

static inline void local_irq_restore(uint64_t flags) {
    SET_CSR(sstatus, flags);
}

void spinlock_init(spinlock_t *lock, const char *name);
void spinlock_acquire(spinlock_t *lock, uint64_t *flags);
void spinlock_release(spinlock_t *lock, uint64_t flags);

#endif /* SPINLOCK_H */
