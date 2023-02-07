//
// Created by nudelerde on 01.01.23.
//

#include "features/lock.h"

void spinlock::lock() {
    asm volatile(R"(
    .lock_loop:
        lock btsq $0, (%0)
        jnc .done
    .lock_spin:
        pause
        testq $1, (%0)
        jnz .lock_spin
        jmp .lock_loop
    .done:
        nop
    )" ::"r"(&data));
}

void spinlock::unlock() {
    data = 0;
}

bool spinlock::is_locked() const {
    return data & 1;
}

bool spinlock::try_lock() {
    bool success;
    asm volatile(R"(
    lock btsq $0, (%1)
    setnc %0
    )"
                 : "=r"(success)
                 : "r"(&data));
    return success;
}
