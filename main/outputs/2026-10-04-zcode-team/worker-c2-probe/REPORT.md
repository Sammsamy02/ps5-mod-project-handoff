# REPORT — worker-c2-probe (team 2026-10-04-zcode-team)

C2(b) ALTERNATIVE correspondence probe, igf7- build. Built, host-tested,
determinism-verified. NOT executed on console (controller stages + launches;
see `STAGE-PLAN-IGF7.md`). CLAIM.json written first; this report last.

## 1. Deliverables (all under `outputs/2026-10-04-zcode-team/worker-c2-probe/`)

| file | what |
|---|---|
| `modbridge_ingame_facts.prx` | igf7-3dd949879a0a, 19389 B, sha256 `9d92ac4892e3111147c4eeb14a9498dce44844ced9a90dda892e9f54d2b2ee12` |
| `build_igf7_c2_probe.py` | build script (igf6 packaging pipeline reused; no-arg mode = determinism double-build) |
| `check_igf7_artifact.py` | artifact gates (igf6 checks + no-expected-bytes gate) |
| `build-receipt.json` | determinism + gates + provenance receipt |
| `EXPECTED-VALUES-OFFLINE.json` | THE OFFLINE EXPECTED VALUES (read independently from the retained image; NOT in the PRX) |
| `STAGE-PLAN-IGF7.md` | controller staging plan + interpretation matrix |
| `src/` | 11 files byte-identical to the igf6 lineage (sha-verified) + `c2_data_probe.c` + `module_entry.c` (new) |
| `tests/` | host logic test (compat headers, harness, generator, validator, runner) |
| `determinism/run1`, `run2` | independent builds; artifact-checks.json each |
| `CLAIM.json` | written FIRST |

## 2. Offline expected values (independence preserved)

