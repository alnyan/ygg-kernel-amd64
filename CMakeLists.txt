cmake_minimum_required(VERSION 3.14)
project(yggdrasil-kernel)
enable_language(C ASM)

set(SRC arch/amd64/kernel.c
        arch/amd64/acpi_osl_mem.c
	    arch/amd64/acpi_osl_printf.c
	    arch/amd64/acpi_osl_thread.c
	    arch/amd64/acpi_osl_init.c
	    arch/amd64/acpi_osl_table.c
	    arch/amd64/acpi_osl_irq.c
	    arch/amd64/acpi_osl_hw.c
	    arch/amd64/hw/rs232.c
	    arch/amd64/hw/con.c
	    arch/amd64/hw/gdt.c
        arch/amd64/hw/gdt_s.S
	    arch/amd64/hw/acpi.c
	    arch/amd64/mm/mm.c
	    arch/amd64/hw/apic.c
	    arch/amd64/hw/idt.c
        arch/amd64/hw/exc_s.S
	    arch/amd64/hw/exc.c
        arch/amd64/hw/irq0.S
	    arch/amd64/hw/timer.c
	    arch/amd64/hw/ioapic.c
        arch/amd64/hw/irqs_s.S
        arch/amd64/sys/spin_s.S
	    arch/amd64/cpu.c
	    arch/amd64/mm/heap.c
	    arch/amd64/mm/map.c
	    arch/amd64/mm/phys.c
	    arch/amd64/mm/vmalloc.c
	    arch/amd64/mm/pool.c
	    arch/amd64/hw/ps2.c
	    arch/amd64/hw/irq.c
	    arch/amd64/hw/rtc.c
        arch/amd64/fpu.S
	    arch/amd64/cpuid.c
        arch/amd64/sched_s.S
        arch/amd64/syscall_s.S
	    arch/amd64/syscall.c
	    arch/amd64/binfmt_elf.c

        sys/debug.c
	    sys/ubsan.c
	    sys/panic.c
	    sys/string.c
	    sys/config.c
	    sys/ctype.c
	    sys/errno.c
	    sys/kernel.c
	    sys/time.c
	    sys/char/input.c
	    sys/char/ring.c
	    sys/char/line.c
	    sys/char/tty.c
	    sys/char/chr.c
	    sys/char/pipe.c
	    sys/block/pseudo.c
	    sys/block/part_gpt.c
	    sys/block/ram.c
	    sys/block/blk.c
	    sys/block/cache.c
	    sys/execve.c
	    sys/dev.c
	    sys/sys_file.c
	    sys/sys_sys.c
	    sys/thread.c
	    sys/snprintf.c
	    sys/random.c
	    sys/reboot.c
	    sys/init.c
	    sys/mem/shmem.c
	    sys/mem/slab.c
	    sys/console.c
	    sys/display.c
	    sys/wait.c
	    sys/sched.c
	    sys/font/psf.c
	    sys/font/logo.c
	    sys/mod.c

        fs/vfs.c
	    fs/vfs_ops.c
	    fs/vfs_access.c
	    fs/fs_class.c
	    fs/ofile.c
	    fs/node.c
	    fs/sysfs.c
	    fs/tar.c
	    fs/ext2/block.c
	    fs/ext2/ext2.c
	    fs/ext2/node.c

        drivers/pci/pci.c
	    drivers/pci/pcidb.c
	    drivers/ata/ahci.c
	    drivers/usb/usb_uhci.c
	    drivers/usb/usb.c
	    drivers/usb/driver.c
	    drivers/usb/device.c
	    drivers/usb/usbkbd.c
	    drivers/usb/hub.c

        net/net.c
	    net/if.c
	    net/socket.c
	    net/util.c
	    net/ports.c
	    net/unix.c
	    sys/sys_net.c)

#### CFLAGS and LDFLAGS
set(CMAKE_C_FLAGS "${CFLAGS} \
-ffreestanding \
-fPIE \
-fno-plt \
-fno-pic \
-DARCH_AMD64 \
-DKERNEL_VERSION_STR=\\\"cmake\\\" \
-D__KERNEL__ \
-mcmodel=large \
-mno-sse \
-mno-sse2 \
-mno-red-zone \
-mno-mmx \
-z max-page-size=0x1000 \
-Wall \
-Wextra \
-Wno-unused \
-O0 \
-ggdb \
-Werror")

set(CMAKE_EXE_LINKER_FLAGS "${LDFLAGS} \
-nostdlib \
-fPIE \
-fno-plt \
-fno-pic \
-static \
-Wl,--build-id=none \
-z max-page-size=0x1000 \
-T${CMAKE_SOURCE_DIR}/arch/amd64/link.ld")

set(ASM_OPTIONS "-x assembler-with-cpp")
set(CMAKE_ASM_FLAGS "${CFLAGS} -D__ASM__")
#### Options
# ...

#### Dependencies
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/include/arch/amd64/acpica)
add_subdirectory(${CMAKE_SOURCE_DIR}/arch/amd64/acpica)

#### Compile font file
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/default8x16.psfu.o
COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/etc &&
${CMAKE_C_COMPILER} -c -DINCBIN_FILE='"default8x16.psfu"' -DINCBIN_START="_psf_start"
 -DINCBIN_END="_psf_end" -o ${CMAKE_BINARY_DIR}/default8x16.psfu.o
 ${CMAKE_CURRENT_SOURCE_DIR}/arch/amd64/incbin.S)
set_source_files_properties(${CMAKE_BINARY_DIR}/default8x16.psfu.o
PROPERTIES EXTERNAL_OBJECT true GENERATED true)

#### Kernel linkage and options
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_executable(kernel ${CMAKE_SOURCE_DIR}/arch/amd64/entry.S
                      ${SRC}
                      ${CMAKE_BINARY_DIR}/default8x16.psfu.o)
target_link_libraries(kernel acpica)
#target_link_libraries(kernel fonts)
