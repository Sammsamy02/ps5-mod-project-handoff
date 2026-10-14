# worker-kernel-debugger — GDB Remote Serial Protocol stub (ZCode priority #5)

Design + host prototype for the live debugger channel: a GDB/LLDB on the
Mac connects to the PS5 game through a kernel-I/O-backed RSP server.
No console contact, no game claims — everything here ran on the host
against mock memory.

## Deliverables

| file | role |
|---|---|
| `gdbstub.h`    | portable core interface; `struct gdb_backend` vtable = the swap point between mock and kernel I/O; x86-64 register file (536-byte `g` layout, `_Static_assert`-checked) |
| `gdbstub.c`    | RSP server: TCP listener, `$data#cksum` framing, `+`/`-` acks, `}` escaping, `*` RLE decode, no-ack mode, full packet dispatch, target.xml/threads.xml generated from the register table (cannot drift) |
| `mock_backend.c` | deterministic 1 MiB mock memory (fake ELF header, magic u64, banner, mock x86-64 prologue) + canned register file |
| `ps5_backend.c`  | kernel-backed backend + the full kernel integration plan (comments at top of file) |
| `kio_shim.h`     | single-purpose ioctl ABI between payload server and kstuffSharp (`KIO_READ_MEM/WRITE_MEM/GET_REGS/SET_REGS`) |
| `main.c` / `Makefile` / `test_rsp_client.py` / `lldb_test.sh` | launcher, build, and the two host test harnesses |
| `HOST-TEST-LOG.txt` | captured evidence for the run reported below |

Build: `make` (clang, `-Wall -Wextra -std=c11`, zero warnings).
PS5 payload build line documented in the Makefile
(`--target=x86_64-unknown-freebsd12`; `GDBSTUB_PS5_READONLY` compiles a
write-disabled variant — also warning-free).

## 1. Protocol implementation

Wire layer (GDB RSP, identical for LLDB `gdb-remote`):
- `$<data>#<sum8-hex>` framing; `+`/`-` acks with retransmit (3 tries);
  `QStartNoAckMode` supported, including the subtle final-acked-OK
  transition. Bad checksum in ack mode -> `-` + honor retransmit; in
  no-ack mode -> packet dropped silently (no retransmit exists, and
  processing a corrupt memory-write is unsafe).
- `}` escaping on send and receive; `*` RLE run-length decode on receive
  (GDB really sends RLE-compressed `M` writes — see test).
- Raw `0x03` outside any packet -> `T02` stop reply (Ctrl-C semantics).

Packets: `?` `g` `G` `p` `P` `m` `M` `x` (LLDB binary read) `X` (binary
write) `Z0`/`z0` (software breakpoint = 0xCC patch + restore; `Z1`–`Z4`
correctly answered "unsupported") `c` `s` `vCont?` `vCont;c/s` `H` `qC`
`qAttached` `qSupported` `qHostInfo` `qProcessInfo` `qRegisterInfoNNN`
(LLDB fallback path) `qfThreadInfo`/`qsThreadInfo` (+ old `qfThreads`
alias) `qXfer:features:read:target.xml` `qXfer:threads:read` `qRcmd`
(monitor: `pid <n>`, `info`, `help`) `vAttach;<pid>` `vKill` `D` `k`.
Unknown packets -> empty reply ("unsupported"), which every client
tolerates; that is also how the stub survives LLDB's `jThreadsInfo`,
`qShlibInfoAddr`, `qMemoryRegionInfo` probes (all visible in the
captured trace).

Registers: 57 regs, gdb-standard x86-64 order — 16 GPRs, rip, eflags,
6 segments, st0–7 (80-bit), fctrl..fop, xmm0–15, mxcsr = 536 bytes =
1072 hex chars per `g`. The served `target.xml` is *self-contained*
(no `xi:include`, so it parses standalone in both gdb and lldb) and is
generated from the same `gdb_reg_table` that packs `g`, so layout drift
is impossible. Target identity: `i386:x86-64` /
`x86_64-unknown-freebsd12` (PS5 = Zen 2 x86-64, OrbisOS FreeBSD-derived)
— an arm64 Mac debugger sees a proper cross-arch x86-64 remote.

Phase-1 execution semantics (honest): the live game is never actually
stopped or steered. `?`/`c`/`s`/`vCont` answer with immediate
`T05` stop replies so debuggers stay in a consistent stopped-remote
state while memory/register inspection proceeds. Real stop/step needs
the kernel stop/cont + int3-trap machinery described below. `k`/`vKill`
are translated to detach — the server can never ask the kernel to kill
the game.

## 2. Host test results (captured in HOST-TEST-LOG.txt)

