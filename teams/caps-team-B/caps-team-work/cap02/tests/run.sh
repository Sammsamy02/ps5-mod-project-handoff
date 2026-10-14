#!/bin/bash
# cap02 host tests (Mac-only). Every step's exit code is checked (no pipe can
# hide a failure, review F3). A step that checks nothing fails. A missing
# saved image FAILS unless CAP02_ALLOW_IMAGE_SKIP=1 is set explicitly.
# The final line is printed only when every step passed; exit code = number
# of failed steps (0 = all passed).
set -u
CAP="$(cd "$(dirname "$0")/.." && pwd)"
cd "$CAP" || exit 99
WHAT="/WORKSPACE/.codex/worktrees/translation-core/what"
IMG="$WHAT/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin"
REAL_TS="$WHAT/outputs/2026-10-08-l26-runtime/typescan-r4"
HARVEST="$WHAT/outputs/2026-10-08-l25-prep/harvest"
OUT="${CAP02_TEST_OUT:-$CAP/tests/test-output.txt}"
SCRATCH="$(mktemp -d "${TMPDIR:-/tmp}/cap02-tests.XXXXXX")" || exit 99
trap 'rm -rf "${SCRATCH:?}"' EXIT
: > "$OUT"

STEPS=0
FAILED=0
log() { echo "$@" | tee -a "$OUT"; }

# run_step "<title>" cmd...   -> runs cmd, appends its output, checks its rc.
run_step() {
    local title="$1"; shift
    STEPS=$((STEPS + 1))
    log ""
    log "== $STEPS. $title =="
    local tmp="$SCRATCH/step.$STEPS.out"
    "$@" > "$tmp" 2>&1
    local rc=$?
    cat "$tmp" | tee -a "$OUT"
    if [ $rc -ne 0 ]; then
        FAILED=$((FAILED + 1))
        log "STEP FAIL: $title (exit $rc)"
    else
        log "STEP PASS: $title"
    fi
}

step_cpp() {
    /usr/bin/clang++ -std=c++20 -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Werror \
        -I "$CAP/PATCH/new-files" "$CAP/tests/test_autoprofile.cpp" -o "$SCRATCH/test_autoprofile" || return 1
    "$SCRATCH/test_autoprofile" "$CAP/evidence" > "$SCRATCH/cpp.out" 2>&1
    local rc=$?
    cat "$SCRATCH/cpp.out"
    [ $rc -eq 0 ] || return $rc
    # the binary must report a non-zero check count and no FAIL line
    grep -q '^FAIL' "$SCRATCH/cpp.out" && return 1
    grep -Eq '^PASS: [1-9][0-9]* checks' "$SCRATCH/cpp.out" || { echo "FAIL: no check count"; return 1; }
}

step_image() {
    if [ ! -f "$IMG" ]; then
        if [ "${CAP02_ALLOW_IMAGE_SKIP:-0}" = "1" ]; then
            echo "SKIP (explicitly allowed by CAP02_ALLOW_IMAGE_SKIP=1): saved image not present"
            return 0
        fi
        echo "FAIL: saved image not present at $IMG (set CAP02_ALLOW_IMAGE_SKIP=1 to allow)"
        return 1
    fi
    python3 -B tools/scan_image_identity.py "$IMG" \
        --expect evidence/expected-townfall-identity.json \
        --out "$SCRATCH/townfall-identity-derived.json" || return 1
    cmp -s "$SCRATCH/townfall-identity-derived.json" evidence/townfall-identity-derived.json \
        || { echo "FAIL: derived identity differs from evidence/townfall-identity-derived.json"; return 1; }
    echo "derived identity == saved evidence (1 file compared)"
}

