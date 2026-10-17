# worker-7-installer — modforge-service/1 vertical slice

Date: 2026-10-04. Output area (everything I wrote):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-7-installer/`

## What was implemented

A coherent, host-tested vertical slice of the installer half of
`modforge-service/1`, built by **consuming Claude's imported C components
unmodified** (vendored byte-identical, origin commit
`43a96969e3bf1f4c9d1e43a55a386328dbac19e6`; hashes in `vendored/VENDORED.txt`):

```
select mods -> compose bundle -> staged owned output -> transactional reversible
install -> durable records -> runtime activation plumbing (status only)
```

- **Selection**: mods are registered against a title with a base mod bundle
  (`.mfb` kind 1) or with **variant bundles** = FOMOD-style choice groups
  (SelectExactlyOne semantics: one option per group, default applies when
  unchosen, explicit choice validated against the option list, group without a
  default refuses with `needs_choices`). This is the console-representable
  subset of FOMOD: the XML installer itself runs host-side per FORMATS.md
  (`modforge/fomod.py`), producing per-option bundles; the console picks.
- **Composition**: the vendored composer `mfx_compose()` (modforge_compose.h)
  does the real work — full bundle validation before any write, load order
  (priority then id), later-mod-wins conflict resolution, dependency/version
  checks, Bethesda plugin order, hash-checked file writes, `loadlist.txt`
  written LAST. On top of it, `prepare` computes a **plan**: per-mod load
  order, **conflict report** (`conflict dest=... previous=... winner=...`,
  same data shape as the FORMATS.md deployment's `conflicts[]`), and
  requirement checks.
- **Requirement/capability model**: mod→mod `requires` (missing mod or version
  too old -> specific blocker naming mod, min version, selected version) and
  mod→runtime frameworks checked against a capability table
  (`state/caps.tsv`, seeded from the vendored `engines/unreal.json` facts:
  `available` / `integrating` / `planned`):
  - unknown component -> `requirement-unavailable ... no runtime component
    provides this capability on PS5`
  - `planned` -> `requirement-unavailable ... runtime component is planned
    (not ported)`; both **refuse the install** (exit 3) with the reason.
  - `integrating` (ue4ss-ps5 today) -> warning only: files install, in-game
    execution not proven. Honest per CLAUDE_BACKEND_MAP LIMITS.
- **Transactional install** (SERVICE.md swap, plus an extended journal):
  1. plan/validate everything before touching the filesystem;
  2. stage the complete new root under `<TITLE>.next/` (composer writes
     marker + files + load list last), journal phase `stage`;
  3. journal phase `swap` records generation, engine/sdk, mod set, and the
     **sha256 the new load list must have**;
  4. rename root→`.prev`, cancel-check, rename `.next`→root, delete `.prev`,
     delete journal, atomically commit records (generation++, loadlist sha).
  - **Ownership guard**: refuses to replace a mod root that lacks the
    `.modforge-root` marker (never wipes a directory modforge didn't create).
- **Crash recovery between journal steps** (runs on every open, `mfs_open`
  semantics): `stage` -> drop `.next`, previous root untouched. `swap`:
  root missing -> finish forward from `.next`, or roll back from `.prev` if
  `.next` was lost; root present -> compare live load-list sha with the
  journal's expected sha: equal = second rename already happened (finish
  cleanup, commit records), different = the root is still the old set (drop
  `.next`, records unchanged). The game always sees the old set or the new
  set, never a mix.
- **Cancel**: during staging (stop cooperating at the Nth file write; the
  composer aborts before further writes, staging + journal removed) and
  between the two renames (first rename rolled back). Previous set preserved
  byte-for-byte in both cases.
- **Durable records**, distinct states: `downloaded` -> `prepared` ->
  `installed`; `running` is **derived, never stored**: it is read from
  `<mod root>/runtime-status.txt` (format owned by the vendored
  `modforge_activation.c`) and `running=1` only when the runtime's last state
  line says `mods-started`. Uninstall drops mods back to `prepared`;
  uninstall-to-empty leaves marker + minimal load list.
- **Runtime activation plumbing (not the engine)**: `simulate-runtime` runs
  the **real** `mfa_activate()` against the installed root (title check,
  full load-list validation, preload of lua/config/script roles — exactly
  what the injected runtime does at module_start) and appends status through
  the **real** `mfa_status_open/write/close()` API. Engine execution itself
  remains an explicit capability gap; nothing here claims it.

## Files

- `src/modinst.c` — the slice (single C99 TU, ~1550 lines): records store,
  capability table, selection/choices, plan (conflicts + requirements),
  journal/recovery, transaction, records/running reporting, fixture builder.
- `vendored/` — imported components, unmodified: `modforge_bundle.h`,
  `modforge_compose.h`, `modforge_loadlist.h`, `modforge_activation.c/.h`
  (+ reference copies `modforge_catalog.h`, `modforge_prx.h`,
  `modforge_service.h`, `docs/`, `engine-facts/`); `VENDORED.txt` has sha256s.
- `test/run_tests.sh` — 108 checks across 9 scenarios (below).
- `receipts/build.log`, `receipts/test-run.log`,
  `receipts/test-run-asan-ubsan.log`.
- `test/tmp/` — sandbox artifacts of the last test run (evidence; regenerable).

## Exact commands and results

```
cc -std=c99 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L \
   -o build/modinst src/modinst.c vendored/modforge_activation.c
# exit 0 (3 benign warnings: two unused params, one set-but-unused var)

test/run_tests.sh
# RESULT pass=108 fail=0

