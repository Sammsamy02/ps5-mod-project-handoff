# Agent IG — P1 (fault-ledger desaturation) build-staging for the AC window

2026-10-10/11. Battery rules honored: /tmp staging + syntax/plan checks only;
NO builds, no `--execute`, no console/network contact; live tree untouched
(post-session sha sweep in §7). Inputs: FX's parked patch set
`parked/P1-fault-ledger/{01-fault-ledger-desaturation.patch, NOTES.md}`,
`agent-FX-parked.md`, PHASEB-PLAN.md GP-correction cross-ref (§5.5 line
367-369: the [KB] fail-closed doctrine explicitly mirrors the P1 fault-ledger
doctrine), the composed-tree CopyinSink lineage, and both drivers.

## 1. APPLY-CHECK VERDICT: PASS

Staging: `/tmp/p1-stage-IG/what` (real copy of `outputs/2026-10-08-l26-runtime`,
symlinks for every other read-only `outputs/*` tree + `.superpowers`, real
writable `2026-10-08-l26-build-variants`; staging-local path rewrite of the
four JSONs whose rows must resolve to patched files — AFTER apply, see §6).

- `/usr/bin/patch -p1 --fuzz=0` (dry-run + real): **4 files patched, zero
  rejects, zero fuzz**. Driver hunk lands at line 170 (+14 offset vs header —
  see drift §4); patch kept a `.orig` backup byte-identical to the live
  pre-patch driver (`810efaae…`).
- Post-patch shas **byte-match FX's construction exactly**:
  - bridge candidate → `a35c7f9111b1f91cead80a05d489db04f6d4a25bb32b6bf0bee09879785baca1`
  - NEW `copyin-probe/candidate/CopyinSink.hpp` → `0b113b1c19b4eac7f9959567487845c8639a28937f94a4881103dcbb163e69cd` (5054 B)
- Both drivers `py_compile` clean; host `clang++ -std=c++20` compiles the new
  header and the runtime self-test exits 0 (mask 0xF) — FX's authoring check
  independently reproduced.
- `build_variants.py --plan`: RC=0, 74 gates, 823 steps. The single `ok:false`
  gate (`G2q-copyin-probe-pin-unrealinitializer.base`) is by-design base-pin
  expectation drift (armored 5f7ae2bc state) — identical on the unpatched
  baseline. `G2q-…runtime_start_bridge.candidate` is GREEN against the new
  a35c7f91 pin; second-hop compose mode `patch-rebased-onto-revised-base`;
  G-stamp-stale-cleanup consumed the stale stamp inside /tmp only.
- `l26_packaging_driver.py --plan`: RC=0, 37 gates, 7 steps, zero failures.
  Derived-manifest instrumentation proves the P1 wiring for BOTH variants:
  `outer_bridge` == `wired_outer_bridge` == a35c7f91; `fault_ledger_overlay`
  section present (0b113b1c replaces frozen 2018c9bd); `lineage_overlays`
  gains its 6th destination `CopyinSink.hpp`; the `lineage_source_pins`
  CopyinSink row repointed 2018c9bd → 0b113b1c.
- Paired BASELINE runs on a byte-identical UNPATCHED stage (`/tmp/p1-baseline-IG`):
  identical gate counts, **zero ok-state flips anywhere**; only detail-level
  deltas (all intended, table in GATE-EXPECTATIONS.md §2). All 808 compile-unit
  argv sets identical after normalization; the composed second-hop
  UnrealInitializer byte-identical (a85d7b5f).

Composition chain verified end-to-end (how P1 reaches the PRX): frozen l24
manifest `lineage_source_pins` carries lineage-root `CopyinSink.hpp`
(2018c9bd) → controller `package_combined.py` copytrees the lineage root to
`stage/`, writes `stage/lifecycle/runtime_start_bridge.cpp` from
`outer_bridge` (a35c7f91), then writes `lineage_overlays` destinations —
the hunk-4 row `CopyinSink.hpp` (0b113b1c) shadows the frozen copytree file
BEFORE the bridge TU's `#include "CopyinSink.hpp"` (bridge line 897) compiles.

## 2. What the AC command sequence is (the one-command-per-step card)

