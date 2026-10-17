#!/usr/bin/env bash
# plan-validate-round5.sh — battery-safe PLAN-MODE validation of the fully
# restored + additions round-5 state, entirely under /tmp (agent II, 2026-10-11).
#
# Builds a /tmp mirror that the build driver can run --plan against WITHOUT
# touching the live tree (the driver resolves ROOT from its own location, so
# placing it at /tmp/r5-plan/outputs/2026-10-08-l26-runtime makes every
# HERE-relative read + every plan write stay under /tmp/r5-plan):
#
#   /tmp/r5-plan/outputs/
#     2026-10-08-l26-runtime/            REAL copies: build_variants.py,
#                                       build_runtime.py, profile/, thread/,
#                                       typescan-r4/{candidate,include,tests,
#                                       CHANGES.json}  (results/ + original/
#                                       not read by --plan; excluded)
#     2026-10-08-l26-build-variants/
#                                       staging-copyin-probe/ (real copy,
#                                       MANIFEST paths repointed at the
#                                       mirror), l26-A -> LIVE symlink (the
#                                       BASE overlays argv references),
#                                       plans/ (writable receipt dir)
#     2026-10-08-l25-build-variants/l25-A/  verbatim copies of
#                                       SOURCE-SELECTION.json +
#                                       pass1/COMPOSITION-RECEIPT.json
#                                       (their absolute pins/deps/overlays
#                                       keep pointing at LIVE files, which
#                                       --plan only READS — the 18k-pin
#                                       sha sweep therefore also re-proves
#                                       the live round-4 tree is intact)
#   /tmp/r5-plan/prep/                   round4-setaside/ + harvest/
#                                       markers-declared.json (real copies)
#
# CHANGES part files (profile/typescan-r4/thread) get their `candidate`,
# `added_headers`, `added_header_hashes` and `include_directories` paths
# rewritten LIVE-runtime-prefix -> mirror prefix so every candidate-side
# check hits the RESTORED mirror files; `original` fields stay live (they
# key the BASE replacement mapping, which must keep matching the live
# BASE receipts — that matching is precisely what this run proves).
#
# Then: restore-round5.sh (apply) -> apply-additions-round5.sh (apply,
# --allow-pending for the in-flight [FL-MAP] sockets) ->
# python3 <mirror>/build_variants.py --plan  -> journal assertions:
#   - G6-link-input-census planned_inputs == 396 (391 + 5 NEW_UNITS)
#   - every G2-part-change-pins row ok (restored CHANGES rows consistent)
#   - G1-base-selection-pins ok (live BASE pins intact)
#   - G2q-* copyin-probe pins ok + second-hop compose outcome recorded
#     (patch-rebased-onto-revised-base expected: the wiring changed the base)
#   - no gate whose ok == false without an accepted-tolerance reason
#
# NO builds, NO console contact, NO live-tree writes (the only live reads
# are the driver's own pin/dep/overlay verifications).
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
LIVE_WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
LIVE_RT=$LIVE_WHAT/outputs/2026-10-08-l26-runtime
LIVE_PREP=$LIVE_WHAT/outputs/2026-10-08-l25-prep
LIVE_BASE=$LIVE_WHAT/outputs/2026-10-08-l25-build-variants/l25-A
LIVE_BV=$LIVE_WHAT/outputs/2026-10-08-l26-build-variants

MROOT=${R5_MIRROR_ROOT:-/tmp/r5-plan}
M=$MROOT/outputs
MRT=$M/2026-10-08-l26-runtime
MPREP=$MROOT/prep

say() { echo "[plan-validate] $*"; }
die() { echo "[plan-validate] FAIL: $*" >&2; exit 1; }

say "mirror root: $MROOT"

# ---------------------------------------------------------------- 1. mirror
rm -rf "$MROOT"
mkdir -p "$MRT" "$MPREP/harvest" "$M/2026-10-08-l25-build-variants/l25-A/pass1" "$M/2026-10-08-l26-build-variants/plans"

# 1a. drivers + parts (real copies)
ditto "$LIVE_RT/build_variants.py" "$MRT/build_variants.py"
ditto "$LIVE_RT/build_runtime.py"  "$MRT/build_runtime.py"
for part in profile thread; do
  [ -d "$LIVE_RT/$part" ] && ditto "$LIVE_RT/$part" "$MRT/$part"