Read from the RETAINED IMAGE
(`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
194,241,424 B) with the sha256 re-verified (`3b6cfad4…3622fc`), the ELF64
program headers re-parsed, and each FILE OFFSET re-derived as
`phdr.offset + (vaddr - phdr.vaddr)` — the spec's file offsets were used only
as a cross-check (all five agree). Full-window uniqueness re-counted over the
whole file.

| id | vaddr | rt (base 0x400000) | file off | len | first bytes (hex) | occurrences |
|---|---|---|---|---|---|---|
| B | 0x7826782 | 0x7C26782 | 0x782A782 | 8 | `5500450035000000` | 1 |
| C | 0x75CFFDA | 0x79CFFDA | 0x75D3FDA | 32 | `42007500...` (BuildIdOverride UTF-16) | 2 (same text; spec §5 footnote) |
| D | 0x7B8845D | 0x7F8845D | 0x7B8C45D | 19 | `43757272...` (Current Changelist) | 1 |
| E | 0x7DD2A40 | 0x81D2A40 | 0x7DD6A40 | 64 | `637c777b...` (AES S-box) | 1 |
| A | 0xBC5F250 | 0xC05F250 | 0xB93B250 | 36 | `04000000...474e55...` (GNU build-id note) | 1 |

B/C/D/E re-derive from phdr 1 (flags 0x4, file = vaddr + 0x4000); A from
phdr 8 (flags 0x0, file = vaddr − 0x324000) — matching the spec's §2 table.

## 3. The probe (igf7-3dd949879a0a)

Sequence inside one synchronous `module_start` (write-through records):
seq1 `entry` marker → seq2 `startup` → seq3 `mapscan` (v3 144-byte-stride
walk; derives base := lowest prot&4 span start and exec-span extent as
recorded cross-checks) → seq3 `mapspans` pages (readable span list
PERSISTED before any data read — closes the igf6 unpersisted-map gap) →
seq4..8 candidates in order B, C, D, E, A (A last), each gated FIRST on the
same-invocation map: refusal + record (`map_untrusted` / `uncovered` /
`covered_not_readable` / `exec_span_protected`) or a gated read recording
`rt_addr, vaddr, image_file_off, len, span_start, span_end, span_prot,
method_used, live_hex` → seq9 `summary` (per-candidate
`{id}_{covered,read,refusal}`, `exec_reads_attempted=0`,
`comparison_on_console=0`, write failures).

Hard properties, build-asserted:
- NO expected bytes in the binary: build + artifact gates assert the five
  windows are absent (raw AND ASCII-hex) from combined object, inner ELF,
  final PRX; no `byte_match`/`expected*` keys; comparison is offline only.
- ZERO raw `syscall` instructions (combined object asserted 0; no 0F 05
  pair in final inner .text). NO mprotect identity (forbidden-symbol gate).
- Imports: EXACTLY the 7 verbatim frozen-contract rows (NID-encoded dynsym:
  7 strong, 0 weak) — console-proven lineage.
- The ONLY foreign dereference is the byte-loop read of ≤64 bytes at a
  map-proven readable, non-exec window. Exec spans (prot 0x4) are never
  read — the igf6 crash mechanism is excluded three ways (gate prot&1,
  explicit derived-exec-span intersection refusal, data-only candidates).
- 11 vendored files byte-identical to worker-igf5-diagnosis-rebuild/src
  (igf6 lineage), sha-verified at build time; module name/path/log path
  unchanged (`/data/plugins/modbridge_ingame_facts.prx`,
  `/data/modbridge-ingame-facts.jsonl`).

## 4. Verification done (host only — no console)

- Determinism double-build: run1 == run2 byte-equal on final PRX, inner ELF
  (69328 B, sha `8cba485d…81088`), combined object, import contract.
- `check_igf7_artifact.py` PASS both runs: fself wrapper magic + segment
  copies, SDK 0x11000000 stamp, init/fini 1 slot each, 7 NID strong imports,
  0 weak, forbidden identities absent, 0 syscalls, expected-bytes gates true.
- Host logic test (`tests/run_host_test.sh`) PASS — links the REAL
  `vmmap_walk` (stride fix exercised with synthetic 144-byte records), REAL
  `ig_map_cover`, REAL record sink, and the REAL `c2_data_probe.c` (one
  host-only read-hook shim, `#ifdef`-gated, never defined for console):
  - trusted map: 5 gated reads in order B,C,D,E,A; every `live_hex` equals
    the offline expected value (offline-comparison rehearsal); 7 readable
    spans persisted; derived base 0x400000; exec extent 0x747c000 flags true;
  - untrusted map: 5 `map_untrusted` refusals, zero bytes emitted;
  - covered_not_readable rodata (prot 4): B–E refuse `covered_not_readable`,
    A still reads correctly.
  30/30 records strict JSON; no `byte_match`/`expected*` keys.

## 5. Honest limits

- `native_executed: false`, `in_game_results: false`. No live correspondence
  claim exists until the controller-staged launch produces records and the
  offline comparison passes per STAGE-PLAN-IGF7.md §6 (PASS rules from spec
  §9: 5/5, or bounded A+B + ≥2 of C/D/E with zero mismatches).
- Tick/SCO remain UNRESOLVED; this probe never reads exec spans.
- Residual risk carried honestly (spec §7): no saved evidence of a successful
  prot-1 or flags-0 deref on this kernel; if the first data read faults, the
  crash localizes by missing seq4 WITH the gating map already persisted —
  a new fact class to re-arbitrate, not an igf6 replay.
- Tool budget: ~34 calls of the 60 allowed.

## 6. Inputs

- `worker-c2-alt-correspondence/C2-ALTERNATIVE-SPEC.md` +
  `candidate-ranges.json` (read-only).
- `worker-igf5-diagnosis-rebuild/` igf6 lineage (read-only; 11 files
  vendored byte-identical).
- Retained capture `decrypted-eboot.bin` (read-only; sha256 re-verified).
