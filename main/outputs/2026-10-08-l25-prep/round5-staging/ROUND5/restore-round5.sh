#!/usr/bin/env bash
# restore-round5.sh — idempotent round-5 restore of the round-4 set-aside
# (agent II staging, 2026-10-11; Claude round-5 build-staging prep).
#
# Restores, per ROUND4-CONTENTS-DECLARATION.md "Round-5 restore procedure":
#   S1  LuaPathShim.{cpp,hpp} + FirstLaunchProfile.{cpp,hpp} from
#       round4-setaside/ back into the candidate tree (COPY, not move: the
#       set-aside dir stays the durable stash for rollback / later rounds).
#       KernelBridgeProbes.{cpp,hpp} STAYS SET ASIDE (KB ships only in a
#       Claude-reviewed gated build — kb_tu_note law).
#   S2  The LuaMod.cpp [SHIM] insertion block (captured verbatim at
#       round4-setaside/LuaMod-insertion-block.txt, 21 lines) reinserted
#       after its anchor `setup_lua_require_paths(lua);` inside prepare_mod.
#       Post-shape: 9530 lines (9509 + 21). Conceptual-compile check: the
#       block's 3-arg extern must match the TU's RC::LuaPathShim_install
#       definition (checked at restore time, see verify_block_extern).
#   S3  The two NEW_UNITS rows back into build_variants.py
#       (lua-pathshim + firstlaunch; census becomes 391+5 = 396).
#   S4  The 3 [SHIM] manifest entries back into harvest/markers-declared.json
#       (manifest becomes 18 markers; round-4 = 15).
#   S5  CHANGES.json reconciliation: refresh LuaMod.cpp candidate_sha256 in
#       BOTH the `changes` and `files` rows to the restored shape, re-add
#       LuaPathShim.hpp + FirstLaunchProfile.hpp to added_headers /
#       added_header_hashes, update round5_row_sync, flip round4_setaside
#       to false. Valid-JSON check after every edit.
#
# Usage:
#   restore-round5.sh            apply (idempotent; skips completed steps)
#   restore-round5.sh --verify   read-only: check every restored sha/hole,
#                                change NOTHING, exit 0 iff fully restored
#   --tree DIR   operate on a mirror of 2026-10-08-l26-runtime instead of
#                the live tree (used by plan-validate-round5.sh; the live
#                default is for ROOT at the AC-power build window only)
#   --prep DIR   mirror of 2026-10-08-l25-prep holding round4-setaside/ +
#                harvest/markers-declared.json (defaults to the live prep)
#
# Battery law honored: this script only EDITS files when run without
# --verify AND only under the tree handed to it. As staged by agent II it
# was executed against a /tmp mirror only; the live-tree run is ROOT's
# step at AC power (see BUILD-CARD-ROUND5.md). No builds, no console.
set -euo pipefail

LIVE_WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
RT_DEFAULT=$LIVE_WHAT/outputs/2026-10-08-l26-runtime
PREP_DEFAULT=$LIVE_WHAT/outputs/2026-10-08-l25-prep

MODE=apply
RT=$RT_DEFAULT
PREP=$PREP_DEFAULT
while [ $# -gt 0 ]; do
  case "$1" in
    --verify) MODE=verify ;;
    --tree)   RT=$2; shift ;;
    --prep)   PREP=$2; shift ;;
    *) echo "usage: $0 [--verify] [--tree <2026-10-08-l26-runtime mirror>] [--prep <2026-10-08-l25-prep mirror>]" >&2; exit 2 ;;
  esac
  shift
done

SA=$PREP/round4-setaside
CAND=$RT/typescan-r4/candidate
DRIVER=$RT/build_variants.py
CHANGES=$RT/typescan-r4/CHANGES.json
MARKERS=$PREP/harvest/markers-declared.json

