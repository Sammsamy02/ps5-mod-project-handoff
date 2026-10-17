# worker-kernel-debugger: GDB remote stub prototype (host build)
#
#   make            build ./gdbstub (mock + ps5 backends linked)
#   make test       build + run the Python RSP conformance client
#   make lldb-test  scripted lldb gdb-remote session (memory r/w + regs)
#   make clean
#
# PS5 payload build (on the console toolchain): same sources, e.g.
#   clang --target=x86_64-unknown-freebsd12 -O2 -o gdbstub.prx \
#       main.c gdbstub.c mock_backend.c ps5_backend.c -DGDBSTUB_PS5
# (link against the payload SDK; kio_shim.h is the only kernel contact)

CC      ?= clang
CFLAGS  ?= -O2 -g -Wall -Wextra -std=c11
BIN      = gdbstub
SRCS     = main.c gdbstub.c mock_backend.c ps5_backend.c
HDRS     = gdbstub.h kio_shim.h
PORT    ?= 2345

.PHONY: all test lldb-test clean

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

test: $(BIN)
	python3 test_rsp_client.py $(PORT)

lldb-test: $(BIN)
	./lldb_test.sh $(PORT)

clean:
	rm -f $(BIN) *.o
