# STAGE-PLAN — igf7-c2-data-correspondence (worker-c2-probe, 2026-10-04)

STATUS: PROPOSAL-ONLY. Nothing here has executed on a console. The game
launch is USER-COORDINATED, never automated. This plan stages ONE launch of
the C2(b) ALTERNATIVE correspondence probe per
`worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md` section 7.

## 1. What is being staged

- Deliverable: `modbridge_ingame_facts.prx`, build **igf7-3dd949879a0a**
- sha256: `9d92ac4892e3111147c4eeb14a9498dce44844ced9a90dda892e9f54d2b2ee12`
- size: 19389 bytes (outer fself), inner ELF 69328 bytes
- path on console: `/data/plugins/modbridge_ingame_facts.prx` (SAME path,
  SAME ini line — same-file replacement of igf6-ad9bda1fc50a; no ini edit)
- log path (UNCHANGED): `/data/modbridge-ingame-facts.jsonl`
- marker: `modbridge.ingamefacts/v4 build=igf7-3dd949879a0a`

Determinism: two independent builds (run1/run2) byte-equal on final PRX,
inner ELF, combined object, and import contract (`build-receipt.json`).

## 2. What changed vs igf6 (and why this is the C2(b) substitute, not a replay)

| mechanism | igf4 (loaded, logged) | igf6 (3 records, then crash) | igf7 (this build) |
|---|---|---|---|
| foreign reads | gate REFUSED exec anchors (survived) | UNGATED exec-span 16 B deref — died on record 4, first exec read | map-GATED DATA-window reads ONLY (8/32/19/64/36 B); exec spans never dereferenced |
| gate | same-invocation VMMAP, prot&1 | none (deliberately) | same-invocation VMMAP, prot&1 + explicit derived-exec-span intersection refusal + untrusted-map refusal of everything |
| map persistence | counts only | counts only — 8685 readable spans NOT saved | readable span list PERSISTED as paged `mapspans` records BEFORE the first data read |
| expected bytes | embedded on-console (`expected_hex` + `byte_match`) | embedded on-console | NONE in the binary (build gate asserts raw+ASCII-hex absence); offline comparison only |
| candidates | Tick/SCO code anchors | Tick/SCO code anchors + GUA 96 B | A (build-id note), B (L"UE5"), C (BuildIdOverride UTF-16), D (Current Changelist ASCII), E (AES S-box 64 B); read order B,C,D,E,A — A last |
| base/geometry | assumed from checkpoint | assumed | DERIVED from the same map (lowest prot&4 span start; exec extent) and recorded as cross-checks |
| raw syscalls | 0 | 0 | 0 (asserted: 0 `syscall` instrs, no 0F 05 in final .text) |
| imports | 7 verbatim NID rows | same 7 | same 7 (dynsym: exactly 7 NID-encoded, 0 weak) |

Rationale: C2(a) direct code-anchor reads are DEAD on this kernel (igf6
fact). The substitute checks file-backed DATA bytes in phdr 1 (rodata) and
phdr 8 (flags-0 aux) — segments with ZERO relocation targets (spec §3), so
`live == file` is the expected invariant. Every candidate byte window is
unique in the 194,241,424-byte retained capture (C's text twice, but the
fixed-address comparison is unaffected — spec §5 footnote).

## 3. Host verification already done (no console)

- `check_igf7_artifact.py` PASS (both runs): fself wrapper/segment copies,
  SDK 0x11000000 stamp, init/fini 1 slot each, exactly 7 NID strong imports,
  0 weak, no forbidden identity (incl. mprotect shims), ZERO `syscall`
  instructions, NO `0F 05` in inner .text, and NO expected byte window
  (raw or ASCII-hex) anywhere in inner ELF or wrapped PRX.
  See `determinism/run*/artifact-checks.json`.
- Host logic test (`tests/run_host_test.sh`) PASS: real `vmmap_walk` (144-B
  stride), real `ig_map_cover` gating, real record sink, three modes —
  trusted (5 gated reads, `live_hex` == offline expected for B,C,D,E,A,
  readable spans persisted, base 0x400000 + exec extent 0x747c000 derived),
  untrusted (5 `map_untrusted` refusals, zero derefs), covered_not_readable
  (B–E refusals, A still read). 30/30 records valid JSON; no
  `byte_match`/`expected*` keys anywhere.

