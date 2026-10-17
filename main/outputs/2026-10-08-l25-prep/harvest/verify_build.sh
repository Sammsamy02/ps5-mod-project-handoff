#!/bin/bash
# verify_build.sh — POST-BUILD, PRE-UPLOAD PRX verification gate (Mac-local, no console).
#
# Catches "the fix didn't compose in" failures BEFORE wasting a console launch.
# The 2026-10-09/10 seam round: the previous build's Lua self-test lines were
# composed OUT of the PRX at link time (absent even in the bare pre-sign ELF)
# and we only discovered it on console. Claude's fix, implemented here: before
# upload, grep the PRX for the critical marker strings in BOTH UTF-8 and
# UTF-16LE to prove they were composed in.
#
# Usage:
#   verify_build.sh <prx-path> [receipt-path-or-dir]
#   verify_build.sh <prx-path> --manifest [manifest.json] [receipt-path-or-dir]
#
#     <prx-path>            the fake-signed PRX/SELF about to be uploaded
#     [receipt-path-or-dir] FINAL-RECEIPT JSON (or a directory containing
#                           FINAL-RECEIPT*.json). Optional: when omitted the
#                           script walks up from the PRX (max 4 parents) and
#                           auto-discovers FINAL-RECEIPT*.json, preferring a
#                           receipt whose prx.sha256 matches the artifact.
#     --manifest [path]     MANIFEST MODE (agent FG, 2026-10-09 hardening:
#                           per-fix declared markers). Section [1/3] then
#                           requires EVERY marker declared in the manifest
#                           (default: markers-declared.json next to this
#                           script) to be composed into the PRX — UTF-16LE
#                           is the PRIMARY encoding (wide STR/StringType
#                           literals are exactly the phantom-build class),
#                           UTF-8 is also checked and both counts are
#                           reported. Per-marker PASS/MISS is printed and
#                           ANY miss exits nonzero. Identity ([2/3]) and
#                           receipt ([3/3]) checks still run in this mode.
#
# Checks:
#   1. Marker grep:
#      default mode  — delegated to ./grep_markers.py, same directory:
#      [LUA-FN], [ST] lua-ffo, [FN] pool-decoder verified  (CRITICAL — gate)
#      [DRY], [FN] seam refused, [FFO] query-fallback,
#      vmt_engine_tick_drain, [ST] fname, [Seeds][L27      (informational)
#      each searched as UTF-8 AND UTF-16LE (wide STR literals land UTF-16LE,
#      narrow char*/snprintf/fmt templates land UTF-8).
#      --manifest    — every marker in markers-declared.json is REQUIRED
#      (per-fix literals with per-entry encodings; see that file's
#      encoding_rules). Runtime-rendered lines (e.g. a logged "[ST] lua-ffo
#      PASS") never appear contiguously in the PRX, so such markers are
#      declared as their composable literals, ALL of which must be present.
#   2. PRX identity: magic must be 4f153d1d (fake-signed SELF;
#      5414f5ee Sony-signed is readback-acceptable but never our output).
#   3. sha256 + byte size must match the FINAL-RECEIPT's prx block
#      (prx.sha256 + prx.bytes are authoritative; prx.path may be stale
#      across same-tree rebuilds, so only sha/bytes gate).
#   4. One-line timing stamp at the end.
#
# Exit codes:
#   0 = PASS (all critical/declared markers composed in; magic + receipt
#       identity match)
#   1 = FAIL — at least one CRITICAL/declared marker missing (do NOT upload)
#   2 = usage / environment error
#   3 = FAIL — bad magic (not a signed SELF; never upload 7f454c46)
#   4 = FAIL — receipt mismatch or no FINAL-RECEIPT found
#
# Permanent console rules honored (outputs/2026-10-06-recovery-fakesign/
# POSTMORTEM.md): this gate runs BEFORE upload; it never touches the console.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
GREP_MARKERS="$SCRIPT_DIR/grep_markers.py"
DEFAULT_MANIFEST="$SCRIPT_DIR/markers-declared.json"

MAGIC_FSELF="4f153d1d"
MAGIC_SONY="5414f5ee"
MAGIC_ELF="7f454c46"

# ---------------------------------------------------------------------------
# Preflight / argument parsing
# ---------------------------------------------------------------------------
if [ "$#" -lt 1 ] || [ "$#" -gt 4 ]; then
  echo "usage: $0 <prx-path> [--manifest [markers.json]] [receipt-path-or-dir]" >&2
  exit 2
