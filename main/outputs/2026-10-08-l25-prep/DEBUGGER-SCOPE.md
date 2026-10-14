# DEBUGGER-SCOPE — game-code crash addresses + live-debugging decision

Agent GX (ZCode), 2026-10-08. Answers Claude round-5 review asks (i) and
(iii) on capability #5 ("crash-log symbolization, Mac-tested"); ask (ii)
(symbols-sidecar pairing) is implemented in the same pass — see §1.
Mac-only: local tool edits + docs; no PRX builds, no console contact, no
network. Survey sources are LOCAL trees (cited by path) plus clearly labeled
[TRAINING-KNOWLEDGE] items.

Crash-history context this doc stands on (all previously receipted):
CE-108255-1 family = Sony's generic "game crashed" UI label (no local Sony
meaning table, no minidumps/core files on retail PS5); the real artifacts are
klog kernel crash blocks (signal, registers incl. RIP, fault VA, module list
with `xotext start:end` + ELF build-id fingerprint) plus eboot-monitor
transitions. klog bare "CRASH"/"SIGSEGV" mining is forbidden (STATE §8:
"CRASH KILL_END" is the normal quit path). See PHASEA-PLANS.md §5 survey.

---

## 1. Symbols-sidecar PAIRING (ask ii — DONE this pass, Mac-tested)

`phasea-tools/symbolize_crash.py` now accepts `--symbols-sidecar FILE`, the
packaging-time PRX symbol sidecar designed in PHASEA-PLANS §5 BUILD GAP
(`modbridge_supplier_composition.symbols.json`, final vaddr/size/name per
section/symbol, emitted beside the bare PRX):

- The sidecar embeds `prx_sha256` (64-hex sha256 of the exact bare PRX it
  describes).
- When `--prx` is also given, the tool hashes that file and HARD-REFUSES
  (exit 2, clear message naming both hashes and paths) on mismatch — a
  sidecar from any other build would mis-attribute every address.
- A sidecar with no valid `prx_sha256` is refused equally (unpairable).
- Without `--prx` the run proceeds but the JSON output records
  `pairing: "unverified (--prx not given; cannot hash-check)"` — never
  silently trusted.
- Symbols load as tier `SIDECAR-EXACT` (best PRX tier below an unstripped
  ELF: the Packager knows the final layout it just wrote).
- The GAME sidecar (`game-symbols-townfall.json`) pairs with the game image
  via its recorded `image_sha256_prefix`; the tool has no `--eboot` input so
  that pairing is REPORTED in the output (`game_symbols_sidecar` block), not
  enforced — stated, not hidden.

Tests: suite grew honestly 18 → 22 (`phasea-tools/phasea-test-receipt.json`,
22 passed / 0 failed): `sidecar-pairing-verified` (real l26-package-A bare
PRX + matching fixture), `sidecar-pairing-mismatch-refused` (package-A
sidecar vs package-B PRX = genuine wrong-build pair), `sidecar-unpairable-
refused`, `sidecar-unverified-no-prx`. Fixture
`phasea-tools/prx-symbols-l26-package-A.json` is an honestly-labeled STAND-IN
derived on the Mac from the real package-A PRX dynsym (2 anchors at final
vaddrs) — the Packager change that emits the real per-build sidecar at
packaging time remains the Phase B needs-X (unchanged verdict).

---

## 2. GAME-CODE ADDRESSES (ask iii) — how addresses inside the GAME are handled

### HANDLING (explicit policy)

A crash address that falls inside the GAME's module range — Townfall eboot,
klog xotext `0x400000:0x787c000`, the proven image-base law — is ALWAYS
resolved to:

1. `module = /app0/eboot.bin` (from the crash block's own module list),
   `module_base`, `module_end`, `module_offset = addr - 0x400000`, plus the
   module `fingerprint` (ELF build-id `04bb4330a7d71dcefe1cc2c4d19769d5`,
   which cross-matched the profile's recorded build_id — PHASEA-PLANS §5).
2. A `GAME-SIDECAR` nearest-anchor hit ONLY when one of the cited anchors
   sits at or below the offset. Hits carry `inside_symbol_bounds: null`
   because anchor sizes are unknown — a hit means "nearest known anchor
   below this offset", NOT function attribution. Distances can be tens of
   MB; the tier name (GAME-SIDECAR, never EXACT) and the null bounds flag
   keep that honest.

That is the cap. Without game symbols the tool cannot name arbitrary game
functions, and it says so in the output rather than guessing.

### Why it caps there (verified, not assumed)

- The shipping eboot is STRIPPED: the pinned decrypted view's section-header
  table lies beyond EOF (shoff `0x6f48a0a8` on the 194 MB file — agent-Z11
  §5), so only program headers are usable; its dynsym is import-only.
  No .symtab exists anywhere locally.
- CE-108255-1 carries no minidump/core; the klog block is the whole artifact.
- No external symbol source exists for a retail PS5 UE title: Sony ships
  none, and fetching third-party "symbol dumps" would violate the
  network-free/provenance laws of this pass (and of the project).

### What COULD improve it (all Mac-side; nothing needs external symbols)

- **More anchors, one name at a time.** The profile's function-identification
  machinery already pins engine functions by pattern; every newly pinned
  function (the way the whole PAK mount chain 0x1769a20–0x1771fa0 and the
  FName ctor chain 0x23d590→0x23d600→0x242470 entered
  `game-symbols-townfall.json`) extends coverage. The sidecar's 22–24 cited
  anchors are today's coverage; it grows with RE work, not tooling.
- **Callstack correlation via the profile pins**: klog registers + known
  anchor addresses let a human (or future script) bracket where in the boot
  the fault sits (e.g. "inside the pak-mount neighborhood").
- **Call-edge decoding in the decrypted image** (the agent-Z11 §5 method,
  proven on the real l24 crash): decode the call instruction ENDING at each
  eboot return address in the decrypted bytes; chained verified call edges
  identify true function STARTS exactly — that is how all five eboot frames
  of the l24 abort were resolved to entry+offset with zero symbols. This is
  manual/disassembly-driven today and is deliberately NOT claimed as a tool
  feature (`symbolize_crash.py` does not automate it — documented boundary).
- **Nothing beyond that.** No DWARF/PDB, no symbol server, no richer kernel
  artifact. If a future build of the game ships a different eboot, the
  recorded fingerprint/base law detects it (build-id mismatch) and anchors
  must be re-derived — the same discipline as the profile's sha hard-pin.

Same story, other modules: addresses in libc.prx / libkernel.sprx etc.
resolve to module+offset only (their fingerprints recorded); the project's
own crashes (l24 abort family) had RIP in libc `abort()` and the meaningful
frames in OUR PRX + the game — both sides of that chain are covered by the
tiers above (DYNSYM/MAP/SIDECAR for ours, module+offset+anchors for the
game's).

---

## 3. §survey — LIVE DEBUGGING (ask i): what exists, and the verdict

### prosper0gdb (LOCAL, primary source: `outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`, extracted at /tmp/gp-kstuff-lite-src; read for GP's `kstuff-lite-v1.11-VERIFIED.md` and re-read this pass)

- **What it is**: the kernel R/W bootstrap inside kstuff-lite v1.11 (victim
  `pktopts` pipe primitive + per-FW offset tables, 63 firmwares 1.00–13.60)
  PLUS a kernel-mode GDB remote stub ("r0gdb") inherited from the PS4 kstuff
  lineage. In v1.11's normal flow only the bootstrap runs; the interactive
  kernel-gdb is a dormant facility (`prosper0gdb/main.c`: `dbg_enter();
  return 0; //p r0gdb() for magic`).
- **Transport**: gdbstub over network, yes — `real_dbg_enter` opens a TCP
  socket, binds the first free port scanning upward FROM 1234, listens and
  accepts exactly ONE connection (`gdb_stub/dbg.c:1254-1288`). Protocol
  subset: `?` stop reason, `g`/`G` register read/write, `m`/`M` memory
  read/write, `c` continue, `s` single-step, `k` kill, `H`, `qOffsets`,
  `qXfer:features:read` (target.xml = `i386:x86-64`). Memory goes through
  kernel copyin/copyout with a guard refusing the `0xffffffff00000000..kdata`
  hole.
- **Breakpoint class**: NOT instruction patching. (a) The GDB `Z`
  insert-breakpoint packet is COMPILED OUT in prosper0gdb builds
  (`#define NO_BREAKPOINT_EMULATION` in `prosper0gdb/dbg.c` removes `Z` from
  the command table). Upstream's "emulation" records up to 64 addresses and
  single-steps EVERY instruction via the x86 trap flag, comparing RIP per
  trap (`set_breakpoint`/`have_breakpoint` + the TF logic in
  `gdbstub_main_loop`) — unusably slow, hence disabled. (b) HARDWARE
  breakpoints exist as the debug-register class: `r0gdb_read_dbregs`/
  `write_dbregs` (DR0–DR7) + RDMSR/WRMSR via the kernel-side API
  (`r0gdb.h:44-52`) — but driving them needs a custom client making
  kernel-side calls. (c) int1/int3 IDT/IST hooks (the KELF) are the trap
  channels — the same hooks PHASEB-PLAN §3.3 cited when declining the kernel
  input route ("IDT hooks = debug-register intercepts").
- **Attach semantics**: KERNEL-context stub — attaching freezes the whole
  kernel (every game/process), one connection, NO UNLOAD PATH (GP verified
  P8: reboot is the only reset). It is not a game user-mode debugger; no
  ptrace-class user debugger exists anywhere in the local trees.

### Other community tools in local records

- etaHEN (resident): bootloader/environment layer, not a debugger; its logs
  are already consumed as evidence.
- kstuffSharp: QUARANTINED (AGENTS.md) — crashed the 13.60 kernel on Oct 6;
  the standing counterexample to casual kernel work.
- PHASEB-PLAN: contains no debugger rung (R0–R4 is a READ ladder via
  kekcall REMOTE_SYSCALL); §3.3 records the only debugger-adjacent decision
  (kernel input route DECLINED).
- [TRAINING-KNOWLEDGE] Wider PS4/PS5 homebrew scene: live debugging is
  dominated by exactly this r0gdb-style kernel gdbstub (sleirsgoevy
  lineage; "prosper0gdb" = Prospero, the PS5 OS codename) plus log/printf
  forensics; user-mode attach à la devkit IDA/gdb sessions is not available
  to the scene, and retail titles have no symbol infrastructure. Treat as
  background, not locally verified fact.

### VERDICT: DEFERRED — live debugging is neither planned as a Phase B rung nor permanently dropped

Reasoning, tied to what the modding workflow actually needed so far:

1. **Necessity is zero to date.** Every crash this project debugged was
   solved from logs + receipts + Mac-side static work: the l24 CE-108255-1
   abort family (agent-Z11 call-graph symbolization from the klog block +
   `combined-supplier.o`, no debugger involved), the 20:58 "ghost crash"
   (a module-list check — now automated in this very tool — killed the
   misattribution chase), the FName GC-storm (log-count deltas + the CB
   static decode), and the Oct-6 kernel crash (postmortem from preserved
   klog + POSTMORTEM.md). Symbolization + receipts have covered 100% of
   actual incidents.
2. **Risk profile is the project's worst class.** Kernel-stop debugging on
   the single 13.60 console, one connection, no unload: one wrong register/
   memory write is an Oct-6-class session outage (all games fail auth until
   reboot). We already learned this lesson at kernel-table confidence once.
3. **The safe overlap already exists.** What live inspection would mainly
   buy — peek memory / read state of a running target without stopping the
   world — is exactly the Phase B kekcall READ ladder (REMOTE_SYSCALL
   targeted reads, fail-closed FW tables, R0–R2 designed and reviewed).
   That route never freezes the kernel.
4. **The headline feature is disabled anyway.** Software breakpoints are
   compiled out (NO_BREAKPOINT_EMULATION); hardware-DR breakpoints require
   custom kernel-side client code — new kernel-resident work, L4-prohibited
   this phase.

**Revisit trigger (explicit, recorded)**: if, after the Packager's exact
symbols sidecar lands, a crash remains unattributable from logs +
symbolization; OR the GATE-1 copyin investigation stalls on a question only
step-through can answer; AND the user approves a kernel-debug window with a
reboot-recovery drill — then a Claude-reviewed session against the pinned
stock kstuff-lite's dormant `r0gdb()` facility is the documented route
(zero new kernel code; hardware-DR class only). Until any of that happens:
crash-log symbolization + targeted reads are the debugger of record for
this project.

---

## 4. Summary table

| Claude ask | Status |
|---|---|
| (i) live debugging planned/dropped? | **DEFERRED** with recorded revisit trigger (§3); survey of prosper0gdb + local records included |
| (ii) sidecar carries PRX sha, tool refuses mismatched pairs | **DONE** — `--symbols-sidecar` + `--prx` hash pairing, hard refuse; 4 new checks, 22/22 (§1) |
| (iii) game-code address handling | **DOCUMENTED** — module+offset always; sparse cited anchors only; honest cap + improvement paths (§2) |
