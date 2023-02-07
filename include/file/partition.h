//
// Created by nudelerde on 21.01.23.
//

#pragma once
#include "data/string.h"
#include "filesystem.h"
#include "device.h"

namespace file {

struct partition {

    string name;
    uint8_t type[16];
    uint8_t guid[16];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    device dev;

    size_t read(void* buffer, size_t offset, size_t size) {
        return dev.read(buffer, starting_lba * 512 + offset, size);
    }

    size_t write(void* buffer, size_t offset, size_t size) {
        return dev.write(buffer, starting_lba * 512 + offset, size);
    }

    [[nodiscard]] bool exists() const {
        for(auto i : type) {
            if (i != 0) return true;
        }
        return false;
    }

    [[nodiscard]] uint64_t size() const {
        return ending_lba - starting_lba;
    }

    [[nodiscard]] const char* type_name() const;

    [[nodiscard]] filesystem create_filesystem() const;
};

shared_ptr<filesystem::implementation> create_ext4_filesystem(const partition& part);

}// namespace file
