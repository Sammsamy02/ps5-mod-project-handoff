#!/usr/bin/env bash
# test_prelaunch_rowcheck.sh -- OFFLINE fixture tests for the STEP 5.5
# loadlist row deploy gate (harvest/prelaunch_rowcheck.py + its wiring in
# prep_modded_launch.py). Mac-only, battery-safe: python3 only, --from-dir
# fixture mode ONLY, zero network calls, zero console contact, no builds.
#
# Cases (task contract):
#   1  all-present                      -> rc 0, ok true
#   2  one-missing (the -6 incident)    -> rc 1, MISSING + exact row text
#   3  wrong-size (row vs file)         -> rc 1, SIZE-MISMATCH declared/actual
#   4  empty-loadlist refuse            -> rc 1, REFUSED (no file rows);
#      4b truly empty (0-byte) list     -> rc 1, REFUSED
#   +   loadlist itself missing         -> rc 1, REFUSED
#   +   malformed row                   -> rc 1, MALFORMED
#   +   --expect sha-mismatch           -> rc 1, SHA-MISMATCH
#   +   --expect full pass + extra pin  -> rc 0 with a warning
#   +   prep wiring: py_compile both, --dry-run shows STEP 5.5, and the
#       _rowcheck_verdict translation is unit-tested from the saved docs.
#
# Receipts land durable under outputs/2026-10-08-l25-prep/evidence/
# rowcheck-tests/<ts>/ (evidence-retention law: nothing evidential lives
# only in /tmp). Run: bash test_prelaunch_rowcheck.sh

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PREP="$(dirname "$HERE")"
ROWCHECK="$HERE/prelaunch_rowcheck.py"
PREPSCRIPT="$HERE/prep_modded_launch.py"
PY=python3

TS="$(date -u +%Y%m%dT%H%M%SZ)"
ROOT="$PREP/evidence/rowcheck-tests/$TS"
mkdir -p "$ROOT"
RECEIPT="$ROOT/RECEIPT.md"

pass=0
fail=0

note() { printf '%s\n' "$*" | tee -a "$RECEIPT"; }

check_rc() { # label expected actual
  if [ "$2" = "$3" ]; then
    note "PASS  $1 (rc=$3)"; pass=$((pass+1))
  else
    note "FAIL  $1 (expected rc=$2, got rc=$3)"; fail=$((fail+1))
  fi
}

check_true() { # label rc_of_assertion(0=ok)
  if [ "$2" = "0" ]; then
    note "PASS  $1"; pass=$((pass+1))
  else
    note "FAIL  $1"; fail=$((fail+1))
  fi
}

assert_json() { # file expr... -- every expr must eval truthy over doc d
  local f="$1"; shift
  "$PY" - "$f" "$@" <<'EOF'
import json, sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    d = json.load(fh)
for expr in sys.argv[2:]:
    if not eval(expr, {"d": d}):
        print("ASSERT-FALSE: %s" % expr)
        sys.exit(1)
sys.exit(0)
EOF
}

# --- fixture builder: exact-size files; loadlist rows generated from the
# ACTUAL byte counts (the wc -c law), mirroring the live composition
# shape (base rows + zzdiag + the ueselftest/enabled.txt 0 B row from
# the -6 incident).
build_fixture() { # dir
  "$PY" - "$1" <<'EOF'
import os, sys
root = sys.argv[1]
os.makedirs(root, exist_ok=True)
def put(rel, data):
    p = os.path.join(root, *rel.split("/"))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(data)
    return len(data)
files = [
    ("ue4ss/UE4SS-settings.ini", b"# settings filler line\n" * 11),
    ("ue4ss/PS5Pad.ini", b"[PadProfile]\nL3+R3 = F12\n"),
    ("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", b"-- spawner\n" * 33),
    ("ue4ss/Mods/TownfallItemSpawner/enabled.txt", b""),
    ("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", b"-- helpers\n" * 21),
    ("ue4ss/Mods/zzdiag/Scripts/main.lua", b"-- zz\n" * 7),
    ("ue4ss/Mods/zzdiag/enabled.txt", b""),
    ("ue4ss/Mods/ueselftest/enabled.txt", b""),
    ("ue4ss/Mods/ueselftest/Scripts/main.lua", b"-- st\n" * 5),
]
header = ("modforge-loadlist\t1\ntitle\tPPSA33286\nengine\tunreal\n"
          "framework\tue4ss-ps5\nmod\tTownfallItemSpawner\t1.0.0")
lines = [header]
for rel, data in files:
    n = put(rel, data)
    role = "config" if rel.count("/") == 1 else "lua_mod"
    lines.append("file\t%s\t%s\t%d" % (role, rel, n))
with open(os.path.join(root, "loadlist.txt"), "w") as f:
    f.write("\n".join(lines) + "\n")
EOF
}

