//
// Created by nudelerde on 21.01.23.
//

#pragma once

#include "device.h"
#include "features/optional.h"

namespace file {

struct partition;

struct GPT {

    file::device dev;
    size_t count{};
    uint64_t starting_lba{};
    size_t partition_entry_size{};
    optional<partition> getPartition(size_t index);
};

}