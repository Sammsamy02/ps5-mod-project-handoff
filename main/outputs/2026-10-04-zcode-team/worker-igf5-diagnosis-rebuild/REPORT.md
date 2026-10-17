# REPORT — worker-igf5-diagnosis-rebuild (team 2026-10-04)

Deliverables in this directory:

| file | what |
|---|---|
| `CLAIM.json` | scope + claims (written first) |
| `binary-diff-igf5-vs-igf4.json` | PART 1: full binary-diff evidence, field by field |
| `src/` | igf6 sources (11 vendored-vs-igf4 files byte-identical, anchor_pages from igf5, CORRECTED predicate from worker-predicate-gc-fix, new `module_entry.c` variant + `code_anchor_direct.cpp`) |
| `build_igf6_probe.py` / `check_igf6_artifact.py` | PART 2: build + artifact gates (syscall gate INVERTED) |
| `determinism/run1,run2/` | two independent builds, byte-equal; per-run receipts + `artifact-checks.json` |
| `modbridge_ingame_facts.prx` | PART 2 deliverable: **igf6-ad9bda1fc50a**, 24173 B, sha256 `03f6a19c67a8bc955047bc2a10d8833e007e461e90e17024048331817f515a4c` |
| `build-receipt.json` | build provenance (frozen lineage hash, vendored identities, expected-bytes chain, commands) |
| `STAGE-PLAN-IGF6.md` | PART 3: user-coordinated staging + falsifiable predictions + interpretation matrix |

Nothing native was executed; all conclusions are static analysis + local
packaging. The igf6 PRX is PROPOSAL-ONLY until the user-coordinated launch.

## PART 1 — Binary diagnosis (igf5-4186048a6dd6 vs igf4, the last known-good)

Compared OUTER fself PRXs and INNER ELFs (determinism run1 `bare-sdk`) with
pinned LLVM 21.1.8 readelf/objdump + python struct. Answering a–g:

- **(a) Dynamic symbols:** IDENTICAL interface. Same 7 NID-encoded UND
  imports (verbatim frozen-contract rows), zero weak, same 2 exports
  (`BaOKcng8g88` 574 B, `KpDMrPHvt3Q` 128 B) — only their addresses moved
  (0x1d60→0x1e10, 0x1fa0→0x2050). The mprotect shim added NO import.
- **(b) Program headers:** same 13-entry structure, same types/flags/
  0x4000 alignments; only sizes/vaddrs differ (igf5 exec LOAD 0x45e7 vs
  igf4 0x47a7; BSS 0x478080 vs 0x4c00b4). The flags-0 auxiliary LOAD is
  present in BOTH.
- **(c) Relocations:** 23 entries in BOTH (RELACOUNT 10, PLTRELSZ 0), same
  types; only the init/fini-array entries' targets moved. No new reloc types.
- **(d) Sections:** same set (.text/.rodata/.data.rel.ro/.init_array/
  .fini_array/.bss); igf5 .text −448 B (igf4's uncorrected predicate TU
  removed; probe+shim added), .rodata +348 B (strings).
- **(e) init/fini arrays:** exactly 1 entry each in BOTH; DT_INIT/DT_FINI
  are the same exported functions; ctor code is byte-identical vendored
  sources. Nothing igf5-specific runs at load time.
- **(f) The `syscall` instruction (0F 05):** **2 sites in igf5, 0 in igf4.**
  Both in a local (visibility-hidden) anchor-read helper at vaddr 0x28c9 /
  0x2a1b, eax=0x4A=74 (SYS_mprotect parsed at build time from the pinned
  SDK `sys/syscall.h:82`). NOT in a load-path hot path: first execution is
  probe seq 4 — AFTER seq 1–3 records are written.
- **(g) fself wrapper:** same magic/field layout; only 4 size-derived
  fields differ (0x10/0x50/0x58/0x68); wrapper overhead 61 B in both;
  trailers byte-identical (crt1 + libkernel.prx metadata, SDK 0x11000000
  stamps); .sdk.json structure identical; Build ID note differs (content
  digest). No packaging defect.

