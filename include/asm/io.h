#pragma once
#include "int.h"

inline void ioWrite8(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1"
                 :
                 : "a"(value), "Nd"(port));
}

inline void ioWrite16(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1"
                 :
                 : "a"(value), "Nd"(port));
}

inline void ioWrite32(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1"
                 :
                 : "a"(value), "Nd"(port));
}

inline uint8_t ioRead8(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}

inline uint16_t ioRead16(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}

inline uint32_t ioRead32(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}