#!/bin/sh
# apply-promotion.sh — OPTIONAL controller helper for the patches.json 51->54
# promotion prepared by worker-e2e-suite-completion (see PROMOTION-PREP.md).
# NOT executed by the preparing worker. The controller owns the frozen
# worker-d1-chain-relink tree; run this only as part of a controller-approved
# promotion step. Every gate must pass BEFORE any write; aborts otherwise.
set -eu

TEAM=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team
NEW=$TEAM/worker-e2e-payload-build/patches.json.new
COPY=$TEAM/worker-e2e-suite-completion/promotion/patches.json.new.copy
TREE=$TEAM/worker-d1-chain-relink/composition
RCPT=$TREE/receipts/patches.json

sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
stop() { echo "PROMOTION-ABORT: $*" >&2; exit 2; }

# gates (no writes yet)
[ "$(sha "$NEW")" = "63c28979d21a9a563f29334cd094568f11fd6cc2d56abfe31158ec14a42c80f9" ] \
  || stop "patches.json.new sha mismatch"
cmp -s "$NEW" "$COPY" || stop "patches.json.new differs from verified copy"
[ "$(sha "$TREE/vendored/modinst_core.c")" = "45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d" ] \
  || stop "composition/vendored/modinst_core.c is not the pristine 51-tuple core (tree diverged?)"
python3 - "$RCPT" <<'EOF' || stop "current receipt is not the 51-tuple chain"
import json,sys
d=json.load(open(sys.argv[1]))
assert len(d["patches"])==51, len(d["patches"])
assert d["patched_sha256"]=="45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d"
EOF
[ -f "$RCPT.51.bak" ] && stop "backup already exists (previous promotion?)"

# writes: backup, install, verify-after
cp -p "$RCPT" "$RCPT.51.bak"
cp "$NEW" "$RCPT"
python3 - "$RCPT" <<'EOF'
import json,sys
d=json.load(open(sys.argv[1]))
ids=[p["id"] for p in d["patches"]]
assert len(ids)==54
assert ids[:51]==[p["id"] for p in json.load(open(sys.argv[1]+".51.bak"))["patches"]]
assert ids[-3:]==["g1-cmd-entry-parse","g1-cmd-entry-allowlist","g1-cmd-entry-consume"]
assert d["patched_sha256"]=="43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0"
print("PROMOTION-OK: 54 tuples; prefix == 51-tuple backup; tail g1-cmd-entry-{parse,allowlist,consume}; patched_sha256 43ea66c1...")
EOF
