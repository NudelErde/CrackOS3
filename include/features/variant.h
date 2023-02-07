//
// Created by nudelerde on 15.12.22.
//

#pragma once

#include "features/math.h"
#include "features/types.h"
#include "features/optional.h"

template<size_t arg1, size_t... others>
struct static_max;

template<size_t arg>
struct static_max<arg> {
    static const size_t value = arg;
};

template<size_t arg1, size_t arg2, size_t... others>
struct static_max<arg1, arg2, others...> {
    static const size_t value = arg1 >= arg2 ? static_max<arg1, others...>::value : static_max<arg2, others...>::value;
};

template<typename... Ts>
struct variant_helper;

template<typename T, typename... Ts>
struct variant_helper<T, Ts...> {
    static constexpr size_t index = sizeof...(Ts);

    template<size_t id, typename... Args>
    static void construct(void* memory, Args&&... args) {
        if constexpr (id == index) {
            new (memory) T(std::forward<Args>(args)...);
        } else {
            variant_helper<Ts...>().template construct<id>(memory, std::forward<Args>(args)...);
        }
    }

    template<typename U>
    static bool is(size_t id) {
        if (id == index) {
            return std::is_same_v<T, U>;
        } else {
            return variant_helper<Ts...>::template is<U>(id);
        }
    }

    template<typename U>
    constexpr static size_t index_of() {
        if (std::is_same_v<T, U>) {
            return index;
        } else {
            return variant_helper<Ts...>::template index_of<U>();
        }
    }

    static void destruct(size_t id, void* memory) {
        if (id == index) {
            reinterpret_cast<T*>(memory)->~T();
        } else {
            variant_helper<Ts...>().destruct(id, memory);
        }
    }

    static void copy(size_t id, const void* from, void* to) {
        if (id == index) {
            new (to) T(*reinterpret_cast<const T*>(from));
        } else {
            variant_helper<Ts...>().copy(id, from, to);
        }
    }

    static void move(size_t id, void* from, void* to) {
        if (id == index) {
            new (to) T(std::move(*reinterpret_cast<T*>(from)));
        } else {
            variant_helper<Ts...>().move(id, from, to);
        }
    }

    static void copy_assign(size_t id, const void* from, void* to) {
        if (id == index) {
            *reinterpret_cast<T*>(to) = *reinterpret_cast<const T*>(from);
        } else {
            variant_helper<Ts...>().copy_assign(id, from, to);
        }
    }

    static void move_assign(size_t id, void* from, void* to) {
        if (id == index) {
            *reinterpret_cast<T*>(to) = std::move(*reinterpret_cast<T*>(from));
        } else {
            variant_helper<Ts...>().move_assign(id, from, to);
        }
    }
};

template<typename T>
struct variant_helper<T> {
    static constexpr size_t index = 0;

    template<size_t id, typename... Args>
    static void construct(void* memory, Args&&... args) {
        if constexpr (id == index) {
            new (memory) T(std::forward<Args>(args)...);
        } else {
            panic("Invalid variant index");
        }
    }

    template<typename U>
    static bool is(size_t id) {
        if (id == index) {
            return std::is_same_v<T, U>;
        } else {
            panic("Invalid variant index");
        }
    }

    template<typename U>
    constexpr static size_t index_of() {
        if (std::is_same_v<T, U>) {
            return index;
        } else {
            return ~0;
        }
    }

    static void destruct(size_t id, void* memory) {
        if (id == index) {
            reinterpret_cast<T*>(memory)->~T();
        } else {
            panic("Invalid variant index");
        }
    }

    static void copy(size_t id, const void* from, void* to) {
        if (id == index) {
            new (to) T(*reinterpret_cast<const T*>(from));
        } else {
            panic("Invalid variant index");
        }
    }

    static void move(size_t id, void* from, void* to) {
        if (id == index) {
            new (to) T(std::move(*reinterpret_cast<T*>(from)));
        } else {
            panic("Invalid variant index");
        }
    }

    static void copy_assign(size_t id, const void* from, void* to) {
        if (id == index) {
            *reinterpret_cast<T*>(to) = *reinterpret_cast<const T*>(from);
        } else {
            panic("Invalid variant index");
        }
    }

    static void move_assign(size_t id, void* from, void* to) {
        if (id == index) {
            *reinterpret_cast<T*>(to) = std::move(*reinterpret_cast<T*>(from));
        } else {
            panic("Invalid variant index");
        }
    }
};

template<typename... Ts>
struct variant {
    uint8_t buffer[static_max<sizeof(Ts)...>::value]{};

    using helper = variant_helper<Ts...>;

    size_t type_index;

public:
    variant() : type_index(sizeof...(Ts)-1) {
        helper::template construct<static_cast<size_t>(sizeof...(Ts)-1)>(buffer);
    }

    template<typename T>
    explicit variant(T&& t) : type_index(helper::template index_of<std::remove_reference<T>>) {
        helper::move(type_index, &t, buffer);
    }

    variant(const variant& other) : type_index(other.type_index) {
        helper::copy(type_index, other.buffer, buffer);
    }

    variant(variant&& other) noexcept : type_index(other.type_index) {
        helper::move(type_index, other.buffer, buffer);
    }

    ~variant() {
        helper::destruct(type_index, buffer);
    }

    variant& operator=(const variant& other) {
        if (this == &other) { return *this; }
        if(other.type_index != type_index) {
            helper::destruct(type_index, buffer);
            type_index = other.type_index;
            helper::copy(type_index, other.buffer, buffer);
        } else {
            helper::copy_assign(type_index, other.buffer, buffer);
        }
        return *this;
    }

    variant& operator=(variant&& other) noexcept {
        if (this == &other) { return *this; }
        if(other.type_index != type_index) {
            helper::destruct(type_index, buffer);
            type_index = other.type_index;
            helper::move(type_index, other.buffer, buffer);
        } else {
            helper::move_assign(type_index, other.buffer, buffer);
        }
        return *this;
    }

    template<typename U>
    variant& operator=(const U& u) {
        size_t id = helper::template index_of<typename std::remove_reference<U>::type>();
        if(id == type_index) {
            helper::copy_assign(type_index, &u, buffer);
        } else {
            helper::destruct(type_index, buffer);
            type_index = id;
            helper::copy(type_index, &u, buffer);
        }
        return *this;
    }

    template<typename U>
    optional<U> get() {
        if (helper::template is<U>(type_index)) {
            return optional<U>(reinterpret_cast<U&>(buffer));
        } else {
            return optional<U>();
        }
    }
};