# Pins recorded at staging time (agent II, 2026-10-11). GA/Rule-10-fixed era:
# agent IB's writable-area upgrade may legitimately change the two shim
# files IN the stash after this pin was taken — the stash copy is always
# the restore source; a pin mismatch is reported as INFO, never fatal.
PIN_SHIM_CPP_GA=dd647b940b693dd4af36418bb69a69f5626010f0ae55b6ac6cd2c1954d936911
PIN_SHIM_HPP_GA=f86ac8a0d9b66551971185a2d0005b382240adc35f9c8375a757e5b0b7867649
# GT-era pins (agent-GT report table; these files are frozen deliverables):
PIN_FL_CPP=fcb5628d2a8d9a5e7b12b3806eebd551eab4cf7ef1a88a671d60cf81f5608f17
PIN_FL_HPP=69279590ddeb21513f225bd36a2e03789ae76ff27a804fe7870f212a3545c16f
PIN_BLOCK=5182412f583122be1f3005066d8a436e8eae7bec69608b6b57117ae9f1ac3b7a
# Round-4 (set-aside) shapes the rollback returns to:
PIN_LUAMOD_R4=aac03fbc5897f500905db50022ea27960ade5e736dc0d9a836b125b51238fd9b
LINES_LUAMOD_R4=9509
LINES_LUAMOD_R5=9530

fail() { echo "[restore-round5] FAIL: $*" >&2; exit 1; }
info() { echo "[restore-round5] $*"; }

sha_of() { shasum -a 256 "$1" | awk '{print $1}'; }

need() { [ -f "$1" ] || fail "missing required file: $1"; }
for f in "$SA/LuaPathShim.cpp" "$SA/LuaPathShim.hpp" "$SA/FirstLaunchProfile.cpp" \
         "$SA/FirstLaunchProfile.hpp" "$SA/LuaMod-insertion-block.txt" \
         "$DRIVER" "$CHANGES" "$MARKERS" "$CAND/src/LuaMod.cpp"; do
  need "$f"
done

# ---------------------------------------------------------------- helpers
verify_files() {
  local ok=0   # 0 = unix success; check_one sets 1 on any hole/drift
  check_one() {
    local dst=$1 stash=$2
    if [ ! -f "$dst" ]; then info "HOLE: $dst absent"; ok=1; return; fi
    local a b; a=$(sha_of "$dst"); b=$(sha_of "$stash")
    if [ "$a" = "$b" ]; then info "OK  restored $dst (sha ${a:0:12})"; else info "DRIFT $dst restored=${a:0:12} stash=${b:0:12}"; ok=1; fi
  }
  check_one "$CAND/src/LuaPathShim.cpp"          "$SA/LuaPathShim.cpp"
  check_one "$CAND/include/LuaPathShim.hpp"      "$SA/LuaPathShim.hpp"
  check_one "$CAND/src/FirstLaunchProfile.cpp"   "$SA/FirstLaunchProfile.cpp"
  check_one "$CAND/include/FirstLaunchProfile.hpp" "$SA/FirstLaunchProfile.hpp"
  # stash pin sanity (INFO-only for the shim pair: IB may upgrade in place)
  [ "$(sha_of "$SA/LuaPathShim.cpp")" = "$PIN_SHIM_CPP_GA" ] || info "INFO stash LuaPathShim.cpp != GA-era pin (expected after agent-IB writable-area upgrade; stash remains the restore source)"
  [ "$(sha_of "$SA/LuaPathShim.hpp")" = "$PIN_SHIM_HPP_GA" ] || info "INFO stash LuaPathShim.hpp != GA-era pin (see above)"
  [ "$(sha_of "$SA/FirstLaunchProfile.cpp")" = "$PIN_FL_CPP" ] || fail "stash FirstLaunchProfile.cpp drifted from GT pin"
  [ "$(sha_of "$SA/FirstLaunchProfile.hpp")" = "$PIN_FL_HPP" ] || fail "stash FirstLaunchProfile.hpp drifted from GT pin"
  [ "$(sha_of "$SA/LuaMod-insertion-block.txt")" = "$PIN_BLOCK" ] || fail "stash LuaMod-insertion-block.txt drifted"
  # KB TU must REMAIN set aside (never restored by round-5)
  if [ -f "$CAND/src/KernelBridgeProbes.cpp" ] || [ -f "$CAND/include/KernelBridgeProbes.hpp" ]; then
    fail "KernelBridgeProbes present in candidate tree — round-5 must NOT ship the KB TU (kb_tu_note law)"
  fi
  info "OK  KernelBridgeProbes still set aside (correct for round-5)"
  return $ok
}

