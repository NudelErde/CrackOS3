#include "memory/heap.h"
#include "asm/regs.h"
#include "data/btree.h"
#include "features/bytes.h"
#include "features/lock.h"
#include "features/smart_pointer.h"
#include "memory/mem.h"
#include "memory/paging.h"

namespace page_allocator {
static void init();
static void* malloc(size_t size);
static void free(void* ptr);
static size_t size(void* ptr);
}// namespace page_allocator

namespace small_allocator {
static void init();
static void* malloc(size_t size);
static void free(void* ptr);
static size_t size(void* ptr);
}// namespace small_allocator

namespace large_allocator {
static void init();
static void* malloc(size_t size);
static void free(void* ptr);
static size_t size(void* ptr);
}// namespace large_allocator

namespace kheap {

void* malloc(size_t size) {
    auto res = small_allocator::malloc(size);
    if (res) return res;
    res = page_allocator::malloc(size);
    if (res) return res;
    return large_allocator::malloc(size);
}
void free(void* ptr) {
    auto addr = VirtualAddress(ptr);
    if (addr.address < 96_Ti) return;
    auto offset = addr.address - 96_Ti;
    if (offset < 10_Ti) {
        page_allocator::free(ptr);
    } else if (offset < 20_Ti) {
        small_allocator::free(ptr);
    } else {
        large_allocator::free(ptr);
    }
}
optional<size_t> get_size(void* ptr) {
    auto addr = VirtualAddress(ptr);
    if (addr.address < 96_Ti) return {};
    auto offset = addr.address - 96_Ti;
    if (offset < 10_Ti) {
        return page_allocator::size(ptr);
    } else if (offset < 20_Ti) {
        return small_allocator::size(ptr);
    } else {
        return large_allocator::size(ptr);
    }
}

Test::Result test() {
    {
        auto ptr1 = static_cast<uint8_t*>(page_allocator::malloc(4096));
        auto ptr1num = reinterpret_cast<uint64_t>(ptr1);
        if (ptr1 == nullptr) return Test::Result::failure("page allocator failed to allocate memory");
        for (size_t i = 0; i < 4096; i++) {
            ptr1[i] = (uint8_t) i;
        }
        for (size_t i = 0; i < 4096; i++) {
            if (ptr1[i] != (uint8_t) i) return Test::Result::failure("invalid memory region mapped");
        }
        page_allocator::free(ptr1);
        auto ptr2 = static_cast<uint8_t*>(page_allocator::malloc(4096));
        auto ptr2num = reinterpret_cast<uint64_t>(ptr2);
        if (ptr2 == nullptr) return Test::Result::failure("page allocator failed to allocate memory");
        if (ptr1num != ptr2num) return Test::Result::failure("allocation after free didn't return the same pointer");
        page_allocator::free(ptr2);
    }
    {
        auto ptr1 = static_cast<uint8_t*>(small_allocator::malloc(12));
        auto ptr1num = reinterpret_cast<uint64_t>(ptr1);
        if (ptr1 == nullptr) return Test::Result::failure("small allocator failed to allocate memory");
        for (size_t i = 0; i < 12; i++) {
            ptr1[i] = (uint8_t) i;
        }
        for (size_t i = 0; i < 12; i++) {
            if (ptr1[i] != (uint8_t) i) return Test::Result::failure("invalid memory region mapped");
        }
        auto size = small_allocator::size(ptr1);
        if (size != 16) return Test::Result::failure("small allocator returned wrong size");
        small_allocator::free(ptr1);
        auto ptr2 = static_cast<uint8_t*>(small_allocator::malloc(12));
        auto ptr2num = reinterpret_cast<uint64_t>(ptr2);
        if (ptr2 == nullptr) return Test::Result::failure("small allocator failed to allocate memory");
        if (ptr1num != ptr2num) return Test::Result::failure("allocation after free didn't return the same pointer");
        small_allocator::free(ptr2);
    }
    {
        auto ptr1 = static_cast<uint8_t*>(small_allocator::malloc(16));
        auto ptr2 = static_cast<uint8_t*>(small_allocator::malloc(16));
        auto ptr3 = static_cast<uint8_t*>(small_allocator::malloc(100));
        auto dif = ptr3 - ptr1;
        if (((size_t) ptr2 & ~(page_size - 1)) != ((size_t) ptr1 & ~(page_size - 1))) return Test::Result::failure("small allocator allocated memory on different pages");
        if (dif % page_size != 0) return Test::Result::failure("small allocator allocated in wrong bucket");
    }

    return Test::Result::success();
}

void init() {
    page_allocator::init();
    small_allocator::init();
    large_allocator::init();
}

};// namespace kheap

