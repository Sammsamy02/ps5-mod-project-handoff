# REPLACE-MECHANICS — how a composition change actually ships

worker-composition-replace-mechanics, 2026-10-04. Read-only analysis + rehearsals
in this worker's dir only (see `REHEARSAL-RECEIPTS.json`). Scripts analyzed:

- Fixed composition script (S):
  `outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py`
  sha256 `36cc8199b7e5a2528badd4da53ff3750075cbbe4d76ca115490b50a008ed8ff3`
- Composition-5 driver (D):
  `outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/build_runtime_composition5.py`
- Reference run: `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3`
  (receipt status `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED`,
  runtime 368 units, sha256 `45dd9901db7d626e…`, 117,779,552 bytes)
- Base receipt (pinned by S:29–31): `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json`
  (sha `bcd23a2a…`, 359 selected runtime units + 3 bootstrap units:
  `activated_mod_source`, `runtime_owner`, `ue4ss_backend`).

All line cites below refer to S unless prefixed D.

---

## 1. Unit inventory: what is replaceable, what is frozen

S composes 368 units into `coherent-tick-file-witness-runtime.o`:

| Class | Units | Mechanism |
|---|---|---|
| **Affected fresh units** (always recompiled, S:93–103) | `unreal-unrealinitializer`, `native-ownedmappedscansource`, `native-psscanbinding`, `native-ownedmappedcandidateselection`, `native-loadercontext`, `native-programfactory`, plus appended `native-ownedserializedelfsource` | `--replace-runtime-unit name=src` **only** for names in `affected` AND in base units (S:79), except the 4 staged-default names (S:83) |
| **Extra units** (appended, S:112–131) | run-3: `lmf-live-module-facts`, `lmf-sce-ops`, `lmf-loader-adapter`, `lmf-base-scan`, `native-query-harness`, `module-runtime-ack-supplier` | `--extra-unit name=src` — **any** path, any name (S:115–117); needs `--extra-receipt` + `--extend-undefined-baseline` (S:113–114) |
| **Bootstrap units** | `activated_mod_source`, `runtime_owner`, `ue4ss_backend` (base receipt `bootstrap_units`) | `--replace-bootstrap-unit name=src` for those 3 names (S:134–137); requires `--replace-receipt` (S:138); note: replacements are recorded INTO `selected_runtime_units` (S:157) even though they are not runtime units |
| **Retained units** (353) | everything else, linked from base-receipt objects (S:159–162) | **No mechanism.** Frozen objects; only their object files' hashes are checked (S:59) |

Affected-set derivation (S:57–61): a unit is affected iff its base `.d`
references any of `OwnedMappedScanSource.hpp`, `NativeLoaderContext.hpp`,
`OwnedMappedCandidateSelection.hpp`, `NativeProgramFactory.hpp`; the resulting
set must be EXACTLY the six names above or S refuses ("Affected header
inventory changed"). Since the base receipt is hash-pinned (S:31, S:42), this
set can never change without a new base receipt.

Fresh-unit compile gates (S:93–101): every resolved dependency of each fresh
unit's `.d` must be in `pins` (base originals + `validate_inputs()` + W globs,
S:51–63) or in the replace-receipt `rr` (S:85–92) — otherwise "Unpinned fresh
dependency" refuses. Extra-unit dep gate (S:122–129) is looser: unpinned deps
under `.superpowers` refuse; **unpinned deps elsewhere (e.g. `outputs/…`) are
auto-pinned** and recorded in `receipt.extra_pinned_dependencies` (S:187).

### Per-target verdicts (the units the two designs need)

