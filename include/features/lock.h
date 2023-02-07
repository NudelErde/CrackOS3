//
// Created by nudelerde on 01.01.23.
//

#pragma once

#include "int.h"

struct spinlock {
    volatile uint64_t data = 0;

    void lock();
    void unlock();
    [[nodiscard]] bool is_locked() const;
    [[nodiscard]] bool try_lock();
};

template<typename LOCK_T>
struct lock_guard {
    LOCK_T& lock;

    explicit lock_guard(LOCK_T& lock) : lock(lock) {
        lock.lock();
    }

    ~lock_guard() {
        lock.unlock();
    }
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};