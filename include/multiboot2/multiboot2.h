//
// Created by nudelerde on 11.12.22.
//

#pragma once

#include "memory/mem.h"

namespace multiboot2 {

uint8_t* getTag(PhysicalAddress address, uint32_t tagType, bool useTmpMapping = true);
size_t getTotalSize(PhysicalAddress address, bool useTmpMapping = true);

}