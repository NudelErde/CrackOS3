#pragma once

#include "int.h"

__attribute__((always_inline)) constexpr inline unsigned long long int operator"" _Ki(unsigned long long t) {
    return t * 1024;
}

__attribute__((always_inline)) constexpr inline unsigned long long int operator"" _Mi(unsigned long long t) {
    return t * 1024_Ki;
}

__attribute__((always_inline)) constexpr inline unsigned long long int operator"" _Gi(unsigned long long t) {
    return t * 1024_Mi;
}

__attribute__((always_inline)) constexpr inline unsigned long long int operator"" _Ti(unsigned long long t) {
    return t * 1024_Gi;
}
