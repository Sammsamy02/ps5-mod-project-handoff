# REPORT — worker-ingame-probe-v2 (team 2026-10-04)

Role: URGENT isolation variant of the in-game probe. v1
(build igf-f4a91d8123b0, sha db8ec550...) was silently load-rejected on
console while the sibling aligned-entry module loaded and logged in the same
launch (checkpoint `zcode_worker_ingame_probe.second_launch_diagnosis`).
Prime suspect — HYPOTHESIS, unproven: the 5 DERIVED import identities
(sceKernelGetModuleList, sceKernelGetModuleInfo,
sceKernelGetExecutableModuleHandle, dlopen, dlclose) are not accepted by the
real console loader despite local SharpProspero resolution. v2 removes every
derived-import call path and carries ONLY the 7 frozen-verbatim,
console-proven identities.

Status: MODULE BUILT AND LOCALLY VERIFIED. NOT RUN IN-GAME. No game launch,
no console contact, no network, no in-game results claimed. The candidate
awaits the user-coordinated launch in STAGE-PLAN-V2.md.

## Deliverables

- `src/` — vendored from `worker-ingame-probe/src/` with labeled changes:
  `import_references.S` (7 rows only), `ingame_abi.h` (derived decls
  removed), `factstore.h` (scan state replaces G1/G3/G4 facts),
  `experiments.{h,c}` (V1..V5), `module_entry.c` (entry/startup markers);
  unchanged vendors: `record_sink.*`, `ingame_libc.*`, `sentinels.c`,
  `boundaries.S`, `layout_bridge.cpp`, `townfall_layout_probes.*`
  (worker-5 origin, unchanged logic).
- `build_ingame.py` — v1 pipeline reused exactly (freestanding prospero-clang
  objects → ld.lld -r with import_references.o FIRST → derived C# Packager on
  frozen SharpProspero → frozen finalize_sdk.py 0x11000000 stamp → pinned
  make_fself), plus a LOUD post-combine gate and igf2- build ids.
- `check_ingame_artifact.py` — v1 checks with 7-strong/0-weak expectations
  and the forbidden-symbol gate.
- `harness/` — vendored; Program.cs `allowedWeak` emptied (dl family gone).
- `build-764f957dc0a6/` — candidate `modbridge_ingame_facts.prx`
  (30,701 bytes, sha256
  `94c10408055c6e6f4d403d1c66f4a8da399fdbae0ef0eb945e1faad16c23b09d`,
  bare-sdk sha `dc867858...4138`), artifact-checks.json, import-contract.json,
  build-receipt.json.
- `determinism/` — two clean rebuilds → identical final PRX hash.
- `STAGE-PLAN-V2.md` — exact controller steps + entry-marker discrimination.
- `CLAIM.json` — claim recorded before any work.

## Import-identity proof table (static evidence)

The v2 import contract contains EXACTLY 7 rows, each copied from the frozen
191-import contract
(`.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json`)
and verified BYTE-IDENTICAL by canonical-JSON equality against the frozen
file both at contract generation and after write (build_ingame.py steps 4a/
4b; any divergence is a loud build failure).

| # | Identity | Row (verbatim: module/soname/library, type) | Frozen evidence source (cited) | Frozen NID (recorded) | Packaged NID base (v2 dynsym) |
|---|----------|---------------------------------------------|-------------------------------|----------------------|-------------------------------|
| 1 | `__error` | libkernel v257 / libkernel.prx / libkernel v1, FUNC/GLOBAL | townfall-declared-imports.json (sha 4ba5f426..., locator /comparisons/2/declared_imports/0, symbol_index 335) + sharp-catalog-coverage.json (sha dc3e6a9..., /mapping/__error/0) | 9BcDykPmo1I#5#J | 9BcDykPmo1I (MATCH) |
| 2 | `sceKernelOpen` | libkernel v257 / libkernel.prx / libkernel v1, FUNC/GLOBAL | modbridge_log_probe.prx (sha c70d26a2..., "SDK-only variant executed and wrote log on console"; wrapper ace9d0a0...) | not recorded in row | 1G3lF1Gg1k8 (⊂ lineage) |
| 3 | `sceKernelWrite` | same row family, FUNC/GLOBAL | same console-proven log-probe evidence | not recorded in row | 4wSze92BhLI (⊂ lineage) |
| 4 | `sceKernelClose` | same row family, FUNC/GLOBAL | same console-proven log-probe evidence | not recorded in row | UK2Tl2DWUns (⊂ lineage) |
| 5 | `sysctl` | libkernel v257 / libkernel.prx / libkernel v1, FUNC/GLOBAL | OBSERVED provider export libkernel.sprx (sha f89a698b..., symbol_index 1464, strict-metadata.json) | DFmMT80xcNI#I#A | DFmMT80xcNI (MATCH) |
| 6 | `getpid` | libkernel v257 / libkernel.prx / libScePosix v1, FUNC/GLOBAL | townfall-declared-imports.json (sha 4ba5f426..., locator /comparisons/64/..., symbol_index 228) + catalog (dc3e6a9...) | HoLVWNanBBc#I#J | HoLVWNanBBc (MATCH) |
| 7 | `clock_gettime` | libkernel v257 / libkernel.prx / libScePosix v1, FUNC/GLOBAL | sharp-catalog-coverage.json (sha dc3e6a9..., /mapping/clock_gettime/0) | not recorded in row | lLMT9vJAck0 (⊂ lineage) |

