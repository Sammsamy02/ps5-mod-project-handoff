# KNOWLEDGE-BASE — Townfall PS5 / UE4SS port. READ THIS FIRST.

Consolidated by `worker-knowledge-consolidation` (read-only synthesis, 2026-10-05) from the
~240 worker dirs of `outputs/2026-10-04-zcode-team/`, the controller outputs, and the
experiment ledger. Purpose: the document a future session reads FIRST instead of
re-deriving. Every entry cites its worker dir and the decisive artifact.

Citation shorthand (same as `worker-project-state-index/INDEX.md`):
- `W/…` = `outputs/2026-10-04-zcode-team/…`
- `C/…` = `outputs/2026-10-04-zcode-controller/…`; `CON:` = `C/console/`
- `L<key>` = ledger entry `zcode_<key>` in `.superpowers/native-observations/transport-experiments-progress.json`
- `SRC:` = composed-runtime sources under `.superpowers/…` as cited by the owning worker
- "in-game proven" = raw console record exists; "derived" = offline from the pinned image, in-game unvalidated

Companion docs to read second: `W/worker-project-state-index/CURRENT-STATE-DIGEST.md`
(3-minute state) and `W/worker-project-state-index/INDEX.md` (per-track tables).
This KB holds the durable facts/methods; those two hold the day's moving state.

---

## DOMAIN A — TARGET FACTS (in-game proven or pinned-image derived)

### A1. Image identity and container (pinned capture)
- Pinned image: `decrypted-eboot.bin`, sha256 `3b6cfad4…622fc`, 194,241,424 B. ELF64 LE,
  `e_type=0xFE10` (ET_SCE_EXEC), EM_X86_64, `e_entry=0x80` (SCE offset-style),
  14 phdrs, `e_phentsize=56`. Section headers UNREADABLE (`e_shoff` ≈1.87 GB > file size;
  `e_shnum=58` is stale — never parse sections; use phdrs only).
  Evidence: `W/worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md` §1.
- PT_LOAD table (page size 16 KiB, all `p_align=0x4000`): L0 `--X` [0x0,0x7479b1c)
  (all code anchors), L1 `R--` [0x747c000,0x7eba918), L3 `RW-` RELRO-covered
  [0x7ebc000,0x913c000), L7 `RW-`+BSS [0x913c000,0x9633418) (GUA BSS), L8 flags=0
  [0x9633420,0xbc602c8) (PT_DYNAMIC). File↔vaddr: `file = p_offset + (vaddr − p_vaddr)`
  (delta 0x4000 for L0/L1). Evidence: same doc §2 (table), byte-verified prologues §3.
- Address semantics: survey addresses are ELF **p_vaddr** values, not file offsets; runtime
  = linked + 0x400000 per segment (below). Linked 0x33ac8c0 decodes mid-instruction (`ud2`);
  linked Tick is 0x2fac8c0. Evidence: SEGMENT-CORRESPONDENCE.md §3;
  `W/worker-seed-table-derivation/REPORT.md` finding 1 (tick convention correction).

### A2. Runtime memory model (exec-only, in-game proven)
- Live exec span `[0x400000,0x787c000)` prot=`0x4` = PROT_EXEC ONLY (SDK `sys/mman.h:52-55`:
  NONE 0/READ 1/WRITE 2/EXEC 4). Reported repeatedly by KERN_PROC_VMMAP (igf2/igf-f4/igf7).
  16 KiB pages (`PAGE_SHIFT 14`). Tail `0x7879b1c..0x787c000` is mapping roundup beyond file
  bytes. Evidence: `W/worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md` §1.
- Data reads AND writes work freely in serviced prot=3 spans (igf4 GUA 96 B read+same-value
  writeback+restore; igf7 candidate reads). A direct data read of an exec-only page is
  crash-correlated (igf6, twice, module-contained death at the FIRST read; game survived).
  Evidence: EXEC-WRITABILITY-EVIDENCE.md §2 table.
- Segment correspondence: single bias **0x400000 now VALIDATED** for L0/L1/L3/L7 (4/4
  immutable data windows matched at base 0x400000 — L`c2_alternative_RESULT`, igf7-3dd949879a0a;
  L7 via igf4 GUA read `0x97ba110 = 0x400000+0x93ba110`).
  Evidence: `W/worker-vtable-swap-design/REPORT.md` §2; `W/worker-c2-alt-correspondence/`.
