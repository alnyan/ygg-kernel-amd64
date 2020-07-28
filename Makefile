export CC=$(CROSS_COMPILE)gcc
export O?=build

all: mkdirs $(O)/kernel

clean:
	rm -rf $(O)

mkdirs:
	@mkdir -p $(DIRS)

include etc/Kernel.makefile