namespace page_allocator {

static constexpr uint64_t page_full_bit = 0b1;

static void init() {}

static void* malloc_traverse(PageTable::PageTable* pageTable, uint8_t level, VirtualAddress resAddress, VirtualAddress lowAdd = 96_Ti, VirtualAddress highAdd = 96_Ti + 10_Ti) {
    auto low = 0;
    auto high = 512;
    if (level == 4) {
        low = lowAdd.l4Offset;
        high = highAdd.l4Offset;
    }
    int notFullNotEmptyNode = -1;
    int emptyNode = -1;
    for (auto i = low; i < high; ++i) {
        auto& entry = pageTable->entries[i];
        if (!entry.present && emptyNode == -1) {
            emptyNode = i;
        } else if (entry.present && (entry.metaData & page_full_bit) == 0) {
            notFullNotEmptyNode = i;
            break;
        }
    }
    int targetNode;
    if (notFullNotEmptyNode != -1) {
        targetNode = notFullNotEmptyNode;
    } else if (emptyNode != -1) {
        targetNode = emptyNode;
        auto& entry = pageTable->entries[targetNode];
        auto page = PhysicalAllocator::alloc(1);
        if (!page) return nullptr;
        entry.addressAndReserved = (page->address) >> 12;
        entry.present = true;
        entry.writeEnabled = true;
        entry.userAllowed = false;
        entry.cacheDisabled = false;
        entry.writeThrough = false;
        entry.accessed = false;
        entry.dirty = false;
        entry.pageSize = false;
        entry.global = false;
        entry.metaData = 0;
    } else {
        return nullptr;
    }

    auto& entry = pageTable->entries[targetNode];
    if (level == 1) {
        resAddress.l1Offset = targetNode;
        auto res = resAddress.as<void*>();
        entry.metaData |= page_full_bit;// page is allocated
        return res;
    } else {
        switch (level) {
            case 2:
                resAddress.l2Offset = targetNode;
                break;
            case 3:
                resAddress.l3Offset = targetNode;
                break;
            case 4:
                resAddress.l4Offset = targetNode;
                break;
            default:
                panic("invalid level in page_allocator::malloc");
        }
        auto nextPage = PhysicalAddress{entry.address()}.mapTmp().as<PageTable::PageTable*>();
        auto res = malloc_traverse(nextPage, level - 1, resAddress);

        bool full = true;
        for (auto entr : nextPage->entries) {
            if (entr.present) {
                if (!(entr.metaData & page_full_bit)) {
                    full = false;
                    break;
                }
            } else {
                full = false;
                break;
            }
        }
        if (full) {
            entry.metaData |= page_full_bit;
        }

        return res;
    }
}

static void free_traverse(PageTable::PageTable* pageTable, uint8_t level, VirtualAddress address) {
    if (level != 1) {
        auto& entry = pageTable->entries[address.getOffset(level).value_or_panic("invalid level in page_allocator::free")];
        auto nextPage = entry.address().mapTmp().as<PageTable::PageTable*>();
        free_traverse(nextPage, level - 1, address);
    }

    if (level == 1) {
        auto& entry = pageTable->entries[address.l1Offset];
        memset(&entry, 0, sizeof(PageTable::PageTableEntry));// clear entry
        PhysicalAllocator::free(entry.address(), 1);
        return;
    } else {
        auto& entry = pageTable->entries[address.getOffset(level).value_or_panic("invalid offset in page_allocator::free")];
        auto nextPage = entry.address().mapTmp().as<PageTable::PageTable*>();

        bool full = true;
        bool empty = true;
        for (auto& entr : nextPage->entries) {
            if (entr.present) {
                empty = false;
                if (!(entr.metaData & page_full_bit)) {
                    full = false;
                }
            } else {
                full = false;
            }
        }
        if (empty) {
            memset(&entry, 0, sizeof(PageTable::PageTableEntry));// clear entry
            PhysicalAllocator::free(entry.address(), 1);
        } else if (full) {
            entry.metaData |= page_full_bit;
        } else {
            entry.metaData &= ~page_full_bit;
        }
    }
}

static void* malloc(size_t size) {
    if (size > page_size) return nullptr;
    auto topLevelPage = PhysicalAddress{getCR3()}.mapTmp().as<PageTable::PageTable*>();
    auto res = malloc_traverse(topLevelPage, 4, VirtualAddress{});
    return res;
}

static void free(void* ptr) {
    if (ptr == nullptr) return;
    auto topLevelPage = PhysicalAddress{getCR3()}.mapTmp().as<PageTable::PageTable*>();
    free_traverse(topLevelPage, 4, VirtualAddress{ptr});
}

static size_t size(void* ptr) {
    if (ptr == nullptr) return 0;
    return page_size;
}

}// namespace page_allocator


