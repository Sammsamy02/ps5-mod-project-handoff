# INTEGRATION — FC-1 add-on module into the next probe (igf6 / combined igf5)

worker-fname-runtime-probe, 2026-10-04. This is a PREPARATION module: it is
NOT in the current igf5 PRX and has never executed on a console. Everything
below is what the controller does at igf6 build time.

## 1. What to vendor

Copy (byte-identical) into the next probe's source tree:

| file | role |
|---|---|
| `src/fc1_pages.h` / `src/fc1_pages.c` | pure 16 KiB page-range / byte-match / hex helpers |
| `src/fc1_windows.h` / `src/fc1_windows.c` | pure FC1-B bounded x86-64 window decoder (fail-closed) |
| `src/fc1_probe.h` / `src/fc1_probe.cpp` | the probe (FC1-A..D, mprotect-first) |

The module already includes the igf5-lineage headers it needs
(`record_sink.h`, `anchor_store.h`, `ingame_abi.h`, `ingame_libc.*`,
`sys_mprotect_shim.h` are vendored copies in `src/`); in the igf6 tree those
names already exist — keep the controller's originals and delete the
duplicates, they are byte-identical (verified by sha256 in the build
receipt of this delivery: `record_sink.c`
`2b6a8c24c202002dc0cc85dcf4890760f30526985da21a5ce362b8d9926921ea`).

## 2. Generated config

`build_fc1_module.py` emits `generated-target/fc1_config.h` (real
constants: runtime = 0x400000 + vaddr, target 0x63d590, the 9 call sites,
anchor strings, SYS_mprotect=74 parsed from the pinned SDK, PAGE 0x4000,
exec span, `IGFC1_SEQ_BASE 10`). Either:

- vendor that generated file next to igf6's own `ingame_config.h` and add
  `-I <dir>` to the compile of `fc1_probe.cpp` only, or
- merge the `IGFC1_*` defines into igf6's generated config (keep the names;
  `fc1_probe.cpp` includes only `"fc1_config.h"`, so if you merge, change
  that one include line).

If the controller merges into a COMBINED igf5 build instead, the same
applies; igf5's own defines are untouched.

## 3. Build script wiring (igf6 `build_*.py`)

- add `"fc1_pages.c"`, `"fc1_windows.c"` to C_SOURCES and
  `"fc1_probe.cpp"` to CPP_SOURCES;
- include dir with the fc1 config for these TUs;
- no new imports: mprotect stays the inline syscall shim; the forbidden
  symbol gate is unchanged (no `sceKernelMprotect`);
- `FC1_NSITES`/`FC1_NANCHORS` come from `fc1_windows.h` (9/5).

## 4. Call site

In `module_entry.c` (or igf6's probe runner), AFTER the current
`ig_run_code_anchor_probe(&sink)` (mapscan + Tick/SCO anchors must run
first so `g_map` is populated and Tick/SCO outcomes exist in the same log):

```c
extern void ig_run_fc1_probe(IgSink *sink);   /* fc1_probe.cpp */
...
ig_run_fc1_probe(&sink);   /* seq 10..21 with IGFC1_SEQ_BASE=10 */
```

Sequence numbers emitted: 10 `fc1_a_candidate`, 11–19 `fc1_b_site`,
20 `fc1_d_anchor_strings`, 21 `fc1_summary`. Renumber by changing
`IGFC1_SEQ_BASE` in the config only.

Requirements/semantics:
- `ig_run_fc1_probe` does NOT need a trusted map. Map trust only changes
  (a) the restore-protection source (current VMMAP covering span vs
  LABELED checkpoint fallback 0x4) and (b) whether FC1-D may read.
- mprotect-first is structural: page-range -> orig-prot -> mprotect
  (READ|EXEC, 16 KiB pages) -> read ONLY on success -> restore -> anomaly
  record on restore failure. mprotect failure emits a refusal record with
  NO read — exactly the igf5 corrected design.
- FC1-C is fail-closed in `fc1_summary`: `fc1_c_fail_closed=1` iff the
  candidate prologue holds while ANY of the 9 sites disagrees.
- `fc1_validated_indicator` (FC1-A 32-byte hard gate + 64-byte compare AND
  all 9 sites pass) is an INDICATOR, not a promotion claim; the controller
  decides per SEGMENT-CORRESPONDENCE rules, together with the Tick/SCO
  outcomes in the same log.
- FC1-D never gates the code verdict; a D mismatch would refute the
  +0x400000 data bias, not the code candidate.

## 5. Host-test path (regression for future edits)

`python3 build_fc1_module.py` in this directory re-runs everything:
capture sha verification -> three-way byte cross-check -> SDK constant
parsing -> pinned-toolchain `-Werror` compile of all module TUs ->
8-scenario host test (`tests/run_host_checks.py` must print
`HOST_CHECKS_PASSED`). The host build compiles `fc1_probe.cpp` with
`-DIGFC1_HOST_TEST`, which swaps in the software-mprotect model
(`tests/host-shims/fc1_host_mprotect_shim.h`) and synthetic arenas mapped
MAP_FIXED at 0x110000000/0x110010000 (macOS arm64 refuses fixed maps
below 4 GiB — keep the bases where they are).

## 6. Known residual risks (carry into the igf6 receipt)

- The syscall NUMBER cannot be verified on-host (inherited igf5 risk;
  refusal path makes it non-fatal by construction).
- Restore-failure anomaly does not gate `fc1_validated_indicator`
  (igf5 semantics: anomaly recorded, verdict kept). If the controller
  wants it gating, that is a one-line policy change in
  `ig_run_fc1_probe_env`.
- The exec span is assumed unchanged since the igf4 VMMAP observation
  (checkpoint constant fallback is labeled as such in every record).
