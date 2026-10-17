# l25 execute-retry pre-verification — compose/order/compile (agent AW)

Snapshot verified (all hashes pinned at 2026-10-08 16:36:44 CDT, re-checked stable 16:42):
- base UI `typescan-r4/candidate/src/UnrealInitializer.cpp` = `cac95e48...` (mtime 15:30:36, unchanged)
- re-anchored UI patch `patches/unrealinitializer-copyin-probe-liveness.patch` = `0f2ecdab...` (agent-R, landed 16:34:18; manifest pin updated 16:34:37)
- driver `build_variants.py` = `cc0a6772...` (agent-R, 16:35:20 — added `check_definition_order` / gate `G2q-order-def-before-use`)
- bridge base `2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp` = `89e4b643...` (matches manifest pin and the packaging driver's outer_bridge expectation)
- bridge patch = `6771e33e...`, bridge ready candidate = `f510c014...` (both match manifest pins)

## 1. Failure root cause (closed loop)

The 16:27 execute composed with Q's ORIGINAL patch (`2681497d`): struct `CopyinProbeLiveness`
inserted at SeedArming **namespace scope AFTER `struct L13AdmissionOps` had closed** (staging
:1067), while the use `CopyinProbeLiveness::note_probe_call(...)` sits inside
`L13AdmissionOps::probe` (staging :982). Namespaces have no complete-class context →
`use of undeclared identifier`. Reproduced byte-exactly: recompiled the preserved failed
staging file with the preserved failed argv (`l25-A/pass1/objects/unreal-unrealinitializer.command.json`,
exit 1) as `-fsyntax-only` → identical `:982:17 error`. NOTE: Q's patch was inherently
order-broken — Q's own ready candidate (`125a0471`, `builds_run:false`) has use@915 < def@1000;
the 16:27 build was its first-ever compile.

## 2. Per-patch verdicts

### unrealinitializer-copyin-probe-liveness.patch (re-anchored, 0f2ecdab)
- Driver-exact apply (`/usr/bin/patch --forward --no-backup-if-mismatch -i <patch> <target>`,
  fresh tempdir, absolute target): **RC=0, no .rej, no fuzz, zero offset** on base `cac95e48`
  → composed = `44b05cc6d8d054d334603a723f70deb3241e3be6cc2d129473ff6f1a4a21e4c4` (148877 B).
  This is byte-what the retry's `patch-rebased-onto-revised-base` path will write (manifest
  base pin `c4364dd7` ≠ current base → patch path, correct).