## 4. Staging steps (controller, user-coordinated)

1. Verify the staged file hash BEFORE replacing the console-side PRX
   (`shasum -a 256` must equal `9d92ac48…b2ee12`; refuse divergent content).
2. Copy `modbridge_ingame_facts.prx` (igf7-3dd949879a0a) to
   `/data/plugins/modbridge_ingame_facts.prx` — replacing the igf6 payload
   (igf6 must NOT remain staged anywhere).
3. Truncate/rename any stale `/data/modbridge-ingame-facts.jsonl` BEFORE the
   launch (recommend rename, keep provenance).
4. Launch Townfall via ploader with the plugin ini line UNCHANGED.
5. The plugin is INERT after one synchronous module_start pass (records
   seq1..9 + mapspan pages, channel closed, returns 0). No unload, no
   threads, no writes to game memory.

## 5. Expected evidence (falsifiable predictions)

- seq1 `entry`, seq2 `startup`, seq3 `mapscan` (with `derived_base`,
  `base_matches_expected`, `exec_span_*`, `exec_extent_matches_expected`,
  `trusted`), then `mapspans` pages (readable span list; expect thousands —
  igf6 counted 8685), THEN seq4..8.
- seq4..8 in order B, C, D, E, A; each either `cand_read` (fields:
  `rt_addr`,`vaddr`,`image_file_off`,`len`,`span_start`,`span_end`,
  `span_prot`,`method_used="gated_map_covered_data_read"`,`live_hex`) or
  `cand_refusal` (`refusal` in {`map_untrusted`,`uncovered`,
  `covered_not_readable`,`exec_span_protected`}, NO bytes). No
  `byte_match`, no expected bytes, anywhere.
- seq9 `summary`: per-candidate `{B,C,D,E,A}_{covered,read,refusal}`,
  `reads_performed`, `refusals`, `exec_reads_attempted=0`,
  `comparison_on_console=0`, `write_failures`.
- NO crash is expected: zero syscall sites; the only foreign dereferences
  are five bounded (<=64 B) reads, each gated on a same-invocation readable,
  non-exec covering span. Residual risk (spec §7): no saved evidence shows a
  successful deref of a prot-1 or flags-0 aux span; if the kernel faults on
  prot-1 reads despite the map, the crash localizes by record absence with
  the map evidence ALREADY PERSISTED (mapspans pages precede seq4).

## 6. Offline comparison + interpretation matrix (controller)

Offline (after re-verifying capture sha256
`3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`): compare
each `live_hex` against `worker-c2-probe/EXPECTED-VALUES-OFFLINE.json`
(bytes re-read independently from the retained capture at phdr-derived file
offsets); verify `rt_addr - vaddr == recorded base == 0x400000` and each
`cand_read` span shows `prot & 1`.

| outcome | meaning |
|---|---|
| 5/5 `cand_read`, all `live_hex` == file bytes, base 0x400000, spans prot&1 | PASS (spec §9 full) — recommend controller acceptance |
| A and B match + >=2 of {C,D,E} match + zero mismatches (refusals justified by persisted mapspans) | bounded PASS (spec §9 minimum) |
| B matches WITHOUT A | NOT a pass (A is the only link-identity carrier) |
| any `live_hex` != file bytes | FAIL — contradicting evidence; promotion stops |
| recorded base != 0x400000 | FAIL-equivalent |
| records stop at seq4 (mapspans present, no crash news elsewhere) | died at FIRST DATA read — NEW fact class (not an igf6 replay): re-arbitrate before any reuse of this design |
| ZERO records | staging/channel issue — recheck ini/log path (igf6/igf4 lineage says the binary loads) |
| Tick/SCO | UNRESOLVED regardless — this probe never reads them; C2(b) acceptance remains the controller's explicit act (spec §9) |

## 7. Abort / rollback

- Any crash or zero-record outcome: STOP, capture the console state, do NOT
  re-launch without controller reconciliation (never blind replay).
- Rollback payload: igf4 probe (known-good,
  `worker-anchor-validation/modbridge_ingame_facts.prx`,
  sha256 4aa624cc…) restored to the same path.
- igf7 remains proposal-only; no promotion claim is made from host checks
  alone.
