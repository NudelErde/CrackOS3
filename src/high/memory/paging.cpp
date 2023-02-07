#include "memory/paging.h"
#include "asm/regs.h"
#include "features/bytes.h"
#include "interrupt/interrupt.h"
#include "memory/mem.h"
#include "out/panic.h"


namespace PageTable {

static bool map_physical_high = false;

static optional<PageTableEntry*> getPageEntry(PageTable* page, uint64_t index) {
    if (index < 512) {
        return &page->entries[index];
    }
    return {};
}

static PageTable* getL4() {
    if (map_physical_high) {
        return PhysicalAddress(getCR3()).mapTmp().as<PageTable*>();
    } else {
        return reinterpret_cast<PageTable*>(getCR3());
    }
}

static optional<PageTable*> getPage(PageTableEntry* entry) {
    if (entry->present) {
        return entry->address().mapTmp().as<PageTable*>();
    }
    return {};
}

static PageTable* allocatePage() {
    optional<PhysicalAddress> ppage = PhysicalAllocator::alloc(1);
    if (!ppage) panic("Could not allocate page for pagetable");
    VirtualAddress vpage = ppage->mapTmp();
    auto* page = vpage.as<PageTable*>();
    memset(page, 0, page_size);
    return page;
}

static PageMetaData constructMetaData(PageTableEntry* entry) {
    PageMetaData meta{};
    meta.data = entry->metaData;
    meta.data |= entry->metaData2 << 3;
    return meta;
}

void init() {
    constexpr VirtualAddress identityMapBase = 64_Ti;
    map_physical_high = false;
    // check if 1GiB pages are supported
    uint32_t unused, edx;
    cpuid(0x80000001, &unused, &unused, &unused, &edx);
    if (!(edx & (1 << 26))) {
        panic("1GiB pages are not supported");
    }
    PageTable* identityMappingPages[64];
    auto pages_opt = PhysicalAllocator::alloc(64);
    if (!pages_opt) {
        panic("could not allocate identity mapping pages");
    }
    auto pages = pages_opt.value();
    for (uint64_t i = 0; i < 64; i++) {
        identityMappingPages[i] = (PageTable*) (pages.address + i * page_size);
    }
    auto* level4 = (PageTable*) getCR3();
    for (uint8_t i = 0; i < 64; i++) {
        level4->entries[identityMapBase.l4Offset + i].raw = (uint64_t) identityMappingPages[i];
        level4->entries[identityMapBase.l4Offset + i].present = 1;
        level4->entries[identityMapBase.l4Offset + i].writeEnabled = 1;
        for (uint64_t j = 0; j < 512; ++j) {
            auto& entry = identityMappingPages[i]->entries[j];
            entry.raw = 512_Gi * i + 1_Gi * j;
            entry.present = 1;
            entry.writeEnabled = 1;
            entry.pageSize = 1;
        }
    }
}
void map(PhysicalAddress p, VirtualAddress v, Flags f) {
    PageTable* currentPage = getL4();
    uint8_t level = 4;
    PageTableEntry* entry = nullptr;
    while (level > 1) {// 4 -> 3 -> 2 -> 1
        entry = getPageEntry(currentPage, v.getOffset(level).value()).value();
        auto page = getPage(entry).value_or_create(allocatePage);
        if (!entry->present) {
            entry->addressAndReserved = get(VirtualAddress(page)).value_or_panic("Can not unmap tmp mapped memory").address >> 12;
        }
        entry->present = 1;
        entry->writeEnabled |= f.writeable;
        entry->userAllowed |= f.user;
        entry->writeThrough |= f.writeThrough;
        entry->cacheDisabled |= f.cacheDisabled;
        currentPage = page;
        level--;
    }
    entry = getPageEntry(currentPage, v.getOffset(1).value()).value();
    entry->addressAndReserved = p.address >> 12;
    entry->present = 1;
    entry->writeEnabled = f.writeable;
    entry->userAllowed = f.user;
    entry->writeThrough = f.writeThrough;
    entry->cacheDisabled = f.cacheDisabled;
}

void unmap(VirtualAddress v) {
    auto entry = optional(getL4())
                         .map<PageTableEntry*>(getPageEntry, v.l4Offset)
                         .map<PageTable*>(getPage)
                         .map<PageTableEntry*>(getPageEntry, v.l3Offset)
                         .map<PageTable*>(getPage)
                         .map<PageTableEntry*>(getPageEntry, v.l2Offset)
                         .map<PageTable*>(getPage)
                         .map<PageTableEntry*>(getPageEntry, v.l1Offset);
    if (!entry) return;
    entry.value()->present = 0;
}

optional<PhysicalAddress> get(VirtualAddress v) {
    if (v.address >= 64_Ti && v.address < 96_Ti) {
        return PhysicalAddress{v.address - 64_Ti};
    }
    auto l4 = getL4();
    auto entry = getPageEntry(l4, v.l4Offset);
    if (!entry) return {};
    auto l3 = getPage(entry.value());
    if (!l3) return {};
    entry = getPageEntry(l3.value(), v.l3Offset);
    if (!entry) return {};
    if (entry.value()->pageSize == 1) {
        return PhysicalAddress{entry.value()->address().address + v.offset + v.l1Offset * 4_Ki + v.l2Offset * 2_Mi};
    }
    auto l2 = getPage(entry.value());
    if (!l2) return {};
    entry = getPageEntry(l2.value(), v.l2Offset);
    if (!entry) return {};
    if (entry.value()->pageSize == 1) {
        return PhysicalAddress{entry.value()->address().address + v.offset + v.l1Offset * 4_Ki};
    }
    auto l1 = getPage(entry.value());
    if (!l1) return {};
    entry = getPageEntry(l1.value(), v.l1Offset);
    if (!entry) return {};
    return PhysicalAddress{entry.value()->address().address + v.offset};
}

optional<PageMetaData> getMetaData(VirtualAddress v, uint8_t level) {
    if (level > 4) {
        return {};
    }
    auto* level4 = getL4();
    auto l4_opt = getPageEntry(level4, v.l4Offset);
    if (level == 4)
        return l4_opt.map<PageMetaData>(constructMetaData);
    auto l3_opt = getPage(l4_opt.value()).map<PageTableEntry*>(getPageEntry, v.l3Offset);
    if (level == 3)
        return l3_opt.map<PageMetaData>(constructMetaData);
    auto l2_opt = getPage(l3_opt.value()).map<PageTableEntry*>(getPageEntry, v.l2Offset);
    if (level == 2)
        return l2_opt.map<PageMetaData>(constructMetaData);
    auto l1_opt = getPage(l2_opt.value()).map<PageTableEntry*>(getPageEntry, v.l1Offset);
    if (level == 1)
        return l1_opt.map<PageMetaData>(constructMetaData);

    return {};
}

bool setMetaData(VirtualAddress v, uint8_t level, PageMetaData metaData, bool allocateIfNotPresent) {
    if (level > 4) {
        return false;
    }
    uint8_t currentLevel = 4;
    auto* currentPage = getL4();
    while (currentLevel > 0) {
        auto entry = getPageEntry(currentPage, v.getOffset(currentLevel)).value();
        if (currentLevel == level) {
            entry->metaData = metaData.data;
            entry->metaData2 = metaData.data >> 3;
            return true;
        }
        auto nextPage = getPage(entry);
        if (!nextPage && !allocateIfNotPresent) return false;
        if (!nextPage) {
            nextPage = allocatePage();
            entry->addressAndReserved = nextPage >> 12;
            entry->present = 1;
            entry->writeEnabled = 1;
        }
        currentPage = nextPage.value();
    }
    return false;
}
void clear_startup() {
    auto gdt = getGDTR();
    auto virtualGDTAddress = PhysicalAddress(gdt.base).mapTmp().address;
    setGDTR(virtualGDTAddress, gdt.size);
    PhysicalAllocator::clear_startup();
    map_physical_high = true;
    auto l4 = getL4();
    l4->entries[0].raw = 0;
    Interrupt::clear_startup();
}


}// namespace PageTable