verify_block() {
  local lines sha
  lines=$(wc -l < "$CAND/src/LuaMod.cpp" | tr -d ' ')
  # STACKED-TREE: the byte-exact block presence below is the invariant;
  # line count varies with the P2/world-fix stack (>= 9530 accepted).
  if [ "$lines" -lt "$LINES_LUAMOD_R5" ]; then info "HOLE: LuaMod.cpp has $lines lines (< $LINES_LUAMOD_R5)"; return 1; fi
  # byte-exact block at anchor+1 (anchor = the prepare_mod call site)
  local anchor_line block_begin
  anchor_line=$(grep -n '^            setup_lua_require_paths(lua);$' "$CAND/src/LuaMod.cpp" | tail -1 | cut -d: -f1)
  [ -n "$anchor_line" ] || { info "HOLE: anchor line not found"; return 1; }
  block_begin=$((anchor_line + 1))
  if ! python3 - "$CAND/src/LuaMod.cpp" "$SA/LuaMod-insertion-block.txt" "$block_begin" <<'PY'
import sys
path, blockp, begin = sys.argv[1], sys.argv[2], int(sys.argv[3])
lines = open(path, encoding='utf-8').read().splitlines(keepends=True)
block = open(blockp, encoding='utf-8').read().splitlines(keepends=True)
got = lines[begin-1:begin-1+len(block)]
sys.exit(0 if got == block else 1)
PY
  then info "HOLE: block at LuaMod.cpp:$block_begin is not byte-exact vs round4-setaside/LuaMod-insertion-block.txt"; return 1; fi
  # conceptual compile: the block's extern must match the TU definition
  python3 - "$CAND/src/LuaPathShim.cpp" <<'PY' || { info "HOLE: block extern (3-arg RC::LuaPathShim_install) has no matching definition in LuaPathShim.cpp"; return 1; }
import sys, re
src = open(sys.argv[1], encoding='utf-8').read()
pat = re.compile(r'auto\s+LuaPathShim_install\s*\(\s*lua_State\*\s*\w*\s*,\s*const\s+char\*\s*\w*\s*,\s*const\s+char\*\s*\w*\s*\)\s*->\s*void')
sys.exit(0 if pat.search(src) else 1)
PY
  sha=$(sha_of "$CAND/src/LuaMod.cpp")
  info "OK  LuaMod.cpp restored: $lines lines (stacked), block byte-exact at :$block_begin, sha ${sha:0:12}"
  return 0
}

verify_driver() {
  grep -q "^    'lua-pathshim':{'source':HERE/'typescan-r4/candidate/src/LuaPathShim.cpp','template':'luamod'" "$DRIVER" \
    || { info "HOLE: lua-pathshim NEW_UNITS row absent"; return 1; }
  grep -q "^    'firstlaunch':{'source':HERE/'typescan-r4/candidate/src/FirstLaunchProfile.cpp','template':'unreal-unrealinitializer'" "$DRIVER" \
    || { info "HOLE: firstlaunch NEW_UNITS row absent"; return 1; }
  python3 - "$DRIVER" <<'PY' || { info "HOLE: census mismatch"; return 1; }
import sys
sys.dont_write_bytecode = True
import importlib.util
spec = importlib.util.spec_from_file_location('drv_check', sys.argv[1])
# import executes module top-level (definitions only; main() is guarded)
m = importlib.util.module_from_spec(spec)
try:
    spec.loader.exec_module(m)
except SystemExit:
    raise
n = len(m.NEW_UNITS)
expect = 391 + n
print(f"[restore-round5] OK  NEW_UNITS={n} census=391+{n}={expect}")
sys.exit(0 if n == 5 and expect == 396 else 1)
PY
  return 0
}

