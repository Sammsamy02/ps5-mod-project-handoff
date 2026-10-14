# Agent R — l25 twin-variant build machinery (Claude-approved l25 item 5, prep only)

Author: agent-R, 2026-10-08. Mac-only. No console/network contact. No toolchain
build executed (plan/dry-run only). No file under `typescan-r4/candidate/src/`,
`typescan-r4/include/`, `tests/`, or `profile/` was written by this agent;
`build_runtime.py` was imported, never modified.

## 1. Deliverables

| File | Role |
|---|---|
| `outputs/2026-10-08-l25-runtime/build_variants.py` | Twin-variant runtime build driver. Produces variant A (normal registration) and variant B (`-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`, diagnostic only) as independent pass1/pass2 twins from one tree state, with `--plan` (no toolchain) and `--execute` modes. |
| `outputs/2026-10-08-l25-runtime/l25_packaging_driver.py` | Per-variant packaging driver. Stages `outputs/2026-10-08-l25-package-A/` and `-B/` from the frozen l24 packaging scripts, derives per-variant `BUILD-INPUTS.json`, runs the two packaging passes per variant, enforces the l24 acceptance set (20/20 static checks, 238/2 constructor arrays, 207 exact import identities, identical PRX twins), and writes final + cross-variant receipts. Modes: `--plan` / `--prepare` / `--execute`. |
| `outputs/2026-10-08-l25-build-variants/plans/PLAN.json` | Current dry-run plan for the runtime stage (662 steps, 45 gates; ~8 MB — includes every planned compile argv with input hashes). |
| `outputs/2026-10-08-l25-build-variants/plans/PACKAGING-PLAN.json` | Current dry-run plan for the packaging stage (57 gates). |
| This report | Operator procedure for the final twin build. |

Output layout at `--execute`:

```
outputs/2026-10-08-l25-build-variants/
  staging-copyin-probe/UnrealInitializer.cpp   (composed second hop; see 3)
  l25-A/  pass1/ pass2/ (runtime-l25-A.o each), SOURCE-SELECTION.json,
          TWIN-RECEIPT.json, CHANGE-SELECTION.json, ACTUAL-DEPENDENCIES.json,
          VARIANT-MARKER.json, headers.vfs.json
  l25-B/  ... same, runtime-l25-B.o, VARIANT-MARKER.json carries the UNSAFE label
  VARIANT-PAIR-RECEIPT.json                     (A != B proof, delta confinement)
  EXECUTION-RECEIPT.json
outputs/2026-10-08-l25-package-A/  pass-1/ pass-2/, FINAL-RECEIPT-VARIANT-A.json, ...
outputs/2026-10-08-l25-package-B/  ... FINAL-RECEIPT-VARIANT-B.json (UNSAFE labeled)
  VARIANT-PAIR-PACKAGING-RECEIPT.json (in l25-build-variants/)
```

## 2. Variant contract compliance

- Exact spelling used everywhere (driver constant, argv injection, receipts):
  `MODBRIDGE_L25_SKIP_DELETE_LISTENER`, injected as the single token
  `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER` at `argv[1:1]`.
- Per agent O's final state, the switch wraps BOTH delete-listener
  registrations (composed file lines 546 and 561). Variant B therefore skips
  BOTH registrations; receipts say `delete_listener_registration:
  "skipped (diagnostic only)"`.
- Variant B is labeled unsafe in `VARIANT-MARKER.json`, every
  `COMPOSITION-RECEIPT.json`, `TWIN-RECEIPT.json`, the packaging
  `PREPARATION-RECEIPT.json` note line, `FINAL-RECEIPT-VARIANT-B.json`, and
  both pair receipts. Label text: stale object pointers are NOT cleared;
  diagnostic A/B isolation only; never promote long-term.
- `UnrealInitializer.cpp` itself was never edited by this agent.

## 3. copyin-probe integration (agent Q, per coordinator instruction)

Q could not ride the one-hop replacement (WIRING.md); `build_variants.py` now
performs the wiring for BOTH variants (observation-only instrumentation
belongs in A and B alike):