Corroborating static evidence (llvm-readelf on the packaged bare PRXes):

- v2 dynsym: exactly 7 NID-encoded strong imports
  (`1G3lF1Gg1k8#A#B, 4wSze92BhLI#A#B, 9BcDykPmo1I#A#B, DFmMT80xcNI#A#B,
  HoLVWNanBBc#B#B, UK2Tl2DWUns#A#B, lLMT9vJAck0#B#B`), ZERO weak entries.
- ALL 7 NID bases are a strict subset of the 191 NID bases in the frozen
  lineage PRX that LOADED ON CONSOLE (bare-sdk/modbridge_runtime_aligned_
  entry.prx) — i.e., every identity v2 asks for was already accepted by the
  real loader inside this game.
- v1's dynsym had 12 bases: the same 7 plus EXACTLY 5 extra
  (`IuxnUuXk6Bg, UteVS6B1ZrU, kUpgrXIrz7Q, oiT0vgzmW0M, wJABa1X4+ec`) —
  the 5 derived identities v2 drops. (The `#A#B` suffix is table-local
  module/library ordinal encoding; identity lives in the base, which is why
  3/7 bases match the frozen evidence NIDs exactly.)
- LOUD GATES passed: forbidden symbols (`sceKernelGetModuleList`,
  `sceKernelGetModuleInfo`, `sceKernelGetExecutableModuleHandle`, `dlopen`,
  `dlclose`, `dlsym`, `__dlopen`, `__dlclose`, `__dlsym`, `_DYNAMIC`,
  `__DYNAMIC`) appear NOWHERE in the combined object (llvm-nm) or packaged
  dynsym; Packager fail-closed set-equality enforced strong=={7}; 0 weak.
- Packaging invariants unchanged from v1: DT_INIT=module_start /
  DT_FINI=module_stop explicit, arrays loader-unowned (1 sentinel slot
  each), SDK stamp 0x11000000, make_fself segment equality, frozen lineage
  PRX hash re-verified before build (3b998f09...).
- Determinism: two clean rebuilds → identical final hash
  (94c10408..., 30,701 bytes) and identical bare-sdk hash.

## Controller additions applied

1. ENTRY MARKER FIRST: `module_start` writes+flushes seq=1 `exp:"entry"`
   (fields `phase:"module_start"`, `note:"entry marker precedes all scans"`)
   as the FIRST action after the channel opens — before the VMMAP snapshot,
   before the ctor walk, before ANY pointer read. Envelope exactly as
   specified by the controller.
2. STARTUP RECORD: seq=2 `exp:"startup"` right after the init-array walk —
   `ctors_run`, `channel_open_rc`, `channel_open_errno`,
   `entry_marker_written`, `write_failures_so_far` — separating module entry
   from channel-open and later-experiment failures.
3. HYPOTHESIS DISCIPLINE: "v1 rejected at load due to the derived imports"
   is marked UNPROVEN here and in STAGE-PLAN-V2.md section 5. v2's
   entry-marker evidence structure is what can move it to a conclusion:
   records present → hypothesis strongly supported (only material import
   difference was the 5 derived rows); v2 also silently rejected →
   hypothesis refuted for practical purposes, suspicion moves to other
   build differences (small import table itself, DynamicWriter output at 7
   rows, size/segments, environment state).
4. The discriminating logic (absent file = load/pre-open failure; entry-only
   = load+channel OK, later failure; documented with the ploader FAIL-popup
   caveat) is written out in STAGE-PLAN-V2.md section 4.