verify_markers() {
  python3 - "$MARKERS" <<'PY' || return 1
import sys, json
m = json.load(open(sys.argv[1]))
ids = [x['id'] for x in m['markers']]
need = ['shim-installed', 'shim-io-open', 'shim-write-rejected']
missing = [i for i in need if i not in ids]
if missing:
    print(f"[restore-round5] HOLE: manifest missing {missing} (count {len(ids)})")
    sys.exit(1)
print(f"[restore-round5] OK  manifest markers={len(ids)} (3 [SHIM] present)")
sys.exit(0 if len(ids) >= 18 else 1)
PY
}

verify_changes() {
  python3 - "$CHANGES" "$CAND" <<'PY' || return 1
import sys, json, hashlib
cp, cand = sys.argv[1], sys.argv[2]
c = json.load(open(cp))
def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
luamod = cand + '/src/LuaMod.cpp'
want = sha(luamod)
bad = 0
for key in ('changes', 'files'):
    for row in c.get(key, []):
        if row.get('candidate', '').endswith('/LuaMod.cpp'):
            if row['candidate_sha256'] != want:
                print(f"[restore-round5] HOLE: {key} LuaMod.cpp row sha {row['candidate_sha256'][:12]} != actual {want[:12]}"); bad += 1
ah = c.get('added_headers', [])
for h in (cand + '/include/LuaPathShim.hpp', cand + '/include/FirstLaunchProfile.hpp'):
    if h not in ah:
        print(f"[restore-round5] HOLE: added_headers missing {h.split('/')[-1]}"); bad += 1
ahh = c.get('added_header_hashes', {})
for h in (cand + '/include/LuaPathShim.hpp', cand + '/include/FirstLaunchProfile.hpp'):
    if h not in ahh:
        print(f"[restore-round5] HOLE: added_header_hashes missing {h.split('/')[-1]}"); bad += 1
    elif ahh[h] != sha(h):
        print(f"[restore-round5] HOLE: added_header_hashes stale sha for {h.split('/')[-1]}"); bad += 1
if c.get('round4_setaside') is not False:
    print("[restore-round5] HOLE: round4_setaside flag still true"); bad += 1
if 'round5_row_sync' not in c:
    print("[restore-round5] HOLE: round5_row_sync block absent"); bad += 1
sys.exit(1 if bad else 0)
PY
  [ $? -eq 0 ] && info "OK  CHANGES.json reconciled (LuaMod rows, added_headers/hashes, round5_row_sync, setaside=false)"
}

# ---------------------------------------------------------------- S1 files
restore_files() {
  local n=0
  cp_if_needed() {
    local dst=$1 src=$2
    if [ -f "$dst" ] && [ "$(sha_of "$dst")" = "$(sha_of "$src")" ]; then
      info "SKIP (already restored): $dst"
    else
      [ ! -e "$dst" ] || fail "refusing to overwrite divergent $dst without --verify-first reconciliation (run --verify to inspect)"
      cp -p "$src" "$dst"
      info "RESTORED $dst"
      n=$((n+1))
    fi
  }
  cp_if_needed "$CAND/src/LuaPathShim.cpp"        "$SA/LuaPathShim.cpp"
  cp_if_needed "$CAND/include/LuaPathShim.hpp"    "$SA/LuaPathShim.hpp"
  cp_if_needed "$CAND/src/FirstLaunchProfile.cpp" "$SA/FirstLaunchProfile.cpp"
  cp_if_needed "$CAND/include/FirstLaunchProfile.hpp" "$SA/FirstLaunchProfile.hpp"
  info "S1 done ($n file(s) copied; stash preserved at $SA)"
}

