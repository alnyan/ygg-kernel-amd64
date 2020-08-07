ifneq (,$(wildcard ./Kernel.config))
include Kernel.config
endif

ifeq ($(ENABLE_UNIX),1)
KERNEL_DEF+=-DENABLE_UNIX=1
KERNEL_OBJ+=$(O)/net/unix.o
endif

ifeq ($(ENABLE_NET),1)
KERNEL_DEF+=-DENABLE_NET=1
KERNEL_OBJ+=$(O)/net/if.o \
		    $(O)/net/net.o \
		    $(O)/net/socket.o \
		    $(O)/sys/sys_net.o
endif

ifeq ($(ENABLE_VESA),1)
KERNEL_DEF+=-DVESA_ENABLE=1 \
			-DVESA_WIDTH=$(VESA_WIDTH) \
			-DVESA_HEIGHT=$(VESA_HEIGHT) \
			-DVESA_DEPTH=$(VESA_DEPTH)
KERNEL_OBJ+=$(O)/arch/amd64/hw/vesa.o
endif
