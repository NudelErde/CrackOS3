#include "stdint.h"

typedef uint64_t size_t;
#define saveReadSymbol(name) [&]() -> uint64_t { \
    uint64_t tmp;                                \
    asm volatile("movabs $" name ", %0"          \
                 : "=r"(tmp)::);                 \
    return tmp;                                  \
}()
#include "features/bytes.h"
constexpr uint64_t page_size = 4096;

namespace low {

// ye dis not fast
static void* memcpy(void* dest, const void* src, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        ((uint8_t*) dest)[i] = ((uint8_t*) src)[i];
    }
    return dest;
}

static void* memset(void* dest, uint8_t val, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        ((uint8_t*) dest)[i] = val;
    }
    return dest;
}

union Color {
    struct {
        uint8_t foreground : 4;
        uint8_t background : 4;
    };
    uint8_t value;
    Color(uint8_t foreground, uint8_t background) : foreground(foreground), background(background) {}
};

static int print_char = 0;

static void put_char(char c, Color color) {
    *(char*) (0xB8000ull + 2 * print_char) = c;
    *(char*) (0xB8000ull + 2 * print_char + 1) = color.value;
    print_char++;
}

static void print_string(const char* str) {
    for (; *str; ++str) {
        if (*str == '\n') {
            while (print_char % 80) {
                put_char(' ', {0, 0});
            }
        } else {
            put_char(*str, {0, 10});
        }
    }
}

static void print_hex(uint64_t num, int minDigits = 1) {
    constexpr char hex[] = "0123456789ABCDEF";
    constexpr int maxDigits = sizeof(uint64_t) * 2;

    minDigits -= 1;
    bool print = false;

    for (int i = maxDigits - 1; i >= 0; --i) {
        int digit = (num >> (i * 4)) & 0xF;

        if (i <= minDigits) print = true;
        if (digit) print = true;

        if (print) put_char(hex[digit], {0, 10});
    }
}

[[noreturn]] static void stop() {
    while (true) {
        asm("cli");
        asm("hlt");
    }
}

static void check_kernel_smaller_than_1gb() {
    auto start = saveReadSymbol("high_start");
    auto end = saveReadSymbol("high_end");
    if (end - start > 1_Gi) {
        print_string("[LOW]: Kernel can not be bigger than 1GiB.\n Stop\n");
        stop();
    }
}

alignas(page_size) static char l3PageBuffer[page_size];// new level 3
alignas(page_size) static char l2PageBuffer[page_size];// new level 2

static void map_high_kernel_to_high_memory() {
    auto start = saveReadSymbol("high_start");
    auto physical_start = saveReadSymbol("high_physical_start");
    union PointerToPagingInfo {
        uint64_t value;
        struct {
            uint64_t offset : 12;
            uint64_t l1Offset : 9;
            uint64_t l2Offset : 9;
            uint64_t l3Offset : 9;
            uint64_t l4Offset : 9;
            uint64_t padding : 16;
        };
    };
    union PageTableEntry {
        uint64_t raw;
        struct {
            uint64_t present : 1;
            uint64_t write : 1;
            uint64_t user : 1;
            uint64_t writeThrough : 1;
            uint64_t cacheDisable : 1;
            uint64_t accessed : 1;
            uint64_t dirty : 1;
            uint64_t largePage : 1;
            uint64_t global : 1;
            uint64_t available : 3;
            uint64_t padding : 64 - 12;
        };
    };
    struct Page {
        PageTableEntry entries[page_size / sizeof(PageTableEntry)];
    };
    static_assert(sizeof(Page) == page_size);
    static_assert(sizeof(PointerToPagingInfo) == sizeof(uint64_t));

    uint64_t cr3 = 0;
    asm("mov %%cr3, %0"
        : "=r"(cr3));
    Page* l4Page = (Page*) cr3;
    PointerToPagingInfo kernelAddressInfo;
    kernelAddressInfo.value = start;

    auto& l3Entry = l4Page->entries[kernelAddressInfo.l4Offset];
    if (!l3Entry.present) {
        l3Entry.raw = ((uint64_t) l3PageBuffer) & ~(page_size - 1);
        l3Entry.present = 1;
        l3Entry.write = 1;
        l3Entry.user = 0;
        l3Entry.writeThrough = 0;
        l3Entry.cacheDisable = 0;
        l3Entry.accessed = 0;
        l3Entry.dirty = 0;
        l3Entry.largePage = 0;
        l3Entry.global = 0;
        l3Entry.available = 0;
        memset(l3PageBuffer, 0, page_size);
        asm("invlpg (%0)"
            :
            : "r"((uint64_t) l3PageBuffer)
            : "memory");
    }
    Page* l3Page = (Page*) (l3Entry.raw & ~(page_size - 1));
    auto& l2Entry = l3Page->entries[kernelAddressInfo.l3Offset];
    if (!l2Entry.present) {
        l2Entry.raw = ((uint64_t) l2PageBuffer) & ~(page_size - 1);
        l2Entry.present = 1;
        l2Entry.write = 1;
        l2Entry.user = 0;
        l2Entry.writeThrough = 0;
        l2Entry.cacheDisable = 0;
        l2Entry.accessed = 0;
        l2Entry.dirty = 0;
        l2Entry.largePage = 0;
        l2Entry.global = 0;
        l2Entry.available = 0;
        memset(l2PageBuffer, 0, page_size);
        asm("invlpg (%0)"
            :
            : "r"((uint64_t) l2PageBuffer)
            : "memory");
    }
    // assume virtual address is aligned to 1 gib
    Page* l2Page = (Page*) (l2Entry.raw & ~(page_size - 1));
    for (uint64_t i = 0; i < 512; ++i) {
        auto& page_2mib = l2Page->entries[i];
        uint64_t address = physical_start + (i * 2_Mi);
        page_2mib.raw = address;
        page_2mib.present = 1;
        page_2mib.write = 1;
        page_2mib.user = 0;
        page_2mib.writeThrough = 0;
        page_2mib.cacheDisable = 0;
        page_2mib.accessed = 0;
        page_2mib.dirty = 0;
        page_2mib.largePage = 1;
        page_2mib.global = 0;
        page_2mib.available = 0;
        asm("invlpg (%0)"
            :
            : "r"(address)
            : "memory");
    }
}

static void jump_to_high_kernel(uint64_t multiboot_header) {
    uint64_t entry = saveReadSymbol("high_main");
    uint64_t stack = saveReadSymbol("start_stack") + 0x100000 - 0x10;
    asm volatile("movq %0, %%rsp\n\t"
                 "movq %1, %%rax\n\t"
                 "movq %2, %%rdi\n\t"
                 "callq *%%rax\n\t"
                 :
                 : "r"(stack), "r"(entry), "r"(multiboot_header)
                 : "rax", "rdi");
    print_string("[LOW]: returned from high kernel\n");
    stop();
}

}// namespace low

extern "C" void main(uint64_t multiboot_header) {
    using namespace low;
    print_string("[LOW]: main in x64\n");
    check_kernel_smaller_than_1gb();
    map_high_kernel_to_high_memory();
    print_string("[LOW]: mapped high kernel\n");
    jump_to_high_kernel(multiboot_header);

    stop();
}