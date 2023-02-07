//
// Created by nudelerde on 28.01.23.
//

#pragma once

#include "features/smart_pointer.h"
#include "int.h"

template<typename T>
struct array {
private:
    unique_ptr<uint8_t> ptr;

public:
    size_t count{};
    size_t element_size{};

    array() = default;
    array(size_t count) : count(count), element_size(sizeof(T)), ptr(new uint8_t[element_size * count]) {}
    array(size_t count, size_t element_size) : count(count), element_size(element_size), ptr(new uint8_t[element_size * count]) {}

    T& operator[](size_t index) {
        return *reinterpret_cast<T*>(ptr.get() + index * element_size);
    }
    const T& operator[](size_t index) const {
        return *reinterpret_cast<T*>(ptr.get() + index * element_size);
    }

    uint8_t* data() {
        return ptr.get();
    }

    struct iterator {
        uint8_t* data;
        size_t element_size;

        iterator(uint8_t* data, size_t element_size) : data(data), element_size(element_size) {}

        iterator& operator++() {
            data += element_size;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            data += element_size;
            return tmp;
        }

        iterator& operator--() {
            data -= element_size;
            return *this;
        }

        iterator operator--(int) {
            iterator tmp = *this;
            data -= element_size;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return data == other.data;
        }

        bool operator!=(const iterator& other) const {
            return data != other.data;
        }

        T& operator*() {
            return *reinterpret_cast<T*>(data);
        }

        T* operator->() {
            return reinterpret_cast<T*>(data);
        }

        const T& operator*() const {
            return *reinterpret_cast<T*>(data);
        }

        const T* operator->() const {
            return reinterpret_cast<T*>(data);
        }

        iterator& operator+=(size_t n) {
            data += n * element_size;
            return *this;
        }

        iterator operator+(size_t n) const {
            iterator tmp = *this;
            tmp.data += n * element_size;
            return tmp;
        }

        iterator& operator-=(size_t n) {
            data -= n * element_size;
            return *this;
        }

        iterator operator-(size_t n) const {
            iterator tmp = *this;
            tmp.data -= n * element_size;
            return tmp;
        }

        size_t operator-(const iterator& other) const {
            return (data - other.data) / element_size;
        }

        T& operator[](size_t n) {
            return *reinterpret_cast<T*>(data + n * element_size);
        }

        const T& operator[](size_t n) const {
            return *reinterpret_cast<T*>(data + n * element_size);
        }

        bool operator<(const iterator& other) const {
            return data < other.data;
        }

        bool operator>(const iterator& other) const {
            return data > other.data;
        }

        bool operator<=(const iterator& other) const {
            return data <= other.data;
        }

        bool operator>=(const iterator& other) const {
            return data >= other.data;
        }
    };

    iterator begin() {
        return iterator(ptr.get(), element_size);
    }

    iterator end() {
        return iterator(ptr.get() + count * element_size, element_size);
    }
};