Full runbook: `round5-staging/P1/BUILD-CARD.md`. Compact:

1. APPLY — `patch -p1 --fuzz=0 < parked/P1-fault-ledger/01-fault-ledger-desaturation.patch`
   then `python3 round5-staging/P1/p1_ac_row_updates.py` (the JSON row updates,
   §3). Preconditions + verification commands: `round5-staging/P1/APPLY.md`.
2. VERIFY — the two construction shas above + host self-test (APPLY.md §3).
3. SYNC+SNAPSHOT — `SNAP=$(harvest/snapshot_tree.sh | awk '/^SNAPSHOT-ID /{print $2}')`; record it.
4. BUILD — `BRIDGE_SNAPSHOT_ID="$SNAP" python3 …/build_variants.py --execute`
   (archive `l26-build-variants/l26-A|B` AND `l26-package-A|B` first — both
   drivers refuse overwrites, and the package-dir reuse check would fail on
   the old bridge pin).
5. PACKAGE — `l26_packaging_driver.py --prepare` then `--execute`; P-gates
   P0–P13 unchanged (20/20 static, 207 imports, twins identical, A/B differ).
   P1 receipt checks: BUILD-INPUTS `fault_ledger_overlay` + 6th overlay row;
   STAGED-SOURCE-PINS `lineage/CopyinSink.hpp` == 0b113b1c.
6. VERIFY — `harvest/grep_markers.py <prx> --marker copyin_fault_ledger_st …`
   (all four new tags composed in; count 0 = header did not reach the PRX =
   STOP). Optional markers.json registration 54→55 (noted as count change).
   Console leg (separate AC gate): one-shot early `copyin_fault_ledger_st=15`
   / `_st_pass=1` in supplier-diag.jsonl.

## 3. Row updates the patch does NOT carry (staging census finding)

`harvest/bind_launch.sh --all` preflight gate 4 enforces
`harvest/pipeline-pins.json` — MISSED by FX's NOTES §4 census:
- `pins.bridge_gated`: checks MANIFEST row AND disk == `94f81a17…` → must
  become `a35c7f91…` or every --all launch aborts.
- `pins.pkgdrv_all`: pinned `ee3aa230…` but the live driver is `810efaae…` —
  ALREADY STALE PRE-P1 (driver edited 2026-10-11 02:28Z by the P8 238→239
  rebaseline, pins file dated 2026-10-10 03:16Z). P1's AC is the moment to
  re-baseline honestly; post-P1 value `6d05f322…` if live base stays 810efaae.

`round5-staging/P1/p1_ac_row_updates.py` performs these two edits (plus
verifies/repairs the MANIFEST copyinsink rows): refuses unless the code files
byte-match FX's construction; py_compile-guards the driver pin; idempotent;
writes `p1_row_update_receipt.json` with old values for rollback. Tested in
staging: change-run, idempotent re-run, and negative-run on the unpatched
tree (correctly aborts: "bridge candidate is 94f81a17…, expected a35c7f91…").

## 4. Drift from FX's expectations (all reconciled, none blocking)

1. `l26_packaging_driver.py`: FX authored hunk 4 against `086e5149…`; live is
   `810efaae…` (post-FX edits: HH sidecar P12/P13 + P8 238→239 rebaseline).
   Hunk context unaffected → clean apply at +14 lines offset; behavior
   verified by plan runs. FX's "086e5149 (current)" note is stale, nothing more.
2. `pkgdrv_all` pin staleness (§3) — pre-existing, not P1-caused.
3. Pre-existing cosmetic inconsistency in the live driver (flagged, NOT
   P1's): P8 execute-gate checks `.init_array == 239` (frozen efbfc656 build
   shows init_slots 239) while its PLAN text and two FINAL-RECEIPT literals
   still say 238.
4. Staging artifacts (documented so nobody chases them): the JSON path
   rewrites (live→/tmp prefix, applied AFTER the patch because hunk 3's
   context carries the original absolute paths); macOS `/tmp` vs
   `/private/tmp` aliasing (drivers compare `str(resolved)` against JSON keys
   verbatim — canonicalize to `/private/tmp`); the vfs-overlay
   `planned_sha256` delta in the paired PLAN.json diff is prefix-string only
   (51==51 aliases; P1 adds no alias — at AC the overlay hash is expected
   UNCHANGED; if it moves at AC, stop).