done
# 1b. typescan-r4 minus results/ + original/ (not read by --plan)
mkdir -p "$MRT/typescan-r4"
ditto "$LIVE_RT/typescan-r4/candidate" "$MRT/typescan-r4/candidate"
ditto "$LIVE_RT/typescan-r4/include"   "$MRT/typescan-r4/include"
ditto "$LIVE_RT/typescan-r4/tests"     "$MRT/typescan-r4/tests"
ditto "$LIVE_RT/typescan-r4/CHANGES.json" "$MRT/typescan-r4/CHANGES.json"
# 1c. BASE receipts verbatim (absolute pins intentionally still LIVE):
#     SOURCE-SELECTION + pass receipts + the G7 llvm-nm baselines the plan
#     reads (pass{1,2}/undefined.stdout)
ditto "$LIVE_BASE/SOURCE-SELECTION.json" "$M/2026-10-08-l25-build-variants/l25-A/SOURCE-SELECTION.json"
for pp in pass1 pass2; do
  mkdir -p "$M/2026-10-08-l25-build-variants/l25-A/$pp"
  [ -f "$LIVE_BASE/$pp/COMPOSITION-RECEIPT.json" ] && ditto "$LIVE_BASE/$pp/COMPOSITION-RECEIPT.json" "$M/2026-10-08-l25-build-variants/l25-A/$pp/COMPOSITION-RECEIPT.json"
  [ -f "$LIVE_BASE/$pp/undefined.stdout" ] && ditto "$LIVE_BASE/$pp/undefined.stdout" "$M/2026-10-08-l25-build-variants/l25-A/$pp/undefined.stdout"
done
# 1d. QPART (HERE/copyin-probe) real copy — its MANIFEST pins repoint below;
#     l26-A symlink (BASE overlay jsons argv-referenced live)
ditto "$LIVE_RT/copyin-probe" "$MRT/copyin-probe"
mkdir -p "$M/2026-10-08-l26-build-variants/staging-copyin-probe"
ln -s "$LIVE_BV/l26-A" "$M/2026-10-08-l26-build-variants/l26-A"
# 1e. prep mirror (stash + markers + round5-staging for the world-fix patches)
ditto "$LIVE_PREP/round4-setaside" "$MPREP/round4-setaside"
ditto "$LIVE_PREP/harvest/markers-declared.json" "$MPREP/harvest/markers-declared.json"
ditto "$LIVE_PREP/round5-staging" "$MPREP/round5-staging"

# 1f. repoint the QPART MANIFEST at the mirror (so G2q rebases Q's patch onto
#     the MIRROR's restored+wired UnrealInitializer.cpp)
python3 - "$MRT/copyin-probe/MANIFEST.json" "$LIVE_RT" "$MRT" <<'PY'
import sys
path, live_rt, mrt = sys.argv[1], sys.argv[2], sys.argv[3]
raw = open(path, encoding='utf-8').read()
assert live_rt in raw, 'MANIFEST does not reference the live runtime tree as expected'
raw = raw.replace(live_rt, mrt)
open(path, 'w', encoding='utf-8').write(raw)
print('[plan-validate] QPART MANIFEST repointed at the mirror')
PY

# 1g. rewrite the three CHANGES part files: candidate-side paths -> mirror
python3 - "$MRT" "$LIVE_RT" <<'PY'
import sys, json
mrt, live_rt = sys.argv[1], sys.argv[2]
for part in ('profile', 'typescan-r4', 'thread'):
    p = f'{mrt}/{part}/CHANGES.json'
    try:
        raw = open(p, encoding='utf-8').read()
    except FileNotFoundError:
        print(f'[plan-validate] note: no {part}/CHANGES.json in mirror'); continue
    n = raw.count(live_rt)
    raw = raw.replace(live_rt, mrt)
    json.loads(raw)
    open(p, 'w', encoding='utf-8').write(raw)
    print(f'[plan-validate] {part}/CHANGES.json: {n} live-runtime path(s) repointed at the mirror')
PY

# ------------------------------------------------- 2. restore + additions
say "running restore-round5.sh against the mirror"
bash "$HERE/restore-round5.sh" --tree "$MRT" --prep "$MPREP" > "$MROOT/restore.log" 2>&1 \
  || { tail -30 "$MROOT/restore.log"; die "restore failed in the mirror (log: $MROOT/restore.log)"; }
tail -3 "$MROOT/restore.log"

say "running apply-additions-round5.sh against the mirror"
set +e
bash "$HERE/additions/apply-additions-round5.sh" --tree "$MRT" --prep "$MPREP" --what "$MROOT" --allow-pending \
  > "$MROOT/additions.log" 2>&1
ARC=$?
set -e
tail -5 "$MROOT/additions.log"
[ $ARC -eq 0 ] || die "additions failed in the mirror rc=$ARC (log: $MROOT/additions.log; rc=2 means unresolved [FL-MAP] sockets even with --allow-pending — inspect)"

