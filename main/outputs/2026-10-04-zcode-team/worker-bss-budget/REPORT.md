# REPORT — worker-bss-budget

Date: 2026-10-05 · Local only. ELF struct parsing (python3, no new deps), file
reads, greps. No console contact, no engine/source writes, no recompiles.

## Question

Can the PRX afford the launch-12 capture-capacity bump 16,384 → 32,768
(+768 KiB lmf BSS + ~36.3 MiB `g_bridge_vmmap` + spanpub table, per
`worker-capture-22k-fixture/CAPTURE-CAPACITY.md` §5)?

## Answer: AFFORDABLE (+19.60 MiB delta on a proven-20.56 MiB BSS; no binding limit found)

### What was done

1. **Measured the shipped PRX** (`prx-candidate-l10-2/bare-sdk/
   modbridge_supplier_composition.prx`, sha `ff1055b2…`, 30,834,648 B) with a
   purpose-built ELF64 struct parser:
   - `.bss` = 0x148F5E0 = **21,558,752 B (20.56 MiB)**, carried entirely by
     PT_LOAD[4] (filesz 23,456 + bss = memsz 21,582,208).
   - PT_LOAD sums: filesz 31.33 MiB, **memsz 51.90 MiB**; l10-1 twin identical.
   - Composed object `build/combined-supplier.o` NOBITS = 21,553,867 B
     (everything else in the link adds 92 B; 4,793 B link alignment).
   - Named arrays: `g_bridge_vmmap` 19,005,440 (16,384×1,160), `g_spanpub_records`
     393,216, `window_anchor_verify::mappings` 393,216, lmf `capture::mappings`
     **24,576 (kMaxVmMappings still 1024)**, other 1,742,304.
   - Side finding: the EDITION.json edit-4 staged header override
     (`kMaxVmMappings 1024→16384`) did NOT reach the shipped l10-2 artifact —
     exactly the fold CAPTURE-CAPACITY.md §5.1 says launch-12 must verify first.
2. **Limits search** (pinned `sdk-v0.43` + project docs): the only documented
   machine caps are FreeBSD-derived process rlimits in
   `machine/vmparam.h` — MAXTSIZ/MAXDSIZ 32 GiB, MAXSSIZ 512 MiB.
   **No PRX/dynlib-specific per-module BSS or segment cap is documented
   anywhere** (libkernel.h, ldscript, docs all silent). So the constraint is
   empirical, and the mission's suggested anchor needed a correction: the
   frozen_prx baseline (~30.7 MiB memsz, loads every time) has **.bss of only
   70.3 KiB** — it proves total-image loading is a non-constraint but says
   nothing about BSS. The BSS axis is proven by the current runtime class
   itself: six real console boots (runs 1/2/3 pids 317/341/355 + run5 session
   370 ×3) with `module_start_return=0`, 235/235 ctors, and the full 19 MiB
   `g_bridge_vmmap` walked (entries 22761/22761/10028/10028/9993/9993,
   walk_stop=0, spanpub saturated) — ≥19.5 MiB of BSS physically resident and
   exercised every boot; all observed refusals were pad/INI config-class.
3. **Computed the budget**: all four scaling arrays → 32,768 gives
   **+20,553,728 B (+19.60 MiB)** — note this budgets a **fourth** scaling
   array (`window_anchor_verify::mappings`, +384 KiB) that CAPTURE-CAPACITY.md
   §5.3's list omits, and treats lmf as 1024→32,768 (+744 KiB, the fold plus
   bump), not 16,384→32,768. Post-bump: .bss 42,112,480 B (40.16 MiB);
   writable segment 40,188,672 B (16 KiB-aligned); total PT_LOAD memsz
   74,978,914 B (71.51 MiB) = **1.378×** the console-proven image and **815×**
   under MAXDSIZ. Spanpub top-16 table: +0 (bounded at 16 by design).
4. **Alternative lever** (proposal-only, not required): keep 16,384; feed
   `bridge_spanpub_publish` from the walk stream (it already parses all 22,761
   records) so tail interest-spans register in the bounded top-16 table and the
   envelope covers the full walk — zero added BSS, but it rewrites pin-D1
   envelope semantics and needs re-derived fixtures (E4/H5/T1–T5) as a NEW
   bounded revision.

### Verdict numbers (one line each)

- Shipped .bss: 21,558,752 B (20.56 MiB) — measured.
- Launch-12 delta: +20,553,728 B (+19.60 MiB) — computed from the real struct
  sizes (24 B records verified from the actual struct definitions; 1,160 B
  `kve_sizeof_` pinned by static_assert in the shipped unit).
- After: 42,112,480 B BSS in a 71.51 MiB-memsz image.
- Documented limit: MAXDSIZ 32 GiB → 815× margin. Empirical limit: the
  six-boot class → 1.378× image / 2.05× BSS multiple.
- **Unknown, stated plainly:** no documented kernel-loader cap exists to check
  against; kernel-internal policy (loader segment caps, sysctl temp-buffer
  sizing for a 36.25 MiB request) is unverifiable from the pin. Six boots give
  zero negative signal; the launch-12 verification pass retires the residual.

### Honest limits of this work

- Static ELF/struct analysis + document audit only; no target run. "Affordable"
  means no identified or documented constraint binds; it is not a boot proof.
- The deployed-console artifact sha for the six boots was not pinned in the
  baselines I could read; the boots are attributed to the same runtime class
  (identical .bss layout, same bridge tags, same capacity constants).
- The 40.16 MiB residency figure is worst-case-untouched-assumes-resident; real
  residency during a walk already proven at ~19.5 MiB.

## Deliverables

- `CLAIM.json` — machine-readable claim, measured/computed values, verdict.
- `BSS-BUDGET.md` — full budget tables, limits audit, unknown, alternative lever.
- `REPORT.md` — this file.