step_identity() {
    local n=0
    for f in PATCH/new-files/AutoLayoutPacks.generated.hpp PATCH/new-files/AutoProfileGenerator.hpp \
             PATCH/01-shadow-launch-ready.diff; do
        [ -f "$f" ] || { echo "FAIL: missing $f"; return 1; }
        n=$((n + 1))
        # comments in the generator mention no game; any hit fails
        if grep -Eiq 'townfall|silent ?hill|ppsa[0-9]|stellar|blade' "$f"; then
            echo "FAIL: game identity text in $f"; return 1
        fi
    done
    [ $n -gt 0 ] || { echo "FAIL: checked zero files"; return 1; }
    echo "PASS: no game identity in shipped files (checked $n files)"
}

step_manifest() {
    python3 - "$CAP/PATCH" <<'EOF'
import hashlib, json, sys
from pathlib import Path
d = Path(sys.argv[1]); m = json.loads((d / "MANIFEST.json").read_text())["sha256"]
bad = [n for n, h in m.items() if hashlib.sha256((d / n).read_bytes()).hexdigest() != h]
print(f"checked {len(m)} pinned patch files; mismatched={bad}")
sys.exit(1 if bad or not m else 0)
EOF
}

# Apply on a scratch copy of the LIVE candidate (read-only copy), then check the
# same pin rule build_variants.py G2 uses, and that a second apply refuses.
step_apply() {
    mkdir -p "$SCRATCH/pre/typescan-r4" "$SCRATCH/post/typescan-r4" || return 1
    cp -R "$REAL_TS/candidate" "$REAL_TS/CHANGES.json" "$SCRATCH/pre/typescan-r4/" || return 1
    cp -R "$REAL_TS/candidate" "$REAL_TS/CHANGES.json" "$SCRATCH/post/typescan-r4/" || return 1
    python3 -B PATCH/apply_cap02.py --typescan-root "$SCRATCH/post/typescan-r4" || return 1
    python3 -B PATCH/apply_cap02.py --typescan-root "$SCRATCH/post/typescan-r4" --execute || return 1
    if python3 -B PATCH/apply_cap02.py --typescan-root "$SCRATCH/post/typescan-r4" --execute > /dev/null 2>&1; then
        echo "FAIL: a second apply did not refuse"; return 1
    fi
    echo "second apply refused (expected)"
    python3 - "$SCRATCH/pre/typescan-r4" "$SCRATCH/post/typescan-r4" <<'EOF'
import hashlib, json, sys
from pathlib import Path
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
def drift(root):
    d = json.loads((root / "CHANGES.json").read_text())
    n, bad = 0, set()
    for r in d["files"]:
        c = r["candidate"]
        if "/typescan-r4/candidate/" not in c: continue
        rel = c.split("/typescan-r4/", 1)[1]
        n += 1
        if sha(root / rel) != r["candidate_sha256"]: bad.add(rel)
    return d, n, bad
pre, post = Path(sys.argv[1]), Path(sys.argv[2])
_, n_pre, bad_pre = drift(pre)
d, n_post, bad_post = drift(post)
hdr = 0; hbad = []
for p, h in d["added_header_hashes"].items():
    if Path(p).name in ("AutoProfileGenerator.hpp", "AutoLayoutPacks.generated.hpp"):
        hdr += 1
        if sha(p) != h: hbad.append(p)
new = sorted(bad_post - bad_pre)
print(f"G2-style pin check: unpatched copy {n_pre} pins, patched copy {n_post} pins + {hdr} new headers")
if bad_pre:
    print(f"NOTE: {len(bad_pre)} pin(s) already drift in the LIVE candidate tree before cap02 "
          f"(integrator work in flight, not caused by cap02): {sorted(bad_pre)}")
print(f"drift added by cap02: {new}; cap02 row drifts: {'candidate/src/UE4SSProgram.cpp' in bad_post}; header hash mismatches: {hbad}")
ok = n_post > 0 and hdr == 2 and not new and not hbad and "candidate/src/UE4SSProgram.cpp" not in bad_post
sys.exit(0 if ok else 1)
EOF
}

