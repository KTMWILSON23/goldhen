ifndef OO_PS4_TOOLCHAIN
$(error OO_PS4_TOOLCHAIN is not set)
endif

ifndef GOLDHEN_SDK
$(error GOLDHEN_SDK is not set)
endif

PLUGIN_NAME := xxfwilson23_shellui_metadata_collector
RAINBOW_METADATA_READY ?= 0
RAINBOW_COLLECTOR ?= 1
CC := clang
LD := ld.lld
FSELF := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

CFLAGS := \
	--target=x86_64-pc-freebsd12-elf \
	-std=c11 -Wall -Wextra -Werror \
	-Wno-unused-parameter -fPIC -funwind-tables \
	-D__ORBIS__ -D__PS4__ \
	-DXXF_RAINBOW_METADATA_READY=$(RAINBOW_METADATA_READY) \
	-DXXF_RAINBOW_COLLECTOR=$(RAINBOW_COLLECTOR) \
	-isysroot $(OO_PS4_TOOLCHAIN) \
	-isystem $(OO_PS4_TOOLCHAIN)/include \
	-I$(GOLDHEN_SDK)/include

LDFLAGS := \
	-m elf_x86_64 --script $(OO_PS4_TOOLCHAIN)/link.x \
	--eh-frame-hdr -pie -z max-page-size=0x4000 -z common-page-size=0x4000 \
	-L$(OO_PS4_TOOLCHAIN)/lib -L$(GOLDHEN_SDK)

LIBS := -lkernel -lSceLibcInternal -lmonosgen
CRT := $(GOLDHEN_SDK)/build/crtprx.o

.PHONY: all clean
all: $(BIN_DIR)/$(PLUGIN_NAME).prx

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PLUGIN_NAME).elf: $(CRT) $(OBJS)
	$(LD) $(LDFLAGS) $(CRT) $(OBJS) $(LIBS) -o $@

$(BIN_DIR)/$(PLUGIN_NAME).prx: $(BUILD_DIR)/$(PLUGIN_NAME).elf | $(BIN_DIR)
	$(FSELF) -in $< -out $(BUILD_DIR)/$(PLUGIN_NAME).oelf \
		--lib $@ --paid 0x3800000000000011

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
