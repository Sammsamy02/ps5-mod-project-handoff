#!/usr/bin/env bash
# apply-additions-round5.sh — wire the round-5 ADDITIONS on top of the
# restored state (agent II staging; ROOT executes at AC power).
# Prerequisite: restore-round5.sh has completed (its --verify exits 0).
#
# A. GT first-launch wiring: apply additions/fl-wiring-unrealinitializer.patch
#    to UnrealInitializer.cpp (GT section-3 slot: BEFORE the
#    consume_required_slot_seeds admission call; provenance comments in the
#    patch header), then refresh the UnrealInitializer.cpp candidate_sha256
#    rows in CHANGES.json (changes[] + files[]).
# B. Marker additions into harvest/markers-declared.json from
#    additions/markers-add-round5.json:
#      - every entry's required text is first GREPPED against the restored
#        tree sources (refuse on any miss — the FW phantom-build law);
#      - IB's [SHIM] write-area + the 8 [FL] entries land (18 -> 27);
#      - [FL-MAP] template sockets are NOT auto-added: they print as
#        PENDING with IA's artifact paths and the script exits 2 unless
#        --allow-pending is given (the build card refuses to build with
#        pending sockets unless ROOT explicitly acknowledges).
# C. IA world-fix: if round5-staging/world-fix/02-core-fix.patch +
#    03-map-logger.patch have landed, apply them in order and refresh the
#    CHANGES rows for every file they touch (LuaUObject.hpp header row =>
#    consumer cascade is the driver's job at --plan; LuaMod.cpp row refresh
#    happens here). If not landed: report PENDING (same --allow-pending law).
#
# Idempotent: re-running detects completed steps and skips.
# --verify : read-only check of every surface this script owns.
# --tree/--prep/--what: mirror overrides (see restore-round5.sh).
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
LIVE_WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
WHAT=$LIVE_WHAT
RT=$LIVE_WHAT/outputs/2026-10-08-l26-runtime
PREP=$LIVE_WHAT/outputs/2026-10-08-l25-prep
MODE=apply
ALLOW_PENDING=0
while [ $# -gt 0 ]; do
  case "$1" in
    --verify) MODE=verify ;;
    --allow-pending) ALLOW_PENDING=1 ;;
    --tree) RT=$2; shift ;;
    --prep) PREP=$2; shift ;;
    --what) WHAT=$2; shift ;;
    *) echo "usage: $0 [--verify] [--allow-pending] [--tree <rt-mirror>] [--prep <prep-mirror>] [--what <root>]" >&2; exit 2 ;;
  esac
  shift
done

CAND=$RT/typescan-r4/candidate
CHANGES=$RT/typescan-r4/CHANGES.json
MARKERS=$PREP/harvest/markers-declared.json
WORLDFIX=$PREP/round5-staging/world-fix
ADD=$HERE/markers-add-round5.json
PATCH_FL=$HERE/fl-wiring-unrealinitializer.patch

fail() { echo "[apply-additions] FAIL: $*" >&2; exit 1; }
info() { echo "[apply-additions] $*"; }
sha_of() { shasum -a 256 "$1" | awk '{print $1}'; }

[ -f "$ADD" ] || fail "missing $ADD"
[ -f "$PATCH_FL" ] || fail "missing $PATCH_FL"
[ -f "$MARKERS" ] || fail "missing $MARKERS"
[ -f "$CHANGES" ] || fail "missing $CHANGES"

# The patch's internal path is rooted at the WHAT root (a/outputs/...):
# apply with -d "$WHAT" so mirrors work too.
UI=$CAND/src/UnrealInitializer.cpp
[ -f "$UI" ] || fail "missing $UI"

wiring_applied() { grep -q "RC_first_launch_try_profile" "$UI"; }

verify_wiring() {
  wiring_applied || { info "HOLE: FL wiring absent from UnrealInitializer.cpp"; return 1; }
  local a b
  a=$(grep -n "RC_first_launch_try_profile(fl_mods_dir" "$UI" | head -1 | cut -d: -f1)
  b=$(grep -n "^            SeedArming::consume_required_slot_seeds" "$UI" | head -1 | cut -d: -f1)
  if [ -z "$a" ] || [ -z "$b" ] || [ "$a" -ge "$b" ]; then
    info "HOLE: wiring call (line $a) not BEFORE the admission call (line $b)"; return 1
  fi
  grep -q "#include <UE4SSProgram.hpp>" "$UI" || { info "HOLE: UE4SSProgram include absent"; return 1; }
  info "OK  FL wiring present and ordered (try@$a < consume@$b)"
}

