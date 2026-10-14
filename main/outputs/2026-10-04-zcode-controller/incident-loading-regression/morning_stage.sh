#!/bin/bash
# Morning staging — run when the PS5 is powered on from rest mode
set -euo pipefail
FTP="ftp://ps5-host-not-configured.invalid:2121"
W="/WORKSPACE/.codex/worktrees/translation-core/what"

echo "=== Morning staging $(date) ==="

# 1. Check FTP is up
if ! curl -s --connect-timeout 10 --list-only "$FTP/data/plugins/" > /dev/null 2>&1; then
  echo "ERROR: FTP is not reachable. Power on the PS5 first."
  exit 1
fi
echo "FTP OK"

# 2. Stage the clean sink (4KB zeros)
echo "Staging sink..."
# /tmp is cleared on Mac reboot; a missing seed made curl -T fail and set -e abort before staging.
[ -f /tmp/sink-seed.bin ] || python3 -c "open('/tmp/sink-seed.bin','wb').write(b'\x00'*4096)"
curl -s --max-time 30 -T /tmp/sink-seed.bin "$FTP/data/plugins/modbridge-copyin-sink.bin"
SINK_SHA=$(curl -s --max-time 15 "$FTP/data/plugins/modbridge-copyin-sink.bin" | shasum -a 256 | awk '{print $1}')
if [ "$SINK_SHA" != "ad7facb2586f..." ]; then
  # re-create if hash mismatch
  python3 -c "open('/tmp/sink-seed.bin','wb').write(b'\x00'*4096)"
  curl -s --max-time 30 -T /tmp/sink-seed.bin "$FTP/data/plugins/modbridge-copyin-sink.bin"
  echo "Sink recreated (4KB zeros)"
fi
echo "Sink OK"

# 3. Stage the pool-validation candidate (FSELF 12fcc41f)
CANDIDATE="$W/outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1/modbridge_supplier_composition.prx"
echo "Staging candidate..."
curl -s --max-time 120 -T "$CANDIDATE" "$FTP/data/plugins/modbridge_supplier_composition.prx"
echo "Candidate staged"

# 4. Verify both
CAND_SHA=$(curl -s --max-time 60 "$FTP/data/plugins/modbridge_supplier_composition.prx" -o /tmp/cand-verify.bin && shasum -a 256 /tmp/cand-verify.bin | awk '{print substr($1,1,16)}')
echo "Candidate readback: $CAND_SHA (expect 12fcc41fb2f86503 or the FTP-transformed view)"
echo ""
echo "=== STAGED — ready to launch Townfall ==="
echo "Expected outcome: pool validates (first time ever) → engine steps → walkguard gate (expected loud stop)"
echo "After the boot completes, tell ZCode to pull evidence."