- **UE4SS side (unit `unreal-unrealinitializer`)**: the driver composes a
  second hop in its OWN staging dir
  (`outputs/2026-10-08-l25-build-variants/staging-copyin-probe/UnrealInitializer.cpp`).
  Base = the CURRENT typescan-r4 candidate (O's `efdef253…`); Q's
  pure-addition patch is applied on top with `/usr/bin/patch --forward`
  (no fuzz, no .rej tolerated; offsets allowed). Q's ready-made
  `125a0471…` file is adopted ONLY if the base is still byte-identical to
  Q's recorded base `c4364dd7…` — since O landed, that path is impossible,
  so the rebase path is used and O's listener edits are preserved (they are
  never reverted). Verified composition right now: applies cleanly, composed
  sha256 `68ecf9d81e296d402bd71c42c687284d2ff217ec7a5dd88b33f44d389da0bf97`,
  contains both O guards AND Q's `CopyinProbeLiveness` additions, diff vs
  O base = 0 deletions / 96 additions. Quoted includes of the unit resolve
  through -I/vfs overlay, not the source directory (verified: no sibling
  headers in `candidate/src/`), so staging outside typescan-r4 is safe.
- **Bridge side**: `l25_packaging_driver.py` substitutes
  `BUILD-INPUTS.json outer_bridge` with Q's patched
  `runtime_start_bridge.cpp` (`f510c014…`, base `89e4b643…` asserted equal to
  the l24 manifest's outer_bridge). The frozen controller then stages it
  into the lineage exactly as l24 did.
- **Receipts**: Q's MANIFEST files+hashes (both patches, both ready
  candidates, both bases, composed overlay) are surfaced as
  `copyin_probe_wiring` in CHANGE-SELECTION, VARIANT-MARKER,
  COMPOSITION/TWIN receipts, both pair receipts, PREPARATION-RECEIPT and
  FINAL-RECEIPT-VARIANT-*.json, and both plan JSONs (l24-style unit-hash
  surfacing). `QPART/MANIFEST.json` and the composed file are pinned into
  each variant's SOURCE-SELECTION.json.

## 4. Z9 audit integration (edit-path report)

- **Loud no-op guard (G2r)**: every CHANGES row's original must be consumed
  by the l24 composition — unit source, dep-universe path, inherited overlay
  target, or alias-identical (name,digest) universe file. `.json/.ini/.md`
  lineage rows are reported but allowed; any unconsumed code row stops
  `--execute` (plan reports it). Current tree: 22 rows, all classified, none
  NOT-CONSUMED (e.g. `UnrealInitializer.cpp`→unit-source,
  `UObjectArray.hpp`→dep-universe, `townfall-build-facts.json`→lineage-data).
- **O-units proof (G2t + G11)**: `unreal-uobjectarray`,
  `unreal-flat-classlistener`, `lua-luauobject` (via the UObjectArray.hpp
  alias), `unreal-unrealinitializer` must all be in `affected_units` and must
  all receive FRESH objects at execute time (G11 fails if any kept an
  inherited l22/l24 object — the silent no-op signature).
- **Live drift discipline**: plan mode reports pin drift without stopping
  (parallel agents mid-flight); `--execute` keeps the hard l24 pin
  discipline. A green plan is wiring proof, not pin health.

## 5. Dry-run results on the current tree state (2026-10-08 ~15:0x)

- `build_variants.py --plan`: exit 0. 22 change rows verified (O's final 18
  typescan rows + 4 profile rows), 162 affected units / 212 retained, 26
  header aliases, 662 steps, 45 gates, all green except none. Token found in
  exactly one file (the composed overlay); `token_units =
  ['unreal-unrealinitializer']`; conservative define scope NOT triggered.
  Variant A planned argv carries no define; variant B carries
  `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER` on exactly `unreal-unrealinitializer`
  in both passes (2 of 324 planned compiles).
- `l25_packaging_driver.py --plan`: exit 0. All 8 frozen l24 source pins
  verify against the l24 manifest; l25 profile overlay candidates present;
  57 gates (runtime artifacts correctly PENDING-BUILD).
