export CC=$(CROSS_COMPILE)gcc
export O?=build

all: mkdirs $(O)/kernel

clean:
	rm -rf $(O)

mkdirs:
	@mkdir -p $(DIRS)

include etc/Kernel.makefile

install-headers:
	@if [ "$(INSTALL_HDR)x" = x ]; then exit 1; fi
	@$(foreach hdr,$(KERNEL_USER_HDR), \
		echo "INSTALL $(hdr)"; \
		install -D -m 0644 "$(hdr)" "$(INSTALL_HDR)/$(hdr:include/user/%=ygg/%)"; \
	)
