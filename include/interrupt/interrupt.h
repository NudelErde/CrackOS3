//
// Created by Florian on 27.11.2022.
//

#pragma once

#include "int.h"
#include "memory/mem.h"

namespace Interrupt {

using handler_t = void (*)(uint8_t number, uint64_t code, void* stack_ptr, void* user_ptr);

void enable();
void disable();
bool isEnabled();
void init();
void interrupt(uint8_t number);
void registerHandler(uint8_t number, handler_t handler, void* user_ptr = nullptr);
void sendEOI();
uint8_t get_free_interrupt_number();
void clear_startup();
void setNativeInterruptHandler(VirtualAddress address, uint8_t number, uint8_t stack);

struct Guard {
    Guard() {
        wasEnabled = isEnabled();
        Interrupt::disable();
    }

    ~Guard() {
        if (wasEnabled) {
            Interrupt::enable();
        }
    }

    void release() {
        wasEnabled = false;
    }

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&&) = delete;
    Guard& operator=(Guard&&) = delete;

private:
    bool wasEnabled;
};

}// namespace Interrupt