namespace small_allocator {

void* allocate_page() {
    auto topLevelPage = PhysicalAddress{getCR3()}.mapTmp().as<PageTable::PageTable*>();
    auto res = page_allocator::malloc_traverse(topLevelPage, 4, VirtualAddress{}, 96_Ti + 10_Ti, 96_Ti + 20_Ti);
    memset(res, 0, page_size);
    return res;
}

void free_page(void* page) {
    if (page == nullptr) return;
    auto topLevelPage = PhysicalAddress{getCR3()}.mapTmp().as<PageTable::PageTable*>();
    page_allocator::free_traverse(topLevelPage, 4, VirtualAddress{page});
}

namespace List {
struct Node {
    using type = void*;
    static constexpr size_t element_count = (page_size - sizeof(Node*)) / sizeof(type);
    Node* next;
    type elements[element_count];
};

static_assert(sizeof(Node) == page_size, "invalid node size");

bool is_empty(Node* list) {
    return list == nullptr;
}
void push(Node** list, void* data) {
    if (list == nullptr) {
        panic("invalid usage of list in small_allocator::List::push");
    }
    while (*list != nullptr) {
        for (auto& element : (*list)->elements) {
            if (element == nullptr) {
                element = data;
                return;
            }
        }
        list = &(*list)->next;
    }
    (*list) = reinterpret_cast<Node*>(allocate_page());
    (*list)->next = nullptr;
    (*list)->elements[0] = data;
}
void* peek(Node* list) {
    while (list != nullptr) {
        for (const auto& element : list->elements) {
            if (element != nullptr) {
                return element;
            }
        }
        list = list->next;
    }
    return nullptr;
}
void remove(Node** list, void* data) {
    if (list == nullptr) {
        panic("invalid usage of list in small_allocator::List::remove");
    }
    while (*list != nullptr) {
        bool empty = true;
        for (auto& element : (*list)->elements) {
            if (element == data) {
                element = nullptr;
            }
            if (element != nullptr) {
                empty = false;
            }
        }
        if (empty) {
            auto next = (*list)->next;
            free_page(*list);
            *list = next;
        } else {
            list = &(*list)->next;
        }
    }
}
}// namespace List

struct Data {
    List::Node* freeList[10];// 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
} data;

static void init() {
    memset(&data, 0, sizeof(data));
}
static size_t get_pref_size(size_t size) {
    if (size <= 4) return 4;
    if (size <= 8) return 8;
    if (size <= 16) return 16;
    if (size <= 32) return 32;
    if (size <= 64) return 64;
    if (size <= 128) return 128;
    if (size <= 256) return 256;
    if (size <= 512) return 512;
    if (size <= 1024) return 1024;
    if (size <= 2047) return 2047;
    return 0;
}
static size_t get_pref_size_from_bucket(uint8_t bucket) {
    if (bucket == 0) return 4;
    if (bucket == 1) return 8;
    if (bucket == 2) return 16;
    if (bucket == 3) return 32;
    if (bucket == 4) return 64;
    if (bucket == 5) return 128;
    if (bucket == 6) return 256;
    if (bucket == 7) return 512;
    if (bucket == 8) return 1024;
    if (bucket == 9) return 2047;
    panic("invalid bucket in small_allocator::get_pref_size_from_bucket");
}
static uint8_t get_bucket(size_t prefSize) {
    if (prefSize == 4) return 0;
    if (prefSize == 8) return 1;
    if (prefSize == 16) return 2;
    if (prefSize == 32) return 3;
    if (prefSize == 64) return 4;
    if (prefSize == 128) return 5;
    if (prefSize == 256) return 6;
    if (prefSize == 512) return 7;
    if (prefSize == 1024) return 8;
    if (prefSize == 2047) return 9;
    panic("invalid prefSize in small_allocator::get_bucket");
}
static size_t get_bitmap_element_count(size_t bucket) {
    if (bucket == 0) return 992;
    if (bucket == 1) return 504;
    if (bucket == 2) return 253;
    if (bucket == 3) return 127;
    if (bucket == 4) return 63;
    if (bucket == 5) return 31;
    if (bucket == 6) return 15;
    if (bucket == 7) return 7;
    if (bucket == 8) return 3;
    if (bucket == 9) return 2;
    panic("invalid bucket in small_allocator::bitmap_element_count");
}
static void* malloc(size_t size) {
    auto prefSize = get_pref_size(size);
    if (prefSize == 0) return nullptr;
    auto bucket = get_bucket(prefSize);

    if (List::is_empty(data.freeList[bucket])) {
        auto* page = allocate_page();
        memset(page, 0, page_size);
        *(reinterpret_cast<uint8_t*>(page) + page_size - sizeof(uint8_t)) = bucket;// bucket number at end of page
        List::push(&data.freeList[bucket], page);
    }

    auto* page = reinterpret_cast<uint8_t*>(List::peek(data.freeList[bucket]));
    size_t bitmap_element_count = get_bitmap_element_count(bucket);
    size_t bitmap_size = (bitmap_element_count + 7) / 8;
    auto* bitmap = page + page_size - sizeof(uint8_t) - bitmap_size;
    size_t result = ~0;
    size_t count = 0;
    for (size_t i = 0; i < bitmap_element_count; ++i) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            if (result == ~0) {
                result = i;
                bitmap[i / 8] |= (1 << (i % 8));
            }
            ++count;
        }
    }
    if (result == ~0) {
        panic("invalid bitmap in small_allocator::malloc");
    }
    if (count == 1) {
        List::remove(&data.freeList[bucket], page);
    }
    return page + result * prefSize;
}
static void free(void* ptr) {
    if (ptr == nullptr) return;
    auto* page = reinterpret_cast<uint8_t*>(reinterpret_cast<size_t>(ptr) & ~(page_size - 1));
    auto offset = reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(page);
    uint8_t bucket = *(uint8_t*) (page + page_size - sizeof(uint8_t));
    auto prefSize = get_pref_size_from_bucket(bucket);
    size_t bitmap_element_count = get_bitmap_element_count(bucket);
    size_t bitmap_size = (bitmap_element_count + 7) / 8;
    auto* bitmap = page + page_size - sizeof(uint8_t) - bitmap_size;
    auto index = offset / prefSize;
    bool page_full = true;
    bool page_empty = true;
    for (size_t i = 0; i < bitmap_element_count; ++i) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            page_full = false;
        }
        if (bitmap[i / 8] & (1 << (i % 8))) {
            page_empty = false;
        }
    }
    bitmap[index / 8] &= ~(1 << (index % 8));
    if (page_empty) {
        free_page(page);
    } else if (page_full) {
        List::push(&data.freeList[bucket], page);
    }
}
static size_t size(void* ptr) {
    if (ptr == nullptr) return 0;
    auto* page = reinterpret_cast<uint8_t*>(reinterpret_cast<size_t>(ptr) & ~(page_size - 1));
    auto bucket = *(page + page_size - sizeof(uint8_t));
    auto prefSize = get_pref_size_from_bucket(bucket);
    return prefSize;
}

}// namespace small_allocator

