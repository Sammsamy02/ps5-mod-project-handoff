# INTEGRATION.md — wiring multi_mod_lifecycle into the installer slice

worker-multi-mod, 2026-10-04. Target: `worker-7-installer/src/modinst.c`
(vendored composer origin 43a96969e3bf1f4c9d1e43a55a386328dbac19e6, unmodified).
Everything below is a proposal for the implementing worker; nothing has been
applied to modinst.c by this worker.

## 1. State model changes (records store)

`modrec` (modinst.c:240) gains: `version[64]`, `priority` (long), `layout`
(int), `reason[256]`, `deps` (parsed list). `records_load`/`records_save`
(modinst.c:296/340) switch the `mod` line to the append-only 10-column shape
in DESIGN.md §d — old files parse, new files parse in the old reader.
State strings: extend the accepted set with `installing`,
`installed_disabled` (store `installed` only as the load-list-named case, or
rename to `installed_enabled` behind the same parser — either reads legacy
`installed` as enabled).

Mapping from module types:
- `mm_store` rows ↔ `g_mods[]`; `mm_manifest` ↔ facts loaded from each
  bundle's modinfo at register/prepare time (id, version, priority,
  load_order, framework lines, file list, requires lines are all already in
  the modinfo grammar `cmd_make_bundle` emits — modinst.c:1385-1400).
- `mm_caps` ↔ `caps_load()` (modinst.c:372): add the family table
  (verdict + shared_policy) next to the existing component/maturity table;
  both stay in `caps.tsv`.

## 2. Call placement inside cmd_install (modinst.c:969)

The transaction skeleton does not change: journal stage → compose → journal
swap → renames → commit records. The state machine slots into three points:

1. **Validation (before any FS touch)** — replace the hand-rolled blocker
   loop `plan_report` (modinst.c:664) guards with `mm_plan_install()`:
   - keep the bundle loading + game-agreement checks exactly as-is
     (plan_load, modinst.c:604);
   - `mm_plan_install(ids, n, caps, &plan, reason)` subsumes: dependency
     blockers (modinst.c:704-728), framework blockers/warnings
     (modinst.c:729-747, against the extended caps), conflict
     classification (new), capacity gate (new), and load ordering
     (replaces the priority/id insertion sort at modinst.c:647-656 for
     N-mod sets).
   - `reason` maps to the user-facing actionable string; exit codes: keep
     EX_NOTREADY for blocked_dependency/order/capacity, and route
     MM_ERR_CONFLICT to EX_NOTREADY as well in M2 (a dedicated
     "classification" exit can come with the UI milestone).
2. **Compose** — `mfx_compose(&io, sizes, n, ...)` (modinst.c:1069)
   composes in its own (priority,id) order. For M2: pass `plan.order` —
   requires a new vendored entry point, e.g.
   `mfx_compose_ordered(&io, sizes, order, n, ...)` that skips its internal
   sort. Until that lands, a safe subset ships: refuse
   (blocker `load-order-violation`) when `plan.order` differs from the
   composer's priority/id order — i.e. only dependency-consistent priority
   sets install. This keeps the vendored composer untouched and honest.
3. **Commit** — at the existing records commit block (modinst.c:1128-1148)
   call `mm_install_commit(&store, &plan)` right where per-mod states are
   currently stamped, then `records_save()`. The module mutates records
   nowhere else; the plan is pure, so crash windows are unchanged: recovery
   (`recover_title`, modinst.c:862) stays authoritative, and after it
   resolves the live root, `mm_recover(loadlist, permod_dirs, generation)`
   replaces the hand-rolled row repair in `records_commit_from_root`
   (modinst.c:815-850) to get the disabled-state mapping for free.

## 3. Toggle/uninstall endpoints

New service commands (behind `mfs_request`, contract/modforge_service.h),
each one journal-wrapped like cmd_install:

- `enable --title T --mod X` → `mm_enable` guards → per-mod backend:
  layout 1: remove `Mods/X/enabled.txt` (journal phase `toggle`, cheap
  single-file transaction); layout 0: run the full install transaction with
  the re-derived enabled set (`plan` from mm_enable's internal order).
- `disable --title T --mod X` → `mm_disable` guards (enabled-dependent
  refusal maps to EX_NOTREADY with `reason`) → inverse of enable.
- `uninstall --title T --mod X [--force]` → `mm_uninstall` guards → install
  transaction with the surviving set (modinst already does exactly this via
  `--remove`; add the force-cascade id list from the module).

`--mods`/`--remove` CLI parsing (split_mods) already handles the sets; the
module's plan tells the transaction *which* set, the journal stays
per-title.

## 4. Runtime/per-mod directories (roadmap M2 wording)

UE4SS-family mods keep `Mods/<id>/` + `enabled.txt` in the runtime root
(`layout=1`); merged-container asset mods stay in the composed root
(`layout=0`). `mm_recover`'s `permod_dirs` input is the dirent listing of
the runtime mod root; the load-list ids remain the merged truth. Do NOT
mix: a mod is either layout 0 or 1 for its lifetime (changing layout is an
update, a new prepare+install).

## 5. Failure surfacing

`read_runtime_status` (modinst.c:1229) already parses the runtime status
file; extend `cmd_records` to call `mm_mark_failed(id, detail)` when it sees
`state=failed`, so the Failed state only ever enters from that ack (plus
recovery integrity failures). `cmd_simulate_runtime` keeps its role as the
host-side ack writer for tests.

## 6. Test/regression notes

- Keep modinst's existing 108/108 green: the only behavioral change for
  single-mod flows is the load-order check (identical unless a mod declares
  `requires`, which none of the existing fixtures do).
- Add: the T-permutation determinism check, dependency refusal strings,
  legacy-records parse, toggle idempotency, force-cascade, and the four-way
  classification matrix — all already encoded in `test_multi_mod.c`
  (184 checks) and portable to the slice's PlayMode-style harness.
- Crash-point fixtures to add host-side: crash between toggle journal
  writes (new `toggle` phase) and the existing stage/swap crashes.

## 7. Honest gaps (not designed here, named for the implementing worker)

- `mfx_compose_ordered` vendored change (touched file = needs the
  composition gate owner, R-COMP4-01 lineage).
- Disk-use bytes per mod for the UI header (acquisition queue owns byte
  budgets; the module only enforces the mod-count gate).
- Real classification corpus for the family/shared-policy table (M2's
  "one documented conflict case" exit criterion maps to the
  container-repack case T1 exercises; more families need real bundles).
- Console E2E for ≥3 coexisting mods (M2 exit criterion, out of scope for a
  host prototype).