# ------------------------------------------------------------- 3. --plan
say "driver --plan (full selection/gate computation; NO toolchain; cwd = the what root because BASE pins carry what-relative paths)"
( cd "$LIVE_WHAT" && python3 "$MRT/build_variants.py" --plan ) > "$MROOT/plan.log" 2>&1 \
  || { tail -40 "$MROOT/plan.log"; die "driver --plan crashed (log: $MROOT/plan.log)"; }

# ------------------------------------------------------------- 4. verdicts
python3 - "$MROOT/plan.log" "$MROOT" <<'PY'
import sys, json, re
log_path, mroot = sys.argv[1], sys.argv[2]
gates = []
other = []
for line in open(log_path, encoding='utf-8', errors='replace'):
    line = line.strip()
    if not line:
        continue
    try:
        j = json.loads(line)
    except json.JSONDecodeError:
        other.append(line)
        continue
    if 'gate' in j:
        gates.append(j)
    else:
        other.append(j)

print(f"[plan-validate] gate records: {len(gates)}; non-gate JSON lines: {len(other)}")

def g(name):
    return [x for x in gates if x['gate'] == name]

# census expectation: G6 planned 391+5=396
g6 = g('G6-link-input-census')
if not g6:
    print('[plan-validate] HOLE: no G6-link-input-census record'); sys.exit(1)
planned = g6[0]['detail'].get('planned_inputs')
print(f"[plan-validate] G6 planned_inputs = {planned} (expect 396)")
if planned != 396:
    print('[plan-validate] FAIL: census != 396'); sys.exit(1)

# G2 rows: every part-change pin must be ok (True). In plan mode the record's
# 'ok' prints as true/false; drift rows would also appear in the warning line.
g2 = g('G2-part-change-pins')
bad_rows = [x for x in g2 if x['ok'] is not True]
print(f"[plan-validate] G2-part-change-pins rows = {len(g2)}, not-ok = {len(bad_rows)}")
for x in bad_rows[:10]:
    print('  DRIFT:', json.dumps(x['detail'])[:220])
if bad_rows:
    sys.exit(1)

# G1 live BASE pins
g1 = g('G1-base-selection-pins')
if g1:
    ok = g1[0]['ok'] is True
    print(f"[plan-validate] G1-base-selection-pins ok={g1[0]['ok']} pins={g1[0]['detail'].get('pins')} drift={len(g1[0]['detail'].get('drift') or [])}")
    if not ok:
        sys.exit(1)
else:
    print('[plan-validate] HOLE: no G1 record'); sys.exit(1)

# G2q copyin-probe composition outcome
gq = [x for x in gates if x['gate'].startswith('G2q')]
for x in gq:
    d = json.dumps(x.get('detail') or {})
    print(f"[plan-validate] {x['gate']} ok={x['ok']} {d[:240]}")

# overall: any gate with ok exactly false?
false_gates = [x for x in gates if x['ok'] is False]
accepted = []
def is_accepted_false(x):
    # G2q-copyin-probe-pin-unrealinitializer.base is an EXPECTATION pin, not
    # an invariant (build_variants.py compose_copyin_probe comment): the UE4SS
    # base legitimately changes when agents revise UnrealInitializer.cpp —
    # the round-5 wiring is exactly such a revision. Accepted IFF the Q patch
    # then rebased cleanly and the composed second hop carried the token.
    if x['gate'] != 'G2q-copyin-probe-pin-unrealinitializer.base':
        return False
    override = g('G2q-second-hop-override')
    # NOTE: the driver records G2q-overlay-compose ONLY on failure — a clean
    # rebase has no overlay-compose record; success is proven by the
    # second-hop-override gate itself (ok + rebase mode + token present).
    return (override and override[0]['ok'] is True
            and override[0]['detail'].get('mode') == 'patch-rebased-onto-revised-base'
            and override[0]['detail'].get('composed_has_token') is True)
for x in list(false_gates):
    if is_accepted_false(x):
        accepted.append(x)
        false_gates.remove(x)
for x in accepted:
    print(f"[plan-validate] ACCEPTED ok=false (documented expectation pin; Q rebase succeeded + token present): {x['gate']}")
if false_gates:
    print(f"[plan-validate] WARNING: {len(false_gates)} gate(s) recorded ok=false:")
    for x in false_gates[:10]:
        print('  FALSE:', x['gate'], json.dumps(x.get('detail'))[:200])
    print("[plan-validate] (plan mode tolerates drift records, but ok=false gates must each carry an accepted reason before --execute)")
    sys.exit(1)

print('[plan-validate] PLAN VERDICT: GREEN — census 396, all checked gates ok (1 documented expectation-pin drift accepted via successful Q rebase)' if accepted
      else '[plan-validate] PLAN VERDICT: GREEN — census 396, all checked gates ok')
PY

echo "[plan-validate] artifacts: $MROOT/{restore.log,additions.log,plan.log} (+ mirror tree)"