fi

PRX=$1
shift

MANIFEST_MODE=0
MANIFEST_PATH=""
RECEIPT_ARG=""
while [ "$#" -gt 0 ]; do
  case "$1" in
    --manifest)
      MANIFEST_MODE=1
      shift
      if [ "$#" -gt 0 ] && [ "${1:0:1}" != "-" ]; then
        MANIFEST_PATH=$1
        shift
      fi
      ;;
    *)
      if [ -n "$RECEIPT_ARG" ]; then
        echo "usage: $0 <prx-path> [--manifest [markers.json]] [receipt-path-or-dir]" >&2
        exit 2
      fi
      RECEIPT_ARG=$1
      shift
      ;;
  esac
done
if [ "$MANIFEST_MODE" -eq 1 ] && [ -z "$MANIFEST_PATH" ]; then
  MANIFEST_PATH=$DEFAULT_MANIFEST
fi

T_START=$(python3 -c 'import time; print(int(time.time()*1000))')
FAIL_REASON=""

if [ ! -f "$PRX" ]; then
  echo "FAIL: PRX not found: $PRX" >&2
  exit 2
fi
for TOOL in python3 xxd shasum stat; do
  command -v "$TOOL" >/dev/null 2>&1 || { echo "FAIL: required tool missing: $TOOL" >&2; exit 2; }
done
if [ "$MANIFEST_MODE" -eq 0 ] && [ ! -f "$GREP_MARKERS" ]; then
  echo "FAIL: helper not found next to this script: $GREP_MARKERS" >&2
  exit 2
fi
if [ "$MANIFEST_MODE" -eq 1 ] && [ ! -f "$MANIFEST_PATH" ]; then
  echo "FAIL: marker manifest not found: $MANIFEST_PATH" >&2
  exit 2
fi

echo "==================================================================="
echo "verify_build.sh — post-build pre-upload gate"
echo "  prx: $PRX"
if [ "$MANIFEST_MODE" -eq 1 ]; then
  echo "  mode: --manifest (per-fix declared markers REQUIRED): $MANIFEST_PATH"
fi
echo "==================================================================="

# ---------------------------------------------------------------------------
# 1. Marker grep (critical gate)
# ---------------------------------------------------------------------------
if [ "$MANIFEST_MODE" -eq 1 ]; then
  echo ""
  echo "[1/3] DECLARED-MARKER manifest scan (UTF-16LE primary; UTF-8 checked, both reported)"
  python3 - "$PRX" "$MANIFEST_PATH" <<'PYEOF'
import hashlib, json, sys
from datetime import datetime, timezone

prx_path, manifest_path = sys.argv[1], sys.argv[2]
try:
    with open(prx_path, "rb") as f:
        data = f.read()
except OSError as e:
    print("RESULT: MANIFEST-ERROR"); print("detail: unreadable prx: %s" % e); sys.exit(2)
try:
    with open(manifest_path) as f:
        m = json.load(f)
except Exception as e:
    print("RESULT: MANIFEST-ERROR"); print("detail: unreadable manifest %s: %s" % (manifest_path, e)); sys.exit(2)

markers = m.get("markers") or []
if not markers:
    print("RESULT: MANIFEST-ERROR"); print("detail: manifest declares no markers: %s" % manifest_path); sys.exit(2)

print("manifest: %s (schema %s, %d declared markers)" % (manifest_path, m.get("schema"), len(markers)))
print("prx: sha256=%s bytes=%d" % (hashlib.sha256(data).hexdigest(), len(data)))

def scan(text):
    return {"utf-8": data.count(text.encode("utf-8")),
            "utf-16le": data.count(text.encode("utf-16-le"))}

miss = []
for mk in markers:
    entries = []
    for req in (mk.get("required") or []):
        text = req.get("text")
        if not text:
            print("RESULT: MANIFEST-ERROR"); print("detail: marker %r has a required entry without text" % mk.get("id")); sys.exit(2)
        encs = req.get("encodings") or ["utf-8", "utf-16le"]
        either = bool(req.get("either_encoding"))
        counts = scan(text)
        present = {e: counts.get(e, 0) for e in encs}
        ok = (any(v > 0 for v in present.values()) if either
              else all(v > 0 for v in present.values()))
        entries.append((text, encs, either, present, ok))
    # [gate law 2026-10-12] a marker with ZERO required entries checks nothing: fail it.
    marker_ok = bool(entries) and all(e[4] for e in entries)
    state = "PASS" if marker_ok else "MISS"
    print("[%s] %-26s canonical %r  <- %s" % (state, mk.get("id"), mk.get("canonical_marker"), mk.get("fix")))
    for text, encs, either, present, ok in entries:
        mode = "either-encoding" if either else ("BOTH required" if len(encs) > 1 else "+".join(encs))
        counts_s = " ".join("%s=%d" % (e, present[e]) for e in encs)
        print("        entry %-40r %-14s %s  (%s)" % (text, mode, counts_s, "OK" if ok else "REQUIRED-MISSING"))
    if not marker_ok:
        miss.append(mk.get("id"))