step_gates() {
    local rc=0
    python3 -B "$HARVEST/no_detour_gate.py" --root "$SCRATCH/post/typescan-r4/candidate" \
        > "$SCRATCH/nd.out" 2>&1 || rc=1
    tail -n 3 "$SCRATCH/nd.out"
    # fs_throw: the scratch path misses the baseline, so compare patched vs
    # unpatched scratch copies: the patch must add no throwing fs call.
    python3 -B "$HARVEST/fs_throw_gate.py" --root "$SCRATCH/pre/typescan-r4/candidate" > "$SCRATCH/fs.pre" 2>&1
    python3 -B "$HARVEST/fs_throw_gate.py" --root "$SCRATCH/post/typescan-r4/candidate" > "$SCRATCH/fs.post" 2>&1
    local pre_n post_n
    pre_n=$(grep -Ec '^ +[^ ].*:[0-9]+: ' "$SCRATCH/fs.pre")
    post_n=$(grep -Ec '^ +[^ ].*:[0-9]+: ' "$SCRATCH/fs.post")
    grep -Ei 'checked [0-9]+ file' "$SCRATCH/fs.post" | tail -n 1
    echo "fs_throw site lines: unpatched=$pre_n patched=$post_n"
    [ "$pre_n" = "$post_n" ] || { echo "FAIL: patch changed the fs_throw site count"; rc=1; }
    grep -Eqi 'checked [1-9][0-9]* file' "$SCRATCH/fs.post" || { echo "FAIL: fs_throw checked zero files"; rc=1; }
    # field_gate: patched scratch tree must give the same verdict lines as unpatched
    python3 -B "$HARVEST/field_gate.py" --root "$SCRATCH/pre/typescan-r4/candidate" > "$SCRATCH/fg.pre" 2>&1
    local fg_pre=$?
    python3 -B "$HARVEST/field_gate.py" --root "$SCRATCH/post/typescan-r4/candidate" > "$SCRATCH/fg.post" 2>&1
    local fg_post=$?
    tail -n 3 "$SCRATCH/fg.post"
    echo "field_gate rc: unpatched=$fg_pre patched=$fg_post"
    [ $fg_post -eq 0 ] || rc=1
    # Compare the gate's VERDICT lines (fields read / proven / unproven / PASS).
    # Scan-size lines legitimately differ: the patched tree has the 2 new
    # headers (+2 files scanned, a few more classes indexed, line shifts).
    local pat='distinct engine fields read|unproven member read|^FIELD GATE: (PASS|FAIL)'
    grep -E "$pat" "$SCRATCH/fg.pre" > "$SCRATCH/fg.pre.sum"
    grep -E "$pat" "$SCRATCH/fg.post" > "$SCRATCH/fg.post.sum"
    cat "$SCRATCH/fg.post.sum"
    echo "field_gate verdict lines compared: $(wc -l < "$SCRATCH/fg.post.sum" | tr -d ' ')"
    [ "$(wc -l < "$SCRATCH/fg.post.sum" | tr -d ' ')" -ge 3 ] || { echo "FAIL: field_gate verdict lines missing"; rc=1; }
    if ! diff "$SCRATCH/fg.pre.sum" "$SCRATCH/fg.post.sum"; then
        echo "FAIL: field_gate verdict/counts changed"; rc=1
    fi
    local fpre fpost
    fpre=$(sed -nE 's/^FIELD GATE: scanned ([0-9]+) files.*/\1/p' "$SCRATCH/fg.pre")
    fpost=$(sed -nE 's/^FIELD GATE: scanned ([0-9]+) files.*/\1/p' "$SCRATCH/fg.post")
    echo "field_gate files scanned: unpatched=$fpre patched=$fpost (expect +2 new headers)"
    [ -n "$fpre" ] && [ -n "$fpost" ] && [ "$fpost" -eq $((fpre + 2)) ] \
        || { echo "FAIL: field_gate did not scan exactly the 2 new headers extra"; rc=1; }
    return $rc
}

