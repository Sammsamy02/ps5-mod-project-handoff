#!/bin/bash
# run_labelled_build.sh <label> <prev-label> — ONE labelled, fully gated l26 build
# (Claude overnight run 2026-10-12). Mac-only: no console contact of any kind.
#
#   0. memory check (>= 4 GB available, else wait and retry; never push)
#   1. pre-compile gates: fs_throw_gate (baseline ratchet, 1e49188b compiled set
#      + live candidate tree), no_detour_gate (default scope; see note) and field_gate
#      (every engine field the runtime reads must be in this game's proven layout data) and
#      layout_binder_gate (every declared field passes the runtime layout binder's startup checks)
#   2. archive the CURRENT build/package dirs as archive-r6/<prev-label>
#      (drivers refuse non-fresh dirs; intermediates are kept, never deleted)
#   3. snapshot_tree.sh -> SNAPSHOT-ID
#   4. build_variants.py --execute   (L26_BUILD_JOBS=4)
#   5. l26_packaging_driver.py --execute
#   6. PRX magic + twin identity
#   7. verify_build.sh --manifest (exact PRX)
#   8. post-compile fs_throw_gate on the NEW build's actual compiled file set
#   9. RESULT file + BUILD-LEDGER row
# Every step that fails stops the build (exit 1) and says which step.
# Note: no_detour_gate's default scope is the candidate tree only (45 files);
# the shared port code is not scanned yet — a known gap, reported, not fixed here.
set -u -o pipefail
LABEL="${1:?usage: run_labelled_build.sh <label> <prev-label>}"
PREV_LABEL="${2:?usage: run_labelled_build.sh <label> <prev-label>}"
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
L26="$WHAT/outputs/2026-10-08-l26-runtime"
CAND="$L26/typescan-r4/candidate"
PAIR_ROOT="$WHAT/outputs/2026-10-08-l26-build-variants"
PKG_A="$WHAT/outputs/2026-10-08-l26-package-A"
PREP="$WHAT/outputs/2026-10-08-l25-prep"
H="$PREP/harvest"
EV="$PREP/evidence/builds-20261012/$LABEL"
ARCH="$PAIR_ROOT/archive-r6/$PREV_LABEL"
LEDGER="$PREP/BUILD-LEDGER-20261012.md"
BASELINE="$H/fs_throw_baseline-1e49188b.txt"
BASE_COMPILED="$PREP/evidence/fs-throw-gate-20261012/compiled-files-1e49188b.txt"
PRX="$PKG_A/pass-1/modbridge_supplier_composition.prx"
mkdir -p "$EV"
exec > >(tee -a "$EV/build.log") 2>&1
say()  { printf '[%s] %s\n' "$(date -u +%H:%M:%SZ)" "$*"; }
fail() { say "FAIL at step: $*"; printf 'RESULT=FAIL\nLABEL=%s\nSTEP=%s\n' "$LABEL" "$*" > "$EV/RESULT"; exit 1; }
memcheck() {
  local i avail
  for i in $(seq 1 30); do
    avail=$(vm_stat | awk '/Pages free|Pages inactive|Pages purgeable/ {gsub("\\.","",$NF); s+=$NF} END{printf "%d", s*16384/1048576}')
    if [ "$avail" -ge 4096 ]; then say "memory ok: ${avail} MB available"; return 0; fi
    say "memory tight: ${avail} MB available; waiting 60 s ($i/30)"; sleep 60
  done
  fail "memory stayed below 4 GB for 30 minutes"
}
cd "$WHAT" || fail "cd worktree"
# B7 switch for EVERY step (build_variants AND l26_packaging_driver read it; the packager adjusts the
# undefined-symbol census by it). Default ON; L26_LUA_AS_CPP=0 only for a deliberate diagnostic build.
export L26_LUA_AS_CPP="${L26_LUA_AS_CPP:-1}"
say "B7 lua-as-cpp: L26_LUA_AS_CPP=$L26_LUA_AS_CPP"
say "=== labelled build $LABEL (archiving current dirs as $PREV_LABEL) ==="
if [ "${RESUME_AT_PACKAGE:-0}" = 1 ]; then
  # Resume a build of THIS label whose compile passed but packaging failed (e.g. a packaging-contract fix):
  # same compiled objects, same snapshot; the failed package dir is archived (never deleted).
  grep -q '^STEP=packaging' "$EV/RESULT" 2>/dev/null || { echo "RESUME_AT_PACKAGE: $EV/RESULT is not a packaging failure"; exit 1; }
  grep -q 'PASS {"mode": "execute"' "$EV/build_variants.log" || { echo "RESUME_AT_PACKAGE: build_variants did not pass"; exit 1; }
  N=1; while [ -e "$EV/RESULT.attempt-$N" ]; do N=$((N+1)); done
  mv "$EV/RESULT" "$EV/RESULT.attempt-$N"; cp -p "$EV/packaging.log" "$EV/packaging.log.attempt-$N"
  SNAP=$(awk '/^SNAPSHOT-ID /{print $2}' "$EV/snapshot.stdout")
  MAN=$(awk '/^SNAPSHOT-MANIFEST /{print $2}' "$EV/snapshot.stdout")
  say "=== RESUME $LABEL at 5/9 (attempt $((N+1))); snapshot $SNAP ==="
  python3 -I - "$MAN" "$WHAT" <<'PYEOF' || fail "candidate changed since the compile (resume refused)"