## 5. Console-visible behavior: NOTHING beyond logging (verified, cited)

- Fail-closed gate line BYTE-UNCHANGED: `bridge_copyin_range_faulted` is
  byte-identical live vs patched (regex-extracted both, `==` true), including
  `if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no
  ledger read`. The patch contains NO hunk touching it.
- Bridge TU delta census (difflib, live vs patched): **+37 / −1 lines**; the
  single deletion is `g_copyin_faults.record(addr, len);` (return previously
  discarded) replaced by result handling whose branches only emit one-shot
  `diagnostic_event`s (hunk 2c); hunk 2a adds two one-shot log-guard statics;
  hunk 2b adds `bridge_fault_ledger_self_test()` (LOCAL ledger;
  `g_copyin_faults` never touched) + its call first in
  `bridge_copyin_directory_prepare()`. 22 added non-comment lines, all
  logging/self-test.
- Header (hunk 1): `overlaps()` for VALID queries unchanged (same loop, same
  real-range semantics; invalid query ranges still fail closed `return true`);
  `record()` semantic changes confined to the two never-normal arms — invalid
  range (unreachable today: the only call site pre-checks `valid_range`) and
  capacity-64 (previously poisoned ALL queries; now observable only via
  `saturated_capacity()`, which the admission gate never calls). Real recorded
  faults still refuse. No import delta (`diagnostic_event` already an
  undefined symbol of the composed object).
- No UE4SS.log/HUD/menu changes: P1 emits only supplier-diag diagnostic events.

## 6. Staging procedure notes (for reproduction)

Apply order matters: patch FIRST, then the staging path-rewrite (hunk 3
context contains the original absolute paths). The rewrite covers
`copyin-probe/MANIFEST.json` + the three parts `CHANGES.json` (G2
`is_relative_to(HERE)` fidelity); everything else resolves via absolute
live paths or symlinked `outputs/*` + `.superpowers`. Both drivers run with
cwd = stage worktree root (dep files carry root-relative `.superpowers/…`
entries). This rewrite is /tmp-ONLY — at AC the live absolute paths are
already correct and NO rewrite happens.

## 7. Live-tree integrity (post-session sweep)

bridge `94f81a17…`, MANIFEST `f310a7a3…`, packaging driver `810efaae…`,
frozen lineage sink `2018c9bd…`, stale stamp `c92dc562…` (still present —
my plan runs deleted only the /tmp copy), pipeline-pins mtime pre-session;
no `CopyinSink.hpp` candidate live; live `plans/PLAN.json` mtime predates the
session; no `.rej`/`.orig`/`__pycache__` in the live tree. Writes this
session: `round5-staging/P1/*` + this report + /tmp only.

## Deliverables

- `round5-staging/P1/APPLY.md` — exact apply procedure + preconditions + rollback
- `round5-staging/P1/p1_ac_row_updates.py` (+ tested; receipt on run) — the AC JSON edits
- `round5-staging/P1/EXPECTED-MARKERS.md` — new [ST]/diag markers, PRX grep, console signature
- `round5-staging/P1/GATE-EXPECTATIONS.md` — gate-flip table (none flip; detail deltas listed)
- `round5-staging/P1/BUILD-CARD.md` — the one-page AC runbook
- /tmp evidence: `/tmp/p1-stage-IG` (patched stage, plan logs, host self-test),
  `/tmp/p1-baseline-IG` (paired baseline)

## Honest limits

No target compile, no `--execute`, no packaging run, no console session (the
[ST]=15 signature remains console-unproven until the planned AC session —
by design per FX's NEVER-APPLY-WITHOUT flag). The plan-mode pin-remap section
of the staged packaging manifest is not AC-identical (staging symlinks vs
live prefixes) — the P1-relevant sections (outer_bridge, wiring,
fault_ledger_overlay, overlays, sink pin) were verified identical by direct
instrumentation. P2 interplay: P2's `03-packaging-driver.patch` is authored
ON TOP of P1 hunk 4 — apply P1 first (unchanged guidance).

— agent IG (ZCode), 2026-10-10/11.
