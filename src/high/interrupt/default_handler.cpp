//
// Created by Florian on 27.11.2022.
//

#include "interrupt/default_handler.h"
#include "asm/regs.h"
#include "memory/paging.h"
#include "out/log.h"
#include "out/panic.h"

static const char* get_name(uint8_t number) {
    const char* names[]{
            "Division by zero",
            "Debug",
            "Non-maskable interrupt",
            "Breakpoint",
            "Overflow",
            "Bound range exceeded",
            "Invalid opcode",
            "Device not available",
            "Double fault",
            "Coprocessor segment overrun",
            "Invalid TSS",
            "Segment not present",
            "Stack-segment fault",
            "General protection fault",
            "Page fault",
            "Reserved",
            "x87 floating-point exception",
            "Alignment check",
            "Machine check",
            "SIMD floating-point exception",
            "Virtualization exception",
            "Control protection exception"};
    if (number < sizeof(names) / sizeof(names[0])) {
        return names[number];
    } else if (number < 32) {
        return "Reserved";
    } else {
        return "Unknown";
    }
}
static bool page_fault_recursion_stop;

static void trace_page_fault(VirtualAddress address) {
    Log::printf(Log::Warning, "PageFault", "Try to trace address %x %x %x %x %x\n", address.l4Offset,
                address.l3Offset, address.l2Offset, address.l1Offset, address.offset);
    auto* page = PhysicalAddress(getCR3()).mapTmp().as<PageTable::PageTable*>();
    uint8_t level4 = address.l4Offset;
    auto entry = page->entries[level4];
    Log::printf(Log::Error, "PageFault", "Level 4 Page at %x: entry[%i] = %x\n", page, level4, entry.raw);
    if (!entry.present) {
        Log::printf(Log::Error, "PageFault", "Link to level 3 page not present\n");
        return;
    }
    page = entry.address().mapTmp().as<PageTable::PageTable*>();
    uint8_t level3 = address.l3Offset;
    entry = page->entries[level3];
    Log::printf(Log::Error, "PageFault", " Level 3 Page at %x: entry[%i] = %x\n", page, level3, entry.raw);
    if (!entry.present) {
        Log::printf(Log::Error, "PageFault", "Link to level 2 page not present\n");
        return;
    }
    page = entry.address().mapTmp().as<PageTable::PageTable*>();
    uint8_t level2 = address.l2Offset;
    entry = page->entries[level2];
    Log::printf(Log::Error, "PageFault", "  Level 2 Page at %x: entry[%i] = %x\n", page, level2, entry.raw);
    if (!entry.present) {
        Log::printf(Log::Error, "PageFault", "Link to level 1 page not present\n");
        return;
    }
    page = entry.address().mapTmp().as<PageTable::PageTable*>();
    uint8_t level1 = address.l1Offset;
    entry = page->entries[level1];
    Log::printf(Log::Error, "PageFault", "   Level 1 Page at %x: entry[%i] = %x\n", page, level1, entry.raw);
    if (!entry.present) {
        Log::printf(Log::Error, "PageFault", "Link to page page not present\n");
        return;
    }
    Log::printf(Log::Error, "PageFault", "    Physical Page %x", entry.address().address);
}

void page_fault_handler(uint8_t, uint64_t error, void* stack, void*) {
    if (page_fault_recursion_stop) {
        panic("Page fault in page fault handler");
    }
    page_fault_recursion_stop = true;
    uint64_t address = getCR2();
    bool present = error & (1 << 0);
    bool write = error & (1 << 1);
    bool user = error & (1 << 2);
    bool reserved = error & (1 << 3);
    bool instruction = error & (1 << 4);
    bool protection_key = error & (1 << 5);
    bool shadow_stack = error & (1 << 6);
    bool hlat = error & (1 << 7);
    bool sgx = error & (1 << 15);
    Log::printf(Log::Error, "PageFault", "Address: %x (%s%s%s%s%s%s%s%s%s)\n", address,
                present ? "present " : "not-present ",
                write ? "write " : "read ",
                user ? "user " : "",
                reserved ? "reserved " : "",
                instruction ? "instruction " : "",
                protection_key ? "protection key " : "",
                shadow_stack ? "shadow stack " : "",
                hlat ? "hlat " : "",
                sgx ? "sgx " : "");

    trace_page_fault(VirtualAddress(address));


    Log::printf(Log::Fatal, "PageFault", "Can not recover from page fault\n");
}

void syscall_handler(uint8_t, uint64_t, void*, void*) {
    Log::printf(Log::Fatal, "Syscall", "Syscall not implemented\n");
}

void Interrupt::init_default_handlers() {
    Guard guard;
    for (int i = 0; i < 32; ++i) {
        registerHandler(i, [](uint8_t number, uint64_t, void*, void*) {
            Log::printf(Log::Fatal, "Interrupt", "Unhandled interrupt %i (%s)\n", number, get_name(number));
        });
    }
    page_fault_recursion_stop = false;
    registerHandler(14, page_fault_handler);
}