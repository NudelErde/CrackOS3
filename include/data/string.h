//
// Created by nudelerde on 21.01.23.
//

#pragma once

#include "features/string.h"
#include "memory/mem.h"
#include "int.h"

struct string {
private:
    bool owns_data = true;

public:
    char* data;
    size_t length;

    constexpr string() : data(nullptr), length(0) {}
    constexpr string(const char* str) : data((char*) str), length(strlen(str)), owns_data(false) {}
    string(const string& str) : data(new char[str.length]), length(str.length) {
        memcpy(data, str.data, length);
    }
    string(string&& str) noexcept {
        if (str.owns_data) {
            data = str.data;
            length = str.length;
        } else {
            data = new char[str.length + 1];
            length = str.length;
            memcpy(data, str.data, length);
            data[length] = '\0';
        }
        str.data = nullptr;
        str.length = 0;
    }
    ~string() {
        if (owns_data)
            delete[] data;
    }
    string& operator=(const string& str) {
        if (this == &str)
            return *this;
        if (owns_data) {
            if (length < str.length) {
                delete[] data;
                data = new char[str.length + 1];
                data[str.length] = '\0';
            }
        } else {
            data = new char[str.length + 1];
            data[str.length] = '\0';
            owns_data = true;
        }
        length = str.length;
        memcpy(data, str.data, length);
        return *this;
    }
    string& operator=(string&& str) noexcept {
        if (this == &str)
            return *this;
        if (owns_data) {
            delete[] data;
        }
        if (str.owns_data) {
            data = str.data;
            length = str.length;
        } else {
            data = new char[str.length + 1];
            length = str.length;
            memcpy(data, str.data, length);
            data[length] = '\0';
        }
        str.data = nullptr;
        str.length = 0;
        return *this;
    }
    bool operator==(const string& str) const {
        if (length != str.length)
            return false;
        return memcmp(data, str.data, length) == 0;
    }
    bool operator!=(const string& str) const {
        return !(*this == str);
    }
    bool operator<(const string& str) const {
        if (length != str.length)
            return length < str.length;
        return memcmp(data, str.data, length) < 0;
    }
    bool operator>(const string& str) const {
        if (length != str.length)
            return length > str.length;
        return memcmp(data, str.data, length) > 0;
    }
    bool operator<=(const string& str) const {
        if (length != str.length)
            return length <= str.length;
        return memcmp(data, str.data, length) <= 0;
    }
    bool operator>=(const string& str) const {
        if (length != str.length)
            return length >= str.length;
        return memcmp(data, str.data, length) >= 0;
    }
    char& operator[](size_t index) {
        return data[index];
    }
    const char& operator[](size_t index) const {
        return data[index];
    }
    string operator+(const string& str) const {
        string result;
        result.length = length + str.length;
        result.data = new char[result.length + 1];
        memcpy(result.data, data, length);
        memcpy(result.data + length, str.data, str.length);
        result.data[result.length] = '\0';
        return result;
    }
    string& operator+=(const string& str) {
        char* new_data = new char[length + str.length + 1];
        memcpy(new_data, data, length);
        memcpy(new_data + length, str.data, str.length);
        new_data[length + str.length] = '\0';
        if (owns_data)
            delete[] data;
        data = new_data;
        length += str.length;
        owns_data = true;
        return *this;
    }
    static string from_char_array(const char* ptr, size_t size) {
        for(size_t i = 0; i < size; i++) {
            if(ptr[i] == '\0') {
                size = i;
                break;
            }
        }
        string result;
        result.length = size;
        result.data = new char[size + 1];
        memcpy(result.data, ptr, size);
        result.data[size] = '\0';
        return result;
    }
};