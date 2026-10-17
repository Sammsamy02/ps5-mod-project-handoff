# host_test Makefile — macOS host verification of the staged shadow
# integration (worker-shadow-integrate).
#
#   make            # build host_test
#   make run        # build + run against the retained decrypted eboot
#   make asan       # build + run under AddressSanitizer/UBSan
#
# The eboot path points at the sha-pinned image (sha256 3b6cfad4..., the same
# pin as SeedTable.h) retained by worker-game-version; read-only, no console
# contact.

EBOOT ?= ../../worker-game-version/console-decrypted-eboot.bin
CXX ?= clang++
# C++20: the pinned UE4SS headers use std::span / unordered_map::contains;
# the port build already compiles them as C++20.
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror -pedantic
# -I ../pinned: shadow_copy.{hpp,cpp} + SeedTable.h are the verbatim pinned
# units; the staged headers include "shadow_copy.hpp" co-located (composition
# drops shadow_copy.* / shadow_module_source.hpp / shadow_arm.hpp side by side).
INC = -I ../ue4ss_headers -I .. -I ../pinned
# Static API macros of the pinned headers (empty export decorations).
DEFINES = -DRC_HELPERS_BUILD_STATIC -DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC

.PHONY: all run asan clean

all: host_test

host_test: host_test.cpp mock_owned_range_scanner.cpp \
           ../shadow_module_source.hpp ../shadow_arm.hpp \
           ../pinned/shadow_copy.hpp ../pinned/shadow_copy.cpp \
           ../pinned/SeedTable.h
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INC) -o $@ \
	    host_test.cpp mock_owned_range_scanner.cpp ../pinned/shadow_copy.cpp

run: host_test
	./host_test "$(EBOOT)" | tee run-output.txt

asan: host_test.cpp mock_owned_range_scanner.cpp \
      ../shadow_module_source.hpp ../shadow_arm.hpp \
      ../pinned/shadow_copy.hpp ../pinned/shadow_copy.cpp \
      ../pinned/SeedTable.h
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INC) -g -fsanitize=address,undefined \
	    -o host_test.asan host_test.cpp mock_owned_range_scanner.cpp ../pinned/shadow_copy.cpp
	./host_test.asan "$(EBOOT)" | tee run-output-asan.txt

clean:
	rm -f host_test host_test.asan