namespace large_allocator {

static btree_map<VirtualAddress, size_t>* ptr_size;// size in pages
static spinlock* lock;

static void init() {
    ptr_size = new btree_map<VirtualAddress, size_t>();
    lock = new spinlock();
}
static void* malloc(size_t size) {
    lock_guard<spinlock> guard(*lock);
    size_t pages = (size + page_size - 1) / page_size;
    VirtualAddress last_region_end = 96_Ti + 20_Ti;
    ptr_size->iterate_kv([&pages, &last_region_end](const VirtualAddress& key, const size_t& value) -> bool {
        if (key.address - last_region_end.address > pages * page_size) {
            return false;
        }
        last_region_end = key.address + value * page_size;
        return true;
    });
    if (last_region_end.address + pages * page_size > 96_Ti + 30_Ti) {
        panic("out of memory in large_allocator::malloc");
    }
    auto start_address = last_region_end;
    for (size_t i = 0; i < pages; ++i) {
        auto page = PhysicalAllocator::alloc(1).value_or_panic("out of physical memory in large_allocator::malloc");
        PageTable::map(page, start_address + i * page_size, {.writeable = true, .user = false, .writeThrough = false, .cacheDisabled = false});
    }
    ptr_size->insert(start_address, pages * page_size);
    return start_address.as<void*>();
}
static void free(void* ptr) {
    if (ptr == nullptr) return;
    lock_guard<spinlock> guard(*lock);

    auto size = ptr_size->find(VirtualAddress(ptr))
                        .value_or_panic("removed non existing ptr in large_allocator::free");
    for (size_t i = 0; i < size; i += page_size) {
        auto page = PageTable::get(VirtualAddress(ptr) + i).value_or_panic("invalid page in large_allocator::free");
        PhysicalAllocator::free(page, 1);
        PageTable::unmap(VirtualAddress(ptr) + i);
    }

    if (!ptr_size->remove(VirtualAddress(ptr))) {
        panic("removed non existing ptr in large_allocator::free");
    }
}
static size_t size(void* ptr) {
    if (ptr == nullptr) return 0;
    lock_guard<spinlock> guard(*lock);
    auto res = ptr_size->find(VirtualAddress(ptr));
    return res.value_or(0);
}

}// namespace large_allocator


void* operator new(size_t size) {
    return memset(kmalloc(size), 0, size);
}
void* operator new[](size_t size) {
    return memset(kmalloc(size), 0, size);
}
void operator delete(void* ptr) noexcept {
    kfree(ptr);
}
void operator delete[](void* ptr) noexcept {
    kfree(ptr);
}
void operator delete(void* ptr, size_t) noexcept {
    kfree(ptr);
}
void operator delete[](void* ptr, size_t) noexcept {
    kfree(ptr);
}