- Packaging prepare smoke test (mock variant builds in /tmp, deleted after):
  staging OK for both variants; the derived manifests PASS the frozen
  `prep_guard.require_ready()`; `outer_bridge` resolves to Q's `f510c014…`;
  lineage overlays resolve to the l25 profile candidates
  (`RuntimeBuildProfile@39a56800`, `CompiledNativeLayoutDeclarations@b158080f`,
  `JoinedLayoutPreflight@1646a99e`); adapted controller differs from the
  frozen one by exactly the one `b.note(...)` line and compiles; re-prepare
  after staging verifies-and-reuses (no silent overwrite, no drift).
- Note: `agent-K review fixes` and phase-tag work are reflected in whatever
  rows/hashes the owning agents last wrote; the final `--plan` before
  `--execute` is the authoritative pin check.

## 6. Gate inventory

Runtime stage (per variant unless noted): G1 BASE selection pins (15457);
G2 part CHANGES pins; G2r rows-consumed; G2s unit-source rows affected; G2t
listener units affected; G2q Q deliverable pins + second-hop override;
G3 fresh output dirs; G4 token present (execute refuses a no-op variant B);
G5 define units; G6 link census 391 unique inputs; G7 llvm-nm undefined set
== BASE baseline (no import drift; `new_os_imports` is empty); G8 pass1/pass2
twin identity (bytes + per-unit object hashes); G9 A != B (pair); G10
cross-variant object deltas confined to define units (pair); G11 O-units got
fresh objects.

Packaging stage: P0 runtime inputs exist; P1 dir fresh/reuse-verified; P2
frozen sources pin; P3/P4 controller passes exit 0; P5 BUILD-RECEIPT
MAC_PACKAGE_CHECKS_PASS; P6 20/20 static checks; P7 exactly 207 exact ELF
import identities (NID/provider/version decoded, getter present); P8
constructor arrays 238/2 preserved; P9 pass-1/pass-2 PRX twins identical;
P10 variant A/B PRXes differ; P11 import name sets equal (207 each). The
in-packager gates (readelf census vs manifest lists, 191+16 contract
pre-check, entry alignment, fself magic/segments/digest) run unchanged
inside the frozen controller.

## 7. Operator procedure — final twin build (after all code agents confirm landing)

All commands from `/WORKSPACE/.codex/worktrees/translation-core/what`.
Nothing below touches the console. Serialize with other heavy local work
(16 GB Mac).

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what

# 0. Preconditions (human/coordinator): listener instrumentation, phase tags,
#    review fixes landed; owning agents refreshed their CHANGES.json hashes;
#    no agent is mid-edit (single owner per file at build time).

# 1. Validate wiring + pins on the final tree (no toolchain runs):
python3 outputs/2026-10-08-l25-runtime/build_variants.py --plan
#    EXPECT: exit 0; G2 rows all ok; G2r no NOT-CONSUMED; G2t four units true;
#    G4 token present in exactly the composed overlay; token_units =
#    ['unreal-unrealinitializer']; conservative false. If G2q reports
#    patch-failed-rebase-needed, agent Q (or coordinator) must rebase the
#    unrealinitializer patch onto the new base first.

# 2. Twin runtime builds (A then B; each a pass1/pass2 twin):
python3 outputs/2026-10-08-l25-runtime/build_variants.py --execute
#    EXPECT: exit 0; prints per-variant/pass runtime pins; gates G1..G11 true;
#    artifacts under outputs/2026-10-08-l25-build-variants/ (see section 1).
#    Any failure: do NOT rerun in place (dirs refuse overwrite by design);
#    inspect the receipts/logs, fix root cause, move the failed tree aside.

# 3. Packaging dry-run on the built variants:
python3 outputs/2026-10-08-l25-runtime/l25_packaging_driver.py --plan
#    EXPECT: exit 0; no P-await-runtime-artifacts gates remain.

# 4. (Optional) stage and inspect the two package dirs:
python3 outputs/2026-10-08-l25-runtime/l25_packaging_driver.py --prepare

