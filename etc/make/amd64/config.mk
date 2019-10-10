### This file contains configurable options for amd64 platform
### Defaults
AMD64_MAX_SMP?=2
AMD64_KERNEL_STACK?=65536

QEMU_MEM?=512
QEMU_SMP?=2

### From config
ifdef AMD64_TRACE_IRQ
DEFINES+=-DAMD64_TRACE_IRQ
OBJS+=$(O)/sys/amd64/hw/irq_trace.o
endif

DEFINES+=-DAMD64_MAX_SMP=$(AMD64_MAX_SMP)
DEFINES+=-DAMD64_KERNEL_STACK=$(AMD64_KERNEL_STACK)