print("SUMMARY: declared=%d pass=%d miss=%d" % (len(markers), len(markers) - len(miss), len(miss)))
if miss:
    print("MISSING DECLARED MARKERS (fix the build, not the gate): " + ", ".join(str(x) for x in miss))
    sys.exit(1)
print("all DECLARED markers composed in (scanned %s)" % datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
sys.exit(0)
PYEOF
  MARKER_RC=$?
  echo "  -> declared-marker gate exit code: $MARKER_RC"
  if [ "$MARKER_RC" -eq 2 ]; then
    echo "FAIL: manifest scanner environment error" >&2
    exit 2
  fi
  if [ "$MARKER_RC" -ne 0 ]; then
    FAIL_REASON="declared-marker-missing"
  fi
else
  echo ""
  echo "[1/3] composition-marker grep (UTF-8 + UTF-16LE)"
  python3 "$GREP_MARKERS" "$PRX"
  MARKER_RC=$?
  echo "  -> marker gate exit code: $MARKER_RC"
  if [ "$MARKER_RC" -eq 2 ]; then
    echo "FAIL: marker scanner environment error" >&2
    exit 2
  fi
  if [ "$MARKER_RC" -ne 0 ]; then
    FAIL_REASON="critical-marker-missing"
  fi
fi

# ---------------------------------------------------------------------------
# 2. PRX identity: magic / sha256 / size
# ---------------------------------------------------------------------------
echo ""
echo "[2/3] PRX identity"
MAGIC=$(xxd -p -l 4 "$PRX")
SHA=$(shasum -a 256 "$PRX" | awk '{print $1}')
SIZE=$(stat -f %z "$PRX")
echo "  magic=$MAGIC  size=$SIZE  sha256=$SHA"

if [ "$MAGIC" = "$MAGIC_FSELF" ]; then
  echo "  magic OK (fake-signed SELF, $MAGIC_FSELF)"
elif [ "$MAGIC" = "$MAGIC_SONY" ]; then
  echo "  magic NOTE: Sony-signed sprx ($MAGIC_SONY) — readback-acceptable, but our pipeline output must be $MAGIC_FSELF"
  [ -z "$FAIL_REASON" ] && FAIL_REASON="magic-sony-not-fself"
else
  echo "  magic FAIL: $MAGIC (expected $MAGIC_FSELF; $MAGIC_ELF = plain ELF / decrypted view — NEVER upload)"
  if [ -z "$FAIL_REASON" ]; then FAIL_REASON="magic"; fi
  # Magic already dooms the artifact; still finish the report below.
fi

# ---------------------------------------------------------------------------
# 3. FINAL-RECEIPT match (sha256 + bytes authoritative)
# ---------------------------------------------------------------------------
echo ""
echo "[3/3] FINAL-RECEIPT match"
RECEIPT_RESULT=$(python3 - "$PRX" "$RECEIPT_ARG" <<'PYEOF'
import glob, json, os, sys
from datetime import datetime, timezone

prx_path, receipt_arg = sys.argv[1], sys.argv[2]

import hashlib
with open(prx_path, "rb") as f:
    data = f.read()
actual_sha = hashlib.sha256(data).hexdigest()
actual_size = len(data)

candidates = []
if receipt_arg:
    if os.path.isdir(receipt_arg):
        candidates = sorted(glob.glob(os.path.join(receipt_arg, "FINAL-RECEIPT*.json")))
    else:
        candidates = [receipt_arg]
else:
    # Walk up from the PRX directory (max 4 parents) auto-discovering receipts.
    d = os.path.dirname(os.path.abspath(prx_path))
    for _ in range(5):
        candidates.extend(sorted(glob.glob(os.path.join(d, "FINAL-RECEIPT*.json"))))
        parent = os.path.dirname(d)
        if parent == d:
            break
        d = parent

candidates = [c for c in candidates if os.path.isfile(c)]
if not candidates:
    print("RESULT: NO-RECEIPT")
    print("detail: no FINAL-RECEIPT*.json found (arg given: %r; walked up from PRX dir)" % (receipt_arg or "none"))
    sys.exit(0)

def load(c):
    try:
        with open(c) as f:
            j = json.load(f)
        return j.get("prx") or {}
    except Exception as e:
        return {"_error": str(e)}

# Prefer a receipt whose prx.sha256 matches the artifact.
chosen = None
for c in candidates:
    p = load(c)
    if p.get("sha256") == actual_sha:
        chosen = (c, p)
        break
if chosen is None:
    # Fall back to the newest receipt; report the mismatch against it.
    newest = max(candidates, key=os.path.getmtime)
    chosen = (newest, load(newest))

c, p = chosen
if "_error" in p:
    print("RESULT: NO-RECEIPT")
    print("detail: unreadable receipt %s: %s" % (c, p["_error"]))
    sys.exit(0)

exp_sha = p.get("sha256")
exp_size = p.get("bytes")
sha_ok = exp_sha == actual_sha
size_ok = exp_size == actual_size
when = datetime.fromtimestamp(os.path.getmtime(c), timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
print("receipt: %s" % c)
print("  receipt prx block (mtime %s): sha256=%s bytes=%s" % (when, exp_sha, exp_size))
print("  artifact                    : sha256=%s bytes=%s" % (actual_sha, actual_size))
if sha_ok and size_ok:
    print("RESULT: MATCH")
elif sha_ok:
    print("RESULT: SIZE-MISMATCH (sha matched but bytes differ: %s vs %s)" % (exp_size, actual_size))
else:
    print("RESULT: SHA-MISMATCH (sha %s != receipt %s)" % (actual_sha, exp_sha))
print("OTHER-CANDIDATES: %d" % (len(candidates) - 1))
PYEOF
)
echo "$RECEIPT_RESULT"

# Only an exact "RESULT: MATCH" is acceptable. Note: the SHA-MISMATCH /
# SIZE-MISMATCH lines contain the substring MATCH, so anchor on the full
# token, not a substring.
case "$RECEIPT_RESULT" in
  *"RESULT: MATCH"*) : ;;
  *"RESULT: SHA-MISMATCH"*)  [ -z "$FAIL_REASON" ] && FAIL_REASON="receipt-sha" ;;
  *"RESULT: SIZE-MISMATCH"*) [ -z "$FAIL_REASON" ] && FAIL_REASON="receipt-size" ;;
  *"RESULT: NO-RECEIPT"*)    [ -z "$FAIL_REASON" ] && FAIL_REASON="receipt-missing" ;;
  *)                         [ -z "$FAIL_REASON" ] && FAIL_REASON="receipt-unknown" ;;
