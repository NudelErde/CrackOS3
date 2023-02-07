//
// Created by nudelerde on 15.12.22.
//

#pragma once
#include "int.h"

template<typename Number>
Number abs(Number n) {
    return n < 0 ? -n : n;
}

template<typename Number>
Number min(Number a, Number b) {
    return a < b ? a : b;
}

template<typename Number>
Number max(Number a, Number b) {
    return a > b ? a : b;
}

template<typename Number, typename ...Numbers>
auto min(Number number, Numbers... numbers) {
    return min(number, min(numbers...));
}

template<typename Number, typename ...Numbers>
auto max(Number number, Numbers... numbers) {
    return max(number, max(numbers...));
}
