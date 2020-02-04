/*
 * amd64 loader linker script
 * (this is not kernel)
 */

ENTRY(_start)

SECTIONS {
	. = 0x100000;

    _loader_start = .;

	.text : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}

	.rodata : ALIGN(4K)
	{
		*(.rodata)
	}

	.data : ALIGN(4K)
	{
		*(.data)
	}

	.bss : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

    _loader_end = .;
}
