#!/bin/sh
# lldb_test.sh - host test with a real debugger frontend.
#
# macOS ships no gdb; LLDB speaks the same GDB Remote Serial Protocol
# ("gdb-remote"), so this exercises the stub exactly the way
# "gdb -ex 'target remote :2345'" would on a gdb-equipped host:
# handshake -> target.xml -> registers -> memory read/write -> detach.
#
# Usage: ./lldb_test.sh [port]
set -u
PORT="${1:-2345}"
BIN="./gdbstub"

if [ ! -x "$BIN" ]; then
    echo "build first: make"
    exit 2
fi

"$BIN" -p "$PORT" >/dev/null 2>lldb_stub_stderr.log &
STUB=$!
trap 'kill $STUB 2>/dev/null' EXIT
sleep 1

lldb -b \
    -o "gdb-remote 127.0.0.1:$PORT" \
    -o "version" \
    -o "target list" \
    -o "register read rip rax rbx rsp" \
    -o "memory read --size 1 --format x --count 16 0x400000" \
    -o "memory read --format s 0x400040" \
    -o "memory write -s 8 0x401000 0x1122334455667788" \
    -o "memory read --size 8 --format x --count 1 0x401000" \
    -o "disassemble -s 0x400100 -c 4" \
    -o "detach" \
    -o "quit" 2>&1

echo "--- stub log tail ---"
tail -5 lldb_stub_stderr.log