verify_markers_added() {
  python3 - "$MARKERS" "$CAND" "$ADD" <<'PY'
import sys, json
markers_path, cand, add_path = sys.argv[1], sys.argv[2], sys.argv[3]
m = json.load(open(markers_path))
ids = {x['id'] for x in m['markers']}
add = json.load(open(add_path))
want = [e['id'] for e in add['entries']]
missing = [i for i in want if i not in ids]
bad = 0
if missing:
    print(f"[apply-additions] HOLE: markers-declared.json missing {missing} (count {len(ids)})"); bad += 1
else:
    print(f"[apply-additions] OK  addition markers present (manifest count {len(ids)})")
# literal ground-truth: every required text must grep in the tree sources
import re
def source_text(srcfile):
    return open(srcfile, encoding='utf-8', errors='replace').read()
fl = source_text(cand + '/src/FirstLaunchProfile.cpp')
shim = source_text(cand + '/src/LuaPathShim.cpp')
luamod = source_text(cand + '/src/LuaMod.cpp')
for e in add['entries']:
    for r in e['required']:
        if e['id'].startswith('fl-map'):
            hay = luamod      # [FL-MAP] literals land in LuaMod.cpp via 03-map-logger.patch
        elif '[FL]' in r['text']:
            hay = fl
        else:
            hay = shim
        # strip format-brace tails already handled: 'text' is the literal prefix
        if r['text'].rstrip() not in hay:
            print(f"[apply-additions] HOLE: required text {r['text']!r} ({e['id']}) not found in tree source"); bad += 1
sys.exit(1 if bad else 0)
PY
}

verify_changes_ui() {
  python3 - "$CHANGES" "$UI" <<'PY'
import sys, json, hashlib
cp, ui = sys.argv[1], sys.argv[2]
c = json.load(open(cp))
want = hashlib.sha256(open(ui, 'rb').read()).hexdigest()
bad = 0
for key in ('changes', 'files'):
    for row in c.get(key, []):
        if row.get('candidate', '').endswith('/UnrealInitializer.cpp'):
            if row['candidate_sha256'] != want:
                print(f"[apply-additions] HOLE: {key} UnrealInitializer.cpp row sha != actual (wiring applied but row stale?)"); bad += 1
sys.exit(1 if bad else 0)
PY
  [ $? -eq 0 ] && info "OK  CHANGES UnrealInitializer.cpp rows match the wired file"
}

apply_wiring() {
  if wiring_applied; then info "SKIP (wiring present)"; else
    (cd "$WHAT" && patch -p1 --forward --silent < "$PATCH_FL") || fail "wiring patch did not apply cleanly (inspect $PATCH_FL vs the tree)"
    info "APPLIED fl-wiring-unrealinitializer.patch"
  fi
  verify_wiring || fail "post-apply wiring verification failed"
}

refresh_changes_ui() {
  python3 - "$CHANGES" "$UI" <<'PY'
import sys, json, hashlib, re
cp, ui = sys.argv[1], sys.argv[2]
want = hashlib.sha256(open(ui, 'rb').read()).hexdigest()
raw = open(cp, encoding='utf-8').read()
out, inrow = [], 0
for line in raw.splitlines(keepends=True):
    if '/UnrealInitializer.cpp"' in line and '"candidate"' in line:
        inrow = 1
    elif inrow and '"candidate_sha256"' in line:
        line = re.sub(r'"candidate_sha256": "[0-9a-f]{64}"', f'"candidate_sha256": "{want}"', line)
        inrow = 0
    out.append(line)
raw = ''.join(out)
json.loads(raw)
open(cp, 'w', encoding='utf-8').write(raw)
print(f'[apply-additions] CHANGES UnrealInitializer.cpp rows refreshed -> {want[:12]}')
PY
}

apply_markers() {
  if verify_markers_added >/dev/null 2>&1; then info "SKIP (addition markers present)"; return; fi
  python3 - "$MARKERS" "$ADD" <<'PY'
import sys, json
path, add_path = sys.argv[1], sys.argv[2]
add = json.load(open(add_path))
m = json.load(open(path))
have = {x['id'] for x in m['markers']}
entries = [e for e in add['entries'] if e['id'] not in have]
if not entries:
    print('[apply-additions] nothing to add'); sys.exit(0)
def fmt(e):
    d = json.dumps(e, indent=1)
    return '\n'.join(('  ' + ln if ln else ln) for ln in d.splitlines())
blob = ',\n'.join(fmt(e) for e in entries)
raw = open(path, encoding='utf-8').read()
tail = raw.rstrip()
assert tail.endswith('}'), 'unexpected manifest tail (expected ... ] ... })'
head = tail[:tail.rfind(']')].rstrip()
assert head.endswith('}'), 'unexpected last entry tail'
open(path, 'w', encoding='utf-8').write(head + ',\n' + blob + '\n ]\n}')
check = json.load(open(path))
ids = [x['id'] for x in check['markers']]
print(f'[apply-additions] manifest now {len(ids)} markers')
PY
  verify_markers_added || fail "post-add marker verification failed (literal grep or count)"
}

