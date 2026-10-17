# DESIGN.md — M2 multi-mod lifecycle (N-mod state machine + conflict classification)

worker-multi-mod, 2026-10-04. Basis: `worker-7-installer/src/modinst.c` (the tested
single-mod transaction slice) and `worker-product-roadmap/PRODUCT-ROADMAP.md` M2.
Working prototype: `multi_mod_lifecycle.{h,c}`, tests `test_multi_mod.c`
(184 checks, 0 failures, host C99). This is design + prototype; nothing here is
console-proven, and no claim is made that M2 is complete.

## a. N-mod state machine

### Stored per-mod states (records.txt `mod` line, state column)

Extends the single-mod store `{downloaded, prepared, installed}`; `running`
stays derived-only, exactly as modinst.c treats it (the runtime status file is
the only source; never stored).

```
downloaded          archive registered; bytes on disk; not validated
prepared            validated + analyzed for the title; NOT in the game root
installing          transaction in flight (records never persist it: recovery
                    resolves it deterministically)
installed_enabled   files in root AND named in the active load list
installed_disabled  files in root, NOT in the active load list (toggle target)
failed              evidence-backed failure of the mod's last action
```

Key move: the old single `installed` splits into **enabled/disabled** so the
roadmap's "per-mod `enabled.txt`" toggle is a *state*, not a re-download. Two
backends realize it (module is agnostic; `layout` field selects):

- `layout=1` per-mod directory (UE4SS-style `Mods/<mod>/` + `enabled.txt`):
  disable = remove `enabled.txt` (cheap, no re-compose).
- `layout=0` merged container compose (the modinst root): disable = one
  transaction that re-composes the surviving set (new generation, same
  journal stage/swap discipline).

### The transaction unit is the SET, not the mod

modinst.c is already set-atomic: one journal, one root swap, the game sees the
old set or the new set. M2 keeps that and multiplies the membership:
`mm_plan_install(new_ids)` computes target = currently-enabled ∪ new, then the
host wraps `plan` in the existing journal transaction and calls
`mm_install_commit` only after the rename swap (the exact place modinst
commits records today). Toggle/uninstall of ONE mod is a set transaction with
n±1 membership.

### Transition table (what each op checks, what it mutates)