- RELRO premise (CORRECTED): UE vtables live in RELRO-covered `.data.rel.ro` inside L3, and
  L3 is mapped **READ-ONLY** at runtime: live `[0x82bc000,0x953c000)` prot=1 (igf3 seq 4
  vmmap). "Vtables are writable, swaps need no mprotect" is FALSE; a vtable-slot swap needs
  `mprotect R->RW` on a 16 KiB DATA page (open fact D-1). Heap-tier FuncPtr swaps stand on
  proven prot=3 writes with zero syscalls. Evidence: `W/worker-vtable-swap-design/REPORT.md` §2.

### A3. GUObjectArray and engine identity (rung 1→2 PROMOTED with in-game evidence)
- GUObjectArray = **layout A** at runtime `0x97ba110` (linked 0x93ba110, L7 BSS), 71,959
  objects, read+write proven. 1,565 rip-relative lea refs target it image-wide, 0 mov-imm32.
  Evidence: L`predicate_gc_fix`, L`anchor_validation_RESULTS`; `W/worker-anchor-validation/REPORT.md`;
  offline corroboration `W/worker-seed-table-derivation/REPORT.md` finding 5.
- Axes: SHIPPING, STATS-OFF (244-marker cost model, game evidence; never ask the user).
  Identity: `5.6.1-0+++UE5`, build-id `04bb4330`. Engine version resolvable via settings
  `[EngineVersionOverride] MajorVersion=5 MinorVersion=6`. Evidence: L`worker_axes_detector`,
  L`worker_layout_identity`; `W/worker-required-slots-feasibility/REPORT.md` slot-6 row.
- Kernel: `r229358/releases/13.60 Jul 17 2026` (live g7_fw_summary). Module-list ABI is
  **3-arg** (4-arg EINVAL); fw `0x13600007`. Evidence: CON:c2-alternative-results.jsonl;
  `W/worker-liveprobe/` (L`worker_liveprobe`).

### A4. Kernel interface facts (in-game proven, each earned by a launch)
- `KERN_PROC_VMMAP` sysctl: works in-game (trusted=1 every run; ~10k–22.8k records with a
  save loaded) and is the per-transition verification instrument. Buffer must be bridge-owned
  and large (16,384 entries ≈19 MiB .bss; 1,408 ENOMEMs with a save loaded).
  Evidence: L`composition5_launch2_result`, L`composition5_launch4_result`.
- `KERN_PROC_PATHNAME` returns ENOENT in-game; every observed `kve_path` region is ZEROS —
  no kernel path exists on this kernel. Evidence: L`composition5_launch7_result`,
  L`composition5_launch8_result`.
- DT_DEBUG is never filled in-game (link-map route dead); RTLD_NOLOAD self-retention
  REFUTED in the payload env. Evidence: L`worker_liveprobe`; L`composition5_launch1_result` root cause.
- PS5 RELA model: the file image stores NO absolute pointers in data — 1,661,599 relative
  relocs in `.rela.dyn` (file 0x932F430). Raw vtable scans find nothing for this reason.
  Evidence: `W/worker-gmalloc-derivation/REPORT.md` §2.

### A5. mprotect import situation (decisive for any protection change)
- mprotect is UNAVAILABLE as an import: absent from the frozen 191-row observer contract;
  no `sceKernelMprotect` declaration in pinned SDK v0.43; zero mprotect strings in observed
  libkernel exports; no NID anywhere. `libc.a`'s `mprotect` resolves its weak
  `kernel_mprotect` to **0** in the composed-PRX toolchain (Packager static weak resolution)
  → calling libc mprotect() in-game jumps to address 0. The ONLY mechanism ever demonstrated
  is the raw inline `syscall` (74=SYS_mprotect, FreeBSD numbering) — crash-correlated with
  console+game loss (igf5) and never recorded returning.
  Evidence: `W/worker-import-audit-weakify/AUDIT-RESULTS.md` §2.4/§4;
  `W/worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md` §3 (all four sub-points).
- The PolyHook platform port has NO Prospero branch for MemProtector/MemAccessor/
  near-address allocator (`ue4ss-polyhook-platform-port-1/report.md:20-22`) — hook plumbing
  is unproven independently of the kernel's answer. Evidence: EXEC-WRITABILITY-EVIDENCE.md §5.

