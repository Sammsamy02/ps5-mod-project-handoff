# cap01-kread-probe — Mac-only build. NO deploy/test targets: loading is
# a user-authorized console step (see TEST-CARD.md), never a make target.
# v0 = preserved provenance (console evidence 2026-10-13; LSTAR classifier
# bug, see LSTAR-DIAGNOSIS.md). v1 = CF-flag fix, distinct probe ID/log path.

PS5_PAYLOAD_SDK ?= /WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

ELF_V0 := cap01-kread-probe-v0.elf
ELF_V1 := cap01-kread-probe-v1.elf

# The SDK's bin wrappers resolve the real LLVM tools via prospero-llvm-config,
# which scans PATH for llvm-config-XX. Homebrew llvm@18 is keg-only, so add it
# to PATH here (override with LLVM_BIN=... if the toolchain lives elsewhere).
LLVM_BIN ?= /opt/homebrew/opt/llvm@18/bin
export PATH := $(LLVM_BIN):$(PATH)

CFLAGS := -Wall -Werror -O2 -g

all: $(ELF_V0) $(ELF_V1)

$(ELF_V0): KernelReadProbe.c
	$(CC) $(CFLAGS) -o $@ $^

$(ELF_V1): KernelReadProbe-v1.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(ELF_V0) $(ELF_V1)

.PHONY: all clean
