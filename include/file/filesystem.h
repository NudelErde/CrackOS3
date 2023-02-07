//
// Created by nudelerde on 26.01.23.
//

#pragma once

#include "file.h"
#include "features/smart_pointer.h"

namespace file {

struct filesystem {

    [[nodiscard]] file open(const char* token) {
        return impl->open(token);
    }

    [[nodiscard]] bool valid() const {
        return impl.get() != nullptr;
    }

    struct implementation {
        virtual ~implementation() = default;
        virtual file open(const char* token) = 0;
    };

    shared_ptr<implementation> impl;
};

}