/*
 * Userspace program linker script,
 * partially derived from GCC's -static one
 */
ENTRY(_start)

SECTIONS {
    . = 0x400000;

    .text : ALIGN(4K) {
        KEEP (*(SORT_NONE(.init)))
        *(.text)
        KEEP (*(SORT_NONE(.fini)))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
    }

    .rodata : {
        *(.rodata)
        *(.eh_frame)
    }

    .data : ALIGN(4K) {
        *(.data)
    }

    .bss : ALIGN(4K) {
        *(.bss)
    }

    __program_end = .;
    __heap_start = .;
}