apply_worldfix() {
  # Apply order per world-fix/APPLY-ORDER-AND-PROBE-EXPECTATIONS.md (agent IK
  # addendum, 2026-10-11): 02 -> 02b -> 03 (02->02b is the syntax-receipted
  # order; 03 touches disjoint LuaMod.cpp regions).
  local applied_any=0
  local before_f
  before_f=$(mktemp /tmp/r5-worldfix-before.XXXX)
  find "$CAND" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.inc' -o -name '*.inl' \) -exec shasum -a 256 {} + > "$before_f" 2>/dev/null || true
  for p in 02-core-fix.patch 02b-getworld-fallback.patch 03-map-logger.patch; do
    local pf=$WORLDFIX/$p
    if [ ! -f "$pf" ]; then
      info "PENDING: $WORLDFIX/$p not landed (agent IA in flight; see $WORLDFIX/01-diagnosis.md)"
      continue
    fi
    # IA/IK patches are CANDIDATE-relative (a/include/..., a/src/...):
    # apply with cwd = the candidate tree (verified against their headers).
    # STACKED-TREE (root, AC execution): the world-fix patches were
    # already applied by their OWN labelled build earlier in the queue —
    # patch(1) then reports "previously applied" and exits nonzero WITHOUT
    # touching files. Detect that state (marker grep) and treat as done.
    if (cd "$CAND" && patch -p1 --forward --silent < "$pf") 2>/dev/null; then
      info "APPLIED $p"
      applied_any=1
    elif grep -q -- "FL-MAP" "$CAND/src/LuaMod.cpp" 2>/dev/null && grep -q -- "GetNamePrivate().ToString()" "$CAND/include/LuaType/LuaUObject.hpp" 2>/dev/null && grep -q -- "TypeChecker" "$CAND/include/LuaType/LuaUObject.hpp" 2>/dev/null; then
      info "SKIP $p (already applied by the world-fix labelled build; markers verified in-tree)"
    else
      rm -f "$before_f"; fail "IA patch $p did not apply cleanly and world-fix markers absent — reconcile with IA's report"
    fi
  done
  if [ "$applied_any" = 1 ]; then
    # refresh CHANGES rows ONLY for candidate files the patches changed
    python3 - "$CHANGES" "$CAND" "$before_f" <<'PY'
import sys, json, hashlib, re
cp, cand, before = sys.argv[1], sys.argv[2], sys.argv[3]
def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
changed = {}
for line in open(before):
    line = line.strip()
    if not line: continue
    old_sha, path = line.split('  ', 1)
    try:
        now = sha(path)
    except FileNotFoundError:
        continue
    if now != old_sha:
        changed[path] = now
if not changed:
    print('[apply-additions] world-fix applied but no candidate file changed sha — nothing to refresh (suspicious: reconcile with IA)')
    sys.exit(1)
raw = open(cp, encoding='utf-8').read()
out, cur = [], None
for line in raw.splitlines(keepends=True):
    mm = re.search(r'"candidate": "(.*?)"', line)
    if mm and mm.group(1) in changed:
        cur = changed[mm.group(1)]
    elif cur and '"candidate_sha256"' in line:
        line = re.sub(r'"candidate_sha256": "[0-9a-f]{64}"', f'"candidate_sha256": "{cur}"', line)
        cur = None
    out.append(line)
raw = ''.join(out)
json.loads(raw)
open(cp, 'w', encoding='utf-8').write(raw)
print(f'[apply-additions] CHANGES rows refreshed for {len(changed)} world-fix-touched file(s): ' + ', '.join(sorted(p.split("/")[-1] for p in changed)))
PY
    rm -f "$before_f"
  else
    rm -f "$before_f"
  fi
  # [FL-MAP] marker sockets: refuse unless IA's literals are pinned
  local pend
  pend=$(python3 - "$ADD" <<'PY'
import sys, json
add = json.load(open(sys.argv[1]))
print(len(add.get('pending_templates', [])))
PY
)
  if [ "$pend" != "0" ]; then
    info "PENDING: $pend [FL-MAP] template socket(s) unresolved — pin the literal/encodings from IA's landed 03-map-logger.patch into markers-declared.json, then re-run --verify"
    [ "$ALLOW_PENDING" = 1 ] || exit 2
    info "--allow-pending given: continuing (ROOT acknowledges the socket stays open)"
  fi
}

case "$MODE" in
  verify)
    rc=0
    verify_wiring || rc=1
    verify_markers_added || rc=1
    verify_changes_ui || rc=1
    [ $rc -eq 0 ] && info "VERIFY: additions fully wired" || info "VERIFY: addition holes remain"
    exit $rc
    ;;
  apply)
    bash "$HERE/../restore-round5.sh" --verify --tree "$RT" --prep "$PREP" >/dev/null 2>&1 \
      || fail "restore-round5.sh --verify did not pass first (run the restore before the additions)"
    info "restore verified; applying additions (tree: $RT, prep: $PREP)"
    apply_wiring
    refresh_changes_ui
    verify_changes_ui || fail "CHANGES refresh verification failed"
    apply_worldfix          # patches FIRST so the [FL-MAP] literals exist in-tree
    apply_markers           # then declare + grep-verify every marker against the sources
    info "additions complete — final word:"
    exec bash "$0" --verify --tree "$RT" --prep "$PREP" --what "$WHAT"
    ;;
esac