| Target | Unit | Change mechanism today? |
|---|---|---|
| `lmf_to_native_loader_facts.cpp` | extra `lmf-loader-adapter` | **YES — proven (R2/R3).** Point `--extra-unit lmf-loader-adapter=<staging copy>` at a staging dir. Content change reaches the composed object; staging-local headers (same basename, quoted include) override pinned `W/include` copies via the quoted-include same-dir rule; staging files are auto-pinned into the receipt. No script change needed — only the argv. |
| `NativeLoaderContext.cpp` | affected `native-loadercontext` | **NO flag mechanism.** S:75 pre-stages `W/src/NativeLoaderContext.cpp` into `new_sources`; S:83 refuses any `--replace-runtime-unit` for a name already in `new_sources`. Also uses zero quoted includes (angle-bracket only), so the staging-header trick cannot redirect its headers either. Requires a **script revision** (edit the `new_sources` default path or extend the flag logic) re-pinned via D:66–67. |
| `NativeProgramFactory.cpp` | affected `native-programfactory` | **NO — same as above** (`new_sources` default `W/src/NativeProgramFactory.cpp`, S:83 refusal; angle-bracket includes only). |
| `PsMappedImageCaller.hpp` (lives in `ue4ss-native-ps-scan-abi-1/include/`, hash-pinned; consumed by 9 units incl. 6 affected + extra `lmf-loader-adapter`) | header | **NO composition-level mechanism.** Not under W, so not in the W glob pins, but it is pinned via the base originals/validate_inputs (S:51–53) and re-checked for every affected unit dep (S:64–67 → "Changed input"). All affected consumers include it through angle-bracket/-I resolution, where `W`+template `-I` paths win (S:94, S:118 prepend at argv[1:1]). Only an lmf-style extra unit with a QUOTED include of a staging copy can see modified content — i.e. only by changing the *consumer* TU, not the header for the retained units. |
| `UE4SSProgram.cpp` | retained `program` (source `native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp`, object `coherent-full-1/program.o`) | **NO — double-locked.** (1) S:79 refuses `--replace-runtime-unit program=…` (proven, refusal-probe-run-1). (2) An `--extra-unit` re-implementation hits ld -r duplicate-symbol errors for its 82 strong text symbols + 1 strong data symbol (`_ZN2RC12UE4SSProgram4initEv`, `…6updateEv`, both ctors/dtors, `…24setup_mod_directory_pathEv`, `_ZNK2RC12UE4SSProgram24get_game_executable_pathEv`, etc. — see §2). The 4,712 WEAK definitions (inline/template bodies, `W`/`V`) CAN be overridden by a strong extra-unit definition (proven T4). |
| `PsScanBinding.cpp` / `OwnedMappedCandidateSelection.cpp` | affected, NOT in `new_sources` | **YES via `--replace-runtime-unit`** (S:79–84 pass) — but sources live in `ue4ss-native-mapped-candidate-selection-1/src/` (frozen); a staging copy works, and any NEW dependency of the replacement needs a `--replace-receipt` entry (rr fallback, S:96–100). Their includes are angle-bracket only, so modified headers are NOT reachable this way. |

### ld -r duplicate-symbol semantics (proven, T1–T6)

Invocation pattern identical to S:165 (`ld.lld -m elf_x86_64 -r … -Map=… -o …`):

- strong + strong → **hard error** `duplicate symbol: <sym>`, exit 1. No
  first-wins. ⇒ `--extra-unit` can never replace a retained strong definition.
- strong + weak (either order) → links; **strong wins** (disassembly verified).
- weak + weak → links; **first object in the link order wins**.
- Practical rule for designs: an overlay extra unit may (a) add new symbols,
  (b) override retained symbols that are weak; it must not redefine any of the
  82 strong `program` symbols listed by
  `llvm-nm --extern-only --defined-only coherent-full-1/program.o`.

### Gate chain every change must survive (S:165–184)

1. `ld.lld -r` succeeds (duplicate symbols refuse here).
2. Undefined-delta vs the **notify-cancel base** (not run-3), S:167–179:
   added imports must each be declared by a pinned SDK `target/lib/*.so` or be
   `_DYNAMIC`; any removed symbol or changed binding refuses.
3. Rust witness exports must remain defined (S:180–182).
4. Full re-check of every pin (S:183–184).

---

## 2. The proven shipping procedure (unit list → flags → driver → gate)

