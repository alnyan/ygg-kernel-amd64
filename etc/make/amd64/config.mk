### This file contains configurable options for amd64 platform
### Defaults
AMD64_SMP?=1						# SMP enabled
AMD64_MAX_SMP?=2					# Maximum SMP CPUs supported
AMD64_KERNEL_STACK?=65536			# Kernel stack size (when bootstrapping + task kstack)

QEMU_MEM?=512
QEMU_SMP?=2

### From config
ifdef AMD64_TRACE_IRQ
DEFINES+=-DAMD64_TRACE_IRQ
OBJS+=$(O)/sys/amd64/hw/irq_trace.o
endif

ifeq ($(AMD64_SMP),1)
DEFINES+=-DAMD64_SMP
DEFINES+=-DAMD64_MAX_SMP=$(AMD64_MAX_SMP)
OBJS+=$(O)/sys/amd64/hw/ap_code_blob.o
endif

DEFINES+=-DAMD64_KERNEL_STACK=$(AMD64_KERNEL_STACK)
