#!/bin/bash
# snapshot_tree.sh — FROZEN-SOURCE SNAPSHOT for the l26 build pipeline (agent FG,
# 2026-10-09; Claude build-discipline item c: "build from a frozen snapshot /
# git commit, and record the commit in the stamp" — the outputs tree is NOT a
# git repo, so a frozen tar + per-file sha256 manifest is the equivalent).
#
# What it freezes (the exact inputs the next build consumes):
#   - the whole candidate tree    2026-10-08-l26-runtime/typescan-r4/candidate/
#   - every part CHANGES.json     l26-runtime/{profile,typescan-r4,thread}/CHANGES.json
#   - the build drivers           l26-runtime/{build_variants.py,build_runtime.py,
#                                 l26_packaging_driver.py}
#   - the verifier + marker gate  2026-10-08-l25-prep/harvest/{verify_build.sh,
#                                 grep_markers.py,markers-declared.json}
#   - this script itself          (snapshot_tree.sh — the snapshot pipeline is
#                                 part of the provenance)
#
# Output (under 2026-10-08-l26-runtime/snapshots/):
#   snapshot-<UTC ts>.tar.gz          the frozen snapshot (arc paths relative
#                                     to outputs/)
#   snapshot-<UTC ts>.manifest.json   sidecar manifest: tar sha256 + EVERY
#                                     file's sha256 (schema tree-snapshot/1)
#   snapshot-<UTC ts>.sha256          plain `shasum -a 256 -c` line for the tar
#
# Prints on stdout (the ONLY contract the parent wires):
#   SNAPSHOT-ID <first 8 hex of the tar sha256>
#   SNAPSHOT-TAR <abs path>
#
# TWO-COMMAND BUILD SEQUENCE (snapshot -> build -> package -> verify).
# Run everything from the WORKTREE ROOT (…/translation-core/what) — the build
# drivers' base pins contain paths relative to that root:
#
#   cd …/translation-core/what
#   1) SNAP=$(outputs/2026-10-08-l25-prep/harvest/snapshot_tree.sh | awk '/^SNAPSHOT-ID /{print $2}')
#   2) BRIDGE_SNAPSHOT_ID="$SNAP" python3 outputs/2026-10-08-l26-runtime/build_variants.py --execute
#      # build_variants.py stamps BRIDGE_SNAPSHOT_ID (plus BRIDGE_BUILD_ID and
#      # BRIDGE_CHANGE_IDS) into candidate/include/GeneratedBuildStamp.hpp
#      # BEFORE compiling; with the env unset the stamp says the not-generated
#      # value and gate G-stamp-present refuses the execute (upload-grade
#      # discipline: a build without a frozen snapshot never compiles).
#   3) python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --prepare \
#      && python3 outputs/2026-10-08-l26-runtime/l26_packaging_driver.py --execute
#   4) outputs/2026-10-08-l25-prep/harvest/verify_build.sh <built.prx> --manifest
#      # requires EVERY marker in markers-declared.json composed into the PRX
#      # (UTF-16LE primary, UTF-8 reported; nonzero exit on any miss)
#
# No console contact; local files only. Exit codes: 0 = frozen OK;
# 1 = a required component is missing; 2 = usage/environment error.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
OUTPUTS=$(cd "$SCRIPT_DIR/../.." && pwd)   # …/what/outputs
L26="$OUTPUTS/2026-10-08-l26-runtime"
SNAPDIR="$L26/snapshots"

CANDIDATE="$L26/typescan-r4/candidate"
CHANGES_PROFILE="$L26/profile/CHANGES.json"
CHANGES_TYPESCAN="$L26/typescan-r4/CHANGES.json"
CHANGES_THREAD="$L26/thread/CHANGES.json"
DRIVER_VARIANTS="$L26/build_variants.py"
DRIVER_RUNTIME="$L26/build_runtime.py"
DRIVER_PACKAGING="$L26/l26_packaging_driver.py"
VERIFY="$SCRIPT_DIR/verify_build.sh"
GREP_MARKERS="$SCRIPT_DIR/grep_markers.py"
MARKERS_DECLARED="$SCRIPT_DIR/markers-declared.json"
SELF="$SCRIPT_DIR/snapshot_tree.sh"

