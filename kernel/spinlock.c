#include "spinlock.h"
#include "process.h"
#include "kernel.h"
#include "common.h"

static inline int holding(spinlock_t *lock) {
    return lock->locked;
}

void spinlock_init(spinlock_t *lock, const char *name) {
    lock->locked = 0;
    lock->name = name;
    lock->owner = NULL;
}

void spinlock_acquire(spinlock_t *lock, uint64_t *flags) {
    local_irq_save(flags);
    if (holding(lock))
        PANIC("recursive acquire on %s", lock->name);
    while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
        ;
    __sync_synchronize();           /* acquire fence */
    lock->owner = get_cpu();
}

void spinlock_release(spinlock_t *lock, uint64_t flags) {
    if (!holding(lock))
        PANIC("release without holding %s", lock->name);
    lock->owner = NULL;
    __sync_synchronize();           /* ensure owner=NULL is visible before unlock */
    __sync_lock_release(&lock->locked);
    local_irq_restore(flags);
}
