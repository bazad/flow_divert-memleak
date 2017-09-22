TARGET = flow_divert-memleak

ARCH     ?= x86_64
SDK      ?= macosx
ifneq ($(ARCH),x86_64)
CLANG    := $(shell xcrun --sdk $(SDK) --find clang)
ifeq ($(CLANG),)
$(error Could not find clang for SDK $(SDK))
endif
SYSROOT  := $(shell xcrun --sdk $(SDK) --show-sdk-path)
CC       := $(CLANG) -isysroot $(SYSROOT) -arch $(ARCH)
endif
CODESIGN := codesign

CFLAGS    = -O2 -Wall -Werror -Wpedantic

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $^
	$(CODESIGN) -s - $@

clean:
	rm -f -- $(TARGET)