**Diagnosis.** A load-stage *structural* rejection is EXCLUDED: every
loader-parsed structure is field-wise equivalent (differences are pure
content-size consequences). The syscall shim is the ONLY execution-relevant
delta and the first igf5-only instruction that would ever run. The
zero-record fact is sharper than it looks: `record_sink.c` is write-through
(synchronous `sceKernelWrite`, O_APPEND) and writes seq 1–3 BEFORE the first
mprotect — a plain userspace crash after module_start would have left 3
records on disk. Therefore zero records + console+game crash points to
(i) LEADING: a kernel-level fault from unverified raw syscall 74 on this
kernel (panic loses unsynced page-cache writes, takes down console+game), or
(ii) module_start never ran — unlikely given (a)–(g) but not strictly
excludable on host. Per the task instruction the syscall is NOT declared a
confirmed cause; only the user-coordinated launch of a syscall-free build
can decide (see the interpretation matrix in STAGE-PLAN-IGF6.md).

## PART 2 — Rebuild (igf6-ad9bda1fc50a)

Direct-read probe per spec — plain `*(uint8_t*)` dereference, NO mprotect,
NO syscall, NO permission gate:

- Record order: seq1 entry marker → seq2 startup → seq3 mapscan (context
  only, console-proven sysctl path, never gates) → seq4 **direct read Tick
  16 B @ 0x33ac8c0** → seq5 **direct read SCO 16 B @ 0x890aa0** → seq6 GUA
  predicate replay (CORRECTED worker-predicate-gc-fix predicate, cfgH feed
  over a STACK copy of 96 direct-read bytes — zero live derefs inside the
  predicate; cfgL withheld, recorded `not_run`) → seq7 summary. Expected
  bytes `554889e54157415641554154534881ec` from the igf4 evidence file,
  cross-checked against worker-expected-bytes.
- Packaging recipe = igf4's, exactly: import_references-first combine
  (`ld -r -m elf_x86_64`), already-built Packager.dll IN PLACE,
  frozen `finalize_sdk.py` SDK 0x11000000 stamp, pinned make_fself; same
  module name/path/log path/ini line (same-file replacement of igf5).
- Specific igf5 defect avoided BY CONSTRUCTION: the entire syscall mechanism
  is gone. Build gates assert **0** raw `syscall` instructions in the
  combined object (igf5 asserted ≥1) and **no `0F 05` byte pair** in the
  final inner-ELF `.text`; forbidden identities now include
  `sceKernelMprotect` and `igcar_sys_mprotect`.
- Verification PASS (both runs): fself segment copies, SDK stamp,
  init/fini 1 slot each, exactly 7 NID strong imports / 0 weak, no
  forbidden symbol, expected-bytes provenance chain intact.
- **Two-run determinism: EQUAL** on final PRX, inner ELF, combined object,
  and import contract (`build-receipt.json` → `determinism`).

Safety rationale (recorded in-code): known code addresses inside the mapped
exec span; x86-64 NX gates EXECUTE, not READ; VMMAP prot=0x4 is kernel
policy, not hardware read enforcement; igf4's refusal was the GATE refusing
(policy) — the hardware never faulted. Even in the worst case a fault would
localize by absence (write-through records seq1–3 already on disk), unlike
igf5 where the first novel instruction ran in ring 0.

## PART 3 — Handoff

Stage per `STAGE-PLAN-IGF6.md` (user-coordinated): hash-verify, replace
`/data/plugins/modbridge_ingame_facts.prx`, rename stale JSONL, launch, read
seq1–7. Rollback payload = igf4 PRX (sha256 `4aa624cc…`). Honest bounds:
no in-game facts yet, no promotion claim; igf5's root cause is narrowed to
two host-indistinguishable candidates with a decisive experiment staged.
