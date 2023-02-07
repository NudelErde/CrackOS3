//
// Created by nudelerde on 08.01.23.
//

#pragma once

#include "types.h"
#include "optional.h"

template<typename Err>
inline const char* error_to_string(Err err) {
    return "Unknown error";
}

template<typename T, typename E>
struct result {
private:
    optional<T> _value;
    optional<E> _error;
public:

    constexpr result(E e) : _value(), _error(e) {}
    constexpr result(T v) : _value(v), _error() {}

    constexpr operator bool() const {
        return !_error.has_value();
    }
    constexpr bool operator !() const {
        return _error.has_value();
    }

    [[nodiscard]] constexpr bool has_value() const {
        return _value.has_value();
    }

    [[nodiscard]] constexpr  bool has_error() const {
        return _error.has_value();
    }

    constexpr const T& value() const {
        return _value.value_or_panic("result::value() called on error");
    }

    constexpr const E& error() const {
        return _error.value_or_panic("result::error() called on value");
    }

    [[nodiscard]] constexpr const char* error_string() const {
        return error_to_string(_error.value_or_panic("result::error_string() called on value"));
    }
};