# Compile the shadow for x86-64 SysV against the REAL R11 compiled declaration
# (stable copy in profile/units, sha-pinned) and require MATCH.
step_integration() {
    local units="$WHAT/outputs/2026-10-08-l26-runtime/profile/units"
    local lineage="$WHAT/outputs/2026-10-08-l26-build-variants/archive-r6/R11-watch-world-uenum/2026-10-08-l26-package-A/lineage"
    local want=ab8750c5a7aabfeb6692140ecb5b4bae8e74659f40f0f6ab9be4ddd9f6abec9e
    local got
    got=$(shasum -a 256 "$units/CompiledNativeLayoutDeclarations.hpp" | cut -d' ' -f1)
    [ "$got" = "$want" ] || { echo "FAIL: R11 declaration sha $got != $want"; return 1; }
    [ -f "$lineage/NativeLayoutDeclarations.hpp" ] || { echo "FAIL: missing $lineage/NativeLayoutDeclarations.hpp"; return 1; }
    /usr/bin/clang++ -target x86_64-apple-macos11 -std=c++20 -Wall -Werror -I "$CAP/PATCH/new-files" \
        -I "$units" -I "$lineage" "$CAP/tests/integration_harness.cpp" -o "$SCRATCH/harness" || return 1
    "$SCRATCH/harness" > "$SCRATCH/harness.out" 2>&1
    local rc=$?
    cat "$SCRATCH/harness.out"
    [ $rc -eq 0 ] || return $rc
    grep -q 'parsed=1 verdict=MATCH .*member_rows=81 vtable_rows=24' "$SCRATCH/harness.out" \
        || { echo "FAIL: no MATCH with 81/24 rows"; return 1; }
    echo "checked 1 declaration (sha $want)"
}

step_patch02_not_shippable() {
    head -n 1 PATCH/02-NOT-SHIPPABLE-firstlaunch-auto-followon.diff | grep -q '^NOT SHIPPABLE' \
        || { echo "FAIL: PATCH/02 lost its NOT SHIPPABLE banner"; return 1; }
    python3 - "$CAP/PATCH" <<'EOF' || return 1
import json, sys
from pathlib import Path
m = json.loads((Path(sys.argv[1]) / "MANIFEST.json").read_text())["sha256"]
sys.exit(1 if any(k.startswith("02") for k in m) else 0)
EOF
    grep -Eq '02-NOT-SHIPPABLE|firstlaunch' PATCH/apply_cap02.py && { echo "FAIL: apply script references PATCH/02"; return 1; }
    echo "PATCH/02 banner present; not in MANIFEST; not referenced by apply script (3 checks)"
}

run_step "build engine-level layout packs from local evidence" python3 -B tools/build_packs.py
run_step "patch set matches PATCH/MANIFEST.json" step_manifest
run_step "C++ generator tests (clang++ -std=c++20, strict warnings as errors)" step_cpp
run_step "Python image-identity self-test (generic, image-derived)" step_image
run_step "shipped files carry no game identity" step_identity
run_step "shadow compiled x86-64 SysV against the real R11 declaration" step_integration
run_step "apply_cap02.py on a scratch copy of the live candidate + G2 pin rule" step_apply
run_step "gates on the patched scratch tree (no_detour, fs_throw, field_gate)" step_gates
run_step "PATCH/02 stays marked NOT SHIPPABLE" step_patch02_not_shippable

log ""
if [ $STEPS -eq 0 ]; then
    log "FAIL: zero steps ran"; exit 98
fi
if [ $FAILED -ne 0 ]; then
    log "CAP02 HOST TESTS FAILED: $FAILED of $STEPS steps failed"
    exit $FAILED
fi
log "ALL CAP02 HOST TESTS PASSED ($STEPS of $STEPS steps)"
exit 0
