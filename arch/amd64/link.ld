/*
 * amd64 kernel linker script
 */

ENTRY(_entry64)

_kernel_base = 0xFFFFFF0000000000;
/*
 * This will allow us to have initrds up to 32MiB
 * loaded below
 */
_kernel_base_phys = 0x2200000;

SECTIONS {
	. = _kernel_base + _kernel_base_phys;

	.text ALIGN(4K) : AT(ADDR(.text) - _kernel_base)
	{
		*(.multiboot)
		*(.text)
		*(.rodata)
	}

	.data ALIGN(4K) : AT(ADDR(.data) - _kernel_base)
	{
		*(.data)
        *(.data.rel.local)
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - _kernel_base)
	{
		*(COMMON)
		*(.bss)
	}

    . = ALIGN(4K);
    _kernel_end = .;
    _kernel_end_phys = . - _kernel_base;
}