# full suite under AddressSanitizer + UndefinedBehaviorSanitizer (-O1):
cc -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ... (same inputs)
test/run_tests.sh
# RESULT pass=108 fail=0, no sanitizer reports
```

One real bug was found and fixed during development thanks to the sanitizers:
the `--choices` parse buffer was scoped inside its `if`, so the choice-pointer
array dangled (stack-use-after-scope; visible only at -O2). Fixed by hoisting
the buffer; re-verified O2 + sanitizers.

## Test matrix (all passing)

1. **Multi-mod composition with conflict** (A1–A19): 3 mods, shared
   destination; conflict detected at plan (`previous`/`winner`), load order =
   priority-then-id, winner content installed, loser content absent, marker +
   load list present, mods `installed`, generation 1.
2. **Installed vs running** (B1–B7): `runtime not-started running=0` while
   installed; after the real `mfa_status` writes `mods-started`, `running=1`;
   a later `failed` state with code/detail replaces running. Activation check
   (`mfa_activate`) succeeds on the composed root (title + preload).
3. **Cancel mid-transaction** (C1–C9): cancel during staging and between the
   renames; both leave the previous root **byte-for-byte** unchanged
   (tree-hash compare), journal empty, no `.next`/`.prev` leftovers.
4. **Crash between journal steps + recovery** (D-*): crash at `stage-journal`,
   `staged`, `swap-journal` -> recovery rolls back, root is exactly the old
   set, records stay generation 1; crash at `rename1`, `rename2`, `prevdel`
   -> recovery finishes forward (or commits cleanup), root is exactly the new
   set (hash-compared against a reference install), records generation 2. All
   cases: journal removed, no leftovers, exit 86 observed before recovery.
5. **Rollback path** (DR1–DR4): crash after the first rename with the staging
   root destroyed -> recovery restores the previous root byte-for-byte from
   `.prev`; replaced mod not marked installed.
6. **Unsupported requirements** (E1–E9): missing mod dependency, too-old
   dependency, unknown runtime capability, planned-only capability — each
   refuses install (exit 3) with a specific reason naming the mod, the
   requirement, the version and the capability state; nothing written.
7. **FOMOD-style choices** (F1–F11): default variant applied and visible;
   explicit `--choices` changes installed content (exactly one variant, no
   merge); invalid option rejected with the valid list (exit 2); group
   without default demands a choice (exit 4, `needs_choices`).
8. **Uninstall** (G1–G10): removal rebuilds without the mod (remaining mod
   wins the old conflict again; removed files gone; state `prepared`;
   generation advances); uninstall-to-empty keeps marker + load list and the
   empty root still passes `mfa_activate`.
9. **Ownership guard** (H1–H3): unmanaged directory at the root path is
   refused with a specific marker error; foreign file untouched.

## Integration contract (for the controller)

- **Methods**: this driver's commands map 1:1 onto `mfs_request` methods:
  `register-mod`→`mod.register_download`, `prepare`→`mod.prepare` (plan =
  `install.plan` dry run), `install`→`install.apply` (+`--remove`→
  `mod.uninstall`), `records`→`records.list`/`game.status`, `recover`→ part
  of `mfs_open`. Exit codes 2/3/4/5 ≈ `bad_request`/`not_ready`/
  `needs_choices`/`cancelled`.
- **Records serialization**: this slice persists records as a documented
  token/TSV line store (`state/records.txt`, atomic tmp+fsync+rename) instead
  of `records.json` because the host driver deliberately has no JSON parser.
  Field-for-field it carries the same facts (mod id/title/state/bundle,
  variant + choice, game engine/sdk/generation/loadlist-sha). The service
  layer should serialize to `records.json` verbatim; semantics are identical.
- **Journal**: `state/journal/<TITLE>.json` is the SERVICE.md journal with
  two documented extensions: a `stage` phase before the swap (so crashes
  during staging also recover) and an `ll_sha` field (the sha256 the new
  load list must have) used to disambiguate "second rename done" from "first
  rename not yet done". Both are additive; a SERVICE.md-only reader can
  still apply the published recovery rules.
- **Capability table**: `state/caps.tsv` (`component <id> <maturity>`) stands
  in for the engines/*.json component maturity the real service resolves.
  It was seeded by hand from the vendored `engines/unreal.json` facts;
  production should read the engine definitions directly.
- **PS5 port**: all filesystem work goes through POSIX wrappers here; on the
  console the same steps map `mfx_io`/`mfa_io` callbacks onto
  sceKernel* (the imported headers were designed for exactly that — no libc
  in the composer/installer core).

## Honest limits / open questions

- **Engine runtime activation is not implemented and not claimed.**
  `running` comes only from a status file the real injected runtime writes
  through `mfa_status_*`. `simulate-runtime` is a host-side simulation of
  that bookkeeping (and a real `mfa_activate` preload check), not engine
  execution; the ue4ss-ps5 component is `integrating` and mods depending on
  it install with a warning, never a running claim.
- **Bundle intake, not archive intake**: the slice starts from per-mod
  `.mfb` bundles (the console-side prepared-input contract of the imported
  backend). ZIP/7z/RAR extraction, classification and the FOMOD XML runner
  are host-side stages (`modforge` Python already implements them) and were
  not re-implemented here; wiring archive intake → bundle production for the
  console flow is open work for the owning worker.
- **FOMOD subset**: choice groups are SelectExactlyOne over pre-resolved
  variant bundles; multi-group mods resolve via the first registered group
  (host should pre-combine). No FOMOD XML evaluation on the console.
- Single-group title flow only; jobs are synchronous CLI steps (the
  contract's async job model/events/threading belong to the service layer);
  records are TSV (mapping above); staged files are fsynced at the directory
  level, not per-file; `mod.forget` and priority overrides are not
  implemented.
- Tested on macOS host only; nothing was run on or written to the PS5.
- All referenced handoff and imported-source paths existed and matched their
  descriptions; nothing materially different was found (no stop condition hit).