import hashlib, json, os, sys
m = json.load(open(sys.argv[1])); W = sys.argv[2]; bad = 0; n = 0
rows = m.get('files', m)
items = rows.items() if isinstance(rows, dict) else [(r['path'], r['sha256']) for r in rows]
for path, sha in items:
    if isinstance(sha, dict): sha = sha.get('sha256')
    p = path if path.startswith('/') else os.path.join(W, path)
    if not os.path.exists(p): p = os.path.join(W, 'outputs', path)
    if p.endswith('GeneratedBuildStamp.hpp'): continue
    if p.endswith('/l26_packaging_driver.py'): print('allowed (packaging fix being resumed for):', path); continue
    n += 1
    if not os.path.exists(p) or hashlib.sha256(open(p, 'rb').read()).hexdigest() != sha: bad += 1; print('changed:', path)
print('resume check: %d snapshot files, %d changed' % (n, bad))
sys.exit(1 if bad or n == 0 else 0)
PYEOF
  if [ -e "$PKG_A" ]; then mkdir -p "$PAIR_ROOT/archive-r6"; mv "$PKG_A" "$PAIR_ROOT/archive-r6/$LABEL-packfail-$N" || fail "archive failed package dir"; say "  archived failed package dir as $LABEL-packfail-$N"; fi
else
[ -e "$EV/RESULT" ] && fail "evidence dir already has a RESULT (labels are single-use): $EV"

say "0/9 memory"; memcheck

