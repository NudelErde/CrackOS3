//
// Created by nudelerde on 11.12.22.
//

#include "out/panic.h"

extern "C" [[maybe_unused]] void __cxa_pure_virtual() {
    panic("pure virtual function called");
}