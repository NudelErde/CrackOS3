//
// Created by nudelerde on 26.01.23.
//

#pragma once
#include "features/smart_pointer.h"

namespace file {

struct file {

    enum class type_t {
        normal,
        directory,
        extra
    };

    struct directory_entry{
        string name;
        type_t type;
    };

    struct implementation {
        virtual ~implementation() = default;
        virtual size_t read(void* buffer, size_t offset, size_t size) = 0;
        virtual size_t get_size() = 0;
        virtual type_t get_type() = 0;
        virtual size_t get_directory_entry_count() = 0;
        virtual size_t get_directory_entries(directory_entry* buffer, size_t offset, size_t size) = 0;
    };

    size_t read(void* buffer, size_t offset, size_t size) {
        return impl->read(buffer, offset, size);
    }

    size_t get_size() {
        return impl->get_size();
    }

    type_t get_type() {
        return impl->get_type();
    }

    size_t get_directory_entry_count() {
        return impl->get_directory_entry_count();
    }

    size_t get_directory_entries(directory_entry* buffer, size_t count, size_t size) {
        return impl->get_directory_entries(buffer, count, size);
    }

    bool is_valid() {
        return impl.get() != nullptr;
    }

    unique_ptr<implementation> impl;
};

}