# ---------------------------------------------------------------- S2 block
restore_block() {
  if verify_block >/dev/null 2>&1; then info "SKIP (block already in place)"; return; fi
  local lines
  lines=$(wc -l < "$CAND/src/LuaMod.cpp" | tr -d ' ')
  # STACKED-TREE ADAPTATION (root, AC execution 2026-10-11): P2 dead-code +
  # world-fix legitimately changed LuaMod.cpp after the round-4 set-aside
  # (9604 lines at restore time). Line count is a drift GUARD, not the
  # correctness mechanism — the unique-anchor + block-extern checks below
  # enforce correctness. Accept >= the round-4 shape.
  [ "$lines" -ge "$LINES_LUAMOD_R4" ] || fail "LuaMod.cpp is $lines lines; BELOW round-4 shape ($LINES_LUAMOD_R4) — inspect manually"
  [ "$lines" = "$LINES_LUAMOD_R4" ] || info "stacked-tree shape accepted ($lines lines >= $LINES_LUAMOD_R4; anchor check enforces correctness)"
  python3 - "$CAND/src/LuaMod.cpp" "$SA/LuaMod-insertion-block.txt" <<'PY'
import sys
path, blockp = sys.argv[1], sys.argv[2]
src = open(path, encoding='utf-8').read()
anchor = '            setup_lua_require_paths(lua);\n'
block = open(blockp, encoding='utf-8').read()
if not block.endswith('\n'):
    block += '\n'
# exactly one call-site anchor (the definition lives at file scope, the catch
# handler mention is different text — verify before inserting)
count = src.count(anchor)
assert count == 1, f'expected exactly 1 anchor, found {count}'
src = src.replace(anchor, anchor + block, 1)
open(path, 'w', encoding='utf-8').write(src)
print(f'[restore-round5] inserted block after anchor; LuaMod.cpp now {len(src.splitlines())} lines')
PY
  verify_block || fail "post-insert verification failed"
  info "S2 done"
}

# ---------------------------------------------------------------- S3 driver
restore_driver() {
  if verify_driver >/dev/null 2>&1; then info "SKIP (NEW_UNITS rows already present)"; return; fi
  python3 - "$DRIVER" <<'PY'
import sys
path = sys.argv[1]
src = open(path, encoding='utf-8').read()
rows = (
"    'lua-pathshim':{'source':HERE/'typescan-r4/candidate/src/LuaPathShim.cpp','template':'luamod','note':'agent GA io/path virtual-cwd shim (whole-TU __PROSPERO__)'},\n"
"    'firstlaunch':{'source':HERE/'typescan-r4/candidate/src/FirstLaunchProfile.cpp','template':'unreal-unrealinitializer','note':'agent GT first-launch profile generator TU (whole-TU __PROSPERO__; extern-C RC_first_launch_* pins/verify API)'},\n"
)
anchor = "    'tcselftest':{'source':HERE/'typescan-r4/candidate/src/ToolchainSelfTest.cpp','template':'unreal-unrealinitializer','note':'agent FF wcslen/wstring-rt toolchain probes'},\n"
assert src.count(anchor) == 1, 'tcselftest NEW_UNITS anchor not unique'
src = src.replace(anchor, anchor + rows, 1)
open(path, 'w', encoding='utf-8').write(src)
print('[restore-round5] NEW_UNITS rows re-added after tcselftest')
PY
  python3 -m py_compile "$DRIVER" || fail "driver py_compile failed after edit"
  verify_driver || fail "post-edit driver verification failed"
  info "S3 done"
}