### A6. Seed constants (the replacement for scanning; derived offline, per-slot validators)
| slot | semantic | runtime (linked) | status |
|---|---|---|---|
| 0 | GUObjectArray | 0x97ba110 (0x93ba110) | live-validated in-game |
| 2 | FName ctor wchar | 0x63d590 (0x23d590) | derived 3× independently; FC-1 validator built, run pending |
| 3 | GMalloc | 0x970add8 (0x930add8) | derived via class-anchor route; data-side validator V1/V2 designed; armed for launch 10 only |
| 4 | StaticConstructObject_Internal | 0x890aa0 (0x490aa0) | derived (population statistic + expected bytes) |
| 6 | engine version | settings route | `[EngineVersionOverride]` |
| 10 | tick | 0x33ac8c0 (0x2fac8c0) | NOT seeded; vtable fallback unchanged |

Evidence: `W/worker-seed-table-derivation/{SEED-TABLE.json,SEED-INTEGRATION.md,REPORT.md}`;
GMalloc `W/worker-gmalloc-derivation/{GMALLOC-CANDIDATES.json,REPORT.md}`;
arming split launch-9 (3-slot safe) vs launch-10 (4-slot armed): L`launch10_armed_table_ready`,
`W/worker-launch10-prebuild/REPORT.md` (gate 15/15 GO, delta = the 2-line SeedTable swap).

### A7. Import contracts
- Observer/facts lineage: frozen **191-row** UND import contract, exact (all GLOBAL, zero
  weak rows). Composition: **196 imports**, all bind (proven by sdg2 DIAG after the comp-4
  missing-wiring root cause). Composed `PrepareActivatedUE4SSRuntime` is the **4-arg** form;
  the bridge calls the `StartActivatedRuntimeWithAck` one-call helper.
  Evidence: `W/worker-import-audit-weakify/AUDIT-RESULTS.md`; `W/worker-ploader-imports/`;
  L`composition_startup_diagnosis`; L`worker_composition5_gate` critical_catch.

---

## DOMAIN B — METHODS (proven techniques, reuse these)

### B1. VMMAP variable-stride walker
Kernel KERN_PROC_VMMAP records are VARIABLE SIZE; parse per-record `kve_structsize`
(offsets: structsize@0, start@8, end@16, prot@56), never a fixed stride, never `len%stride`
sanity. The probe team's mode-1 per-record walk (`worker-c2-probe/src/vmmap_walk.c`) is
host-tested and in-game proven (igf7); bridge r5 adopted the same contract and walked
22,828 entries clean.
Evidence: L`composition5_launch3_result` (root cause + fix r5 8872cf30);
`W/worker-module-start-wiring/runtime_start_bridge_r12.cpp`; `W/worker-c2-probe/src/vmmap_walk.c`.

### B2. RELA reconstruction (finding data addresses in a RELA-only image)
File image has no absolute pointers → reconstruct `.rela.dyn` (relative relocs), keep
code-valued arrays (21,445), and identify vtables by their contents (e.g. array at linked
0x7ec26e0 = FMallocBinned3 vtable containing 0xb1440). Then: class anchors (UTF-16
"FMallocBinned3" lea xrefs) → ctor (2 lea-to-vtable sites) → factory (in-place global
construction with distinctive address-space constants) → bootstrap store (exactly ONE
image-wide `mov [rip+…],rbx` → 0x930add8) → consumers (11 rip-relative loads, pure virtual
dispatch). "One writer at init, many virtual-dispatch readers" is the classic global
fingerprint. Evidence: `W/worker-gmalloc-derivation/REPORT.md` (scripts `derive*_gmalloc.py`).

### B3. Seed derivation (3 independent successes + the honest-refusal standard)
- GUA: rip-relative lea census (1,565 refs, exact target, cross-checked live).
- FName ctor: 5-anchor module-name-xref cluster with consensus voting (3/4 → 0x23d590),
  byte-for-byte agreement across three independent workers.