say "1/9 pre-compile gates"
python3 -I "$H/fs_throw_gate.py" --files-from "$BASE_COMPILED" --root "$CAND" --baseline "$BASELINE" > "$EV/gate-fs-throw-precompile.txt"; rc=$?
tail -3 "$EV/gate-fs-throw-precompile.txt"; [ $rc -eq 0 ] || fail "fs_throw_gate pre-compile rc=$rc"
python3 -I "$H/no_detour_gate.py" > "$EV/gate-no-detour.txt" 2>&1; rc=$?
tail -2 "$EV/gate-no-detour.txt"; [ $rc -eq 0 ] || fail "no_detour_gate rc=$rc"
python3 -I "$H/field_gate.py" --root "$CAND" --list-out "$EV/engine-fields-read.txt" > "$EV/gate-field.txt" 2>&1; rc=$?
grep -E "NEW UNPROVEN|distinct engine fields|FIELD GATE: (PASS|FAIL)" "$EV/gate-field.txt" | tail -8; [ $rc -eq 0 ] || fail "field_gate rc=$rc"
# [2026-10-14] EXTRA roots (additive, stricter): new runtime headers live in typescan-r4/include and the #10 sources in
# cap10/native; the default scopes above do not cover them (a site in include/ was missed once).
XROOTS=()
for d in "$WHAT/outputs/2026-10-08-l26-runtime/typescan-r4/include" "$WHAT/outputs/2026-10-08-l26-runtime/cap10/native"; do [ -d "$d" ] && XROOTS+=(--root "$d"); done
if [ ${#XROOTS[@]} -gt 0 ]; then
  python3 -I "$H/no_detour_gate.py" "${XROOTS[@]}" > "$EV/gate-no-detour-extra-roots.txt" 2>&1 || fail "no_detour_gate extra roots (see gate-no-detour-extra-roots.txt)"
  python3 -I "$H/fs_throw_gate.py" "${XROOTS[@]}" > "$EV/gate-fs-throw-extra-roots.txt" 2>&1 || fail "fs_throw_gate extra roots (see gate-fs-throw-extra-roots.txt)"
  grep -hE "NO-DETOUR GATE: (PASS|FAIL)|FS-THROW GATE: (PASS|FAIL)|checked [0-9]+ files|scanned [0-9]+" "$EV/gate-no-detour-extra-roots.txt" "$EV/gate-fs-throw-extra-roots.txt" | tail -4
fi
python3 -I "$H/layout_binder_gate.py" > "$EV/gate-layout-binder.txt" 2>&1; rc=$?
grep -E "LAYOUT BINDER GATE: (checked|PASS|FAIL)|^  FAIL" "$EV/gate-layout-binder.txt" | tail -8; [ $rc -eq 0 ] || fail "layout_binder_gate rc=$rc (a declared field the runtime binder would refuse at startup)"

say "2/9 archive current build/package dirs -> $ARCH"
if [ -e "$PAIR_ROOT/l26-A" ] || [ -e "$PKG_A" ]; then
  [ -e "$ARCH" ] && fail "archive target already exists: $ARCH"
  mkdir -p "$ARCH" || fail "mkdir archive"
  for f in EXECUTION-RECEIPT.json PACKAGING-EXECUTION-RECEIPT.json PACKAGING-PREP-RECEIPT.json VARIANT-PAIR-RECEIPT.json VARIANT-PAIR-PACKAGING-RECEIPT.json; do
    [ -e "$PAIR_ROOT/$f" ] && cp -p "$PAIR_ROOT/$f" "$ARCH/$f"
  done
  for d in "$PAIR_ROOT/l26-A" "$PAIR_ROOT/staging-copyin-probe" "$PKG_A"; do
    if [ -e "$d" ]; then mv "$d" "$ARCH/$(basename "$d")" || fail "archive $d"; say "  archived $(basename "$d")"; fi
  done
fi
[ -e "$PKG_A" ] && fail "package dir still present after archive"
[ -e "$PAIR_ROOT/l26-A" ] && fail "variant dir still present after archive"

say "3/9 snapshot"
SNAP_OUT=$(bash "$H/snapshot_tree.sh" 2>"$EV/snapshot.stderr"); rc=$?
echo "$SNAP_OUT" > "$EV/snapshot.stdout"; cat "$EV/snapshot.stderr" | tail -2
SNAP=$(echo "$SNAP_OUT" | awk '/^SNAPSHOT-ID /{print $2}')
[ $rc -eq 0 ] && [ -n "$SNAP" ] || fail "snapshot rc=$rc id='$SNAP'"
say "  SNAPSHOT-ID $SNAP"

say "4/9 build (L26_BUILD_JOBS=4)"; memcheck
# B7 (Lua as C++) is the production default since R10; R13c was built without it by mistake (launch C
# showed [LUA-CPP] mode=longjmp). Default ON; L26_LUA_AS_CPP=0 only for a deliberate diagnostic build.
LUACPP="$L26_LUA_AS_CPP"
L26_LUA_AS_CPP="$LUACPP" L26_BUILD_JOBS=4 BRIDGE_SNAPSHOT_ID="$SNAP" python3 "$L26/build_variants.py" --execute > "$EV/build_variants.log" 2>&1; rc=$?
if [ "$LUACPP" = "1" ]; then n=$(grep -c "G-LUACPP-scope" "$EV/build_variants.log"); echo "lua-as-cpp gate: $n G-LUACPP-scope lines"; [ "$n" -gt 0 ] || fail "lua-as-cpp requested but build_variants.log has 0 G-LUACPP-scope lines"; fi
grep -E '^(PASS|FAIL|PLAN) |EXPECTED-STATUS|gates failed' "$EV/build_variants.log" | tail -3
[ $rc -eq 0 ] || fail "build_variants rc=$rc (see build_variants.log)"
[ -f "$PAIR_ROOT/l26-A/TWIN-RECEIPT.json" ] || fail "TWIN-RECEIPT missing"
fi

say "5/9 package"; memcheck
python3 "$L26/l26_packaging_driver.py" --execute > "$EV/packaging.log" 2>&1; rc=$?
grep -E '^(PASS|FAIL|PREPARED|PLAN) |gates failed' "$EV/packaging.log" | tail -3
[ $rc -eq 0 ] || fail "packaging rc=$rc (see packaging.log)"
[ -f "$PKG_A/FINAL-RECEIPT-VARIANT-A.json" ] || fail "FINAL-RECEIPT missing"

say "6/9 PRX identity"
[ -f "$PRX" ] || fail "PRX missing"
SHA=$(shasum -a 256 "$PRX" | cut -d' ' -f1); SHA2=$(shasum -a 256 "$PKG_A/pass-2/modbridge_supplier_composition.prx" | cut -d' ' -f1)
MAGIC=$(xxd -l 4 -p "$PRX"); BYTES=$(stat -f %z "$PRX")
say "  sha256=$SHA bytes=$BYTES magic=$MAGIC"
[ "$MAGIC" = "4f153d1d" ] || fail "PRX magic $MAGIC is not the fake-signed SELF 4f153d1d"
[ "$SHA" = "$SHA2" ] || fail "packaging twins differ"

say "7/9 verify_build --manifest"
bash "$H/verify_build.sh" "$PRX" --manifest "$H/markers-declared.json" "$PKG_A" > "$EV/verify_build.txt" 2>&1; rc=$?
grep -E '^VERDICT|^RESULT' "$EV/verify_build.txt" | tail -2
[ $rc -eq 0 ] || fail "verify_build rc=$rc"

say "8/9 post-compile fs gate (actual compiled set of this build)"
python3 -I - "$PAIR_ROOT/l26-A/ACTUAL-DEPENDENCIES.json" "$EV/compiled-files.txt" "$WHAT" <<'PYEOF' || fail "extract compiled set"
import json, os, sys
d = json.load(open(sys.argv[1])); W = sys.argv[3]; acc = set()
EXT = ('.cpp', '.cc', '.c', '.hpp', '.h', '.inl', '.inc')
def walk(x):
    if isinstance(x, dict):
        for v in x.values(): walk(v)
    elif isinstance(x, list):
        for v in x: walk(v)
    elif isinstance(x, str) and x.endswith(EXT):
        acc.add(x if x.startswith('/') else os.path.join(W, x))
walk(d)
ws = sorted(p for p in acc if p.startswith(W))
if not ws: sys.exit('ZERO compiled files extracted')
open(sys.argv[2], 'w').write('\n'.join(ws) + '\n'); print(len(ws), 'compiled files')
PYEOF
python3 -I "$H/fs_throw_gate.py" --files-from "$EV/compiled-files.txt" --baseline "$BASELINE" > "$EV/gate-fs-throw-postcompile.txt"; rc=$?
tail -2 "$EV/gate-fs-throw-postcompile.txt"; [ $rc -eq 0 ] || fail "fs_throw_gate post-compile rc=$rc"

say "8b/9 final symbols for crash reports (cap05; Mac-only, reads the package, never changes the PRX; NON-FATAL)"
# Non-fatal on purpose: the PRX is already built and verified here, and labels are single-use.
# A failure only means crash reports for this build stay unsymbolized; it is written to
# $EV/FINAL-SYMBOLS-STATUS and can be redone by hand later (PATCH/APPLY.md step 4).
FSYM="$PKG_A/modbridge_supplier_composition.prx.final-symbols.json"
if [ -e "$FSYM" ]; then
  FSYM_STATUS="WARN: existing $FSYM left untouched (never overwritten); step skipped"
else
  python3 -I -B "$H/crashcap/final_symbols.py" --object "$PKG_A/pass-1/build/combined-supplier.o" \
    --bare "$PKG_A/pass-1/bare-original/modbridge_supplier_composition.prx" --packaged "$PRX" \
    --out "$FSYM" > "$EV/final-symbols.txt" 2>&1; rc=$?
  tail -1 "$EV/final-symbols.txt"
  if [ $rc -eq 0 ]; then
    FSYM_STATUS="PASS"; cp -p "$FSYM" "$EV/" 2>/dev/null
  else
    FSYM_STATUS="WARN: final_symbols rc=$rc (layout not reproduced; crash reports for this build would be unsymbolized; build result unaffected)"
  fi
fi
printf 'FINAL_SYMBOLS=%s\n' "$FSYM_STATUS" > "$EV/FINAL-SYMBOLS-STATUS"
say "8b/9 final symbols: $FSYM_STATUS"

say "9/9 record"
BUILD_ID=$(sed -n 's/^#define BRIDGE_BUILD_ID "\(.*\)"/\1/p' "$CAND/include/GeneratedBuildStamp.hpp")
cp -p "$PKG_A/FINAL-RECEIPT-VARIANT-A.json" "$EV/" 2>/dev/null
printf 'RESULT=PASS\nLABEL=%s\nPRX_SHA256=%s\nBYTES=%s\nMAGIC=%s\nSNAPSHOT_ID=%s\nBUILD_ID=%s\nARCHIVED_PREVIOUS_AS=%s\nFINISHED_UTC=%s\n' \
  "$LABEL" "$SHA" "$BYTES" "$MAGIC" "$SNAP" "$BUILD_ID" "$ARCH" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$EV/RESULT"
[ -f "$LEDGER" ] || printf '# Build ledger — overnight 2026-10-12 (Mac-only; variant A only)\n\n| Label | PRX sha256 | Snapshot | Build id | Gates | Package location now |\n|---|---|---|---|---|---|\n' > "$LEDGER"
printf '| %s | `%s` | %s | %s | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/%s/` when the next build starts) |\n' \
  "$LABEL" "$SHA" "$SNAP" "$BUILD_ID" "$LABEL" >> "$LEDGER"
say "=== $LABEL PASS: $SHA ==="
