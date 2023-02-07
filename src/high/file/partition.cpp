//
// Created by nudelerde on 25.01.23.
//

#include "file/partition.h"

#include "file/device.h"

namespace file {

static uint8_t ext4_type[16]{0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4};

const char* partition::type_name() const {
    if (memcmp(type, ext4_type, 16) == 0) return "ext4";
    return "unknown";
}

filesystem partition::create_filesystem() const {
    filesystem fs;
    if (memcmp(type, ext4_type, 16) == 0) {
        fs.impl = create_ext4_filesystem(*this);
    }
    return fs;
}

}// namespace file