## What changed from v1 (isolation rationale)

- DROPPED ENTIRELY: G1 exec-handle + module-info echo, G3 dlopen/dlclose
  NOLOAD (and its 4 weak dl declarations), G4 module-list both-units scan.
  No source path references any of those identities (gate-enforced).
- Base discovery REPLACED by an import-free route: V2 enumerates candidate
  image bases from the sysctl VMMAP span set (RX span starts, plus readable
  spans adjacent to an exec span for split-header images), reads ELF magic +
  program headers IN-MEMORY (every read span-validated BEFORE dereference —
  v1's containment discipline), parses each candidate's PT_DYNAMIC under two
  recorded address hypotheses (mode 1: base+p_vaddr, the v1 G2 spelling;
  mode 2: absolute p_vaddr for ET_EXEC-style images), and chooses the main
  eboot candidate = LARGEST executable image whose dynamic section parses;
  every candidate is recorded with full evidence and the choice carries
  provenance so the controller can overrule offline.
- Records: same marker/schema/record file; NEW build_id prefix `igf2-`;
  complete bounded VMMAP list now emitted (20 entries x ≤48 page records,
  `list_truncated` explicit) instead of v1's first-16 summary.
- Module name, soname, log path, entry contract (force_align_arg_pointer,
  single guard, explicit lifecycle, quiesce inert, no unload) are UNCHANGED
  so the staged file replaces v1's at
  `/data/plugins/modbridge_ingame_facts.prx` with NO ploader.ini change.
- V5 keeps v1's G7 note verbatim: `kernel_get_fw_version` is crt1-provided,
  a PRX links no crt1, so only sysctl KERN_VERSION is recorded.

## Experiments (record order, one+ bounded JSONL records each)

- `entry`(1) / `startup`(2) — controller markers (module_entry.c).
- V1 `v1_vmmap_full` + `v1_vmmap_page` parts — full KERN_PROC_VMMAP walk
  (proven sysctl import): complete bounded list {start,end,prot} + bit
  counts + distinct-prot histogram over ALL kept spans (≤1024; 4 MiB growth
  cap; `spans_truncated`/`list_truncated` flags).
- V2 `v2_elf_base_scan` + `v2_scan_page` parts — candidates + chosen base.
- V3 `v3_dtdebug` — DT_DEBUG presence/value at the chosen base's _DYNAMIC +
  bounded link-map walk (≤64, dedup, span-validated; rc codes: -1 needs-base
  base, -5 dynamic unreadable, -6 no DT_DEBUG, -7 value 0, -8 r_debug
  unreadable).
- V4 `v4_layout_predicates` — vendored worker-5 predicates at chosen_base
  with profile vaddrs (GUObjectArray 0x93ba110, Tick 0x2fac8c0,
  StaticConstructObject 0x490aa0): BOTH GUA layout hypotheses + classifiers,
  verdicts + rcode strings + bytes evidence; needs-base skip ONLY if no
  candidate parsed.
- V5 `v5_fw_summary` — KERN_VERSION verbatim + summary (records_written,
  write_failures, ctor_events, import_profile, dropped_identities, quiesce).

## Bounds honored

- ≤45 tool calls (22 used); no nested subagents; no network; no console
  contact. Existing files immutable: v1 tree, frozen lineage, frozen
  contract all read-only (lineage PRX hash re-verified before each build);
  all writes confined to this worker dir. Credentials untouched. No in-game
  result claimed; honest rc/errno/skip codes throughout.

## Risks / open items

- Load-acceptance of even the 7 proven rows in THIS exact module is the
  experiment's subject — that is the point of the entry marker (STAGE-PLAN
  section 4/5).
- Scan heuristics: images whose ELF header is not at a span start/adjacent
  span start are not enumerated; dynamic mode selection is a scored 2-way
  hypothesis (recorded); the chosen candidate is recorded with all evidence
  for offline overrule. If the eboot is NOT the largest parseable executable
  image, V4 verdicts run at the wrong base — the candidate table lets the
  controller detect and re-target this.
- Bounds: ≤64 recorded candidates (`cands_truncated`), ≤960 listed vmmap
  entries (`list_truncated`), 64-node link-map cap, 2 KiB records with
  `__trunc`.
- If the game's mappings exceed 1024 spans, containment uses the first 1024
  (flagged) — the same bound v1 shipped with.
