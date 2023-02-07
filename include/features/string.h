//
// Created by nudelerde on 11.12.22.
//

#pragma once

#include "int.h"

constexpr inline size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Function to implement strcmp function
inline int strcmp(const char* X, const char* Y) {
    while (*X) {
        // if characters differ, or end of the second string is reached
        if (*X != *Y) {
            break;
        }

        // move to the next pair of characters
        X++;
        Y++;
    }

    // return the ASCII difference after converting `char*` to `unsigned char*`
    return *(const unsigned char*) X - *(const unsigned char*) Y;
}