# 5. Packaging passes (2 per variant) + acceptance + final receipts:
python3 outputs/2026-10-08-l25-runtime/l25_packaging_driver.py --execute
#    --execute re-runs prepare in verify-and-reuse mode if 4 was done.
#    EXPECT: P0..P11 all true; FINAL-RECEIPT-VARIANT-A/B.json written;
#    VARIANT-PAIR-PACKAGING-RECEIPT.json written.

# 6. Record the four headline artifacts for review:
#    outputs/2026-10-08-l25-build-variants/VARIANT-PAIR-RECEIPT.json
#    outputs/2026-10-08-l25-build-variants/VARIANT-PAIR-PACKAGING-RECEIPT.json
#    outputs/2026-10-08-l25-package-A/FINAL-RECEIPT-VARIANT-A.json
#    outputs/2026-10-08-l25-package-B/FINAL-RECEIPT-VARIANT-B.json
```

## 8. Console discipline reminders (for whoever deploys later — NOT this task)

- This task made and will make NO console contact. Upload approval is
  pending and separate.
- The packaged `modbridge_supplier_composition.prx` files are FSELF-wrapped
  (magic `4f153d1d`) by the frozen pipeline itself; never re-sign or rewrap
  by hand, and never upload a downloaded/modified executable unsigned
  (bricks the game; see the 2026-10-06 postmortem).
- ANY future upload of either PRX must be verified by decrypt-OFF readback
  (`curl -Q "SITE SELF"`): magic `4f153d1d` AND full sha256 equality with the
  corresponding FINAL-RECEIPT pin BEFORE launch. Plain-ELF (`7F454C46`) in a
  download means the decryption default bit you.
- Label variant B artifacts distinctly when staging (e.g.
  `modbridge_supplier_composition.l25B.diagnostic.prx`) and re-hash after any
  rename; keep the UNSAFE stale-pointer warning attached wherever B is named.
- No rapid-fire launch tests; kstuff quarantine and ploader autoload rules
  stand (PS5 rules in AGENTS.md).

## 9. Failure modes worth knowing (all loud, none silent)

- Unrefreshed CHANGES.json hash after an agent edit → G2 fails at --execute.
- A row keyed on a non-consumed original → G2r fails (Z9's silent no-op).
- Q patch stops applying cleanly on a future O revision → G2q hard-fails at
  --execute demanding a rebase; plan shows patch-failed-rebase-needed.
- Any new OS import → G7 (llvm-nm vs l24 baseline) and, later, the frozen
  packager's census both stop the build.
- Variant B accidentally identical to A (guard missing/no-op) → G4 at plan
  and G9/G10 at execute.
- Output dirs already exist → refused (l24 no-overwrite discipline).

## 10. Addendum (E3 routing fix + Z10 hash audit, same day)

**E3 (Z13 combined compile-smoke):** Z13 proved on the real prospero-clang
that `typescan-r4/candidate/include/Unreal/UObjectArray.hpp` (the 5-slot
vtable ABI repair, GetAllocatedSize at +0x30) was not routed by
`target_compile.py` (only include/, candidate/generated_include/, src parent)
— and no l25 headers.vfs.json exists yet (plan mode writes none; the l24 BASE
overlay has zero UObjectArray entries by design). Diagnosis of the twin
machinery: the route WAS already implicitly present at --execute (the
include_dirs rule adds parents of replaced .hpp candidates, and the
(name,digest) alias maps the port-root copy to the candidate), but it was
unproven and invisible to the part-local smoke. Fixes:

- `typescan-r4/target_compile.py:4-9` — the part smoke now derives
  `-I` routes from the header-row candidates (union of `files` + legacy
  `changes` keys), so `candidate/include` is on its include chain exactly
  like the real build. Routing only; no source edits.
- `build_variants.py:161-176` — new gate **G12b** (coverage half): every
  BASE consumer of every replaced header must be recompiled. Result:
  UObjectArray.hpp → 162 consumers, 0 retained; all 10 header rows 0 gaps.
- `build_variants.py:178-236 + 378-384 + 425-431` — new gate **G12**
  (resolution half): for EVERY affected unit, in every variant/pass (plan:
  planned argv; execute: actual argv, hard-fail), clang include lookup is
  simulated for each replaced-header spelling (-I dirs in argv order, then
  the merged vfs overlay) and must resolve to the l25 candidate. Proof for
  UObjectArray.hpp (identical in A/B, pass1/pass2, 162 units each):
  - spelling `UObjectArray.hpp` → winner
    `typescan-r4/candidate/include/Unreal/UObjectArray.hpp`
    sha `69a2391aae357945f7f26350e179b96f2b968950edd759a47daddd9e2307f71d`
    (l25 candidate) via `-I typescan-r4/candidate/include/Unreal` (direct);
  - spelling `Unreal/UObjectArray.hpp` → SAME winner `69a2391a…` via the
    port-root `-I` + **vfs-overlay redirect** (the merged alias maps the
    port-root path to the candidate; this is the entry headers.vfs.json will
    carry at execute);
  - replaced original (port root): `b01bf4ef84638bf6fa2fb52386d3a3f53ad6f2bdd27de6c898eda417b5554ce1`.
    Both digests stated per the coordinator's request.
  - violations: none, 4/4 pass-variant combos.
- Affected units: **162 before and after** the E3 fix — no delta, no
  regression (the routing was already in the argv; the fix made it explicit,
  gate-enforced, and visible to target_compile.py). PLAN.json now carries
  `header_routing_proof`; total gates 50 (was 45).

**Z10 bookkeeping (bridge sha):** audited every hash my drivers reference
against disk. Verdict: **no dangling pins**.
- Q's cited bridge base `89e4b643…` matches TWO on-disk files:
  `outputs/2026-10-08-combined-outer-lifecycle/candidate-v2/runtime_start_bridge.cpp`
  (the l24 manifest's `outer_bridge`) and
  `outputs/2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp`
  (Q's recorded `base_path`). Z10's "actual ba095f56" matches no bridge file
  anywhere I can find (v1 candidate = `1261971f…`, v2 = `89e4b643…`); it
  appears to be a bookkeeping error on Z10's side, not a defect in Q or the
  drivers.
- My packaging driver pins `outer_bridge` = Q's bridge candidate
  `f510c01484e3edcc…` (on-disk verified) and asserts the l24 manifest's
  `89e4b643` equals Q's recorded base before substituting — so a future
  drift of either file fails loudly at `--prepare/--execute`, never blind.
- Also re-verified: Q patches (`6771e33e…`, `2681497d…`), Q UE4SS ready
  candidate (`125a0471…`), outer_entry (`bc490893…`), and O's current
  UnrealInitializer.cpp (`efdef253…`) all exist and match. The full 106-pin
  sweep still happens inside the frozen `prep_guard.require_ready()` at
  packaging time.

Re-run after the fix: `build_variants.py --plan` exit 0 (50 gates, 662
steps); `l25_packaging_driver.py --plan` exit 0 (57 gates). No --execute
performed (still root-gated on Z12 + Z13 refix).

## 11. Addendum 2 — execute-failure E-COMP1 fix (copyin-probe composition order)

**Failure (root's execute):** variant A pass1, `unreal-unrealinitializer`:
`staging-copyin-probe/UnrealInitializer.cpp:982: use of undeclared identifier
'CopyinProbeLiveness'`. The execute-time rebase of Q's patch onto the drifted
base (`cac95e48`, post-AO-G3) placed the struct-definition hunk at composed
:1067 — AFTER the call site (:982). The hunk's generic admission-context
matched a later similar region. G2q had only checked token presence, not
definition/use ORDER.

**Fixes (coordinator-sanctioned scope: patch file + build_variants.py):**

1. `copyin-probe/patches/unrealinitializer-copyin-probe-liveness.patch`
   REGENERATED (new sha `0f2ecdab87010a83cfe522d11b784d1e81871313f52df6bbbdddcc6ac9cdf496`,
   was `2681497d…`): the struct-definition hunk is re-anchored from the
   post-admission region to the stable early landmark — immediately after the
   extern-C bridge declaration block / `#include "NativePropertyReadBackend.inl"`,
   BEFORE `struct L13AdmissionOps` (the enclosing struct of the probe lambda).
   All three hunks' CONTENT is byte-identical to Q's original (87 + 1 + 8
   added lines plus one separator blank = 97 added lines, 0 deletions);
   only anchor/line numbers changed. New hunks: `@@ -852,6 +852,94 @@`
   (struct), `@@ -979,6 +1067,7 @@` (probe-lambda call), `@@ -1220,6
   +1309,14 @@` (post-complete note).
