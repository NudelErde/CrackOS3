#pragma once

#include "regs.h"

inline void hlt() {
    asm("hlt");
}

inline void cli() {
    asm("cli");
}

inline void sti() {
    asm("sti");
}

inline void flush_cache() {
    asm("wbinvd");
    asm("invd");
    auto cr3 = getCR3();
    setCR3(cr3);
}