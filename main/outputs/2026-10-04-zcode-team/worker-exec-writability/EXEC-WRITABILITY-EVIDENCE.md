# EXEC-WRITABILITY-EVIDENCE — can the in-game module make the game's exec-only text writable (and readable)?

Worker: worker-exec-writability, 2026-10-04. Evidence synthesis only: no console contact,
no execution, no nested subagents. Every claim cites a retained file path (absolute paths
abbreviated: `CON=` = `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/console/`,
`TEAM=` = `.../outputs/2026-10-04-zcode-team/`, `SPIKE=` = `.../.superpowers/native-dependency-spike/`,
`SDK=` = `.../.superpowers/native-sdk-build-check/ps5-payload-sdk/target/include`). Raw console
records are quoted from the cumulative lineage logs; these two files are append-across-runs
accumulations (all igf-lineage builds share one log path), which is exactly why they are the
ground truth that corrects two stale checkpoint summaries (section 2.4).

The question this document answers: the whole port's hook architecture hangs on whether our
in-game module can `mprotect` the game's exec-only span `[0x400000,0x787c000)` (VMMAP prot=0x4)
to add WRITE (and READ). Every PLH detour is a code READ+WRITE
(`SPIKE:ue4ss-native-polyhook-sources-1/port/polyhook/sources/x64Detour.cpp:322-408`), and the
EngineTick + FName-verification hooks arm at init with detours (SIGNATURES-INVENTORY FLAG-1,
`TEAM:worker-all-signatures-inventory/SIGNATURES-INVENTORY.md:206-218`).

---

## 1. The trusted live record of the span (what the kernel itself reported)

1. **The span and its protection, measured in-game.** The igf-lineage vmmap experiments
   (sysctl `KERN_PROC_VMMAP` over the live process) returned, among ~1252 entries, exactly one
   entry covering the game text:
   `{"start":"0x0000000000400000","end":"0x000000000787c000","prot":"0x0000000000000004"}`
   (`CON:igf6-results.jsonl`, build `igf-f4a91d8123b0`, seq 5 `g5_vmmap`). The same span with
   `prot: 4` appears in the earlier paged dump (`CON:igf6-results.jsonl`, build `igf2-764f957dc0a6`,
   seq 4 `v1_vmmap_page` part 0 of 48) and was re-confirmed by igf7's summary:
   `exec_span_start=0x400000, exec_span_end=0x787c000, exec_extent_matches_expected=1,
   exec_reads_attempted=0` (`CON:c2-alternative-results.jsonl`, build `igf7-3dd949879a0a`, seq 9).