note "# rowcheck offline fixture test receipt"
note ""
note "- utc run : $TS"
note "- tool     : $ROWCHECK"
note "- prep     : $PREPSCRIPT"
note "- root     : $ROOT (fixtures + per-case outputs live here)"
note "- law      : fixture mode only (--from-dir); zero network/console"
note ""

# --- CASE 1: all present -> rc 0 -----------------------------------------
FX="$ROOT/fx-all"; build_fixture "$FX"
"$PY" "$ROWCHECK" --from-dir "$FX" --out-dir "$ROOT/case1" --json \
  > "$ROOT/case1.json" 2> "$ROOT/case1.stderr"; rc=$?
check_rc "case1 all-present (rc)" 0 "$rc"
assert_json "$ROOT/case1.json" \
  'd["ok"] is True' \
  'd["counts"]["rows"] == 9' \
  'd["counts"]["pass"] == 9' \
  'd["counts"]["missing"] == 0 and d["counts"]["size_mismatch"] == 0' \
  'd["refusal"] is None' \
  'all(r["status"] == "PASS" for r in d["rows"])'
check_true "case1 all-present (asserts)" $?
[ -f "$ROOT/case1/rowcheck-receipt-"*.json ] 2>/dev/null || true
ls "$ROOT"/case1/rowcheck-receipt-*.json >/dev/null 2>&1
check_true "case1 receipt written durably" $?

# --- CASE 2: one missing (the exact -6 incident row) -> rc 1 --------------
FX2="$ROOT/fx-missing"; build_fixture "$FX2"
rm "$FX2/ue4ss/Mods/ueselftest/enabled.txt"
"$PY" "$ROWCHECK" --from-dir "$FX2" --out-dir "$ROOT/case2" --json \
  > "$ROOT/case2.json" 2> "$ROOT/case2.stderr"; rc=$?
check_rc "case2 one-missing / the -6 row (rc)" 1 "$rc"
assert_json "$ROOT/case2.json" \
  'd["ok"] is False' \
  'd["counts"]["missing"] == 1' \
  'd["counts"]["pass"] == 8' \
  'any(r["status"] == "MISSING" and r["path"] == "ue4ss/Mods/ueselftest/enabled.txt" and r["raw"] == "file\tlua_mod\tue4ss/Mods/ueselftest/enabled.txt\t0" and r["declared_size"] == 0 for r in d["rows"])' \
  'any("-6" in (r.get("detail") or "") for r in d["rows"])'
check_true "case2 one-missing (asserts: exact row + -6 cite)" $?

# --- CASE 3: wrong size -> rc 1 --------------------------------------------
FX3="$ROOT/fx-wrongsize"; build_fixture "$FX3"
printf 'ZZZ' >> "$FX3/ue4ss/Mods/zzdiag/Scripts/main.lua"
"$PY" "$ROWCHECK" --from-dir "$FX3" --out-dir "$ROOT/case3" --json \
  > "$ROOT/case3.json" 2> "$ROOT/case3.stderr"; rc=$?
check_rc "case3 wrong-size (rc)" 1 "$rc"
declared=$("$PY" -c 'import json,sys; d=json.load(open(sys.argv[1])); print([r for r in d["rows"] if r["status"]=="SIZE-MISMATCH"][0]["declared_size"])' "$ROOT/case3.json")
actual=$("$PY" -c 'import json,sys; d=json.load(open(sys.argv[1])); print([r for r in d["rows"] if r["status"]=="SIZE-MISMATCH"][0]["actual_size"])' "$ROOT/case3.json")
if [ "$actual" = "$((declared + 3))" ]; then
  note "PASS  case3 wrong-size (declared=$declared actual=$actual, +3 bytes)"; pass=$((pass+1))
else
  note "FAIL  case3 wrong-size (declared=$declared actual=$actual, expected +3)"; fail=$((fail+1))
