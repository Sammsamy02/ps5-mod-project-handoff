# SPDX-License-Identifier: GPL-3.0-or-later
# Fixed local tools from verified official archives; no install/deployment target.
LLVM_BIN := build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin
PYTHON ?= python3
CC := $(LLVM_BIN)/clang
LD := $(LLVM_BIN)/ld.lld
CFLAGS := --target=x86_64-sie-ps5 -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fPIC -fno-plt -mno-red-zone -Wall -Wextra -Werror
OBJECTS := build/entry.o build/native.o build/probe.o build/report.o

.PHONY: all deps test verify inspect clean
all: verify

deps:
	$(PYTHON) verify_dependencies.py

test:
	$(PYTHON) -m unittest discover -s tests -v

build/%.o: %.c report.h payload_args.h | deps
	$(CC) $(CFLAGS) -c $< -o $@

build/entry.o: entry.S | deps
	$(CC) --target=x86_64-sie-ps5 -c $< -o $@

build/native-probe.elf: $(OBJECTS) elf_x86_64.x | deps
	$(LD) -m elf_x86_64 -pie -e _start -T elf_x86_64.x --eh-frame-hdr -z max-page-size=0x4000 --hash-style=gnu -o $@ $(OBJECTS)

verify: build/native-probe.elf
	$(PYTHON) verify_elf.py $< > build/static-report.json
	cat build/static-report.json

inspect: verify
	$(LLVM_BIN)/llvm-readelf -h -l -d -r -s build/native-probe.elf > build/readelf.txt
	$(LLVM_BIN)/llvm-objdump -d build/native-probe.elf > build/disassembly.txt

# Keep the large verified local dependency cache. Remove build products only.
clean:
	rm -f $(OBJECTS) build/native-probe.elf build/static-report.json build/readelf.txt build/disassembly.txt build/test-report
