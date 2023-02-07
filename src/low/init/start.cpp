asm(R"(
.section .multiboot_header, "ad"
header_start:
.align 8
	# magic number
	.int 0xe85250d6 # multiboot2
	# architecture
	.int 0 # protected mode i386
	# header length
	.int header_end - header_start
	# checksum
	.int 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

	# end tag
	.word 0
	.word 0
	.int 8
header_end:
)");

asm(R"(
.global __start
.global gdt64
.global gdt64_end
.extern main
.section .text
.code64
.align 16
main_x64:
    mov $0b10000, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov %rbx, %rdi
    call main

stop_64:
    hlt
    jmp stop_64

.code32
.align 16
__start:
    cli
    mov $start_stack_top, %esp
    push %ebx
	call clear
	mov $hello_string, %esi
	call print

	call check_if_x64
    mov $x64_supported, %esi
    call print

	call setup_paging

    mov $x64_paged, %esi
    call print

    pop %ebx
	call __go_to_x64

	
__32fail:
    jmp __32fail

__go_to_x64:
    mov $page_table_l4, %eax
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

    lgdt gdt64_end

    jmp $0b1000,$main_x64

    ret

setup_paging:
    mov $page_table_l3, %eax
    or $0b11, %eax
    movl %eax, page_table_l4

    mov $page_table_l2, %eax
    or $0b11, %eax
    movl %eax, page_table_l3
  
    mov $0, %ecx
setup_paging_loop:
    mov $0x200000, %eax
    mul %ecx
    or $0b10000011, %eax
    mov %eax, page_table_l2(,%ecx,8)

    inc %ecx

    cmp $512, %ecx
    jne setup_paging_loop

    ret


check_if_x64:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $(1<<21), %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    cmp %ecx, %eax
    je __32fail

    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb __32fail
    mov $0x80000001, %eax
    cpuid
    test $(1<<29), %edx
    jz __32fail

    ret

clear:
    mov $0xB8000, %edi
	mov $2000, %ecx
clear_loop:
	movb $0x20, (%edi)
	inc %edi
	movb $0x0A, (%edi)
	inc %edi
	dec %ecx
	jnz clear_loop
	ret

print:
    mov $0xB8000, %edi
print_loop:
	movb (%esi), %al
	movb %al, (%edi)
	inc %edi
	movb $0x0A, (%edi)
	inc %edi
	inc %esi

	cmp %al, 0
	jne print_loop
	ret

.section .bss
.align 0x1000
page_table_l4:
.zero 0x1000
page_table_l3:
.zero 0x1000
page_table_l2:
.zero 0x1000
start_stack_bottom:
.zero 0x4000
start_stack_top:

.section .data
hello_string:
    .asciz "[x86] CrackOS  \0"
x64_supported:
    .asciz "[x64] CrackOS  \0"
x64_paged:
    .asciz "[x64] PageTable\0"
debug_message:
    .asciz "[x64] Debugging\0"

.align 0x1000
gdt64:
	.quad 0 # zero entry
	.quad (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) # code segment
	.quad (1 << 41) | (1 << 44) | (1 << 47) # data segment
	.quad (1 << 43) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 47) | (1 << 53) # ring 3 code segment
	.quad (1 << 41) | (1 << 44) | (1 << 45) | (1 << 46) | (1 << 47) # ring 3 data segment
	.quad 0
	.quad 0
gdt64_end:
    .word gdt64_end - gdt64 - 1
	.quad gdt64

)");
