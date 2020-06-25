### This file contains configurable options for amd64 platform
### Defaults
AMD64_SMP?=1						# SMP enabled
AMD64_MAX_SMP?=2					# Maximum SMP CPUs supported
AMD64_KERNEL_STACK?=65536			# Kernel stack size (when bootstrapping + task kstack)

QEMU_MEM?=512
QEMU_SMP?=2

ifeq ($(AMD64_SMP),1)
DEFINES+=-DAMD64_SMP \
		 -DAMD64_MAX_SMP=$(AMD64_MAX_SMP)
OBJS+=$(O)/arch/amd64/hw/ap_code_blob.o \
	  $(O)/arch/amd64/smp/smp.o \
	  $(O)/arch/amd64/smp/ipi.o \
	  $(O)/arch/amd64/smp/irq_ipi_s.o
DIRS+=$(O)/arch/amd64/smp
else
DEFINES+=-DAMD64_MAX_SMP=1
endif

ifeq ($(VESA_ENABLE),1)
DEFINES+=-DVESA_ENABLE=1 \
   	 -DVESA_WIDTH=$(VESA_WIDTH) \
   	 -DVESA_HEIGHT=$(VESA_HEIGHT) \
   	 -DVESA_DEPTH=$(VESA_DEPTH) \
   	 -DVESA_MODE=0
OBJS+=$(O)/arch/amd64/hw/vesa.o
endif

### From config
DEFINES+=-DAMD64_KERNEL_STACK=$(AMD64_KERNEL_STACK)
