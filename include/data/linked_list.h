//
// Created by Florian on 21.11.2022.
//

#pragma once

#include "features/optional.h"
#include "memory/heap.h"

template<typename T>
struct linked_list {
    struct node {
        T elem;
        node* next;
        node* prev;
    };
    struct iterator {
        node* current;
        explicit iterator(node* current) : current(current) {
        }
        iterator& operator++() {
            current = current->next;
            return *this;
        }
        iterator& operator--() {
            current = current->prev;
            return *this;
        }
        T& operator*() {
            return current->elem;
        }
        const T& operator*() const {
            return current->elem;
        }
        T* operator->() {
            return &current->elem;
        }
        const T* operator->() const {
            return &current->elem;
        }
        bool operator==(const iterator& other) const {
            return current == other.current;
        }
        bool operator!=(const iterator& other) const {
            return current != other.current;
        }
    };
    node* first;
    node* last;
    size_t size;

    iterator begin() {
        return iterator(first);
    }
    iterator end() {
        return iterator(nullptr);
    }

    linked_list() : first(nullptr), last(nullptr), size(0) {}
    linked_list(const linked_list& other) = delete;
    linked_list& operator=(const linked_list& other) = delete;
    linked_list(linked_list&& other) noexcept {
        first = other.first;
        last = other.last;
        size = other.size;
        other.first = nullptr;
        other.last = nullptr;
        other.size = 0;
    }
    linked_list& operator=(linked_list&& other) noexcept {
        first = other.first;
        last = other.last;
        size = other.size;
        other.first = nullptr;
        other.last = nullptr;
        other.size = 0;
        return *this;
    }
    ~linked_list() {
        while (first != nullptr) {
            auto next = first->next;
            delete first;
            first = next;
        }
    }

    optional<node*> find_node(size_t index) {
        if (index >= size || index < 0) {
            return {};
        }
        node* current = first;
        for (size_t i = 0; i < index; i++) {
            current = current->next;
        }
        return current;
    }

    T& operator[](size_t index) {
        return find_node(index).value_or_panic("linked_list::operator[]: index out of bounds")->elem;
    }

    const T& operator[](size_t index) const {
        return find_node(index).value_or_panic("linked_list::operator[]: index out of bounds")->elem;
    }

    void push_back(const T& elem) {
        auto node = new linked_list::node();
        node->elem = elem;
        node->next = nullptr;
        node->prev = last;
        if (last != nullptr) {
            last->next = node;
        }
        last = node;
        if (first == nullptr) {
            first = node;
        }
        size++;
    }

    void pop_back() {
        remove(size - 1);
    }

    void remove(size_t index) {
        if (size == 1 && index == 0) {
            delete first;
            first = nullptr;
            last = nullptr;
            size = 0;
            return;
        }
        auto* node = find_node(index).value_or_panic("linked_list::remove: index out of bounds");
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            first = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        } else {
            last = node->prev;
        }
        delete node;
        size--;
    }

    void clear() {
        while (first != nullptr) {
            auto next = first->next;
            delete first;
            first = next;
        }
        last = nullptr;
        size = 0;
    }
};