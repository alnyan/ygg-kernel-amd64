### This file contains configurable options for amd64 platform
### Defaults
AMD64_SMP?=1						# SMP enabled
AMD64_MAX_SMP?=2					# Maximum SMP CPUs supported
AMD64_KERNEL_STACK?=65536			# Kernel stack size (when bootstrapping + task kstack)

QEMU_MEM?=512
QEMU_SMP?=2

VESA_ENABLE?=0
VESA_MODE?=0
VESA_WIDTH?=1024
VESA_HEIGHT?=768
VESA_DEPTH?=32

### From config
ifdef AMD64_TRACE_IRQ
DEFINES+=-DAMD64_TRACE_IRQ
OBJS+=$(O)/sys/amd64/hw/irq_trace.o
endif

ifeq ($(AMD64_SMP),1)
DEFINES+=-DAMD64_SMP
DEFINES+=-DAMD64_MAX_SMP=$(AMD64_MAX_SMP)
OBJS+=$(O)/sys/amd64/hw/ap_code_blob.o \
	  $(O)/sys/amd64/smp/smp.o \
	  $(O)/sys/amd64/smp/ipi.o \
	  $(O)/sys/amd64/smp/irq_ipi_s.o
DIRS+=$(O)/sys/amd64/smp
else
DEFINES+=-DAMD64_MAX_SMP=1
endif

ifdef AMD64_IDLE_RING3
DEFINES+=-DAMD64_IDLE_RING3
endif

ifdef ENABLE_RTL8139
OBJS+=$(O)/sys/amd64/hw/pci/rtl8139.o
endif

ifeq ($(VESA_ENABLE),1)
DEFINES+=-DVESA_MODE=$(VESA_MODE) \
		 -DVESA_WIDTH=$(VESA_WIDTH) \
		 -DVESA_HEIGHT=$(VESA_HEIGHT) \
		 -DVESA_DEPTH=$(VESA_DEPTH) \
		 -DVESA_ENABLE=1
OBJS+=$(O)/sys/amd64/hw/vesa.o
endif

PCI_UHCI_ENABLE?=1
ifeq ($(PCI_UHCI_ENABLE),1)
DEFINES+=-DPCI_UHCI_ENABLE
OBJS+=$(O)/sys/amd64/hw/pci/usb_uhci.o
endif

DEFINES+=-DAMD64_KERNEL_STACK=$(AMD64_KERNEL_STACK)
