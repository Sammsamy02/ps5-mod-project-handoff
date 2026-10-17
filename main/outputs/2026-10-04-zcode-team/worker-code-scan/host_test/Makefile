# host_test Makefile — macOS host verification of the shadow-copy scanner.
# Usage:
#   make            # build host_test
#   make run        # build + run against the retained decrypted eboot
#   make asan       # build + run under AddressSanitizer/UBSan
#
# The eboot path points at the sha-pinned image (sha256 3b6cfad4...) already
# retained by worker-game-version; no console contact, file is read-only.

EBOOT ?= ../../worker-game-version/console-decrypted-eboot.bin
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -pedantic

.PHONY: all run asan clean

all: host_test

host_test: host_test.cpp ../shadow_copy.cpp ../shadow_copy.hpp
	$(CXX) $(CXXFLAGS) -o $@ host_test.cpp ../shadow_copy.cpp

run: host_test
	./host_test "$(EBOOT)"

asan: host_test.cpp ../shadow_copy.cpp ../shadow_copy.hpp
	$(CXX) $(CXXFLAGS) -g -fsanitize=address,undefined -o host_test.asan host_test.cpp ../shadow_copy.cpp
	./host_test.asan "$(EBOOT)"

clean:
	rm -f host_test host_test.asan