- SCO: population statistic where strict intersection is empty (dominant terminal near-call
  in 744/1918 anchor windows; prologue equals another worker's recorded expected bytes).
- Standard: NO derivation → NO seed → gate the slot off so the runtime throws loudly and
  quickly (never guess a constant; never re-rate). Per-slot arming booleans behind
  data-side validators. Evidence: `W/worker-seed-table-derivation/REPORT.md` findings 2-4;
  `W/worker-seed-table-derivation/SEED-INTEGRATION.md`.

### B4. Map-gated data-side reads (the igf7/c2-alt discipline)
Before ANY read: walk the trusted VMMAP, find the covering span, verify prot grants the
access, else write a structured refusal record (`covered_not_readable`, rc=3) and skip.
Expected values computed OFFLINE only (build gate asserts none appear in the PRX).
In-game proven: 4/4 candidate windows read, 1 honest refusal (uncovered A), exec reads = 0,
console survived the whole session. Evidence: L`worker_c2_probe` (host 30/30 strict JSON);
CON:c2-alternative-results.jsonl (igf7 seq4/5/8/9); `W/worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md`.

### B5. Multi-window identity gate (replacing dead header/link-map anchoring)
Identity = data-window anchors at predetermined vaddrs, all igf7-verified immutable windows,
REQUIRED set B+D+E (+C||C2), each checked for mapping coverage AND byte-match; unverified
windows are logged-not-gating (A). VmScan identity_source=2 achieved launch 5.
Evidence: L`composition5_launch5_result`; `W/worker-anchor-validation/REPORT.md` (r6 walk contract).

### B6. Write-through record discipline
Records are persisted BEFORE each novel operation (entry → startup → mapscan → then act),
single-writer per log path. This is why igf5's recovered seq1-3 localize its death to the
first mprotect and why every launch has a root cause. Evidence:
EXEC-WRITABILITY-EVIDENCE.md §2.2; `W/worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md` App. B.

### B7. Fail-open runtime discipline
`module_start` returns 0 on any internal failure after stamping a loud stage code
(`modbridge module_start failed: <stage> code=<n> detail=…` with timestamp+pid prefixes —
entry r4); the game continues playable; runtime-status.txt carries the stage. Honored in
all 8 composition-5 launches. The one exception class: internal init can throw fatally
BEFORE any exec touch only in the safe Shape-A arming (loud missing-gmalloc throw).
Evidence: L`composition5_launch1_result` ("fail-open honored", game playable);
`W/worker-diag-channel-postmortem/REPORT.md` (3-line fail-open marker, host-audited 33/33);
L`worker_launch9_prediction` (arming rule).

### B8. Offline native-ness oracle (R3 prerequisites)
A named UFunction is native iff the image ships an FClassFunctionLinkInfo-style static pair
{UTF-16 name-string pointer, text-range thunk pointer}; string-without-pair = script-only
(refused by the native RegisterHook gate). Turns F-1 ("does ANY carrier exist") into a
bounded in-game question over an image-confirmed candidate list.
Evidence: `W/worker-r3-carrier-prep/CLAIM.json` CCP-2/CCP-3 + `image-oracle-results.json`
(positive/negative controls); `W/worker-r3-carrier-prep/CARRIER-CANDIDATES.md`.

### B9. Ridealong law
ploader.ini entries: mode-0 consumes no slot; modules register at mode 2; MAX_PLUGINS=10
per title; every module gets a SINGLE-WRITER log path (observer sole UE4SS.log reader,
qdrv1 sole ingame-facts.jsonl writer); non-interference audited; pre-image recorded and
preserved byte-for-byte on edit. Evidence: `W/worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md`
+ VERIFICATION-RECEIPTS.json; L`worker_ridealongs_combined`, L`ploader_ini_preimage`.

---

## DOMAIN C — ENGINEERING PATTERNS (the composition/gate/receipt system and its rules)

### C1. Gate + receipt pipeline (fail-closed, deterministic)
- Build twice, require byte-identical D1 determinism (triple for canon: fresh-compose ==
  fixed-reuse == candidate); gate BEFORE upload; readback band check after; receipts are
  sha-pinned and REFUSE stale regeneration (edited sources → re-run regen tools first).
  Gate v1 = 8 checks (W1 chain defined, W2 module_start CALLS the chain [ELF64 .rela.text +
  objdump], W3 exports, I1 imports==196, D1, F1 diag markers, S1 size, P1 packaging) with a
  NEGATIVE CONTROL that fails exactly the known comp-4 defect. Gate v2 adds itemized UND
  delta, configured-roots symbols, substitution-refusal callee set, identity-path guard, D2
  honesty strings; comp6 FINAL = 13/13 GO; launch-10 armed candidate = 15/15 GO.
  Evidence: `W/worker-composition5-gate/` (6 receipts); `W/worker-gate-evolution/gate_check_composition5_v2.py`
  + baseline; L`worker_composition5_gate`, L`worker_gate_evolution`, L`comp6_gate_final_go`,
  L`worker_launch10_prebuild`; `W/worker-comp6-driver/ONE-PASS-CHECKLIST.md` §1 (receipt regen law).
- Change control: change-manifest (baseline sha, changed units, expected EMPTY import delta,
  identity invariants); `--replace-receipt` pins replaced units; ODR sha-equality pre-flight
  between port and staged header copies (strong+strong duplicate = error; Procedure B is the
  only replacement route). Evidence: `C/comp6-integration/change-manifest.json`;
  `W/worker-configured-roots-design/`; L`worker_replace_mechanics`; `C/comp6_one_pass.py`.

### C2. Console safety (standing, earned by incidents)
One console actor (controller); workers never touch the console; one-dispatch rule; ≥30 s
dispatch gaps; timeout = crash-signal (an unresponsive dispatch is a crash until proven
otherwise); no re-dispatch while state is unreconciled; evidence sinks on console are
`/data/...` and on host are owned `outputs/...`, NEVER `/tmp`; no inline syscalls and no
exec-span reads from ploader modules; game-file mutations and launches require explicit
per-session user authorization. Incidents that earned these: argv double-dispatch crash,
igf5 console+game loss, FTP /tmp read refusal. Evidence:
`W/worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md` §0 + §f + Appendix B.

### C3. Ledger + honesty discipline
Every launch gets exactly one `zcode_*` ledger entry (gaps get retroactive entries — see
L`composition5_launch6_result`); raw cumulative lineage logs are the ground truth that
corrects stale checkpoint narratives; claims trace to receipts; spot-check recomputation of
hashes; honesty notes separate leading hypotheses from proven facts (igf6 "hardware-enforced"
downgraded by user directive; igf5 kernel-fault attribution = LEADING, unconfirmed).
Evidence: `W/worker-project-state-index/{INDEX.md,REPORT.md}`; EXEC-WRITABILITY-EVIDENCE.md §2.2.

### C4. Evidence channels and acceptance
Per-rung artifacts pinned to channels: aligned_entry.log (C1), ingame-facts.jsonl (C2),
runtime-status.txt (C3), console capture (C4), mod print() tag; post_launch_check.py
classifies NOT_STARTED→MODS_STARTED with stale-repeat rejection (20/20 host cases);
rung_check.py replays RUNG1/2 from qdrv1 records and stays UNVERIFIED_DRIVER otherwise;
menu runbook CP0–CP7. Evidence: `W/worker-acceptance-evidence/EVIDENCE-SPEC.md`;
`W/worker-post-launch-tools/`; `W/worker-menu-visibility-runbook/`.

### C5. Worker-scope contract
CLAIM.json first, REPORT.md last; proposal-only scopes for most workers; read-only mining
with writes confined to the worker's own directory; no console contact, no nested
subagents; defects caught during verification kept in history, not hidden (worker-virtual-memory
lists 3); receipts are per-run, never reused. Evidence: any worker CLAIM.json (e.g.
`W/worker-exec-writability/CLAIM.json` scope block); `W/worker-virtual-memory/REPORT.md` §defects.

### C6. Compile-time gates over settings keys
Runtime settings keys for gating exec behavior are a REJECTED trap (v3 precedent);
exec-site gating is a compile-time `UE4SS_NO_EXEC_MODE` gate (15 if-constexpr sites, loud
refusals + summary, mechanically applied via 28-anchor script). Evidence:
L`worker_no_exec_gate`; `W/worker-no-exec-mode-gate/`.

### C7. FTP/fself channel laws
ftpsrv auto-decrypts FSELF on RETR: readback = decrypted ELF, not the stored fself —
full-hash equality is impossible through this channel; use header-page + partial-band
readback laws, and treat "corrupt file ⇒ load failure ⇒ no records" as the safe failure
mode. Freshly written console logs need ~15–30 s + a fresh connection (caching delay).
Evidence: L`worker_ingame_probe` staging.verification; INDEX.md §(a) conventions;
L`v3_MAPPING_BREAKTHROUGH` ftp_note.

---

## DOMAIN D — ANTI-PATTERN LEDGER (proved dead ends; do not re-try)

1. **Header/link-map scans on exec-only (identity anchoring via headers).** DEAD: DT_DEBUG
   never filled in-game; the eboot ELF header is not in any readable span; RTLD_NOLOAD
   self-retention refuted; KERN_PROC_PATHNAME ENOENT; kve_path all-zero. Use data-window
   anchors (B5). Evidence: L`composition5_launch1_result`; L`worker_liveprobe`;
   L`composition5_launch7_result`/`launch8_result`.
2. **ps_scan (patternsleuth) on this target.** STRUCTURALLY DEAD, five stacked gates any
   one fatal: G0 text_base never supplied; G1 scan-source ctor demands permits_code over
   readable spans (none executable); G2 Rust view gate demands an EXEC segment at text_base;
   G3 measured — required-slot anchors are 0/N present in code the module cannot read;
   G4 code-result slots require permits_code at admission. A "working ScanGame" can only
   mean "completes without ps_scan" → seeds (A6). Evidence:
   `W/worker-required-slots-feasibility/REPORT.md` + SLOTS-FEASIBILITY.md §1.
3. **Serialized-ELF tick witness route.** DEAD, four independent blockers: B1 parser refuses
   W+X PT_LOADs at eager construction; B2 matches() byte-compares every non-writable LOAD
   against LIVE text (an exec-page read); B3 resolver needs the exec bytes in the file;
   B4 strategic — tick is OPTIONAL (MappedRequiredSlots=0x5d), the timeout is driven by the
   four required code slots, so a perfect witness changes nothing. Evidence:
   `W/worker-tick-witness-design/TICK-WITNESS-DESIGN.md` §0.
4. **Fixed-stride VMMAP parsing.** DEAD: kernel records are variable-size
   (kve_structsize=144; path truncated before copyout); `sizeof(kinfo_vmentry)`=1160 stride
   + len%1160 sanity fails (-4) — cost launches 2-3. Use B1. Evidence:
   L`composition5_launch3_result`; `W/worker-c2-probe/src/vmmap_walk.c`.
5. **PRX-substitution temptation (module substitutes itself for the game exe path).** DEAD
   BY DIRECTIVE AND DESIGN: r10/r11 removed own-module substitution; the PRX never
   fabricates/substitutes `game_executable_path`; configured roots (Resolution B) is the
   honest route; gates B2/B3 now enforce the refusal callee-set and identity-path guard.
   Evidence: L`composition5_launch8_result` resolution_fork; `W/worker-configured-roots-design/`;
   `W/worker-gate-evolution/gate_check_composition5_v2.py`.
6. **Stale receipt classes.** DEAD/REFUSED: an archived r5 gate receipt was stale vs the
   rebuilt dir → fresh-r11 gate pair re-run mandatory; reuse-sha checks can be
   self-consistent against a stale receipt (see 7) — the gate must re-derive, receipts must
   be regenerated after ANY edit (ONE-PASS-CHECKLIST §1), and the one-pass runner refuses
   stale/incomplete receipts. Evidence: L`worker_gate_evolution` side-finding;
   L`fresh_r11_gate_pair`; `W/worker-comp6-driver/ONE-PASS-CHECKLIST.md`.
7. **The reuse-lineage bug.** HAPPENED AND FIXED: comp6 driver `--reuse-composition-run`
   mapped to RUN4 (stale composition-4/run-3 object 45dd9901) instead of RUN6; candidate-2
   linked the WRONG lineage while its reuse-sha check passed against the stale receipt;
   byte-proven (12/12 overlay objects identical, delta entirely from the composed object,
   55 symbols only in RUN4). Fix: d1fix E4 (reuse re-verifies RUN6) + triple-determinism
   proof. Lesson: a reuse path must re-verify its target receipt, and D1 must compare
   composed objects, not overlay objects. Evidence: L`d1_root_cause`;
   `W/worker-d1-determinism-diagnosis/REPORT.md`; L`comp6_gate_final_go`.
8. **Direct exec-page reads from a ploader module.** DEAD (crash-correlated, twice,
   module-contained; mechanism NOT localized — exec-only enforcement is the leading
   hypothesis, user-amended). Evidence: L`igf6_RESULTS`; EXEC-WRITABILITY-EVIDENCE.md §2.
9. **Raw inline syscall-74 mprotect from a ploader module.** DEAD-AS-STAGED: igf5 died at
   the first call, no rc/errno ever recorded, console+game lost; ring-0 fault not trappable
   from ring 3. Only the armed IGW-1 probe (module-owned, write-through records, ladder,
   user-coordinated) may ever re-attempt, once. Evidence: EXEC-WRITABILITY-EVIDENCE.md §2
   igf5 row; §2.1 UNKNOWN list; `W/worker-exec-writability/EXEC-WRITABILITY-PROBE-DESIGN.md`.
10. **Calling libc mprotect() in the composed PRX.** DEAD: weak `kernel_mprotect` resolves
    statically to 0 → jump to address 0. Never call weak-resolved libc wrappers here.
    Evidence: `W/worker-import-audit-weakify/AUDIT-RESULTS.md` §2.4/§4.
11. **GMalloc string-idiom / call-shape routes.** DEAD on this image: 0/14 patterns; both
    UTF-8 anchors absent; `DeleteFile %s` route reaches only the stack-guard slot
    0x9131f08; find_global recursion yields linear-sweep artifacts (bogus 0x9131f38
    rejected and documented). Use the class-anchor structural route (B2).
    Evidence: `W/worker-seed-table-derivation/REPORT.md` finding 2;
    `W/worker-gmalloc-derivation/REPORT.md` honest disclosures.
12. **Settings-key gating of exec behavior.** DEAD (v3 precedent trap): settings keys are
    the wrong layer; compile-time gates only (C6). Evidence: L`worker_no_exec_gate`.
13. **Reading availability globals as "hook armed".** TRAP: `ProcessEventAvailable` /
    `EngineTickAvailable` mean AOB-found, NOT hook-armed. Evidence: L`worker_r3_zsyscall_tier`.
14. **Assuming header 5-arg matches composed 4-arg `PrepareActivatedUE4SSRuntime`.**
    CAUGHT: the ps5-mod-bridge header's 5-arg form is a newer revision that will not link;
    bridge calls `StartActivatedRuntimeWithAck`. Evidence: L`worker_composition5_gate`.
15. **/tmp evidence sinks and full-hash fself readback.** DEAD: FTP refuses /tmp reads
    (results lost); ftpsrv decrypts fself on RETR so full-hash equality is impossible —
    use /data + band-readback laws (C7). Evidence: CONSOLE-SAFETY-PROCEDURE.md §0.4;
    L`worker_ingame_probe`.
16. **Trusting checkpoint summaries over raw logs.** DEAD: "igf5 produced ZERO records" was
    stale (recovered seq1-3); igf6 ran TWICE. Raw cumulative lineage logs are ground truth.
    Evidence: EXEC-WRITABILITY-EVIDENCE.md §2.2.
17. **Assuming a single load bias before per-segment proof.** LESSON (now resolved): the
    single-bias model was initially NOT derivable for L7/L8 and was only adopted after
    in-game validation; always start per-segment, collapse to single bias only on proof.
    (Page-sharing caveat: L7/L8 share page 0x9633000.) Evidence:
    SEGMENT-CORRESPONDENCE.md §5; `W/worker-vtable-swap-design/REPORT.md` §2.

---

## DOMAIN E — OPEN QUESTIONS (each with its owning artifact)

### E1. Writability — can the exec span (or RELRO data) be mprotected? (probe in flight for the return)
- Status: NOTHING is proven about any X→? transition. The ONLY mprotect attempt (igf5, raw
  syscall-74) died with no rc; igf6 direct reads died twice module-contained; errno/max_protection
  behavior on this firmware (`releases/13.60`) is unrecorded — all FreeBSD-generic errno
  predictions are marked speculation.
- Owning artifacts: `W/worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md` (evidence reset,
  §2.1 PROVEN/UNKNOWN), `EXEC-WRITABILITY-PROBE-DESIGN.md` (IGW-1: module-owned, single
  inline syscall site, 12 write-through records, ladder T2 X→RX far page 0x7878000 → T3
  restore → T4 X→RWX + 1 B write/readback at 0x787b800 → T5 restore; budget 8 attempts;
  restore-always), and the ARMED build: L`igw_probe_filled` (igd1-f3921fc2e5d4, prx sha
  886c5883…, 19277 B; `sceKernelMprotect` = the one derived not-console-proven import row,
  loudly flagged). Ridealong per L`worker_exec_writability` decision tree:
  W-works → PLH detours viable (+ port plumbing); W-fails/R-works → vtable-swap dispatch-data
  architecture; both-fail → data-side port only (no code interception).
- Residual risk (explicit, user go/no-go): if raw syscall 74 is ring-0-fatal, the
  console+game crash recurs; design bounds it, cannot exclude it
  (`W/worker-exec-writability/CLAIM.json` EW-8).
- Companion open fact D-1: R→RW on a RELRO DATA page (vtable swaps) — same probe class,
  separate page/prot family. Evidence: `W/worker-vtable-swap-design/REPORT.md` §2.

### E2. R3 carrier (zero-syscall Lua hook routing)
- Status: the tier is "conditionally yes with diffs" — SetFuncPtr is fully in-tree and NOT
  Windows-only; Lua RegisterHook already routes to it on PS5; the Spawner needs exactly ONE
  interception service (ExecuteInGameThread; A10 LoadMap second, pcall-guarded). Open fact
  F-1: which image-confirmed native UFunction serves as the FuncPtr-marshal calibration
  carrier (invocation rate measurable only in-runtime; offline oracle narrowed it to a
  ranked candidate list).
- Owning artifacts: `W/worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md` §4.2/§4.3/§6 (F-1 owner,
  falsifiable calibration protocol; DIFF 1 luamod + DIFF 2 staged initializer mandatory);
  `W/worker-r3-carrier-prep/{CLAIM.json,CARRIER-CANDIDATES.md,image-oracle-results.json}`
  (native-ness oracle + ranked candidates + ridealong census spec); launch-10/11 matrix rows
  R5/R9: `W/worker-launch10-matrix-prep/LAUNCH10-MATRIX.md`.
- Permanently dead on R3 (do not plan around): global ProcessEvent/ProcessInternal
  visibility, shipped script RegisterHook drains, A10 (without the pcall-guarded route).
  Evidence: L`worker_r3_zsyscall_tier`.

### E3. ProcessEvent family (the C8 family)
- Status: design exists as a FAMILY of dispatch-data swaps across per-class UClass vtable
  copies (R4), with a registration-time coverage check required (partial swaps create silent
  per-class holes); per-function UFunction FuncPtr swaps are the zero-syscall tier;
  global ProcessEvent interception is NOT reachable data-side and stays dead unless
  writability returns W-works (PLH) — and PLH still lacks the Prospero platform layer
  regardless. ProcessEvent as a Lua ExecuteInGameThread mode is implemented in-composition
  but has ZERO captured console evidence; EngineTick remains the structural precondition.
- Owning artifacts: `W/worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` (§2 swaps, §4 thunk,
  §5 R1→R4 ranking, §6 honest limits); `W/worker-engine-tick-fallback/TICK-FALLBACK-DECISION.md`
  (EngineTick vs ProcessEvent modes, availability timing); L`worker_r3_zsyscall_tier`;
  `W/worker-all-signatures-inventory/SIGNATURES-INVENTORY.md` (FLAG-1/2/3 consumers).

### E4. Other tracked opens (smaller, so they don't get lost)
- FName ctor runtime verification (FC-1): module built (`W/worker-fname-runtime-probe/`), safe
  data-side validator variant delivered (`W/worker-fc1-validator-probe/`, fc1pool1-78508a2e,
  staged as 7th ini entry); exec-side remains HELD until the exec-span decision (anti-patterns
  8/9). Rule: never bundle ctor with conversion (L`ue4ss_integration_rules`).
- Required slots: ANSWERED structurally dead (Domain D #2); the remaining open is behavioral —
  whether launch 9/10 reach engine-ready/mods-started (predicted: launch 9 loud Shape-A stop;
  launch-10 armed candidate gate 15/15 awaiting console). Owning: L`worker_launch9_prediction`,
  L`worker_launch10_prebuild`, `W/worker-launch10-prebuild/REPORT.md`.
- ODR enforcement runtime proof (design closed, enforced in comp6 pipeline; proof lands with
  launch 9): `W/worker-configured-roots-design/`; `C/comp6_one_pass.py`.
- Console staging of comp6: PENDING (homebrew servers down at close 04:56Z; candidate
  preserved `C/console/comp6-staged-fself.prx`). Evidence: L`comp6_staging_state`,
  L`supplier_composition_6`.

---

## Cross-cutting honesty rules (apply to everything above)
- "In-game proven" requires a raw console record; derived facts stay "identity and ABI
  unverified" until their validator runs in-game; critic scores never replace regression
  tests; missing evidence = unrated, never imagined.
- Nothing in this KB claims engine-ready, mods-started, spawner activation, completion, or
  parity. The runtime has never reached engine-ready; launch 9 (comp6, 3-slot safe) and
  launch 10 (4-slot armed) were staged/armed and awaiting console + user launches at close.
- Sources: `W/worker-project-state-index/CURRENT-STATE-DIGEST.md` (state),
  `W/worker-project-state-index/INDEX.md` (per-track tables), the ledger
  `.superpowers/native-observations/transport-experiments-progress.json` (259 zcode_* keys at
  read time), and the per-claim worker artifacts cited inline.