esac

# Magic is fatal on its own.
if [ "$MAGIC" != "$MAGIC_FSELF" ] && [ "$MAGIC" != "$MAGIC_SONY" ]; then
  FAIL_REASON="magic"
  EXIT_CODE=3
elif [ -n "$FAIL_REASON" ]; then
  case "$FAIL_REASON" in
    critical-marker-missing|declared-marker-missing) EXIT_CODE=1 ;;
    *)                       EXIT_CODE=4 ;;
  esac
else
  EXIT_CODE=0
fi

# ---------------------------------------------------------------------------
# Verdict + timing stamp
# ---------------------------------------------------------------------------
echo ""
echo "==================================================================="
if [ "$EXIT_CODE" -eq 0 ]; then
  echo "VERDICT: PASS — markers composed in; identity verified. Safe to upload"
  echo "         (post-upload decrypt-OFF readback still required before launch)."
else
  echo "VERDICT: FAIL ($FAIL_REASON) — DO NOT UPLOAD; fix the build, not the gate."
fi

T_END=$(python3 -c 'import time; print(int(time.time()*1000))')
ELAPSED=$((T_END - T_START))
STAMP=$(python3 -c 'from datetime import datetime,timezone; print(datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))')
echo "VERIFY-TIMESTAMP utc=$STAMP prx=$PRX sha256=$SHA bytes=$SIZE magic=$MAGIC mode=$([ $MANIFEST_MODE -eq 1 ] && echo manifest || echo registry) elapsed_ms=$ELAPSED result=$([ $EXIT_CODE -eq 0 ] && echo PASS || echo "FAIL:$FAIL_REASON")"
exit $EXIT_CODE