2. `copyin-probe/MANIFEST.json` — `patch_sha256` updated to the new hash
   plus a `reanchor_note` provenance block (old/new hashes, reason). This is
   the manifest bookkeeping OF the patch; without it G2q would (correctly)
   hard-fail on the stale pin.
3. `build_variants.py` — new gate **G2q-order-def-before-use** (hard fail in
   BOTH plan and execute): after every composition, `struct CopyinProbeLiveness`
   (line of first mention overall) must precede every `CopyinProbeLiveness::`
   use; and for the bridge side, every patch-added static
   (`g_copyin_probe_attempts`, `g_copyin_probe_attempts_late`,
   `g_copyin_probe_last_note_ms`, `g_copyin_retirement_logged`,
   `kCopyinProbeNoteIntervalMs`) must have its definition as the first
   occurrence in the ADOPTED bridge candidate. The bridge is never rebased
   (whole-file adoption), and its defs (lines 915-919) precede all uses
   (1103+) — gate now enforces this permanently. Positive and negative
   (synthetic define-after-use) tests both pass.

**Verification (no --execute, per instruction):**
- New patch applies to the pristine current base `cac95e48` cleanly: rc=0,
  no .rej, no fuzz. Composed sha `44b05cc6d8d054d334603a723f70deb3241e3be6cc2d129473ff6f1a4a21e4c4`.