| op | requires | refuses with | mutates |
|---|---|---|---|
| declare | fresh id, token | bad_request | new row `downloaded` |
| prepare | downloaded / prepared / failed | state | `prepared`; records classification in `reason` (modinst semantics: prepare records the analysis even with blockers; install refuses) |
| plan_install | prepared/downloaded/installed ids; no failed member | state (failed: "re-prepare before installing"); blocked_dependency (names missing dep + versions); conflict (Unsupported/Needs-conversion, actionable); capacity; order (cycle) | nothing (pure) |
| install_commit | a committed root swap | — | every target-set member → `installed_enabled`, reason cleared, generation++ |
| enable | `installed_disabled` (idempotent if enabled) | state; blocked_dependency (deps must be enabled NOW — no silent auto-enable); conflict re-classification vs active set; capacity | → `installed_enabled`, generation++ |
| disable | `installed_enabled` (idempotent) | state; blocked_dependent if any **enabled** transitive dependent (names them; disabled dependents are fine — they are not loading) | → `installed_disabled`, generation++ |
| uninstall | installed (either) | state; blocked_dependent: force=0 refuses with names; force=1 still refuses if ANY transitive dependent is enabled, else cascades all disabled dependents → `prepared` with reason "uninstalled with dependency X" | → `prepared` (cache/analysis kept — same rule as modinst's records_commit_from_root) |
| mark_failed | any but installing | — (evidence hook: runtime status `state=failed` ack, or recovery integrity failure; never predicted) | → `failed` + reason kept verbatim |
| clear_failure | failed | state | → `prepared` (re-validate before enabling) |
| recover | live-root truth | — | see below |

### What happens when mod A depends on mod B

Declared in the bundle modinfo (`requires id>=min`, already parsed by the
vendored loader). Rules, all with actionable reasons:

1. **Install/enable A**: every dep of A must be in the *enabled* set with
   version >= min (`mm_ver_cmp` semantics; integration should call the
   vendored `mfx_ver_cmp`). Missing/too-old → `blocked_dependency`, never a
   silent auto-install (auto-including deps is the acquisition queue's M3
   job, which already builds prerequisite-first plans).
2. **Disable/uninstall B while A is enabled**: refused
   (`blocked_dependent`, names A). Computed transitively (BFS over declared
   edges restricted to installed mods), so a chain c→b→a cannot be broken
   mid-way through the *active* part.
3. **Disable B while A is merely installed-disabled**: allowed. A is not
   loading; if the user later enables A, rule 1 refuses until B returns.
4. **Uninstall B** with A installed-disabled: force=0 refuses (honest: files
   in root would orphan); force=1 cascades A (and A's disabled dependents)
   to `prepared` with a reason naming B. Any *enabled* dependent still
   refuses under force.
5. **Version gate**: dep minimum enforced at every install/enable, not just
   install time (enable re-checks; the test T8 shows 0.9.0 vs required
   1.0.0 refused citing both versions).
6. **Order**: dependency loads before dependent (see c).

## b. Conflict classification — the four states

User-facing, per mod (vs the active set), worst-of wins; each verdict carries
an actionable reason string. Detection rules in evaluation order:

1. **Failed** — *never* produced by static analysis. Only from runtime
   evidence: the runtime status file acking `state=failed` for this mod
   (modinst's `read_runtime_status` already surfaces it) or a recovery
   integrity failure. `mm_class_merge` lets a runtime Failed dominate any
   static verdict. This is the honesty rule: a predicted failure is a
   Unsupported, not a Failed.
2. **Unsupported** —
   - a required framework has no PS5 runtime component, or its caps-table
     maturity is `planned`/`missing` (modinst's blocker vocabulary); or
   - the mod family's caps verdict is `unsupported` (no PS5 backend — the
     "Supported only with an implemented PS5 backend" rule); or
   - family unknown to the capability matrix; or
   - **shared-container collision**: two mods write the same destination
     path and either family's `shared_policy` is `refuse` (e.g.
     `container-repack` over a `.pak`/`.utoc`): override semantics are
     undefined, refuse rather than guess byte precedence.
3. **Needs conversion** — the family's caps verdict is `conversion`
   (different container/family: bsa/ba2, me3-style overrides): installable
   only through the verified on-console converter path.
4. **Supported** — all of the above pass. Two flavors, both honest:
   - clean: no shared destination paths;
   - **last-wins overlap**: shared paths where both families'
     `shared_policy` is `last-wins` (asset-override). Deterministic winner =
     later in load order (same semantics mfx_compose already implements);
     the reason string names both mods and the shared path so the UI can
     show "Asset Overhaul overrides Asset Base on text/ids.txt".
   - `integrating` frameworks do not block (modinst treats them as
     warnings); the reason carries "in-game execution not proven".

The capability inputs are data (`mm_caps`: framework maturity + family
verdict/shared-policy), seeded from the same engine facts modinst loads as
`caps.tsv` — per-title/per-family status stays data, never code (M4 rule
anticipated).

## c. Load order

modinst/mfx_compose order the *compose-time* set by (priority, id) — a total
order, but blind to dependencies. M2 extends it:

```
mm_order_set: Kahn topological sort over the enabled set's dependency edges
(dependency before dependent); among simultaneously-ready nodes pick the
lowest (priority, id).
```

- Deterministic: the test runs all 6 request permutations of {a,b,c} and gets
  the identical order (a, b, c) — priorities (100, 50, 200) alone would have
  put b first, violating a→b; topo+tie-break fixes that.
- Cycle → `MM_ERR_ORDER` with an honest refusal, never a silent reorder.
- Integration path: mfx_compose keeps its own sort for the 2-mod proven
  case; for N mods the host passes the computed order to an extended
  `mfx_compose` entry point that takes an explicit permutation (see
  INTEGRATION.md). When the priority order already satisfies the edges (the
  common case) both orders coincide, so behavior is unchanged for
  dependency-free sets.
- Enable/disable also re-derive the order for the resulting enabled set
  (the module refuses enable-on-cycle the same way), and each membership
  change bumps the game generation — the number already recorded per game
  row and journaled.

## d. Records: tracking N mods

Line store stays TSV, one row per mod, **append-only columns** after the
existing five (`mod id title state bundle`):

```
mod <TAB> id <TAB> title <TAB> state <TAB> bundle <TAB> version <TAB>
    priority <TAB> layout <TAB> reason <TAB> deps
deps := "id>=min;id2>=min2" | "-"
```

- Old parser reads new lines (stops after column 5, ignores the rest) and the
  new parser reads old lines (nf==5 → version "0", priority 100, layout 0,
  no deps) — proven by test T11's legacy fixture. Neither side needs a flag.
- New columns carry what the state machine needs to *re-derive* its guards
  after a cold start: deps (rule engine), version (dep gate), priority/layout
  (order + toggle backend), reason (the UI's per-mod actionable line).
- `game` row unchanged: `title, engine, sdk, generation, loadlist_sha` —
  generation and whole-root sha remain the SET-level truth the journal and
  recovery already use.
- **Concurrency rules**: single-writer per title. All N-mod mutations are
  one transaction (one journal per title, one records_save after the swap);
  there is never a second concurrent writer, so no per-mod locking is
  needed — the same serialization rule the 16GB Mac host discipline already
  imposes on editor writers. Records mutate only at commit points
  (`mm_install_commit`, and the in-module toggles which the host wraps in a
  transaction the same way).
- **Recovery** (`mm_recover`, mirroring `records_commit_from_root`): the
  active load list is post-crash truth. Named in load list →
  `installed_enabled`; per-mod directory present but unnamed →
  `installed_disabled`; otherwise any `installing`/installed residue drops
  to `prepared` with a recovery reason. The stage/swap journal phases of
  modinst decide *which* root is live; this mapping then repairs rows.
  Generation is adopted from the live root, not from the records.

## e. UI states (mod list)

Per-row derived state (`mm_ui_state_of`) + legal actions (`mm_ui_actions_of`)
— the buttons render from the same guards the ops enforce, so the UI can
never offer an action the engine would refuse:

| row state | shown as | reason line | offered actions |
|---|---|---|---|
| downloaded | Not Installed | — | Install |
| prepared | Ready | classification note (e.g. "integrating") | Install, Remove |
| installing | Updating… | — (in flight) | none |
| installed_enabled | Enabled | last-wins note if any | Disable, Update, Uninstall |
| installed_disabled | Disabled | — | Enable, Update, Uninstall |
| failed | Failed | the stored evidence reason, verbatim | Retry (re-prepare), Uninstall |

Plus the header aggregate: title, generation, enabled/total, and the worst
classification banner across the set. Every refusal reason string produced by
the module is written to be user-displayable ("cannot disable 'X': 1 enabled
mod(s) depend on it (ui-menu); disable them first") — the M5 error-taxonomy
work can map these 1:1.

## What this does not claim

No console run, no compose/root work, no UI implementation — this is the M2
*state machine and classification* design as exercised code, per the task
bounds. Exit-criteria items still open for M2: real multi-mod E2E on
console, disk-budget bytes (the module has the count gate; byte budget
stays with the acquisition queue), and the conflict-matrix shakedown.
