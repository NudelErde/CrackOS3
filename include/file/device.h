//
// Created by nudelerde on 21.01.23.
//

#pragma once

#include "features/result.h"
#include "features/smart_pointer.h"
#include "features/optional.h"
#include "int.h"

namespace file {

struct GPT;

struct device {
    enum class error_t {
        SUCCESS = 0,
        OUT_OF_BOUNDS = 1,
        DEVICE_UNAVAILABLE = 2
    };
    struct implementation {
        virtual ~implementation() = default;
        virtual result<size_t, error_t> read(void* buffer, size_t offset, size_t size) = 0;
        virtual result<size_t, error_t> write(void* buffer, size_t offset, size_t size) = 0;
    };

    size_t size;
    shared_ptr<implementation> impl;

    result<size_t, error_t> read(void* buffer, size_t offset, size_t count) {
        return impl->read(buffer, offset, count);
    }

    result<size_t, error_t> write(void* buffer, size_t offset, size_t count) {
        return impl->write(buffer, offset, count);
    }

    optional<GPT> find_partitions();
};

void push_device(const device& dev);
size_t device_count();
device& get_device(size_t index);

}// namespace file