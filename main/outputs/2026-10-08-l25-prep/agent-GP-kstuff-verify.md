# Agent GP report — kstuff-lite v1.11 source verification (2026-10-10)

Full detail: `outputs/2026-10-08-l25-prep/kstuff-lite-v1.11-VERIFIED.md`.
Analysis only: /tmp extraction, no builds, no console, no network; pinned
artifacts untouched. Both pinned hashes re-verified on this Mac and match
(.elf `ab9a6cb4…` = console pldmgr mirror hash; source zip `b4fcf169…`).

## Headline

The real v1.11 source confirms the kekcall architecture but CORRECTS four
load-bearing claims: CHECK return polarity (inverted in both docs), the
0xdeb7-sysentvec story (compiled out in v1.11), the R4 "0xdeb7 syscall range"
(does not exist), and the aarch64 assumption in PHASEB-PLAN's disasm/skeleton
notes (PS5 payload is x86-64). Lite's serviced kekcall set is larger than the
family list (adds sub 6 OBS-only and sub 7 PPR control; WRMSR reserved).

## Claims CONFIRMED (key ones)

- Gate: syscall `0x27` with sub-number in RAX[63:32]; args a1..a6 in
  RDI,RSI,RDX,RCX,R8,R9 (`ps5-kstuff/main.c:297-308`, `uelf/main.c:65-85`).
- Gadget `dlsym(0x1,"getpid")+7` (`main.c:1334`) — +7 IS the `syscall` insn;
  offset duality resolved (any raw syscall stub works; our CRT needs no dlsym).
- kekcalls READ_DR/WRITE_DR/RDMSR/REMOTE_SYSCALL exist with the documented
  packed forms; REMOTE_SYSCALL ABI confirmed: a1=pid, a2=nr, a3=caller ptr to
  6 qwords, retval copied back to caller (`main.c:547-554`,
  `uelf/kekcall.c:64-76` + trap 2/3).
- Channels: SYS_mdbg_call 573, SYS_dynlib_get_info_ex 608, sysctl KERN_PROC —
  all present and used (`main.c:530-562`).
- crypt XTS+HMAC singletons poisoned with 0xdeb7 → decrypt-on-download
  emulation (`main.c:1677-1678`, `uelf/main.c:173-282`).
- CHECK-guarded re-run no-op; no unload path; per-boot ASLR (kdata_base
  anchor + kmalloc'd fake tables + dynamic PML4 windows).
- #6 input: ZERO pad/input code in the whole tree → GD's DECLINE confirmed.

## Claims CORRECTED (every one)

1. **KEKCALL_CHECK polarity inverted**: resident ⇒ returns **0**; not resident
   ⇒ real getppid ⇒ parent pid (nonzero). Proof: `main.c:1336-1340`
   `if(!kekcall(...CHECK)) { notify("ps5-kstuff is already loaded"); return 1; }`.
   GD's `[KB]` skeleton (`check_result = check != 0`) reads it backwards —
   must be fixed before any build.
2. **No 0xdeb7 in sysentvec (v1.11)**: `USE_INT3_SYSCALL_HOOK=1` compiles out
   the marker writes (`main.c:1673-1676`); instead sv_table pointers are
   swapped to fake sysent tables (global + a ShellCore-only p_sysent swap).
   The per-boot varying etaHEN `native=/compat=` values are these fake tables
   (16 KiB-aligned kmalloc), not stock kernel addresses.
3. **No "0xdeb7-extended syscall range" at all** (PHASEB-PLAN R4 rung): 0xdeb7
   is only a pointer poison tag (top 16 bits) enabling intercept-on-call for
   the crypt singletons. R4 is dead; cross-process reads = REMOTE_SYSCALL only.
4. **Architecture**: payload is x86-64 (PS5 Zen2/FreeBSD), not aarch64 —
   PHASEB-PLAN P3.1's "aarch64" disasm note and the §5 skeleton's ARM64 asm
   are wrong-ISA; the project CRT shim ([SP]) is already correct x86-64.
5. **mdbg_call role**: v1.11 does NOT patch ShellCore via mdbg_call — it uses
   its own kernel-context phys_copyin page-walk; mdbg_call is merely hooked
   (pass-through with fixups) and remains callable by any process (still our
   L3 write channel).
6. **Unknown/unimplemented kekcall sub-numbers do not error**: they silently
   degrade to a real getppid (ENOSYS ⇒ not intercepted, `uelf/main.c:75`) —
   sub-number support cannot be probed by return value.

## Lite v1.11 EXACT kekcall table (`uelf/kekcall.c`)

| sub | packed | name | status |
|---|---|---|---|
| 0 | 0x000000027 | GETPPID | passthrough (real syscall) |
| 1 | 0x100000027 | READ_DR | implemented (a1=ptr 6×u64 out) |
| 2 | 0x200000027 | WRITE_DR | implemented (a1=ptr 6×u64 in) |
| 3 | 0x300000027 | RDMSR | implemented (a1=msr; value in retval) |
| 4 | 0x400000027 | WRMSR | reserved, NOT implemented |
| 5 | 0x500000027 | REMOTE_SYSCALL | implemented (pid, nr, args-ptr) |
| 6 | 0x600000027 | shared-area snapshot | OBS builds only (release: falls to getppid) |
| 7 | 0x700000027 | PPR plaintext control | implemented (magic "PPRPLAIN"; fpkg internals) |
| 0xffffffff | 0xffffffff00000027 | CHECK | implemented (**0 = resident**) |

## #1/#3/#6 mapping (from actual surface)

- **#1 reads**: REMOTE_SYSCALL is the only cross-process primitive (upstream
  itself reads SceShellCore's module_info this way; buffers resolve in caller
  space) + RDMSR/READ_DR diagnostics; no generic kernel-memory read kekcall.
