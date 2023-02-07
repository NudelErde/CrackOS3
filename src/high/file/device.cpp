//
// Created by nudelerde on 21.01.23.
//

#include "file/device.h"
#include "data/linked_list.h"
#include "features/string.h"
#include "file/GPT.h"

namespace file {

static linked_list<device>* devices;
static size_t size;
static bool has_init;
static void init() {
    if (has_init) return;
    has_init = true;
    size = 0;
    devices = new linked_list<device>();
}

void push_device(const device& dev) {
    init();
    devices->push_back(dev);
    size++;
}
size_t device_count() {
    init();
    return size;
}
device& get_device(size_t index) {
    init();
    return (*devices)[index];
}

optional<GPT> device::find_partitions() {
    uint16_t sector[256]{};
    if (read(sector, 0, 512).has_error()) return {};
    // check if sector is mbr
    if (sector[0xFF] != 0xAA55) return {};
    // load gpt header
    if (read(sector, 512, 512).has_error()) return {};
    // check if sector is gpt
    if (memcmp(sector, "EFI PART", 8) != 0) return {};
    size_t partition_count = *(uint32_t*) (((uint8_t*) sector) + 0x50);
    uint64_t starting_lba = *(uint64_t*) (((uint8_t*) sector) + 0x48);
    size_t partition_entry_size = *(uint32_t*) (((uint8_t*) sector) + 0x54);
    return GPT{*this, partition_count, starting_lba, partition_entry_size};
}
}// namespace file