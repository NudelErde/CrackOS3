//
// Created by nudelerde on 03.01.23.
//

#pragma once
#include "int.h"

struct duration_t {
    uint64_t nanoseconds;
};

inline duration_t operator"" _s(unsigned long long seconds) {
    return {seconds * 1000000000};
}

inline duration_t operator"" _ms(unsigned long long milliseconds) {
    return {milliseconds * 1000000};
}

inline duration_t operator"" _us(unsigned long long microseconds) {
    return {microseconds * 1000};
}

inline duration_t operator"" _ns(unsigned long long nanoseconds) {
    return {nanoseconds};
}

inline duration_t operator"" _minutes(unsigned long long minutes) {
    return {minutes * 60 * 1000000000};
}

inline duration_t operator"" _hours(unsigned long long hours) {
    return {hours * 60 * 60 * 1000000000};
}

inline duration_t operator"" _days(unsigned long long days) {
    return {days * 24 * 60 * 60 * 1000000000};
}