for TOOL in python3 tar shasum awk; do
  command -v "$TOOL" >/dev/null 2>&1 || { echo "FAIL: required tool missing: $TOOL" >&2; exit 2; }
done

MISSING=0
for REQ in "$CANDIDATE" "$CHANGES_PROFILE" "$CHANGES_TYPESCAN" "$CHANGES_THREAD" \
           "$DRIVER_VARIANTS" "$DRIVER_RUNTIME" "$DRIVER_PACKAGING" \
           "$VERIFY" "$GREP_MARKERS" "$MARKERS_DECLARED" "$SELF"; do
  if [ ! -e "$REQ" ]; then echo "FAIL: snapshot component missing: $REQ" >&2; MISSING=1; fi
done
[ "$MISSING" -eq 0 ] || exit 1

TS=$(python3 -c 'from datetime import datetime,timezone; print(datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"))')
TAR_PATH="$SNAPDIR/snapshot-$TS.tar.gz"
MANIFEST_PATH="$SNAPDIR/snapshot-$TS.manifest.json"
SHASUM_PATH="$SNAPDIR/snapshot-$TS.sha256"
mkdir -p "$SNAPDIR"

# Deterministic, sorted arc list (paths relative to outputs/), no metadata junk.
LIST=$(mktemp "${TMPDIR:-/tmp}/snapshot-list.XXXXXX")
trap 'rm -f "$LIST"' EXIT
{
  # candidate tree (whole, minus Finder droppings)
  (cd "$OUTPUTS" && find "2026-10-08-l26-runtime/typescan-r4/candidate" -type f ! -name '.DS_Store' | LC_ALL=C sort)
  # part CHANGES.json + drivers + verifier stack, explicit
  for F in "2026-10-08-l26-runtime/profile/CHANGES.json" \
           "2026-10-08-l26-runtime/typescan-r4/CHANGES.json" \
           "2026-10-08-l26-runtime/thread/CHANGES.json" \
           "2026-10-08-l26-runtime/build_variants.py" \
           "2026-10-08-l26-runtime/build_runtime.py" \
           "2026-10-08-l26-runtime/l26_packaging_driver.py" \
           "2026-10-08-l25-prep/harvest/verify_build.sh" \
           "2026-10-08-l25-prep/harvest/grep_markers.py" \
           "2026-10-08-l25-prep/harvest/markers-declared.json" \
           "2026-10-08-l25-prep/harvest/snapshot_tree.sh"; do
    echo "$F"
  done
} > "$LIST"

COUNT=$(wc -l < "$LIST" | tr -d ' ')
# [gate law 2026-10-12] the 10 explicit files alone used to satisfy "-lt 10":
# an EMPTY candidate tree still snapshotted. Count the candidate tree on its own.
CAND_COUNT=$(cd "$OUTPUTS" && find "2026-10-08-l26-runtime/typescan-r4/candidate" -type f ! -name '.DS_Store' 2>/dev/null | wc -l | tr -d ' ')
echo "snapshot: candidate tree files=$CAND_COUNT, total list entries=$COUNT" >&2
if [ "$CAND_COUNT" -eq 0 ]; then
  echo "FAIL: candidate tree has ZERO files" >&2
  exit 1
fi
if [ "$COUNT" -lt 10 ]; then
  echo "FAIL: snapshot file list implausibly small ($COUNT entries)" >&2
  exit 1
fi

# COPYFILE_DISABLE keeps ._ AppleDouble entries out of the tar on macOS.
COPYFILE_DISABLE=1 tar -czf "$TAR_PATH" -C "$OUTPUTS" -T "$LIST" || {
  echo "FAIL: tar failed" >&2; exit 1; }