- ORDER in the composed file: struct def at :886 (first mention :886),
  `struct L13AdmissionOps` at :960 (enclosure), member-def uses :939-941,
  probe-lambda call :1070, post-complete call :1317. Definition strictly
  before all uses and before the enclosing struct.
- Syntax compile on the REAL prospero front-end (Z13 /tmp/l25-combined-smoke
  recipe: receipt argv, source swapped to the composed file, E3-fixed -I
  roots, `-fsyntax-only`): **rc=0**, 6 pre-existing warnings only (deprecated
  asmjit header, undefined-inline from l24-era base headers) — the
  undeclared-identifier error is gone.
- `build_variants.py --plan` re-run: exit 0, now **53 gates** all green;
  plan receipt carries the order evidence and composed sha `44b05cc6…`.
- NOTED: `LuaMod.cpp:8078` now also carries a
  `#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` guard (parallel agent
  work). The driver's token detection handles it data-driven: variant B now
  defines the token on exactly `['luamod','unreal-unrealinitializer']`
  (conservative mode off); G10 confines cross-variant object deltas to
  exactly those units. Affected units unchanged at 162.
- Root owns the --execute retry; the operator procedure in section 7 is
  unchanged (step 1's plan now also shows G2q-order-def-before-use).

## 12. Addendum 3 — execute postmortems (retries 1+2) and full pre-clearance

### 12.1 Failure postmortems (honesty box)

**RUN 1 (archived `l25-A.failed-run1-164331`):** variant A pass1 compile of
`unreal-unrealinitializer` failed with `use of undeclared identifier
'CopyinProbeLiveness'` — the execute-time rebase of Q's patch onto the
drifted base (`cac95e48`) placed the struct-definition hunk at composed
:1067, AFTER the probe-lambda call (:982); the hunk's generic
admission-context matched a later similar region. Guard gap: G2q checked
token presence, not definition/use order. FIX (section 11): patch
re-anchored to the stable pre-`L13AdmissionOps` landmark (new sha
`0f2ecdab…`, MANIFEST pin + reanchor_note), new hard gate
**G2q-order-def-before-use** (both modes), verified by clean apply + real
prospero `-fsyntax-only` rc=0 on the composed TU (`44b05cc6…`).

**RUN 2 (archived `l25-A.failed-run2-165532`):** the composed unit now
COMPILED (2.1 MB object emitted), and the include-graph guard then rejected
its own `.d`: the composed staging file
(`2026-10-08-l25-build-variants/staging-copyin-probe/UnrealInitializer.cpp`)
is a new prerequisite path outside the approved universe (l24 baseline deps +
HERE + approved (name,sha) headers). Diagnosis: correct guard, missing
whitelist for driver-generated substitution sources. FIX: attributable
whitelist — compile_unit now exempts ONLY paths listed in
`sel['driver_staged_sources']` (populated solely from this run's compose
step: the composed path AND its sha). A staged path with a mismatched hash
is still rejected; any other new path is still rejected. The whitelist and
its pins are recorded in CHANGE-SELECTION.json (`driver_staged_sources`),
DEPENDENCY-GRAPH.json (`driver_staged_source_pins`) and the execution
receipt. Replay against run 2's actual `.d` files: 13/13 units clean
including `unreal-unrealinitializer` (9 new deps; 8 resolved under l25-runtime,
1 = the whitelisted composed file).

### 12.2 Pre-clearance of the remaining execute path

Verified against run 2's partial artifacts (13 objects + .d under
`l25-A/pass1/objects/`) plus real BASE data:

