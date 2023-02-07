#pragma once
#include "memory/mem.h"
#include "out/panic.h"
#include "types.h"

template<typename T>
struct optional {
private:
    char buffer[sizeof(T)];
    bool initialized;

    void destroy() {
        if (initialized) {
            ((T*) buffer)->~T();
            initialized = false;
        }
    }

public:
    using value_type = T;

    optional() : initialized(false) {
    }
    optional(const T& value) : initialized(true) {
        new (buffer) T(value);
    }
    optional(T&& value) : initialized(true) {
        new (buffer) T(std::move(value));
    }

    optional(const optional& other) : initialized(other.initialized) {
        if (initialized) {
            new (buffer) T(*other);
        }
    }

    optional(optional&& other) : initialized(other.initialized) {
        if (initialized) {
            new (buffer) T(std::move(*other));
        }
        other.initialized = false;
    }

    template<typename... ArgT>
    optional(ArgT&&... args) : initialized(true) {
        new (buffer) T(std::forward<ArgT>(args)...);
    }

    ~optional() {
        destroy();
    }

    T* operator->() {
        return (T*) buffer;
    }

    T& operator*() {
        return *((T*) buffer);
    }

    const T* operator->() const {
        return (T*) buffer;
    }

    const T& operator*() const {
        return *((T*) buffer);
    }

    optional& operator=(const optional& other) {
        if(this == &other) return *this;
        destroy();
        if (other.initialized) {
            new (buffer) T(*other);
            initialized = true;
        }
        return *this;
    }

    optional& operator=(optional&& other)  noexcept {
        destroy();
        if (other.initialized) {
            new (buffer) T(std::move(*other));
            initialized = true;
        }
        other.initialized = false;
        return *this;
    }

    [[nodiscard]] bool has_value() const {
        return initialized;
    }

    T& value() {
        return *((T*) buffer);
    }

    const T& value_or_panic(const char* message) const {
        if (!initialized) {
            panic(message);
        }
        return *((T*) buffer);
    }

    const T& value() const {
        return *((T*) buffer);
    }

    template<typename Func, typename... ArgT>
    T value_or_create(Func func, ArgT... args) {
        if (initialized) {
            return value();
        } else {
            return func(args...);
        }
    }

    template<typename Func, typename... ArgT>
    T value_or_create(Func func, ArgT... args) const {
        if (initialized) {
            return value();
        } else {
            return func(args...);
        }
    }

    const T& value_or(const T& default_value) const {
        if (initialized) {
            return *((T*) buffer);
        } else {
            return default_value;
        }
    }

    operator bool() const {
        return initialized;
    }

    bool operator!() const {
        return !initialized;
    }

    template<typename R, typename Func, typename... ArgT>
    optional<R> map(Func func, ArgT... args) {
        if (initialized) {
            return func(value(), std::forward<ArgT>(args)...);
        } else {
            return {};
        }
    }
};