TAR_SHA=$(shasum -a 256 "$TAR_PATH" | awk '{print $1}')
SNAP_ID=${TAR_SHA:0:8}

python3 - "$TAR_PATH" "$MANIFEST_PATH" "$SNAP_ID" "$TS" "$COUNT" <<'PYEOF'
import hashlib, json, os, sys
from datetime import datetime, timezone
tar_path, manifest_path, snap_id, ts, count = sys.argv[1:6]
files = []
# hash every file we froze (tar -t is the truth about what the tar consumed)
import subprocess
names = subprocess.run(["tar", "-tzf", tar_path], capture_output=True, text=True, check=True).stdout.splitlines()
# reconstruct the outputs root from the snapshot dir layout (…/outputs/<l26>/snapshots)
l26 = os.path.dirname(os.path.abspath(tar_path))
outputs_root = None
probe = l26
for _ in range(4):
    if os.path.basename(probe) == "outputs":
        outputs_root = probe; break
    probe = os.path.dirname(probe)
if outputs_root is None:
    raise SystemExit("FAIL: cannot locate outputs root from " + l26)
for n in sorted(names):
    p = os.path.join(outputs_root, n)
    if not os.path.isfile(p):
        raise SystemExit("FAIL: manifest hash: not a file: " + p)
    b = open(p, "rb").read()
    files.append({"path": n, "bytes": len(b), "sha256": hashlib.sha256(b).hexdigest()})
tar_bytes = os.path.getsize(tar_path)
manifest = {
    "schema": "tree-snapshot/1",
    "created_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    "snapshot_id": snap_id,
    "snapshot_id_rule": "first 8 hex of tar sha256",
    "tar": {"path": tar_path, "bytes": tar_bytes,
            "sha256": hashlib.sha256(open(tar_path, "rb").read()).hexdigest()},
    "file_count": len(files),
    "files": files,
    "frozen_components": {
        "candidate_tree": "2026-10-08-l26-runtime/typescan-r4/candidate",
        "part_changes": ["2026-10-08-l26-runtime/profile/CHANGES.json",
                          "2026-10-08-l26-runtime/typescan-r4/CHANGES.json",
                          "2026-10-08-l26-runtime/thread/CHANGES.json"],
        "drivers": ["2026-10-08-l26-runtime/build_variants.py",
                     "2026-10-08-l26-runtime/build_runtime.py",
                     "2026-10-08-l26-runtime/l26_packaging_driver.py"],
        "verifier_stack": ["2026-10-08-l25-prep/harvest/verify_build.sh",
                            "2026-10-08-l25-prep/harvest/grep_markers.py",
                            "2026-10-08-l25-prep/harvest/markers-declared.json",
                            "2026-10-08-l25-prep/harvest/snapshot_tree.sh"],
    },
    "consumer": "BRIDGE_SNAPSHOT_ID env var for build_variants.py --execute (stamped into candidate/include/GeneratedBuildStamp.hpp as BRIDGE_SNAPSHOT_ID)",
    "console_contact": False,
}
with open(manifest_path, "w") as f:
    json.dump(manifest, f, indent=2, sort_keys=False)
    f.write("\n")
print("manifest: %s (%d files)" % (manifest_path, len(files)), file=sys.stderr)
PYEOF
[ $? -eq 0 ] || { echo "FAIL: manifest generation failed" >&2; exit 1; }

printf '%s  %s\n' "$TAR_SHA" "$(basename "$TAR_PATH")" > "$SHASUM_PATH"

echo "SNAPSHOT-ID $SNAP_ID"
echo "SNAPSHOT-TAR $TAR_PATH"
echo "SNAPSHOT-MANIFEST $MANIFEST_PATH"
echo "snapshot: $COUNT files -> $TAR_PATH (sha256 $TAR_SHA)" >&2
exit 0