# ------------------------------------------------------------- S4 manifest
restore_markers() {
  if verify_markers >/dev/null 2>&1; then info "SKIP (3 [SHIM] entries already present)"; return; fi
  python3 - "$MARKERS" "$PREP" <<'PY'
import sys, json, io
path, prep = sys.argv[1], sys.argv[2]
m = json.load(open(path))
have = {x['id'] for x in m['markers']}
entries = []
def shim(mid, text, srcline, note):
    return {
        "id": mid,
        "fix": "io/path logical->physical resolver + io/os write confinement (agent GA, Claude round-5 item 4; CHANGES round5_row_sync)",
        "canonical_marker": "[SHIM]",
        "required": [{
            "text": text,
            "encodings": ["utf-16le"],
            "source": f"typescan-r4/candidate/src/LuaPathShim.cpp:{srcline} (Output::send(STR(...)) wide literal — Prospero char16_t lands UTF-16LE; contiguous)"
        }],
        "note": note
    }
if 'shim-installed' not in have:
    entries.append(shim('shim-installed', '[SHIM] path-resolver installed', '1623',
        "Round-5 restore: entry reconstructed from ROUND4-CONTENTS-DECLARATION.md + the set-aside TU (original JSON removed at the round-4 set-aside; literal/line re-verified against round4-setaside/LuaPathShim.cpp — line refs are POST agent-IB writable-area upgrade, 2026-10-11). One fire per mod install."))
if 'shim-io-open' not in have:
    entries.append(shim('shim-io-open', '[SHIM] io-open resolved=', '753',
        "Round-5 restore (see shim-installed note). Budgeted: first 10 resolves per install."))
if 'shim-write-rejected' not in have:
    entries.append(shim('shim-write-rejected', '[SHIM] write-rejected', '604',
        "Round-5 restore (see shim-installed note). Budgeted: first 10 refusals per install; the write-confinement proof marker."))
if not entries:
    print('[restore-round5] nothing to add'); sys.exit(0)
# surgical text append (file is hand-formatted; avoid whole-file reformat).
# Raw shape (verified): entries at 2-space indent, fields at 3, tail is
# "  }\n ]\n}" with no trailing newline; json.dumps(e, indent=1) + a uniform
# 2-space shift reproduces exactly that entry style (ensure_ascii escapes
# match the existing \uXXXX usage).
def fmt(e):
    d = json.dumps(e, indent=1)
    return '\n'.join(('  ' + ln if ln else ln) for ln in d.splitlines())
blob = ',\n'.join(fmt(e) for e in entries)
raw = open(path, encoding='utf-8').read()
tail = raw.rstrip()
assert tail.endswith('}'), 'unexpected manifest tail (expected ... ] ... })'
head = tail[:tail.rfind(']')].rstrip()  # drop the closing ] of markers[]
assert head.endswith('}'), 'unexpected last entry tail'
open(path, 'w', encoding='utf-8').write(head + ',\n' + blob + '\n ]\n}')
check = json.load(open(path))
ids = [x['id'] for x in check['markers']]
print(f"[restore-round5] manifest now {len(ids)} markers")
sys.exit(0 if len(ids) == 18 else 1)
PY
  verify_markers || fail "post-edit manifest verification failed"
  info "S4 done"
}

