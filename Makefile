export CC=$(CROSS_COMPILE)gcc
export O?=build

all: chkconfig mkdirs $(O)/kernel

chkconfig:
	@[ -f Kernel.config ] || { \
		echo "Kernel.config is not present. Run 'make defconfig' to make one" >&2; \
		exit 1; \
	}

defconfig: Kernel.defconfig
	cp $< Kernel.config

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
