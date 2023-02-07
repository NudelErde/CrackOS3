#include "interrupt/interrupt.h"
#include "ACPI/APIC.h"
#include "asm/regs.h"
#include "asm/util.h"
#include "int.h"
#include "memory/mem.h"
#include "out/panic.h"
#include "out/vga.h"

struct InterruptGate {
    uint16_t lowAddress;
    SegmentSelector segmentSelector;
    uint8_t interruptStack : 3;
    uint8_t zero0 : 5;
    uint8_t type : 4;
    uint8_t zero1 : 1;
    uint8_t descriptorPrivilegeLevel : 2;
    bool present : 1;
    uint16_t middleAddress;
    uint32_t highAddress;
    uint32_t reserved;
};
static_assert(sizeof(InterruptGate) == sizeof(uint8_t) * 16, "InterruptGate is not 16 bytes");

alignas(page_size) InterruptGate interruptVectorTable[256]{};
static_assert(sizeof(interruptVectorTable) == page_size, "Interrupt vector table size is not page size");
static_assert(alignof(interruptVectorTable) == page_size, "Interrupt vector table alignment is not page size");

void Interrupt::enable() {
    sti();
}

void Interrupt::disable() {
    cli();
}

bool Interrupt::isEnabled() {
    return getFlags() & (1 << 9);
}


static void setHandler(VirtualAddress handler, uint8_t interrupt, uint8_t stack = 0) {
    Interrupt::Guard guard;
    InterruptGate& gate = interruptVectorTable[interrupt];
    gate.lowAddress = handler.address & 0xFFFF;
    gate.middleAddress = (handler.address >> 16) & 0xFFFF;
    gate.highAddress = handler.address >> 32;
    gate.segmentSelector = SegmentSelector(Segment::KERNEL_CODE);
    gate.interruptStack = stack;
    gate.zero0 = 0;
    gate.type = 0xE;
    gate.zero1 = 0;
    gate.descriptorPrivilegeLevel = 0;
    gate.present = true;
    gate.reserved = 0;
}

void Interrupt::setNativeInterruptHandler(VirtualAddress address, uint8_t number, uint8_t stack) {
    setHandler(address, number, stack);
}

uint64_t* interrupt_number_pointer;

uint64_t interrupt_number_single_cpu_buffer;

asm(R"asm(
.text
.global __kernel_interrupt_first_stage
__kernel_interrupt_first_stage:
.rept 256
cli
call __kernel_interrupt_stage_two
.endr
__kernel_interrupt_first_stage_end:
nop
__kernel_interrupt_stage_two:
push %rax
push %rbx
mov 16(%rsp), %rax
movabs $__kernel_interrupt_first_stage, %rbx
sub %rbx, %rax
mov interrupt_number_pointer(%rip), %rbx
mov %rax, (%rbx)
cmp $54, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $66, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $72, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $78, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $84, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $90, %rax
je __kernel_interrupt_stage_two_has_error_code
cmp $108, %rax
je __kernel_interrupt_stage_two_has_error_code
pop %rbx
pop %rax
add $8, %rsp
push $0
jmp interrupt_handler
__kernel_interrupt_stage_two_has_error_code:
pop %rbx
pop %rax
add $8, %rsp
jmp interrupt_handler
)asm");

Interrupt::handler_t interrupt_handlers[256];
void* data[256];

struct interrupt_frame;
extern "C" [[maybe_unused]] __attribute__((interrupt)) void interrupt_handler(interrupt_frame* frame, uint64_t error) {
    auto start = saveReadSymbol("__kernel_interrupt_first_stage");
    auto end = saveReadSymbol("__kernel_interrupt_first_stage_end");
    auto element_size = (end - start) / 256;
    uint64_t interruptNumber = (*interrupt_number_pointer) / element_size - 1;
    if (interruptNumber >= 256) {
        VGA::Text::print("Interrupt: ");
        VGA::Text::print((uint64_t) interruptNumber);
        VGA::Text::print("\n");
        panic("Invalid Interrupt");
    }
    if (interrupt_handlers[interruptNumber] == nullptr) {
        VGA::Text::print("Interrupt: ");
        VGA::Text::print((uint64_t) interruptNumber);
        VGA::Text::print("\n");
        panic("Unhandled Interrupt");
    }
    interrupt_handlers[interruptNumber](interruptNumber, error, frame, data[interruptNumber]);
}

void Interrupt::registerHandler(uint8_t number, handler_t handler, void* user_ptr) {
    Guard guard;
    interrupt_handlers[number] = handler;
    data[number] = user_ptr;
}

void Interrupt::init() {
    Guard guard;
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));
    auto start = saveReadSymbol("__kernel_interrupt_first_stage");
    auto end = saveReadSymbol("__kernel_interrupt_first_stage_end");
    auto element_size = (end - start) / 256;
    for (auto i = 0; i < 256; ++i) {
        auto address = start + i * element_size;
        setHandler(VirtualAddress(address), i);
    }
    interrupt_number_pointer = &interrupt_number_single_cpu_buffer;
    *interrupt_number_pointer = 0;

    setIDTR((uint64_t) interruptVectorTable, page_size);
}

void Interrupt::interrupt(uint8_t number) {
    if (!APIC::get_current_cpu())
        panic("Interrupt::interrupt called before APIC::init");
    auto apic = APIC::get_current_lapic();
    apic.send_interrupt(APIC::LocalAPIC::InterruptType::Normal, number, apic.get_id());
}
void Interrupt::sendEOI() {
    if (!APIC::get_current_cpu())
        panic("Interrupt::sendEOI called before APIC::init");
    auto apic = APIC::get_current_lapic();
    apic.end_of_interrupt() = 0;
}
uint8_t Interrupt::get_free_interrupt_number() {
    for (uint8_t i = 32; i < 255; ++i) {
        if (interrupt_handlers[i] == nullptr)
            return i;
    }
    panic("No free interrupt number");
}
void Interrupt::clear_startup() {
    setIDTR((uint64_t) interruptVectorTable, page_size);
}