| Stage | Status |
|---|---|
| Include-graph guard with whitelist | **verified-against-partial-artifacts** (13/13 `.d` replay clean) |
| Remaining ~149 pass1-A compiles | same code path as the 13 that succeeded; guard replay class cleared |
| DEPENDENCY-GRAPH accumulation (`variant_pins`) | **verified-against-partial-artifacts** (replay: 1414 well-formed pins; composed staging pin present with matching sha `44b05cc6…`) |
| ACTUAL-DEPENDENCIES / SOURCE-SELECTION writes | code-path review: recompute uses `deps()` on the NEW `.d` paths recorded in the pass1 receipt — inputs proven by the replay above; needs-full-run for the write itself |
| G6 link census (391) | **rehearsed on real BASE data**: `ordered_inputs` = 391 unique, each of 374 unit objects exactly once; the driver's census construction (old_to_new mapping + `-o` exclusion) reproduced 391/391 identity-mapped; needs-full-run for the real gate |
| G7 import-delta | comparator code byte-identical to the proven build_runtime lines 143-144 (grep-verified at build_variants.py:501-502); both BASE `undefined.stdout` baselines exist; needs-full-run (requires the linked runtime) |
| G8 twin identity / G11 O-units fresh | mirrored from the l24-proven build_runtime flow; needs-full-run |
| G9 variants-differ / G10 delta confinement | pair-level, plan-inputs verified (define_units = `['luamod','unreal-unrealinitializer']`, conservative off); needs-full-run |
| pass2 + variant-B loop start conditions | code-path review: per-pass fresh dirs, per-pass route_agg, variant_pins accumulation across passes (intentional, mirrors build_runtime), shared pinned staging file (`44b05cc6…` on disk, hash-gated), define injection verified in plan argv for both token units |
| Receipt writes after link | pure writes into the fresh variant dir; targets pre-created |

Residual risks (noted, not blocking): none known in the driver path between
the guard and the receipts; the remaining unknowns are exactly the
needs-full-run gates (G6-G11 at real scale), which are the same proofs the
l24 flow passed at this scale.

### 12.3 Current state

- Run-2 partial archived as
  `outputs/2026-10-08-l25-build-variants/l25-A.failed-run2-165532/`
  (forensics preserved; run-1 archive intact).
- `build_variants.py --plan` re-run after the fix: **exit 0, 53 gates**; the
  only non-true gate is `G2q-copyin-probe-pin-unrealinitializer.base`
  (informational by design: base legitimately drifted from Q's recorded
  `c4364dd7` to `cac95e48`; rebase path taken; patches/candidates all pin).
- No `--execute` performed; root owns retry #3 (procedure unchanged,
  section 7).