2. **prot=0x4 IS PROT_EXEC** in this SDK's numbering: `PROT_NONE 0x00 / PROT_READ 0x01 /
   PROT_WRITE 0x02 / PROT_EXEC 0x04` (`SDK:sys/mman.h:52-55`). So the kernel reports the span
   as EXEC-only: no READ, no WRITE.
3. **Page granularity: 16 KiB.** `PAGE_SHIFT 14` (`SDK:machine/param.h:99`, PS5 branch), parsed
   at build time by the FC-1/igf5 lineage (`TEAM:worker-fname-runtime-probe/REPORT.md` item 8;
   `TEAM:worker-code-anchor-reads/REPORT.md` §3). The span is 16-KiB aligned at both ends
   (`0x400000` and `0x787c000` are both multiples of 0x4000).
4. **The file-image extent inside the span** (static, retained image): LOAD0 `--X` maps
   `[0x400000, 0x7879b1c)` (116.48 MiB), i.e. the live span's tail `0x7879b1c..0x787c000`
   (~9.4 KiB + rounding, parts of the last two 16-KiB pages) is mapping roundup beyond the file
   bytes (`TEAM:worker-required-slots-feasibility/SLOTS-FEASIBILITY.md` §1 table row 0 and §6).
   This matters for probe page selection (PROBE-DESIGN §4).
5. **Known targets inside the span** (all must stay untouched by probe transitions):
   Tick `0x33ac8c0`, SCO `0x890aa0` (igf4 records, `CON:igf6-results.jsonl` seq 4/5; predicted
   cross-check `TEAM:worker-tick-address-validation/REPORT.md`), FName ctor candidate
   `0x63d590` + 9 call sites (`TEAM:worker-fname-runtime-probe/REPORT.md` items 2-3,
   `TEAM:worker-fname-ctor-audit/REPORT.md` §3). All sit far below the span tail.

## 2. The in-game mprotect/attempt history, exhaustively (build by build)

Raw source of truth: the cumulative lineage logs `CON:igf6-results.jsonl` and
`CON:c2-alternative-results.jsonl` (identical prefix; the latter continues to igf7). Build-id
census: `igf-f4a91d8123b0` x7, `igf2-764f957dc0a6` x56, `igf3-de0c0cb644d3` x1035,
`igf4-4891869337b4` x10, `igf5-4186048a6dd6` x3, `igf6-ad9bda1fc50a` x6 (= two launches),
`igf7-3dd949879a0a` x299, `igd1-fc26d6c03038` x2.

| build | mprotect? | what actually happened in-game | record evidence |
|---|---|---|---|
| pre-igf4 lineage (v1/v2/v3, igf2/igf3, igf-f4a91d8123b0) | **No** | mapped the address space (1,035+ mapspan records), measured module bases, prot census; g1 exec-handle probe rc=-1 errno=12 (never an mprotect); **no exec-span byte was ever dereferenced** | `CON:igf6-results.jsonl` g5_vmmap, v1_vmmap_page series |
| igf4 (last known-good) | **No — gate REFUSED before any read** | seq4 `tick_anchor` and seq5 `sco_anchor`: `rc:3, refusal:"covered_not_readable"` — the permission gate saw the covering span prot=0x4 and refused; **nothing was dereferenced; nothing crashed**; GUA read 96 B + same-value writeback + restore in the RW span `[0x970c000,0x9a34000)` prot=0x3 (DATA read+write proven) | `CON:igf6-results.jsonl` igf4 seq4-10 |
| igf5 | **YES — the only attempt ever** | raw inline `syscall` x2 (`0F 05`, eax=74=SYS_mprotect parsed from `SDK:sys/syscall.h:82`), in helper `igcar_sys_mprotect` (`SPIKE:...binary diff`, `TEAM:worker-igf5-diagnosis-rebuild/REPORT.md` Part 1(f)); design: mprotect(page 0x4000, `PROT_READ\|PROT_EXEC`) → read 16 B → restore 0x4 (`TEAM:worker-code-anchor-reads/REPORT.md` §2). **Recovered raw records: seq1 entry, seq2 startup, seq3 mapscan (trusted=1, 8661 readable spans) — then NOTHING.** No seq4 record of any kind: no anchor result, no refusal, no rc, no errno. **Console AND game crashed** (Townfall launch attempt). mprotect rc/errno: **NEVER RECORDED** | `CON:igf6-results.jsonl` igf5 seq1-3; `TEAM:worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md` Appendix B |
| igf6 (rebuild: zero syscall, zero mprotect) | **No** | direct 16-byte deref at 0x33ac8c0 planned as seq4. **Ran TWICE**, each writing exactly seq1-3 (entry/startup/mapscan clean; mapscan totals 10,047 and 10,063; readable_spans 8,685 and 8,705; trusted=1) and dying at/before the first exec-span read. **Module-contained**: game + console survived. Mechanism NOT localized (user amendment 21:17:12Z); "exec-only enforcement" is the LEADING hypothesis, not a proven fact | `CON:c2-alternative-results.jsonl` igf6 x6 (two distinct mapscan buf_lens 0x161370 / 0x161c70); `TEAM:worker-risk-update/RISK-REGISTER-V2.md` R-EXEC-01 |
| igf7 (data-side c2-alt) | **No** | map-gated data reads only: B/C/D/E candidate reads all fine in prot=1 spans, 1 refusal (A uncovered), `exec_reads_attempted=0`, derived base 0x400000 confirmed. Proves the map-gating discipline works and the console survived the whole session | `CON:c2-alternative-results.jsonl` igf7 seq4,5,8,9 |

### 2.1 What is PROVEN vs UNKNOWN

**PROVEN (in-game, raw records):**
- The kernel reports the span `[0x400000,0x787c000)` prot=0x4, repeatedly, across builds (§1).
- Data reads AND writes work in serviced readable/writable memory (igf4 GUA read+writeback,
  igf7 candidate reads) — the module can read/write DATA freely.
- A direct data read of an exec-only page is crash-correlated (igf6, twice, at the exact first
  read; module-contained).
- The raw-syscall mprotect attempt (igf5) is crash-correlated with the harshest outcome
  observed all day (console + game lost), and produced no post-mapscan record.
- The sysctl KERN_PROC_VMMAP mapscan is reliable (trusted=1 every run) and survives as the
  per-transition verification instrument.

**UNKNOWN — the actual question (nothing in the tree answers any of these):**
- Does syscall 74 even RETURN on this kernel? (igf5's zero-post-mapscan records + console loss
  are consistent with a ring-0 fault; the syscall-cause attribution is LEADING, never confirmed
  — `zcode_igf5_diagnosis` + user correction 20:49:22Z in
  `TEAM:worker-comp6-driver/transport-ledger.composition6-mirror.json`).
- X->RX (mprotect add READ keep EXEC): rc? errno?
- X->RWX (add WRITE): rc? errno? EACCES (1)? EPERM (2)? EINVAL? success?
- X->R (drop EXEC): never attempted anywhere, and irrelevant to the port (dropping EXEC on game
  text would break the game; forbidden by design).
- Permission RESTORE behavior (RX/RWX -> back to 0x4): host-tested only
  (`TEAM:worker-fname-runtime-probe/tests` scenario "restore anomaly (read kept, anomaly
  recorded — igf5 semantics)"; `TEAM:worker-fname-runtime-probe/REPORT.md` items 8, 2); never
  exercised in-game.

### 2.2 Corrections to stale checkpoint narratives (important for honesty)

- "igf5 produced ZERO records" (`TEAM:worker-promotion-decision/PROMOTION-DECISION.md` §1.5/1.7,
  `TEAM:worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md` Appendix B) was written before the
  records were recovered. The raw logs now contain igf5 seq1-3. This UPGRADES the igf5
  diagnosis: the write-through sink flushed entry/startup/mapscan BEFORE the first mprotect,
  exactly as designed (`TEAM:worker-igf5-diagnosis-rebuild/REPORT.md` Part 1 diagnosis); the
  recovered records confirm the module ran and died precisely at the novel operation. The
  diagnosis's kernel-fault inference (records only re-appearing after console reboot is
  consistent with lost page-cache writes on panic) remains the leading, unconfirmed hypothesis.
- The igf6 checkpoint entry says "3 of expected 8 records" — the raw log shows SIX igf6 records
  from two separate launches (different mapscan buf_lens/spans). Both launches died at the same
  point. No third attempt is recorded.
- `zcode_igf6_RESULTS.finding` ("PS5 EXEC-ONLY PAGES ARE HARDWARE-ENFORCED") was explicitly
  downgraded by `accurate_recording` (user directive 21:17:12Z): "the exact failure mechanism
  (exec-only hardware enforcement vs other cause) is not isolated. No further broad
  investigation or repeated text-read attempts needed unless a specific remaining fact blocks
  progress." This document is the "specific remaining fact" justification: exec writability is
  precisely such a blocking fact for the whole hook architecture; the probe (PROBE-DESIGN) is
  the single bounded experiment that answers it — per the same directive's carve-out, with user
  coordination.

## 3. Import surface: how could mprotect even be CALLED in-game?

1. **Not in the frozen contract.** The frozen console-proven 191-strong import contract has NO
   mprotect row; `sceKernelMprotect` is on the forbidden-symbol gate of every lineage build
   (`TEAM:worker-code-anchor-reads/REPORT.md` §3;
   `TEAM:worker-import-audit-weakify/AUDIT-RESULTS.md` §2.4: "191 UND imports, all GLOBAL, zero
   weak dynsym rows").
2. **Not declared for in-game use in the pinned SDK v0.43.** Whole-tree grep: no
   sceKernelMprotect declaration; only `kernel_mprotect(pid_t pid, intptr_t addr, size_t size,
   int prot)` / `kernel_set_vmem_protection(pid,...)` in `SDK:ps5/kernel.h:93-95` — these are
   the standalone-payload-scene HOST-side helpers (they act on a target process from outside;
   no NID anywhere maps them for in-process PRX import; `find SDK -name "*nid*"` +
   `grep -i mprotect SDK:ps5/nid.h` = nothing).
3. **Not exported by libkernel.** Observed exports metadata
   (`SPIKE:ue4ss-original-libkernel-exports-1/strict-metadata.json`): zero mprotect strings.
4. **libc mprotect() is a trap in the composed PRX toolchain.** `nm SDK target/lib/libc.a`:
   `T mprotect` with `w kernel_mprotect` — but the Packager resolves weak references STATICALLY
   to 0 instead of emitting dynsym rows (`TEAM:worker-import-audit-weakify/AUDIT-RESULTS.md`
   §2.4, §4), so a libc mprotect() call in the composed module would jump to address 0. The
   igf5 lineage therefore used the inline 3-instruction shim (`igcar_sys_mprotect`,
   `TEAM:worker-code-anchor-reads/REPORT.md` §3) and build-gated `0F 05` counts.
5. **Numbering evidence for syscall 74.** FreeBSD numbering parsed from the pinned SDK
   (`SYS_mprotect 74`, `SDK:sys/syscall.h:82`); console-proven wrappers getpid (20) and sysctl
   (202) work in-game, so low FreeBSD numbers are preserved; 74 sits in the same range but is
   itself UNVERIFIED on this kernel (`TEAM:worker-code-anchor-reads/REPORT.md` §3 residual
   risk). Kernel version string recorded live: `kern_version:
   r229358/releases/13.60 Jul 17 2026 02:16:29` (`CON:c2-alternative-results.jsonl` igf-f4a91d8123b0
   seq 7 g7_fw_summary). No PS5 kernel source is retained in the tree.

## 4. Kernel-side expectations — what can we predict? (marked speculation)

- **RECORD-BACKED:** the ONLY kernel-behavior datapoints are (a) prot=0x4 reported by
  KERN_PROC_VMMAP (§1), and (b) igf6's module-contained death at the first direct read of that
  span (leading hypothesis: the no-read enforcement is real at MMU/kernel level, which
  contradicts the igf6 build-time rationale "x86-64 NX gates EXECUTE, not READ" —
  `TEAM:worker-igf5-diagnosis-rebuild/REPORT.md` Part 2 "Safety rationale", now in doubt per
  R-EXEC-01). If (b)'s leading hypothesis holds, a data WRITE to exec-only text would fault
  identically without a prior successful mprotect — so mprotect is the ONLY candidate mechanism
  (no JIT service, no alternative protection API exists in the SDK: §3).
- **SPECULATION (FreeBSD-generic, no PS5 kernel source retained):** upstream FreeBSD
  `mprotect` returns EACCES when the requested prot exceeds the mapping's max_protection, and
  EPERM/EINVAL for other refusals; a game-text mapping on console is plausibly created with
  max_protection lacking WRITE (self-modifying code excluded), in which case X->RWX fails with
  EACCES while X->RX may succeed if READ is within max_protection. NOTHING in our records
  confirms or denies any of this for this firmware ("releases/13.60"); treat every errno
  prediction as a hypothesis the probe must falsify, not as knowledge.
- **SPECULATION (scope of a success):** PS5 mappings are kernel objects; if X->RX succeeds on
  one 16-KiB page of the span, per-page policy within the same mapping almost certainly
  transfers to sibling pages (same vm_map_entry) — but the probe treats this as inference and
  the port's integrators must re-verify per target page at integration time (PROBE-DESIGN §7).

## 5. The consumers: what exactly needs R and W (from the composed runtime)

- **PLH `x64Detour::hook()`** (`SPIKE:ue4ss-native-polyhook-sources-1/port/polyhook/sources/x64Detour.cpp:322-408`):
  (1) `m_disasm.disassemble(m_fnAddress, …, m_fnAddress + 100)` — **READ** of up to 100 bytes
  at the target (plus `followJmp` re-reads, :331-338); (2) trampoline build in PLH-owned memory
  (`makeTrampoline`, :383-387 — re-encodes copied prologue bytes); (3) `MemoryProtector
  prot(m_fnAddress, m_hookSize, ProtFlag::RWX, *this)` (:398) — **mprotect target page RWX**;
  (4) `writeEncoding(m_hookInsts)` (:399) + nop fill (:407-408) — **WRITE**; (5) protector
  destructor restores old protection. Consumers arming at init: FName-constructor verification
  detour (throw on failure at `SINI:954-958` = staged
  `TEAM:worker-query-s2-arm/staging/UnrealInitializer.cpp`) and EngineTick detour (throw at
  `SINI:1384-1387`), per SIGNATURES-INVENTORY C1/C4/FLAG-1.
- **Non-detour READs that RX alone would unblock:** all ten vtable walks' 16-byte
  jmp-probe (`FLAG-2`, `UnrealVirtualBaseVC.hpp:115,133` +
  `ASMHelper.cpp:9-22,49-69`), ProcessInternal derivation's 164-byte Zydis decode (`FLAG-3`,
  `SINI:1497-1557`), FC-1 validator's mprotect-first 64-byte candidate read + 9 call-site
  windows (`TEAM:worker-fname-runtime-probe/REPORT.md` items 2-3; FC-1 is currently HELD:
  `TEAM:worker-project-state-index/INDEX.md:135` "do NOT launch before exec-span decision").
- **The port plumbing gap regardless of the kernel's answer:** PolyHook's platform layer has NO
  Prospero branch for exactly these operations — `MemProtector.cpp` protection translation
  (Windows/Linux/Apple `#elif` chains only, lines 60-175), `MemAccessor.cpp` old-protection
  retrieval/restore, `Misc.cpp` near-address executable allocator/page-size,
  `PolyHookOs.cpp` (`SPIKE:ue4ss-polyhook-platform-port-1/report.md:20-22`: "This is a missing
  verified contract, not proof that the console cannot offer one"). The igf5-lineage
  restore discipline (current-map sysctl as the old-prot source) is the in-tree precedent for
  implementing the missing lookup.

## 6. Decision tree after the probe (full analysis in PROBE-DESIGN §7; summary here)

- **W-works** (X->RWX rc==0 + write-readback ok + restore ok): PLH detours are viable
  AS DESIGNED at the mechanism level; the remaining work is the platform-port gap (§5), which
  becomes ordinary engineering with a proven primitive. The port runs (hooks included).
- **W-fails-R-works** (X->RX ok, X->RWX refused): detours need a write-alternative. Honest PLH
  assessment: PLH2 has NO no-write hook mode — `hook()` ALWAYS patches the target prologue;
  there is no config to avoid it. Control flow cannot be redirected without writing either
  code or its dispatch data; the viable substitute is writing DISPATCH DATA (vtables, function-
  pointer tables) which lives in serviced WRITABLE memory (proven class: igf4 writeback). That
  covers vtable-dispatched targets (EngineTick C4, ProcessEvent C8, the C5-C9 family) and the
  GNatives table (slot 8) data-side, plus the FC-1 validator replacing the FName detour (C1).
  RX also unblocks FLAG-2/FLAG-3 reads and PLH's disassembly phase for later use.
- **Both-fail** (no R, no W): the hook architecture is DEAD ON THIS TARGET as composed — the
  init-armed detours must be disabled (`HookProcessInternal=false`, `HookProcessLocalScriptFunction=false`,
  FName verifier replaced by FC-1 or gated on seed provenance, EngineTick callback route
  abandoned or replaced by a polling thread — no code interception). What survives data-side:
  all seed constants binding (data addresses), direct CALLS into exec text still EXECUTE (X is
  set — FName ctor use, Conv_NameToString, StaticConstructObject, vtable-target calls like
  ProcessEvent invoked not intercepted), Lua mods, property access, layout inis. What dies:
  intercepting engine/BP execution, engine-tick closure drain (E7) unless replaced by Lua-timer
  or worker-thread polling.
