#include "memory/mem.h"
#include "features/bytes.h"
#include "features/optional.h"
#include "multiboot2/multiboot2.h"
#include "out/panic.h"
#include "util/interval.h"


namespace PhysicalAllocator {

static uint8_t multiboot_memory_info[page_size];
static uint64_t multiboot_memory_info_size = 0;
static uint64_t total_memory;

uint8_t* physical_memory_bitmap;
uint64_t physical_memory_bitmap_size;
uint64_t last_checked_memory_bitmap_index = 0;

static optional<interval<uint64_t>> findSubregionWithSize(interval<uint64_t> region, uint64_t size) {
    interval<uint64_t> reserved[]{
            {saveReadSymbol("trampolineStart"), saveReadSymbol("trampolineEnd")},
            {saveReadSymbol("physical_start"), saveReadSymbol("physical_end")},
            {0xA0000, 0x100000}};

    if (region.size() < size) {
        return {};
    }
    // trivial case: region does not intersect with one of the reserved regions
    optional<interval<uint64_t>> intersection;
    for (auto& r : reserved) {
        if (region.intersects(r)) {
            intersection = r;
            break;
        }
    }
    if (!intersection) {
        return interval<uint64_t>{region.left, region.left + size};
    }
    // split region in part before and after the intersection
    interval<uint64_t> before{region.left, intersection->left - 2};
    interval<uint64_t> after{intersection->right + 2, region.right};
    if (before.valid()) {
        auto res = findSubregionWithSize(before, size);
        if (res) {
            return res;
        }
    }
    if (after.valid()) {
        auto res = findSubregionWithSize(after, size);
        if (res) {
            return res;
        }
    }
    return {};
}

static optional<PhysicalAddress> memoryBitmapIndexToPhysical(uint64_t index) {
    uint32_t entry_size = *(uint32_t*) (multiboot_memory_info + 0);
    uint32_t entry_version = *(uint32_t*) (multiboot_memory_info + 4);
    uint8_t* entries = multiboot_memory_info + 8;
    uint32_t entry_count = (multiboot_memory_info_size - 4) / entry_size;
    struct MemoryMapEntry {
        uint64_t base_addr;
        uint64_t length;
        uint32_t type;
        uint32_t reserved;
    };
    for (uint32_t i = 0; i < entry_count; ++i) {
        auto* entry = (MemoryMapEntry*) (entries + i * entry_size);
        if (entry->type == 1) {
            if (index * page_size < entry->length) {
                return {entry->base_addr + index * page_size};
            } else {
                index -= entry->length / page_size;
            }
        }
    }
    return {};
}

static optional<uint64_t> physicalAddressPageToMemoryIndex(PhysicalAddress address) {
    uint64_t aligned = address.address & ~(page_size - 1);

    uint32_t entry_size = *(uint32_t*) (multiboot_memory_info + 0);
    uint32_t entry_version = *(uint32_t*) (multiboot_memory_info + 4);
    uint8_t* entries = multiboot_memory_info + 8;
    uint32_t entry_count = (multiboot_memory_info_size - 4) / entry_size;
    struct MemoryMapEntry {
        uint64_t base_addr;
        uint64_t length;
        uint32_t type;
        uint32_t reserved;
    };
    uint64_t current = 0;
    for (uint32_t index = 0; index < entry_count; ++index) {
        auto* entry = (MemoryMapEntry*) (entries + index * entry_size);
        if (entry->type == 1) {
            if (aligned >= entry->base_addr && aligned - entry->base_addr < entry->length) {
                return current + (aligned - entry->base_addr) / page_size;
            } else {
                current += entry->length / page_size;
            }
        }
    }
    return {};
}

static void reserveMemory(PhysicalAddress start, uint64_t size, bool reserve = true) {
    if (size == 0) return;
    PhysicalAddress end = {(start.address + size + (page_size - 1)) & ~(page_size - 1)};
    auto startIndexOpt = physicalAddressPageToMemoryIndex(start);
    auto endIndexOpt = physicalAddressPageToMemoryIndex(end);
    if (!startIndexOpt || !endIndexOpt) {
        while(start < end) {
            auto indexOpt = physicalAddressPageToMemoryIndex(start);
            if(indexOpt) {
                if (reserve) {
                    physical_memory_bitmap[*indexOpt / 8] |= 1 << (*indexOpt % 8);
                } else {
                    physical_memory_bitmap[*indexOpt / 8] &= ~(1 << (*indexOpt % 8));
                }
            }
            start.address += page_size;
        }
    } else {
        uint64_t startIndex = *startIndexOpt;
        uint64_t endIndex = *endIndexOpt;
        for (uint64_t index = startIndex; index < endIndex; ++index) {
            if (reserve) {
                physical_memory_bitmap[index / 8] |= 1 << (index % 8);
            } else {
                physical_memory_bitmap[index / 8] &= ~(1 << (index % 8));
            }
        }
    }
}

void init(PhysicalAddress multiboot_info) {
    struct Entry {
        uint32_t type;
        uint32_t size;
    };
    auto* entry = reinterpret_cast<Entry*>(multiboot2::getTag(multiboot_info, 6, false));
    if (!entry) {
        panic("no memory info found");
    }
    multiboot_memory_info_size = entry->size - sizeof(Entry);
    if (multiboot_memory_info_size > page_size) {
        panic("multiboot_memory_info_size > page_size");
    }
    memcpy(multiboot_memory_info, entry + 1, multiboot_memory_info_size);

    // read memory
    total_memory = 0;
    uint32_t entry_size = *(uint32_t*) (multiboot_memory_info + 0);
    uint32_t entry_version = *(uint32_t*) (multiboot_memory_info + 4);
    uint8_t* entries = multiboot_memory_info + 8;
    uint32_t entry_count = (multiboot_memory_info_size - 4) / entry_size;
    struct MemoryMapEntry {
        uint64_t base_addr;
        uint64_t length;
        uint32_t type;
        uint32_t reserved;
    };
    for (uint32_t index = 0; index < entry_count; ++index) {
        auto* entry = (MemoryMapEntry*) (entries + index * entry_size);
        if (entry->type == 1) {
            total_memory += entry->length;
        }
    }

    physical_memory_bitmap_size = total_memory / page_size;

    [&]() {
        for (uint32_t index = 0; index < entry_count; ++index) {
            auto* entry = (MemoryMapEntry*) (entries + index * entry_size);
            if (entry->type == 1) {
                if (entry->length > physical_memory_bitmap_size && entry->base_addr + physical_memory_bitmap_size <= 1_Gi) {
                    auto subregion = findSubregionWithSize({entry->base_addr, entry->base_addr + entry->length}, physical_memory_bitmap_size);
                    if (subregion) {
                        physical_memory_bitmap = (uint8_t*) subregion->left;
                        return;
                    }
                }
            }
        }
        panic("could not store memory bitmap");
    }();
    memset(physical_memory_bitmap, 0, physical_memory_bitmap_size);
    // mark used memory as 1
    reserveMemory(PhysicalAddress{(uint64_t) physical_memory_bitmap}, physical_memory_bitmap_size);// bitmap
    reserveMemory(PhysicalAddress{0}, page_size);                                                  // 0
    reserveMemory(multiboot_info, multiboot2::getTotalSize(multiboot_info, false));                // multiboot info
    auto trampolineStart = saveReadSymbol("trampolineStart");
    auto trampolineEnd = saveReadSymbol("trampolineEnd");
    auto highPhysicalStart = saveReadSymbol("physical_start");
    auto highPhysicalEnd = saveReadSymbol("physical_end");
    reserveMemory(PhysicalAddress{trampolineStart}, trampolineEnd - trampolineStart);      // trampoline
    reserveMemory(PhysicalAddress{highPhysicalStart}, highPhysicalEnd - highPhysicalStart);// high physical
    reserveMemory(PhysicalAddress{0xA0000}, 0x100000 - 0xA0000);                           // Reserved ("Upper Memory")
    last_checked_memory_bitmap_index = 0;
}

optional<PhysicalAddress> alloc(uint64_t count) {
    auto stop_infinite_loop = last_checked_memory_bitmap_index;
    do {
        last_checked_memory_bitmap_index = (last_checked_memory_bitmap_index + 1) % physical_memory_bitmap_size;
        int i = 0;
        for (; i < count; ++i) {
            uint64_t index = (last_checked_memory_bitmap_index + i) % physical_memory_bitmap_size;
            if ((physical_memory_bitmap[index / 8] & (1 << (index % 8))) != 0) {// if bit is set -> page is allocated, try next
                last_checked_memory_bitmap_index = index;
                break;
            }
        }
        if (i == count) {
            auto address = memoryBitmapIndexToPhysical(last_checked_memory_bitmap_index);
            if (!address) return {};
            reserveMemory(*address, count * page_size);
            return address;
        }
    } while (stop_infinite_loop != last_checked_memory_bitmap_index);
    return {};
}

void free(PhysicalAddress address, uint64_t count) {
    reserveMemory(address, count * page_size, false);
}
void clear_startup() {
    physical_memory_bitmap = PhysicalAddress((uint64_t)physical_memory_bitmap).mapTmp().as<uint8_t*>();
}

}// namespace PhysicalAllocator

VirtualAddress PhysicalAddress::mapTmp() const {
    return {address + 64_Ti};
}

optional<uint64_t> VirtualAddress::getOffset(uint8_t level) const {
    switch (level) {
        case 1:
            return l1Offset;
        case 2:
            return l2Offset;
        case 3:
            return l3Offset;
        case 4:
            return l4Offset;
        default:
            return {};
    }
}
