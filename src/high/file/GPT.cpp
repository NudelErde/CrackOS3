//
// Created by nudelerde on 21.01.23.
//

#include "file/GPT.h"
#include "file/partition.h"

namespace file {


optional<partition> GPT::getPartition(size_t index) {
    auto offset = starting_lba * 512 + index * partition_entry_size;
    uint8_t buffer[partition_entry_size];
    if (dev.read(buffer, offset, partition_entry_size).has_error()) return {};
    partition partition;
    char* name = new char[(partition_entry_size - 0x38) / 2 + 1];
    memset(name, 0, (partition_entry_size - 0x38) / 2 + 1);
    for (size_t i = 0; i < (partition_entry_size - 0x38) / 2; i++) {
        name[i] = buffer[0x38 + i * 2];
    }

    partition.name = name;
    memcpy(partition.type, buffer, 16);
    memcpy(partition.guid, buffer + 0x10, 16);
    partition.starting_lba = *(uint64_t*) (buffer + 0x20);
    partition.ending_lba = *(uint64_t*) (buffer + 0x28);
    partition.attributes = *(uint64_t*) (buffer + 0x30);
    partition.dev = dev;
    return partition;
}
}// namespace file