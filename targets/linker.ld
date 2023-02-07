ENTRY(__start)

SECTIONS
{
    . = 0x08000;
    trampolineStart = .;
    .trampoline_text : {
        build/trampoline*.o (.text)
    }
    .trampoline_rodata : {
        build/trampoline*.o (.rodata)
    }
    .trampoline_bss : {
        build/trampoline*.o (.bss)
    }
    .trampoline_data : {
        build/trampoline*.o (.data)
    }
    . = ALIGN(0x1000);
    trampolineEnd = .;
    .boot : {
        KEEP(*(.multiboot_header))
    }
	. = 1M;
	physical_start = .;
	low_start = .;
	. = ALIGN(0x1000);
	.start_text : {
		build/low*.o (.text)
		build/low*.o (.text.*)
	}
	.start_rodata : {
		build/low*.o (.rodata)
		build/low*.o (.rodata.*)
	}
	.start_bss : {
		build/low*.o (.bss)
		build/low*.o (.bss.*)
	}
	.start_data : {
		build/low*.o (.data)
		build/low*.o (.data.*)
	}
	low_end = ALIGN(0x200000);
	high_physical_start = low_end;
	. = 0x200000000000; /*32 Ti*/
	high_start = .;

	code_start = .;
	.text_main : AT(code_start - high_start + high_physical_start) {
		KEEP(*(.text))
		*(.text.*)
	}
	code_end = .;
	. = ALIGN(0x1000);

	rodata_start = .;
	.rodata_main : AT(rodata_start - high_start + high_physical_start) {
		*(.rodata)
		*(.rodata.*)
	}
	.rodata_end = .;
	. = ALIGN(0x1000);
	
	read_only_end = .;

	bss_start = .;
	.bss_main : AT(bss_start - high_start + high_physical_start) {
		*(.bss)
		*(.bss.*)
	}
	bss_end = .;
	. = ALIGN(0x1000);

	data_start = .;
	.data_main : AT(data_start - high_start + high_physical_start) {
		*(.data)
		*(.data.*)
		*(.got)
		*(.got.*)
	}
	data_end = .;
	. = ALIGN(0x1000);
	high_end = .;
	physical_end = high_end - high_start + high_physical_start;
	high_physical_end = physical_end;
	/DISCARD/ : {
		*(.eh_frame)
		*(.comment)
	}
}