fi
assert_json "$ROOT/case3.json" \
  'd["counts"]["size_mismatch"] == 1' \
  'any(r["status"] == "SIZE-MISMATCH" and r["path"] == "ue4ss/Mods/zzdiag/Scripts/main.lua" and "code=-8" in r["detail"] for r in d["rows"])'
check_true "case3 wrong-size (asserts + code=-8 cite)" $?

# --- CASE 4: header-only loadlist -> rc 1 refusal --------------------------
FX4="$ROOT/fx-emptyrows"
mkdir -p "$FX4"
printf 'modforge-loadlist\t1\ntitle\tPPSA33286\nengine\tunreal\nframework\tue4ss-ps5\n' > "$FX4/loadlist.txt"
"$PY" "$ROWCHECK" --from-dir "$FX4" --out-dir "$ROOT/case4" --json \
  > "$ROOT/case4.json" 2> "$ROOT/case4.stderr"; rc=$?
check_rc "case4 header-only loadlist refuse (rc)" 1 "$rc"
assert_json "$ROOT/case4.json" \
  'd["ok"] is False' \
  'd["refusal"] is not None and "ZERO" in d["refusal"]'
check_true "case4 header-only refuse (asserts)" $?

# --- CASE 4b: 0-byte loadlist -> rc 1 refusal ------------------------------
FX4B="$ROOT/fx-emptyfile"; mkdir -p "$FX4B"; : > "$FX4B/loadlist.txt"
"$PY" "$ROWCHECK" --from-dir "$FX4B" --out-dir "$ROOT/case4b" --json \
  > "$ROOT/case4b.json" 2> "$ROOT/case4b.stderr"; rc=$?
check_rc "case4b 0-byte loadlist refuse (rc)" 1 "$rc"
assert_json "$ROOT/case4b.json" \
  'd["ok"] is False and d["refusal"] is not None'
check_true "case4b 0-byte refuse (asserts)" $?

# --- CASE 5: loadlist itself missing -> rc 1 refusal -----------------------
FX5="$ROOT/fx-nolist"; build_fixture "$FX5"; rm "$FX5/loadlist.txt"
"$PY" "$ROWCHECK" --from-dir "$FX5" --out-dir "$ROOT/case5" --json \
  > "$ROOT/case5.json" 2> "$ROOT/case5.stderr"; rc=$?
check_rc "case5 loadlist-missing refuse (rc)" 1 "$rc"
assert_json "$ROOT/case5.json" \
  'd["ok"] is False' \
  'd["refusal"] is not None and "loadlist.txt absent" in d["refusal"]'
check_true "case5 loadlist-missing refuse (asserts)" $?

# --- CASE 6: malformed row -> rc 1 -----------------------------------------
FX6="$ROOT/fx-malformed"; build_fixture "$FX6"
printf 'file\tlua_mod\tue4ss/Mods/broken/only-three-fields\n' >> "$FX6/loadlist.txt"
"$PY" "$ROWCHECK" --from-dir "$FX6" --out-dir "$ROOT/case6" --json \
  > "$ROOT/case6.json" 2> "$ROOT/case6.stderr"; rc=$?
check_rc "case6 malformed row (rc)" 1 "$rc"
assert_json "$ROOT/case6.json" \
  'd["counts"]["malformed"] == 1' \
  'any(r["status"] == "MALFORMED" and "only-three-fields" in r["raw"] for r in d["rows"])'
check_true "case6 malformed row (asserts)" $?

# --- CASE 7: --expect sha mismatch -> rc 1 ---------------------------------
FX7="$ROOT/fx-shamismatch"; build_fixture "$FX7"
"$PY" - "$FX7" > "$ROOT/case7.expect.json" <<'EOF'
import hashlib, json, os, sys
root = sys.argv[1]
p = os.path.join(root, "ue4ss/Mods/zzdiag/Scripts/main.lua")
data = open(p, "rb").read()
real = hashlib.sha256(data).hexdigest()
wrong = ("0" * 64) if real[0] != "0" else ("1" * 64)
json.dump({"ue4ss/Mods/zzdiag/Scripts/main.lua": {"size": len(data),
                                                  "sha256": wrong}},
          sys.stdout)
EOF
"$PY" "$ROWCHECK" --from-dir "$FX7" --expect "$ROOT/case7.expect.json" \
  --out-dir "$ROOT/case7" --json > "$ROOT/case7.json" 2> "$ROOT/case7.stderr"; rc=$?
