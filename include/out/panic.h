#pragma once

[[noreturn]] void panic(const char* message);
inline void assert(bool condition, const char* message) {
    if (!condition) {
        panic(message);
    }
}