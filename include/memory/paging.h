#pragma once

#include "features/optional.h"
#include "memory/mem.h"

namespace PageTable {
struct Flags {
    bool writeable : 1;
    bool user : 1;
    bool writeThrough : 1;
    bool cacheDisabled : 1;
};

struct PageMetaData {
    uint16_t data : 14;
};

union PageTableEntry {
    uint64_t raw;
    struct {
        uint64_t present : 1;      // P
        uint64_t writeEnabled : 1; // RW
        uint64_t userAllowed : 1;  // US
        uint64_t writeThrough : 1; // PWT
        uint64_t cacheDisabled : 1;//PCD
        uint64_t accessed : 1;     // A
        uint64_t dirty : 1;        // D
        uint64_t pageSize : 1;     // PS
        uint64_t global : 1;       // G
        uint64_t metaData : 3;     // AVL
        uint64_t addressAndReserved : 40;
        uint64_t metaData2 : 11;    // AVL
        uint64_t executeDisable : 1;// XD
    } __attribute__((packed));
    [[nodiscard]] inline PhysicalAddress address() const { return addressAndReserved << 12; }
};

static_assert(sizeof(PageTableEntry) == sizeof(uint64_t), "PageTableEntry is not 8 bytes");

struct PageTable {
    PageTableEntry entries[512];
};
static_assert(sizeof(PageTable) == page_size, "PageTable is not page_size bytes");

void init();
void map(PhysicalAddress p, VirtualAddress v, Flags f);
void unmap(VirtualAddress v);
void clear_startup();
optional<PhysicalAddress> get(VirtualAddress v);
optional<PageMetaData> getMetaData(VirtualAddress v, uint8_t level);
/**
* @returns true if the metadata was set, false if not
*/
bool setMetaData(VirtualAddress v, uint8_t level, PageMetaData metaData, bool allocateIfNotPresent);

}// namespace PageTable