Phase A (composition) needs NO script modification for any lmf-side change
(both designs' `lmf_to_native_loader_facts.cpp` / text_base adapter work):
only the argv changes. For `NativeLoaderContext`/`NativeProgramFactory`/
`UE4SSProgram` content, Phase A is impossible without a script revision — the
revision procedure is at §4.

### Procedure A — new composition run with modified staging units (proven R2+R3)

1. **Stage** the modified unit(s) under a worker dir, keeping original
   basenames for any co-staged header (quoted includes resolve from the
   source's dir first; proven by the `#error` guard in R3):
   ```
   outputs/2026-10-04-zcode-team/<worker>/staging-rN/lmf_to_native_loader_facts.cpp
   outputs/2026-10-04-zcode-team/<worker>/staging-rN/lmf_to_native_loader_facts.hpp   # optional
   ```
2. **Run Phase A** (fresh run dir; S refuses an existing one, S:69; D:211–213),
   from the workspace root, with the APPLY-ORDER argv (D:150–166) and ONLY the
   `--extra-unit`/`--replace-*-unit` paths redirected to the staging copies.
   Everything else — receipts, defines, `--extend-undefined-baseline`,
   `--extra-receipt`, `--replace-receipt` — stays verbatim.
3. **Gates** run inside S automatically: input-hash pins (S:42–67), fresh/extra
   compile dep pins (S:96–101, S:122–129), `ld -r` + ELF check (S:165–166),
   undefined-delta/SDK check (S:167–179), Rust exports (S:180–182), final pin
   re-check (S:183–184). Success emits `receipt.json` +
   `undefined-delta.json` + `commands.json` in the run dir.
4. **Driver invocation**: run D with `--composition-run <new run dir>` (no
   `--reuse-composition-run`): Phase A executes S, `chain_symbols_gate`
   re-verifies the startup chain against the NEW object (D:273–291, D:501),
   overlay + packaging proceed (D:502–504).

Rehearsal evidence (this worker, read-only for the trees):
- R2 (path redirect, content identical): status composed, 12/13 fresh units
  **bit-identical** to run-3; the only differing object is the redirected
  lmf-loader-adapter.o by exactly the embedded `__FILE__` path string (8 bytes).
  Undefined set identical to run-3.
- R3 (content change + staging header): compile-time `#error` guard proves the
  staging header won resolution; `wc_rm_rehearsal_marker` is exported by the
  composed object; delta unchanged (`added: [sceKernelGetModuleInfo]` vs base,
  `baseline_extended: true` — same as run-3).

### Procedure B — script revision (required for loadercontext/programfactory/UE4SSProgram/header changes)

1. Copy S, apply the minimal edit (e.g. point the S:75 `new_sources` default
   for `native-loadercontext` at a staged path; or extend S:93's loop to
   recompile `program` with a replacement source; or add a staging `-I`
   insertion). Keep everything else byte-identical.
2. Independent review, then record the new sha256 and update BOTH D:66
   (`COMPOSE4_FIXED`) and D:67 (`COMPOSE4_FIXED_SHA`) — D refuses on drift
   (D:468). This is exactly the composition-4-fix precedent (the delivered
   script itself documents its own sha lineage, S:16–25).
3. Continue with Procedure A step 2 (the APPLY-ORDER argv is unchanged; the
   changed script does the work).
   Alternative (heavier, NOT required): a new base receipt from the upstream
   pipeline — would change the S:31 pin and the affected inventory.

### Receipt / checkpoint re-pinning steps (new run)

The run receipt is produced by S itself; downstream "re-pinning" is:

1. New `receipt.json` + `runtime_sha256` in the NEW run dir (S:189). D's
   `package_phase` reads it live (D:356–360) and pins
   `composition_object_sha256/bytes` (D:409–414) — nothing to hand-edit.
2. `chain_symbols_gate` consumes the same receipt (D:274–280). If the change
   adds a new startup-chain symbol, add it via `--require-symbol` (D:456–458)
   or extend `DEFAULT_REQUIRE_SYMBOLS` (D:129–146).
3. Transport-ledger mirror entry: D:364–379 writes key
   `zcode_supplier_composition_5` (D:413 checkpoint). For a new run, bump this
   checkpoint id (e.g. `zcode_supplier_composition_6`) so the composition-5
   entry is not overwritten.
4. Record the exact new argv in an APPLY-ORDER-style doc (provenance, per the
   worker-query-s2-arm/integration-patch/APPLY-ORDER.md pattern).

### What the composition-5 driver needs changed to point at the new run

Currently it runs `--reuse-composition-run` against composition-4 run-3. To
consume a NEW composed run instead:

| D location | Change |
|---|---|
| D:73–74 (`RUN4`/`RUN5`) | point `RUN5` (fresh-run default) at the new run path, or pass `--composition-run <new>` explicitly |
| D:150–166 (`COMPOSE_ARGV_TAIL`) | inject/redirect the unit flags for the changed units (the only Phase A edit needed for lmf-side work) |
| D:66–67 | only if Procedure B was used: new script path + sha |
| D:413 (`composition_checkpoint`) + D:364 ledger key | new checkpoint id |
| D:462 (`--out` default `prx-candidate-composition5-1`) | new output dir (refuses existing; -1/-2 already used) |
| D:298–300 (`staging/lineage-overlay-1`) | refuses an existing overlay; consciously remove or bump to `lineage-overlay-2` for the new run |
| D:86, D:91–93 (entry/bridge pins) | unchanged if the wiring delivery is unchanged |

`--verify-only` (D:452–455, D:489–498) works against any existing run via
`--composition-run`; note D:490–493 special-cases the default RUN5 → RUN4 for
the symbol gate, which must follow the new default.

---

## 3. Failure modes observed (all fail-closed, as designed)

- `--replace-runtime-unit` for a retained unit → `ValueError: Bad
  replace-runtime-unit spec (target must be a freshly compiled affected unit)`
  (S:79; proven refusal-probe-run-1).
- `--replace-runtime-unit` for a staged-default unit → `…already has a staged
  source` (S:83; code-verified).
- Duplicate strong symbol at `ld -r` → `duplicate symbol:` error, exit 1 (T1).
- Extra-unit deps under `.superpowers` not in pins/rr → `Unpinned extra project
  dependency` (S:127). Extra-unit deps elsewhere auto-pin (S:128, R3 receipt).
- New undefined import not SDK-declared → `Extra-unit imports not declared by
  any pinned SDK library` (S:176).
- Any input drift → `Changed input:` (S:37) / `Unpinned fresh dependency` (S:100).
- Non-determinism note: a redirected unit's object embeds its `__FILE__` path
  (R2's 8-byte delta). Path-stable staging locations keep objects comparable.
