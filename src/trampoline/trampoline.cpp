//
// Created by nudelerde on 03.01.23.
//

asm(R"(
.code16
.align 16
.section .text
.globl trampoline_entry_16
.globl trampoline_data
.globl trampoline_data_pagetable_l4
.globl trampoline_data_gdt_ptr
.globl trampoline_stack_ptr
.extern ap_cpu
trampoline_entry_16:
    cli
    mov $gdt_32_bit_ptr, %eax
    lgdt (%eax)
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmp $0x08, $trampoline_entry_32

.code32
trampoline_entry_32:
    mov $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $trampoline_data_pagetable_l4, %eax
    mov (%eax), %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(0x1 << 5), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $(0x1 << 8), %eax
    wrmsr

    mov %cr0, %eax
    or $(0x1 << 31), %eax
    mov %eax, %cr0

    mov $trampoline_data_gdt_ptr, %eax
    lgdt (%eax)
    jmp $0b1000, $trampoline_entry_64
.code64
trampoline_entry_64:
    mov $0b10000, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $trampoline_stack_ptr, %eax
    mov (%eax), %rsp
    movabs $ap_cpu, %rdi
    call *%rdi

gdt_32_bit:
    .quad 0 # zero
    .quad (0xFFFF) | (0xF << 48) | (1 << 43) | (1 << 44) | (1 << 47) | (1 << 54) | (1 << 55) # 32 bit code
    .quad (0xFFFF) | (0xF << 48) | (1 << 41) | (1 << 44) | (1 << 47) | (1 << 54) | (1 << 55) # 32 bit data
gdt_32_bit_ptr:
    .word gdt_32_bit_ptr - gdt_32_bit - 1
    .long gdt_32_bit

trampoline_data_pagetable_l4:
.quad 0
trampoline_data_gdt_ptr:
.word 0
.quad 0
trampoline_stack_ptr:
.quad 0
)");