`test_rsp_client.py` — byte-level RSP conformance client (its own
framing/ack/RLE/no-ack implementation, independent of the server code):
**72/72 checks pass**, covering handshake, both checksum-failure modes,
`g`-layout end-to-end (every canned register), `p`/`P`/`G` write paths,
memory read/write incl. RLE-compressed write and binary `X` with
escaped bytes, unmapped-read `E01`, breakpoint patch/restore, vAttach +
monitor re-targeting, thread queries, unsupported-packet behavior, raw
interrupt.

`lldb_test.sh` — real debugger frontend (macOS has no gdb; lldb speaks
the same protocol; gdb equivalent: `gdb -ex "target remote :2345"`):

- attach: stop reply + x86-64 disassembly at rip (`subq $0x20,%rsp...`)
- `target list`: `arch=x86_64-unknown-freebsd12, platform=remote-freebsd`
- `register read rip rax rbx rsp` -> `0x400104`, `0x1122334455667788`,
  `0xdeadbeefcafe0000`, `0x7ffffffe00` (exactly the mock values)
- `memory read 0x400000` -> `7f 45 4c 46 02 01 01 00 ...`;
  `memory read --format s 0x400040` -> `"PS5 UE4SS GDB MOCK v1"`
- `memory write -s 8 0x401000 0x1122334455667788` -> readback matches
  (stub trace shows `M401000,8:8877665544332211` then `m401000,200`)
- `disassemble -s 0x400100` -> correct x86-64 decode of the mock
  prologue; `detach` clean. Stub-side trace of the whole session is in
  the log.

Bugs found and fixed during testing (all before the passing run): Z/z
parse offset; no-ack bad-checksum deadlock (now: silent drop); mock
magic overlapping the ELF header; `qfThreadInfo` spelling; missing `x`
packet. Two were real server defects, the rest were test defects —
noted for honesty; final state is green from a clean rebuild.

## 3. Kernel integration plan (how memory access gets swapped in)

Architecture decision: the TCP/RSP server runs in the **payload process**
(FreeBSD userland — socket stack already exists there, kernel sockets in
a kld are unnecessary), and the kernel module exposes memory/register
primitives through a char device. `gdbstub.c` is untouched by this: all
target access goes through `struct gdb_backend`:

```
GDB/LLDB on Mac ──TCP 2345──> payload gdbstub (RSP core, portable)
                                  │ struct gdb_backend
                                  ▼
                            ps5_backend.c ── ioctl /dev/kstuff0 ──> kstuffSharp
                                                                           │ PayloadKernelIo
                                                                           ▼
                                                                    game process memory
```

1. Memory (`m`/`M`/`X`/`x`): `KIO_READ_MEM`/`KIO_WRITE_MEM` wrap
   PayloadKernelIo's existing arbitrary-process read/write
   (`pfind(pid)` + vmspace-ref + the module's mover, PRELE). Fault
   policy: partial-transfer reporting so the stub answers `E01` instead
   of corrupt data. This is intentionally the same read path/service the
   worker-kernel-saferead daemon uses — when that worker publishes its
   final ioctl ABI, `kio_shim.h` gets re-pointed at it and nothing else
   changes (that worker's directory was empty at time of writing, so the
   shim numbers are provisional and clearly marked).

2. Registers (`g`/`p`): `KIO_GET_REGS` performs stop-snapshot-resume:
   `kern_psignal(p, SIGSTOP)` + bounded wait for `PRS_STOPPED` (timeout
   -> `EBUSY` -> `E01`, never a hung debugger), then copy from
   `td->td_frame` (trapframe = user register save area: GPRs, rip, rsp,
   rflags, cs/ss) and `td->td_pcb->pcb_save` (fxsave/xsave block:
   st0-7, fctrl..fop, xmm0-15, mxcsr — probe FXSR vs XSAVE layout once
   at module init), then SIGCONT. `KIO_SET_REGS` is symmetric (phase 2).

3. Breakpoints (`Z0`): 0xCC patch requires (a) writable text — kernel
   item #3: temporarily map the physical page writable via kernel map
   (pmap_kenter-style), patch, re-protect; (b) an observable int3 trap
   (debug-exception hook) to emit async `T05` stop replies and
   step-over on resume. Until both exist, `set_bp` returns `-1` ->
   `E01`; memory-level debugging is fully usable without breakpoints.

4. Threads: today one synthetic thread. Add `KIO_LIST_THREADS`
   (enumerate `p->p_threads`) and route `H`-selected tids through
   `KIO_*_REGS tid != 0` for the real game thread list.

5. Safety: bind 127.0.0.1 by default (explicit `-a` for console use);
   pid ACL (refuse own pid/pid 0/kernel pids); `GDBSTUB_PS5_READONLY`
   build disables all write paths; `k`/`vKill` = detach, never kill;
   partial-fault reads never fabricate data.

## How to run (host)

```
make                       # build
./gdbstub                  # mock backend on 127.0.0.1:2345
python3 test_rsp_client.py # 72-check conformance suite
./lldb_test.sh             # scripted real-debugger session
```
