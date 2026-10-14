# RECEIPT — worker-ingame-probe-v3 (resume) — modbridge_ingame_facts.prx

- Worker: worker-ingame-probe-v3-resume (ZCode team, PS5 mod-manager, fork session)
- Date: 2026-10-04
- Build ID: `igf3-de0c0cb644d3` (prefix `igf3-` per amendment; FROZEN delivery)
- Variant: v3-writer-fix-vmmap-complete-base-hunt
- Module name / log path: UNCHANGED from v2 lineage —
  `modbridge_ingame_facts` / `/data/modbridge-ingame-facts.jsonl`
  (same-file replacement of v2 `igf2-764f957dc0a6`, no ini change)
- Entry marker: seq=1 `exp="entry"` written and flushed FIRST, before any scan
  (module_entry.c; startup record seq=2 separates init-array from later failure)
- Native execution: NONE. In-game results: NONE. Local packaging + host checks only.

## Artifact

| Artifact | Bytes | SHA-256 |
|---|---|---|
| `modbridge_ingame_facts.prx` (fself, deliverable + `determinism/run1/`, `run2/`) | 53165 | `2555b946e73b38f1744a036770103916bffb3b7397e0f7e9ef9dd101e448cd75` |
| inner ELF (`bare-sdk/modbridge_ingame_facts.prx`, SDK 0x11000000) | 109776 | `c52e582d95dff8665911355636049f848177953101009a7da6b48923597e5637` |
| `combined-ingame.o` | — | `f1fd06c09343bd0f77daeddaa53ed8ffd4d233da93ab77bf11fbac983dd9787d` |
| `import-contract.json` | — | `1591387c0809dae3c8066e3b4b952dc8a3828a7fb1cb6ca763eb89bc3bbd1fef` |

Frozen lineage provenance: `.superpowers/native-dependency-spike/
native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx`
sha256 `3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654`
re-verified this session before the build (build-receipt `hash_verified: true`).

## Import-identity proof table (7 rows, byte-identical to the frozen contract)

Source contract: `import-contract.json` (derived Packager); every row below is
COPIED VERBATIM from `.superpowers/native-dependency-spike/
native-full-runtime-aligned-entry-1/import-contract.json` (the console-proven
frozen 191-import contract). Proof method: `json.dumps(row, sort_keys=True)`
string equality against the frozen row, asserted pre-write AND re-verified on
read-back by `build_ingame.py` (LOUD failure on any divergence), and
re-verified independently by this worker post-build.

| # | Name | Library | Module (ver) | Soname | Row proof |
|---|------|---------|--------------|--------|-----------|
| 1 | `__error` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 2 | `sceKernelOpen` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 3 | `sceKernelWrite` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 4 | `sceKernelClose` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 5 | `sysctl` | libkernel v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 6 | `getpid` | libScePosix v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |
| 7 | `clock_gettime` | libScePosix v1 | libkernel (257) | libkernel.prx | BYTE-IDENTICAL |

Packaged-dynsym evidence (`check_ingame_artifact.py`, LOCAL_PACKAGE_CHECKS_PASSED):
exactly 7 GLOBAL dynsym imports, 0 weak, every name NID-encoded:
`1G3lF1Gg1k8#A#B, 4wSze92BhLI#A#B, 9BcDykPmo1I#A#B, DFmMT80xcNI#A#B,
HoLVWNanBBc#B#B, UK2Tl2DWUns#A#B, lLMT9vJAck0#B#B` (set-level identity
asserted by count + encoding gate; no friendly forbidden name anywhere).
Dropped identities (v2 isolation, unchanged): sceKernelGetModuleList/-Info,
GetExecutableModuleHandle, dlopen/dlclose/dlsym/__dlopen/__dlclose/__dlsym.
Forbidden-symbol gate over the ENTIRE combined object: PASSED.

## Local checks performed (all green)

1. `tests/run_host_checks.py` — HOST-CHECKS-PASSED. Compiles the REAL
   `src/record_sink.c` + `src/vmmap_walk.c`. Writer: 5/5 lines emitted, all 4
   non-truncated lines `json.loads`-valid (the v1/v2 missing-comma bug is
   fixed), line 4 flagged `__trunc":1` as designed; 36-bit hex round-trips
   (incl. array `end` 0x0000000500000090), INT64_MIN, empty arrays, nested
   objects, escaping all exact. Walk: mode-1 parses 9 synthetic
   variable-kve_structsize records exactly (kept=8, 36-bit round-trip,
   ksz_144 counter), insane structsize=0 terminator stops with stop_reason=1,
   fixed-stride mode-2 fallback flagged stop_reason=2 (partial record),
   trust gate passes at 8 sane/1 insane and refuses 400%-insane and <8-sane.
2. `build_ingame.py` — packaging pipeline reused EXACTLY from v2 (frozen
   recipe): import_references-first combine, SharpProspero Packager, SDK
   0x11000000 stamp via frozen finalize_sdk.py, pinned make_fself.
3. `check_ingame_artifact.py` — LOCAL_PACKAGE_CHECKS_PASSED: fself header +
   segment bytes equal to inner ELF, sha-of-bare in fself info, init/fini
   slots == 1 each, explicit DT_INIT/DT_FINI, arrays not loader-owned,
   SDK stamp 0x11000000, 7-encoded-import gate.
4. Determinism double-build: `determinism/run1/` vs `run2/` (FROZEN delivery
   source set) — final fself, inner ELF, combined object, and contract all
   BYTE-IDENTICAL. Superseded pre-depth builds (igf3-5e5b6fcf83cd /
   1dfbf3d5…) were removed from determinism/ to keep exactly the frozen
   pair; their hashes remain recorded in this file's git-less history only
   via this note.

## Not claimed

No console run, no in-game facts, no L7 base discovery, no layout promotion.
Everything above is local host evidence only. Staging/launch = STAGE-PLAN-V3.md,
user-coordinated. Candidate != validated per the standing honesty rules.