# ------------------------------------------------------------- S5 CHANGES
restore_changes() {
  if verify_changes >/dev/null 2>&1; then info "SKIP (CHANGES already reconciled)"; return; fi
  python3 - "$CHANGES" "$CAND" <<'PY'
import sys, json, hashlib, re
cp, cand = sys.argv[1], sys.argv[2]
def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
raw = open(cp, encoding='utf-8').read()
luamod_sha = sha(cand + '/src/LuaMod.cpp')
hpp_shim = sha(cand + '/include/LuaPathShim.hpp')
hpp_fl   = sha(cand + '/include/FirstLaunchProfile.hpp')

# 1) refresh LuaMod.cpp candidate_sha256 in both `changes` and `files` rows
out_lines = []
in_luamod_row = 0
for line in raw.splitlines(keepends=True):
    if '/LuaMod.cpp"' in line and '"candidate"' in line:
        in_luamod_row = 1
    elif in_luamod_row and '"candidate_sha256"' in line:
        line = re.sub(r'"candidate_sha256": "[0-9a-f]{64}"', f'"candidate_sha256": "{luamod_sha}"', line)
        in_luamod_row = 0
    out_lines.append(line)
raw = ''.join(out_lines)

# 2) added_headers: append the two hpp paths after the ToolchainSelfTest.hpp
#    entry (it is the array's last element; it gains a trailing comma).
#    Anchors are built from CAND so /tmp mirrors work identically.
anchor_path = cand + '/include/ToolchainSelfTest.hpp'
anchor = '  "' + anchor_path + '"\n'
assert raw.count(anchor) == 1, 'added_headers anchor not unique: ' + anchor_path
raw = raw.replace(anchor, '  "' + anchor_path + '",\n'
                      f'  "{cand}/include/LuaPathShim.hpp",\n'
                      f'  "{cand}/include/FirstLaunchProfile.hpp"\n', 1)

# 3) added_header_hashes: insert the two entries after the ToolchainSelfTest
#    entry (last member of the object)
anchor2 = '  "' + anchor_path + '": "8eb9d3d84de4ff4ff37ec5a0da08833b81ea9909e6593b6ac9943bfc37d7f906"\n'
assert raw.count(anchor2) == 1, 'added_header_hashes anchor not unique'
# the anchor is the object's LAST member (no trailing comma) — it gains one:
add2 = (f'  "{cand}/include/LuaPathShim.hpp": "{hpp_shim}",\n'
        f'  "{cand}/include/FirstLaunchProfile.hpp": "{hpp_fl}"\n')
raw = raw.replace(anchor2, anchor2.replace('\n', ',\n', 1) + add2, 1)

# 4) round5_row_sync record + round4_setaside flip
raw = re.sub(r' "round5_row_sync": \{.*?\},\n', ' "round5_row_sync": {\n'
             '  "date": "2026-10-11",\n'
             '  "by": "restore-round5.sh (agent II staging; ROOT executes at AC power)",\n'
             '  "restored": "LuaPathShim.{cpp,hpp} + FirstLaunchProfile.{cpp,hpp} + LuaMod block + lua-pathshim/firstlaunch NEW_UNITS + 3 [SHIM] markers",\n'
             f'  "luamod_candidate_sha256": "{luamod_sha}",\n'
             '  "census": "391+5=396",\n'
             '  "kb_tu": "stays set aside (Claude-reviewed gated build only)"\n'
             ' },\n', raw, count=1, flags=re.S)
raw = raw.replace(' "round4_setaside": true', ' "round4_setaside": false,\n "round5_restored": true', 1)

json.loads(raw)  # valid-JSON gate
open(cp, 'w', encoding='utf-8').write(raw)
print('[restore-round5] CHANGES.json reconciled')
PY
  verify_changes || fail "post-edit CHANGES verification failed"
  info "S5 done"
}

# ---------------------------------------------------------------- main
case "$MODE" in
  verify)
    rc=0
    verify_files || rc=1
    verify_block || rc=1
    verify_driver || rc=1
    verify_markers || rc=1
    verify_changes || rc=1
    if [ $rc -eq 0 ]; then
      info "VERIFY: FULLY RESTORED (files + block + rows + markers + CHANGES)"
    else
      info "VERIFY: set-aside holes remain (this is the expected state BEFORE the round-5 restore)"
    fi
    exit $rc
    ;;
  apply)
    info "applying round-5 restore to tree: $RT (prep: $PREP)"
    restore_files
    restore_block
    restore_driver
    restore_markers
    restore_changes
    info "restore complete — re-running in --verify mode for the final word:"
    exec bash "$0" --verify --tree "$RT" --prep "$PREP"
    ;;
esac