check_rc "case7 --expect sha-mismatch (rc)" 1 "$rc"
assert_json "$ROOT/case7.json" \
  'd["counts"]["sha_mismatch"] == 1' \
  'any(r["status"] == "SHA-MISMATCH" and r["path"] == "ue4ss/Mods/zzdiag/Scripts/main.lua" for r in d["rows"])'
check_true "case7 sha-mismatch (asserts)" $?

# --- CASE 8: --expect full pass + an extra ignored pin -> rc 0 -------------
"$PY" - "$FX" > "$ROOT/case8.expect.json" <<'EOF'
import hashlib, json, os, sys
root = sys.argv[1]
pins = {}
for rel in ("ue4ss/UE4SS-settings.ini",
            "ue4ss/Mods/ueselftest/enabled.txt",
            "ue4ss/Mods/ueselftest/Scripts/main.lua"):
    p = os.path.join(root, *rel.split("/"))
    data = open(p, "rb").read()
    pins[rel] = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
pins["ue4ss/Mods/not-a-row/main.lua"] = {"sha256": "f" * 64}  # extra pin
json.dump(pins, sys.stdout)
EOF
"$PY" "$ROWCHECK" --from-dir "$FX" --expect "$ROOT/case8.expect.json" \
  --out-dir "$ROOT/case8" --json > "$ROOT/case8.json" 2> "$ROOT/case8.stderr"; rc=$?
check_rc "case8 --expect pass + extra pin (rc)" 0 "$rc"
assert_json "$ROOT/case8.json" \
  'd["ok"] is True and d["counts"]["pass"] == 9' \
  'any("not-a-row" in w for w in d["warnings"])'
check_true "case8 expect-pass + extra-pin warn (asserts)" $?

# --- CASE 9: usage error -> rc 2 --------------------------------------------
"$PY" "$ROWCHECK" --from-dir "$ROOT/does-not-exist" --out-dir "$ROOT/case9" \
  > "$ROOT/case9.json" 2> "$ROOT/case9.stderr"; rc=$?
check_rc "case9 usage error (rc)" 2 "$rc"

# --- CASE 10: prep wiring ----------------------------------------------------
"$PY" -m py_compile "$ROWCHECK" "$PREPSCRIPT" 2> "$ROOT/case10.compile.err"; rc=$?
check_rc "case10 py_compile both scripts" 0 "$rc"

"$PY" "$PREPSCRIPT" --variant A --prx-path /nonexistent/prx-self.prx \
  --dry-run > "$ROOT/case10.dryrun.out" 2>&1; rc=$?
check_rc "case10 prep --dry-run (rc)" 0 "$rc"
grep -q "STEP 5.5" "$ROOT/case10.dryrun.out" \
  && grep -q "prelaunch_rowcheck.py" "$ROOT/case10.dryrun.out" \
  && grep -q "upload the missing rows or fix the" "$ROOT/case10.dryrun.out"
check_true "case10 dry-run shows the 5.5 gate + fix text" $?

# unit-test the prep-side verdict translation from the saved case docs
"$PY" - "$PREPSCRIPT" "$ROOT/case1.json" "$ROOT/case2.json" > "$ROOT/case10.verdict.out" 2>&1 <<'EOF'
import importlib.util, json, sys
spec = importlib.util.spec_from_file_location("prep", sys.argv[1])
prep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)
ok1, s1, f1 = prep._rowcheck_verdict(json.load(open(sys.argv[2])), 0)
ok2, s2, f2 = prep._rowcheck_verdict(json.load(open(sys.argv[3])), 1)
assert ok1 is True and not f1 and "9 pass" in s1, (ok1, s1, f1)
assert ok2 is False and len(f2) == 1 and "1 missing" in s2, (ok2, s2, f2)
assert f2[0]["raw"].endswith("ueselftest/enabled.txt\t0"), f2[0]
print("verdict-translation OK: pass-doc -> gate PASS; -6 doc -> abort")
EOF
check_true "case10 _rowcheck_verdict translation (pass + -6 abort)" $?

# --- verdict -----------------------------------------------------------------
note ""
note "TOTAL: $pass pass, $fail fail"
if [ "$fail" = "0" ]; then
  note "VERDICT: ALL OFFLINE FIXTURE TESTS PASS"
  exit 0
fi
note "VERDICT: FAILURES PRESENT"
exit 1
