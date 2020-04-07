.PHONY: doc
ifeq ($(ARCH),)
$(error Target architecture is not specified: $${ARCH})
endif

ifeq ($(ARCH),amd64)
CFLAGS+=-DARCH_AMD64
endif

include config

export O?=$(abspath build)

# Include base system
include etc/make/conf.mk
# Arch details
include etc/make/$(ARCH)/conf.mk

all: mkdirs config $(TARGETS)

docs: $(HDRS)
	@make -C doc

clean:
	@rm -rf $(O)

mkdirs:
	@mkdir -p $(O) $(DIRS)
