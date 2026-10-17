# SPDX-License-Identifier: GPL-3.0-or-later
# Uses the unchanged ps5-payload-sdk v0.43 compiler wrappers and service stubs.
ifndef PS5_PAYLOAD_SDK
$(error PS5_PAYLOAD_SDK must name the existing pinned SDK directory)
endif
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

BUILD_DIR ?= build
ARTIFACT := $(BUILD_DIR)/modbridge-app-launcher.elf
CFLAGS := -std=c11 -Wall -Wextra -Werror -Os
LDLIBS := -lSceSystemService -lSceUserService

.PHONY: all
all: $(ARTIFACT)

$(BUILD_DIR):
	mkdir -p "$@"

$(ARTIFACT): main.c Makefile | $(BUILD_DIR)
	$(CC) $(CFLAGS) main.c $(LDLIBS) -o "$@"