- Independent order check (own script, not the driver's): struct def@886; uses:
  `note_probe_call`@1070, `note_admission_complete`@1317, static-defs@939-941 — **def < every
  use for all introduced symbols**; all pre-existing symbols used by added lines resolve earlier.
- Syntax-compile (exact failed argv → `-fsyntax-only`, l25-A overlay, proper
  `LLVM_BINDIR`/`LLVM_CONFIG` env): **RC=0, 0 errors, 6 warnings** (all pre-existing:
  Atomic.hpp -Wswitch ×2, UnrealType.hpp enum-conversion ×2, asmjit pragma; base control =
  same 6). **ORDER-SAFE + COMPILES.**

### runtime_start_bridge-copyin-probe-liveness.patch (6771e33e)
- Driver-exact apply on pinned base `89e4b643`: **RC=0, clean** → output **byte-identical** to
  Q's ready candidate `f510c014` (proves candidate = base + patch, no hidden edits).
- build_variants.py does NOT apply this patch (bridge is packaging-side); the l25 packaging
  driver pins base (as l24 outer_bridge sha) and substitutes the ready candidate into
  BUILD-INPUTS (`l25_packaging_driver.py:99-113`); compile happens in the frozen l24 controller.
- Independent order check: all five introduced statics def@915-919, first use@1103 —
  **def < every use**; `diagnostic_event` pre-declared at base :132; all other pre-existing
  symbols used by added lines resolve earlier. **ORDER-SAFE.**
- Syntax-compile with the controller's EXACT preserved flags (prospero-clang++ `-std=c++20
  -Wall -Wextra -Werror -O1 ... -I lineage/*`): **RC=0, zero warnings** (base control: RC=0).
  First compile this candidate has ever had (`builds_run:false`). **COMPILES.**

## 3. Other execute-time mutations in build_variants.py (cc0a6772) — enumeration + verdicts

| # | Mutation | Verdict |
|---|----------|---------|
| M1 | `apply_q_patch` → staging composed UI file (+ readback verify) | SAFE (verified above; G2q pins + G2q-order gate guard it) |
| M2 | `headers.vfs.json` overlay write (merged_aliases) + per-unit `-ivfsoverlay` swap | SAFE — single-hop mapping, no alias-of-alias chaining; G12 `header_route_check` simulates include resolution per unit and hard-fails non-l25 winners; all 17 route checks passed at 16:27 |
| M3 | Per-unit argv rewrite (`rewrite_compile` source/obj/dep with single-occurrence asserts; inject new overlay + l25 `-I` dirs + `-I <source.parent>` at argv head) | SAFE — 16/17 affected units compiled clean at 16:27 with this exact shape; the 17th (unrealinitializer) failed only on M1 content, now fixed. Sibling-header scan of every injected source-parent dir: headers exist only in `l19-r4-owner-variant/candidate` (FField.hpp, ffield's own unit) and `r5-source-integration/source` (2 hpp, that unit's own) — per-unit injection only, no cross-unit hijack |
| M4 | Variant-B define injection `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER` (define_units, argv head) | SAFE — TOKEN present ×3 in base and composed; G4/G5 presence, G9 differ, G10 delta-confinement gates |
| M5 | Link rewrite (old→new object map, `-o`, `-Map=`) | SAFE — G6 census (391 unique inputs, each fresh object exactly once), fixed_inputs pinned |
| M6 | Receipt/JSON writes (TWIN/SOURCE-SELECTION/CHANGE-SELECTION/etc.) | SAFE — data-only, no ordering semantics |
| M7 | **DRIVER BUG: `build_variants.py:465` `variant_pins=actual_pins`** | **HAZARD — MUST FIX.** Sits inside the per-pass `if execute:` block; `actual_pins` is first bound at :521 (post-loop). Pass 1 end → **UnboundLocalError after all 17 units compile successfully, before the pass-1 link**. The 16:27 run never reached it (died in the compile pool). Fix: delete line 465 (`variant_pins` is already correctly accumulated at :463 from each unit's fresh dep file; the post-loop `actual_pins` at :521 feeds only the final receipts and needs no back-copy), or move the assignment after :524. One-line fix; verified semantics of both variants. |
| M8 | Operational: `l25-A/` exists from the failed run | MUST clean before retry — driver refuses to overwrite existing variant dirs (`refuse to overwrite`). Archive/remove `outputs/2026-10-08-l25-build-variants/l25-A/` (staging-copyin-probe/UnrealInitializer.cpp is overwritten unconditionally — no action needed). `l25-B` never created. Re-run `--plan` first (16:26 PLAN.json predates the re-anchor). |
| M9 | Packaging stage: outer_bridge substitution (base pin `89e4b643` matches live file; candidate pin `f510c014` verified) + adapted controller whose only diff is the `b.note(...)` label | SAFE — verified above; happens after build_variants succeeds |

Residual observations (non-blocking):
- Manifest still pins the STALE ready UI candidate `125a0471` with old base `c4364dd7`. If the
  base ever reverted to `c4364dd7`, adopt-ready-candidate would load the order-broken candidate —
  but the new G2q-order gate turns that into a loud failure, never silent breakage. Recommend
  refreshing the ready candidate (new base + 0f2ecdab → `44b05cc6`) when convenient.
- G7 (llvm-nm undefined-set equality): patch adds calls only to `diagnostic_event` (already an
  undefined import of the runtime object) and internal `Output::send`; all new statics are
  TU-internal → no new imports expected; gate enforces at execute.

## 4. GO/NO-GO

**CONDITIONAL GO.**
- Composition-safe: BOTH patches verified order-safe and syntax-compiling against their current
  bases with the driver's own tool and flags; composed UI = `44b05cc6` deterministically; bridge
  candidate verified byte-exact and warning-free under `-Werror`.
- TWO blocking conditions before the execute retry:
  1. Fix `build_variants.py:465` (`variant_pins=actual_pins` UnboundLocalError) — otherwise the
     retry fails again AFTER a full successful pass-1 compile.
  2. Remove/archive `outputs/2026-10-08-l25-build-variants/l25-A/` (driver refuses existing
     variant dirs); re-run `build_variants.py --plan` (post-re-anchor), then `--execute`.

Evidence: `/tmp/l25-compose-verify/` (order_check.py, ui-composed-newpatch.cpp, syntax-*.stderr,
snapshot/ with pinned copies).