- **#3 patching**: no write kekcall; our channel = SYS_mdbg_call 573
  (confirmed still callable). **i-cache question resolved by ISA**: x86-64 has
  coherent i-cache; zero cache-sync code in kstuff; upstream pattern is
  write→readback-verify→target-side mprotect RX (`main.c:1056-1083`). Data/GOT
  patches need nothing.
- **#6**: nothing in source; DECLINE stands.

## Safety (vs quarantined kstuffSharp)

Lite hardcodes per-FW offset tables (63 firmwares, 1.00–13.60, all
kdata_base-relative) with fail-closed unknown-FW exits BEFORE any kernel write
(`offsets.c:181`, `main.c:1322-1353`) and readback/rollback discipline on
shellcore patches. 13.60 is a first-class table. kstuffSharp crashed because it
had one 10.01 offset set, no table selection, no fail-close. Lite on a future
FW = notify + exit (functionality dies, kernel not crashed).

## Rebuild (prepared, NOT run)

Blockers/nondeterminism, in order: (1) source zip has EMPTY submodule dirs
(BearSSL, isa-l_crypto, libtomcrypt) — exact tag-pinned commits required;
(2) pinned binary embeds build path `/home/echo/drakmor/kstuff-lite/…` and
`.comment` = `Ubuntu clang version 18.1.3 (1ubuntu1)` — our sdk-v0.43's LLVM
version is unreadable from its wrapper-only bin dir and may differ;
(3) SDK static libs (libc/Sce*/sqlite3/crt) version-dependent; (4)
`syscalls-ps5.py` regenerates from web sources — the generated .asm must be
pinned; (5) host gcc/yasm/nasm versions. No timestamps found (no
__DATE__/__TIME__; frankenelf.py deterministic). Commands recorded in the
VERIFIED doc §4.2 (ci-ps5-kstuff-ldr.sh path, per-Makefile flags). Fallback
pin if byte-identity busts: sha256 of `.text` + embedded payload.bin blob +
section layout.

## Console plan delta (R0-R2, 3-restart law unchanged)

R0: expect plain-ELF magic `7f454c46` (documented — this distribution is a
plain x86-64 ELF, not a SELF container); identity via hash; `native=/compat=`
now understood as fake-table addresses. R1: CHECK expects return **0** =
resident (polarity fixed). R2: remote-syscall reads only (avoid subs 1/2/3/6/7);
3 passes before any L3 talk.

— agent GP (ZCode)
