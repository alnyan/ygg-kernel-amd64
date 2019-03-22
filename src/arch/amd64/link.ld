/*
 * amd64 loader linker script
 * (this is not kernel)
 */

ENTRY(_start)

_kernel_base = 0xFFFFFF0000000000;
_kernel_base_phys = 0x200000;

SECTIONS {
	. = _kernel_base + _kernel_base_phys;

	.text ALIGN(4K) : AT(ADDR(.text) - _kernel_base)
	{
		*(.multiboot)
		*(.text)
	}

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - _kernel_base)
	{
		*(.rodata)
	}

	.data ALIGN(4K) : AT(ADDR(.data) - _kernel_base)
	{
		*(.data)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - _kernel_base)
	{
		*(COMMON)
		*(.bss)
	}
}

