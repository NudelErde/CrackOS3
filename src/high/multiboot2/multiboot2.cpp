//
// Created by nudelerde on 11.12.22.
//

#include "multiboot2/multiboot2.h"
#include "out/panic.h"

uint8_t* multiboot2::getTag(PhysicalAddress address, uint32_t tagType, bool useTmpMapping) {
    uint8_t* header = useTmpMapping ? address.mapTmp().as<uint8_t*>() : reinterpret_cast<uint8_t*>(address.address);
    size_t size = *reinterpret_cast<uint32_t*>(header);
    size_t offset = sizeof(uint32_t) * 2;
    while(true) {
        auto type = *reinterpret_cast<uint32_t*>(header + offset);
        auto length = *reinterpret_cast<uint32_t*>(header + offset + 4);
        if (type == 0) {
            return nullptr;
        } else if(type == tagType) {
            return header + offset;
        }
        offset += length;
        for(; offset % 8 != 0; offset++);
        if(offset >= size) {
            panic("invalid multiboot2 header");
        }
    }
}

size_t multiboot2::getTotalSize(PhysicalAddress address, bool useTmpMapping) {
    uint8_t* header = useTmpMapping ? address.mapTmp().as<uint8_t*>() : reinterpret_cast<uint8_t*>(address.address);
    size_t size = *reinterpret_cast<uint32_t*>(header);
    return size;
}