# BINARY-DELTA — rebuilt row-0-confirmed probe vs launch-9 probe (2026-10-05)

Reference pair (ARMED deliverables, same size 20013 B):

- OLD = launch-9 `igd1-6ebab07f2111`, fself `48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e`,
  inner `cfed5e3d7e62100649fe1221cd4eaaf7554df421f08792264ae5238acb6d0830` (worker-writability-probe-build/determinism/run-armed-1)
- NEW = `igd1-e030a09eb4d1`, fself `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df`,
  inner `cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9` (this worker, determinism/run-armed-1)

## 1. Inner ELF (69464 → 69464 B): 727 differing bytes in 48 runs — all classified

| class | bytes | where | what |
|---|---|---|---|
| **THE design change** | **1** | `0xc0c0` | row 0 `confirmed` u64 LSB `0x00 → 0x01` (table starts at `0xc0a0`; field at +32). THE ONLY semantic data change in the binary. |
| build-id string | ~18 | `.rodata` pool | `igd1-6ebab07f2111` → `igd1-e030a09eb4d1` (same 17 chars + NUL; content-identical length) |
| string-pool re-layout | 676 | `0x83c3..0x8667` region | the id swap re-sorted the const-string pool: 4 strings shifted **−18 B** (`IGW-1 T4:` `0x84ed→0x84db`, `IGW-1 T5:` `0x85ab→0x8599`, `EMPTY placeholder…` `0x855f→0x854d`, plus the id itself moving to a later pool slot). ZERO string contents changed — every other string byte-identical (`IGW-1 T2/T3`, `D-1` notes, `/data/modbridge-exec-write-probe.jsonl`, `/data/plugins/UE4SS.log`, `modbridge.execwriteprobe`, the entry note all at identical offsets). |
| pointer/reloc updates | ~46 | `.text` displacements, `.data.rel.ro` slots `0xc128/0xc158`, dynamic relocs `0x102f8/0x10310` | RIP-relative LEA displacements and relocation addends updated to point at the 4 shifted strings (−18) — same logical targets, e.g. `0x44ed→0x44db`, `0x45ab→0x4599`. No instruction opcodes/semantics changed. |
| build-id note digests | 24 | `0x10610` (GNU, 16 B) + `0x108f0` (SIE, 8 B) | content hashes over the changed ELF — expected churn |

Corroborations in the same diff: the `mov edx,8` (attempt budget 8) sites appear in
BOTH contexts unchanged (3 occurrences in each binary); the `from_prot/to_prot/page_count`
scalars of ALL six rows are byte-identical; the entry-record note ("no write before the
design's confirmed far page") is byte-identical.

## 2. Per-row readback of the NEW binary's const table (at 0xc0a0, 48 B/row)

| row | addr | from | to | pages | confirmed | note_ptr | vs launch-9 |
|---|---|---|---|---|---|---|---|
| 0 (IGW-1 T2) | 0x7878000 | 4 | 5 | 1 | **1** | 0x4209 | confirmed field 0→1 (the change) |
| 1 (IGW-1 T3) | 0x7878000 | 5 | 4 | 1 | **0** | 0x4356 | 48 bytes fully identical |
| 2 (IGW-1 T4) | 0x7878000 | 4 | 7 | 1 | **0** | 0x44db | identical except relocated note_ptr |
| 3 (IGW-1 T5) | 0x7878000 | 7 | 4 | 1 | **0** | 0x4599 | identical except relocated note_ptr |
| 4 (D-1 R→RW) | 0x82bc000 | 1 | 3 | 1 | **0** | 0x4146 | 48 bytes fully identical |
| 5 (D-1 RW→R) | 0x82bc000 | 3 | 1 | 1 | **0** | 0x42e4 | 48 bytes fully identical |

Rows 1-5 remain `confirmed=0`: they still record-and-skip with `outcome=2`
(`XWP_SKIP_NOT_CONFIRMED`), gating on the T2 verdict exactly as the launch-9 probe did.

## 3. Text-level proof (generated sources, armed run dirs)

```
--- generated/xwp_transitions.c (launch-9)      +++ generated/xwp_transitions.c (new)
5c5                                             5c5
<     {0x7878000ull, 0x4ull, 0x5ull, 1u, 0u,    >     {0x7878000ull, 0x4ull, 0x5ull, 1u, 1u,
--- generated/ingame_config.h
< #define IG_BUILD_ID "igd1-6ebab07f2111"       > #define IG_BUILD_ID "igd1-e030a09eb4d1"
```

Nothing else differs in either generated file.

## 4. FSELF wrapper (20013 → 20013 B): 751 differing bytes, fully derived

The wrapper differs ONLY as a function of the inner-ELF change:
- `0x4e0..0x4ff`: the 32-byte sha256 binding of the inner ELF (`cfed5e3d…` → `cc544d6e…`);
- the embedded inner-ELF segment copies (propagating the inner delta 1:1 — the same
  classified bytes at wrapper offsets);
- `0x4b40..0x4b4f`: the 16-byte GNU build-id note digest copy.
No structural change: same magic, same segment count/layout, init/fini arrays still
exactly 1 slot each (re-verified by `check_xwp_artifact.py`, rc=0).

## 5. SKELETON variant (same classification)

inner 69384 → 69384 B; 467 differing bytes in 41 runs — identical classes: the single
`0xc0c0` confirmed byte, id swap `igd1-d1697d96b522 → igd1-5f1a25c33203`, pool
re-layout, string-pointer updates, note digests.

## 6. Safety-contract audit summary

- Armed semantics unchanged: same 7 verbatim frozen-contract imports + exactly ONE
  derived typed `sceKernelMprotect` (NOT console-proven — loud flag intact in
  `import-contract.json` `8cc757b3…`); ZERO raw `syscall` instructions; no `0F 05`
  pair; init/fini 1 slot each; fail-open; restore-always.
- Budget 8 unchanged; machinery sources byte-identical (the ONLY staged inputs that
  differ are TRANSITIONS.json bytes — the build-id seed by design).
- Bound: 1 byte of semantic data; everything else is build-id churn and its
  deterministic address/hash fallout.
