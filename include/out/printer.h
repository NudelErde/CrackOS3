//
// Created by nudelerde on 15.12.22.
//

#pragma once
#include "data/string.h"
#include "features/bytes.h"
#include "panic.h"

struct printer_impl {
    virtual void print(char c) = 0;
    virtual void print(const char* str) = 0;
    virtual void print(int8_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(int16_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(int32_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(int64_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(uint8_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(uint16_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(uint32_t value, uint8_t base, uint8_t minDigits) = 0;
    virtual void print(uint64_t value, uint8_t base, uint8_t minDigits) = 0;
};

struct printer {
    printer_impl* impl;
    inline void print(char c) const { impl->print(c); }
    inline void print(const char* str) const { impl->print(str); }
    inline void print(const string& str) const { impl->print(str.data); }
    inline void print(int8_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(int16_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(int32_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(int64_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(uint8_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(uint16_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(uint32_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    inline void print(uint64_t value, uint8_t base = 10, uint8_t minDigits = 0) const { impl->print(value, base, minDigits); }
    template<typename first, typename... Args>
    inline const char* printf(const char* format, first f, Args... args) const {
        format = printf_impl(format, f);
        return printf(format, args...);
    }
    template<typename first>
    inline const char* printf(const char* format, first f) const {
        format = printf_impl(format, f);
        return printf(format);
    }
    inline const char* printf(const char* format) const {
        if (print_to_next(format) != 0) {
            panic("Invalid format specifier");
        }
        return format;
    }
    inline const char* printf_impl(const char* format, char c) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (spec == 'c') {
                print(c);
            } else if (!print_num(spec, static_cast<uint64_t>(c))) {
                panic("Invalid format specifier");
            }
        }
        return format;
    }
    inline const char* printf_impl(const char* format, char* str) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (spec == 's') {
                print(str);
            } else if (!print_num(spec, reinterpret_cast<uint64_t>(str))) {
                panic("Invalid format specifier");
            }
        }
        return format;
    }
    inline const char* printf_impl(const char* format, const char* str) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (spec == 's') {
                print(str);
            } else if (!print_num(spec, reinterpret_cast<uint64_t>(str))) {
                panic("Invalid format specifier");
            }
        }
        return format;
    }
    inline const char* printf_impl(const char* format, const string& str) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (spec == 's') {
                print(str);
            } else {
                panic("Invalid format specifier");
            }
        }
        return format;
    }
    template<typename Number>
    inline const char* printf_impl(const char* format, Number value) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (!print_num(spec, static_cast<uint64_t>(value))) {
                panic("Invalid format specifier");
            }
        }
        return format;
    }
    template<typename PtrType>
    inline const char* printf_impl(const char* format, PtrType* value) const {
        if (!*format) return format;
        if (char spec = print_to_next(format)) {
            if (!print_num(spec, reinterpret_cast<uint64_t>(value))) {
                panic("Invalid format specifier");
            }
        }
        return format;
    }

private:
    [[nodiscard]] bool print_num(char format, uint64_t value) const {
        switch (format) {
            case 'd':
            case 'i':
                print((int64_t) value);
                return true;
            case 'u':
                print(value);
                return true;
            case 'x':
            case 'p':
            case 'X':
                print(value, 16);
                return true;
            case 'b':
            case 'B':
                if (value > 1_Ti) {
                    value /= 1_Gi;
                    print(value);
                    print("Gi");
                } else if (value > 1_Gi) {
                    value /= 1_Mi;
                    print(value);
                    print("Mi");
                } else if (value > 1_Mi) {
                    value /= 1_Ki;
                    print(value);
                    print("Ki");
                }
                return true;
            default:
                return false;
        }
    }
    char print_to_next(const char*& format) const {
        for (; *format; ++format) {
            if (*format == '%') {
                char specifier = *(++format);
                if (specifier == '%') {
                    print('%');
                } else {
                    format++;
                    return specifier;
                }
            } else {
                print(*format);
            }
        }
        return 0;
    }
};
