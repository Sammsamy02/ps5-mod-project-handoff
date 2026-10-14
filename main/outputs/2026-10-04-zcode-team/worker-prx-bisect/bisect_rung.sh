#!/usr/bin/env bash
# bisect_rung.sh — SKELETON (controller-owned; NOT executed by worker-prx-bisect)
# One bounded rung of the post-fold bisect. See BISECT-PLAN.md sections 2-3.
# Usage: bisect_rung.sh <rung-id>   e.g. rung-1 = GO + String.hpp only
set -euo pipefail

WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
BISECT=$WHAT/outputs/2026-10-04-zcode-team/worker-prx-bisect
L11=$WHAT/outputs/2026-10-04-zcode-controller/launch11-combined
LN=$WHAT/outputs/2026-10-04-zcode-team/worker-luamod-lineage
RUNG="${1:?rung id required}"
VAULT=$BISECT/vault/$RUNG-pre
RUNG_LOG=$BISECT/RUNG-LOG.md

# ---- variant table (edit the SUBSET per rung; everything else stays GO) ----
# GO bytes:
GO_BRIDGE=$LN/provenance-originals/bridge.faec2336.UNAVAILABLE   # SEMANTIC excision only (see plan)
GO_INIT=$WHAT/outputs/2026-10-04-zcode-team/worker-init-v4r3/staging/UnrealInitializer.cpp   # 22f06286
GO_SEEDVAL=$LN/provenance-originals/SeedValidation.hpp.orig                                   # b3d81b0f
GO_PROGRAM=$LN/offline-proof/sanity-program-baseline/UE4SSProgram.cpp                         # 7b5d4636
GO_LUAMOD=$LN/provenance-originals/LuaMod.cpp.orig                                            # 3c39a86b
GO_STRING=$WHAT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp  # 7c983db4
SEEDVAL_DB07=$BISECT/vault/reconstructed-SeedValidation.db07ac60.hpp   # built by step 0 (verified forward-apply)
RUNG9_PROGRAM=$LN/offline-proof/program-widen-ascii-68b8bd18/UE4SSProgram.cpp  # 68b8bd18 (undo edit 9 only)

expect_sha() { # expect_sha <sha256> <file>
  local want="$1" f="$2" got
  got=$(shasum -a 256 "$f" | awk '{print $1}')
  [ "$got" = "$want" ] || { echo "SHA MISMATCH $f: $got != $want"; exit 2; }
}

# step 0 — one-time: reconstruct db07ac60 (edit-8 "before") from archived diff
#   cp $LN/provenance-originals/SeedValidation.hpp.orig $SEEDVAL_DB07
#   patch $SEEDVAL_DB07 $LN/diffs/SeedValidation.hpp.b3d81b0f-to-db07ac60.diff
#   expect_sha db07ac602cd9e57aeb864780874aa7e9b16158d25aefdc200c32ffd564871f83 $SEEDVAL_DB07

# step 1 — VAULT current (candidate) bytes, hash-verified
mkdir -p "$VAULT"
cp -p $L11/launch10-build-final-units-bridge 2>/dev/null || true   # skeleton: enumerate real paths
# for f in <canonical paths being swapped>; do cp -p "$f" "$VAULT/$(basename $f)"; done
# sha-verify each vault copy == pre-swap live sha (record in vault/MANIFEST)

# step 2 — SWAP variant bytes into canonical paths + verify each
#   (paths stay constant so every pin below remains structurally valid)
# bridge (SEMANTIC excision of edits 3/7/11 — remove #include <sys/syscall.h>,
#   #include <errno.h>, and the whole bridge_kernel_range_verdict block):
#   THEN offline one-TU compile MUST equal GO bridge object 195ed762... BEFORE any pass.
# expect_sha 22f06286... $L11/units/UnrealInitializer.cpp        # when reverting edit 4
# expect_sha b3d81b0f... $L11/units/SeedValidation.hpp           # edits 1/2 mirror
# expect_sha b3d81b0f... $L11/../launch10-build/final-units/SeedValidation.hpp
# expect_sha 7b5d4636... <wpd1 UE4SSProgram.cpp>                 # edits 5+9
# expect_sha 3c39a86b... <luamod-l11-merged/LuaMod.cpp>          # edit 6
# expect_sha 7c983db4... <overlay lineage-overlay-2/Helpers/String.hpp>  # edit 10

# step 3 — EDITIONS.json refresh (units[] shas, bridge_cli.sha256,
#   compose.luamod_merged / initializer_armed / admissions.initializer_fold,
#   comparators.luamod_object=fe4c8113... luamod_source=3c39a86b... when edit 6
#   reverts; bump "edition" marker with "$RUNG")

# step 4 — runner stage-header pins (String.hpp, SeedValidation.hpp shas)
#   ONE-TIME at bisect start: record runner drift (EDITIONS says c81f89c7,
#   disk is 1e7b4831) and re-pin EDITIONS to the on-disk runner.

# step 5 — driver: COMPOSE6_FIXED_SHA = sha256(fixed script as swapped)
#   if bridge reverted: remove bridge_kernel_range_verdict from l.692 tolerated
#   set and from l.733 _bridge_acc; restore checkpoint undefined-delta.json
#   expected_bootstrap_resolutions to the GO 4-symbol set.

# step 6 — fixed script: remove bridge_kernel_range_verdict from linker_provided
#   (only when bridge reverted)

# step 7 — OFFLINE PRE-CHECKS (all mandatory, fail-closed):
#   python3 -m py_compile fixed driver runner
#   bridge one-TU offline compile (pinned argv) -> llvm-nm; sha MUST equal 195ed762...
#   luamod offline object (sanity label first) -> fe4c8113 / 8343578b per variant
#   runner unit-sha verification stage (reads EDITIONS) must PASS

# step 8 — two-pass determinism (controller executes the runner; asides accumulate)
#   python3 $L11/launch11_one_pass.py --full     # candidates prx-candidate-l11-{1,2}
#   record: composition_object, combined, bridge object, final/bare PRX shas; pair must agree

# step 9 — launch + sdiag triage (controller; console contact out of this worker's scope)
#   FIRST sdiag event must be tag=module_start_entered
#   absent  -> killer is IN this rung's reverted-IN subset (or incoherent revert: recheck step 7)
#   present -> subset acquitted; advance ladder per BISECT-PLAN.md section 2

# step 10 — verdict line
#   echo "$RUNG | variant=... | first_event=... | verdict=..." >> $RUNG_LOG
echo "SKELETON ONLY — do not execute without controller authorization" >&2; exit 3
