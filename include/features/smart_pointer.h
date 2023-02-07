//
// Created by nudelerde on 16.12.22.
//

#pragma once

#include "int.h"
#include "out/panic.h"

struct single_deleter {
    template<typename T>
    static void do_delete(T* ptr) {
        delete ptr;
    }
};

struct array_deleter {
    template<typename T>
    static void do_delete(T* ptr) {
        delete[] ptr;
    }
};

template<typename T>
struct [[maybe_unused]] default_deleter {
    using type = single_deleter;
};

template<typename T>
struct [[maybe_unused]] default_deleter<T[]> {
    using type = array_deleter;
};

template<typename T>
struct remove_array {
    using type = T;
};

template<typename T>
struct remove_array<T[]> {
    using type = T;
};

template<typename T, typename deleter = typename default_deleter<T>::type>
class unique_ptr {
    using type = typename remove_array<T>::type;
    type* ptr{};

public:
    unique_ptr() = default;
    unique_ptr(type* ptr) : ptr(ptr) {}
    template<typename... ArgT>
    explicit unique_ptr(ArgT&&... args) : ptr(new T(args...)) {}
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr(unique_ptr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    ~unique_ptr() {
        deleter::do_delete(ptr);
    }
    unique_ptr& operator=(const unique_ptr&) = delete;
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            delete ptr;
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    const type* operator->() const {
        return ptr;
    }
    type* operator->() {
        return ptr;
    }
    const type& operator*() const {
        return *ptr;
    }
    type& operator*() {
        return *ptr;
    }
    const type* get() const {
        return ptr;
    }
    type* get() {
        return ptr;
    }
    const type& operator[](size_t index) const {
        return ptr[index];
    }
    type& operator[](size_t index) {
        return ptr[index];
    }
    type* release() {
        auto tmp = ptr;
        ptr = nullptr;
        return tmp;
    }
    void reset(type* p) {
        deleter::do_delete(this->ptr);
        this->ptr = p;
    }
    void reset() {
        deleter::do_delete(this->ptr);
        this->ptr = nullptr;
    }
    explicit operator bool() const {
        return ptr;
    }
};

struct ref_count {
    size_t strong_count;
    size_t weak_count;
};

template<typename T>
class weak_ptr;

template<typename T, typename deleter = typename default_deleter<T>::type>
class shared_ptr {
    using type = typename remove_array<T>::type;
    type* ptr{};
    ref_count* ref{};

    friend class weak_ptr<T>;

public:
    shared_ptr() = default;
    shared_ptr(decltype(nullptr)) : shared_ptr() {}
    shared_ptr(type* ptr) : ptr(ptr), ref(new ref_count{1, 0}) {}
    shared_ptr(type* ptr, ref_count* ref) : ptr(ptr), ref(ref) {
        if(ref)
            ref->strong_count++;
    }
    shared_ptr(const shared_ptr& other) : ptr(other.ptr), ref(other.ref) {
        if (ref)
            ++ref->strong_count;
    }
    shared_ptr(shared_ptr&& other) noexcept : ptr(other.ptr), ref(other.ref) {
        other.ptr = nullptr;
        other.ref = nullptr;
    }
    template<typename... ArgT>
    explicit shared_ptr(ArgT&&... args) : ptr(new T(args...)), ref(new ref_count{1, 0}) {}
    void reset() {
        if (ref) {
            if (ref->strong_count == 0) panic("shared_ptr: invalid count");
            ref->strong_count--;
            if (ref->strong_count == 0) {
                deleter::do_delete(ptr);
                if (ref->weak_count == 0) {
                    delete ref;
                    ref = nullptr;
                }
            }
        }
    }

    ~shared_ptr() {
        reset();
    }
    shared_ptr& operator=(const shared_ptr& other) {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            ref = other.ref;
            if (ref)
                ++ref->strong_count;
        }
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            ref = other.ref;
            other.ptr = nullptr;
            other.ref = nullptr;
        }
        return *this;
    }

    const type* operator->() const {
        return ptr;
    }
    const type& operator*() const {
        return *ptr;
    }
    type* operator->() {
        return ptr;
    }
    type& operator*() {
        return *ptr;
    }
    type* get() {
        return ptr;
    }
    const type* get() const {
        return ptr;
    }

    type& operator[](size_t index) {
        return ptr[index];
    }

    const type& operator[](size_t index) const {
        return ptr[index];
    }

    explicit operator bool() const {
        return ptr;
    }

    [[nodiscard]] size_t use_count() const {
        return ref->strong_count;
    }

    void swap(shared_ptr& other) {
        auto tmp = ptr;
        ptr = other.ptr;
        other.ptr = tmp;
        tmp = ref;
        ref = other.ref;
        other.ref = tmp;
    }
};

template<typename T>
class weak_ptr {
    using type = typename remove_array<T>::type;
    type* ptr{};
    ref_count* ref{};

public:
    weak_ptr() = default;
    template<typename deleter>
    weak_ptr(const shared_ptr<T, deleter>& other) : ptr(other.ptr), ref(other.ref) {
        ++ref->weak_count;
    }
    weak_ptr(const weak_ptr& other) : ptr(other.ptr), ref(other.ref) {
        ++ref->weak_count;
    }
    weak_ptr(weak_ptr&& other) noexcept : ptr(other.ptr), ref(other.ref) {
        other.ptr = nullptr;
        other.ref = nullptr;
    }
    void reset() {
        if (ref) {
            if (ref->weak_count == 0) panic("weak_ptr: invalid count");
            ref->weak_count--;
            if (ref->weak_count == 0 && ref->strong_count == 0) {
                delete ref;
                ref = nullptr;
            }
        }
    }
    ~weak_ptr() {
        reset();
    }
    weak_ptr& operator=(const weak_ptr& other) {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            ref = other.ref;
            ++ref->weak_count;
        }
        return *this;
    }
    weak_ptr& operator=(weak_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            ref = other.ref;
            other.ptr = nullptr;
            other.ref = nullptr;
        }
        return *this;
    }
    shared_ptr<T> lock() const {
        if (ref && ref->strong_count > 0) {
            return shared_ptr<T>(ptr, ref);
        }
        return shared_ptr<T>();
    }
    void swap(weak_ptr& other) {
        auto tmp = ptr;
        ptr = other.ptr;
        other.ptr = tmp;
        tmp = ref;
        ref = other.ref;
        other.ref = tmp;
    }
};
