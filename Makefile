.PHONY: doc
ifeq ($(ARCH),)
$(error Target architecture is not specified: $${ARCH})
endif

ifeq ($(ARCH),amd64)
CFLAGS+=-DARCH_AMD64
endif

export CC?=$(CC)
export CROSSCC?=$(CROSS_COMPILE)$(CC)
export LD?=$(LD)
export CROSSLD?=$(CROSS_COMPILE)$(LD)
export S=$(abspath src)
export O?=$(abspath build)

include conf/make/none.mk
include conf/make/$(ARCH).mk


all: mkdirs $(TARGETS)

clean:
	@rm -rf $(O)

mkdirs:
	@mkdir -p $(O) $(DIRS)

doc:
	@make -sC doc all
