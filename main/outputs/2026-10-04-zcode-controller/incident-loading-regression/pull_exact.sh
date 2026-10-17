#!/bin/bash
# Evidence pull with exact-file binding (audit fix #1)
# Usage: pull_exact.sh <label>
set -euo pipefail
LABEL="${1:-unnamed}"
TS=$(date +%Y%m%d-%H%M%S)
DIR="/tmp/pulls/${LABEL}-${TS}"
mkdir -p "$DIR"
FTP="ftp://ps5-host-not-configured.invalid:2121"

# Fetch each file; fail loudly if any is unreadable
fetch() {
  local name="$1" path="$2"
  if curl -s --max-time 15 "$FTP$path" -o "$DIR/$name" 2>/dev/null; then
    local sz=$(stat -f%z "$DIR/$name" 2>/dev/null || echo 0)
    echo "$name: $sz B"
    if [ "$sz" = "0" ] && [ "$name" != "sink" ]; then
      echo "WARNING: $name is 0 bytes (may be genuinely empty or fetch failed)"
    fi
  else
    echo "ERROR: failed to fetch $name from $path"
    rm -f "$DIR/$name"
  fi
}

fetch "status.txt"   "/data/modbridge/roots/PPSA33286/runtime-status.txt"
fetch "ue4ss.log"    "/data/plugins/UE4SS.log"
fetch "sdiag.jsonl"  "/data/modbridge-supplier-diag.jsonl"
fetch "sink.bin"     "/data/plugins/modbridge-copyin-sink.bin"
fetch "create-test.bin" "/data/plugins/modbridge-io-create-test.bin" 2>/dev/null || true

# Extract the session from status.txt (the session line is the invocation identity).
# awk, not grep -P: macOS grep has no -P, which silently fell back to max-pid binding.
SESSION=$(awk -F'\t' '$1=="session" && $2 ~ /^[0-9]+$/ {print $2; exit}' "$DIR/status.txt" 2>/dev/null || true)
[ -n "$SESSION" ] || SESSION="unknown"
echo "=== invocation: session $SESSION ==="

# Analyze ONLY the files from this pull
python3 - "$DIR" "$SESSION" <<'PYEOF'
import json, sys, os
d, session = sys.argv[1], sys.argv[2]
sf = os.path.join(d, "sdiag.jsonl")
if not os.path.isfile(sf) or os.path.getsize(sf) == 0:
    print("sdiag unavailable"); sys.exit(0)
recs = [json.loads(l) for l in open(sf) if l.strip()]
# Bind to the session from status.txt, not max PID
target_pid = int(session) if session != "unknown" else None
if target_pid:
    window = [r for r in recs if r["pid"] == target_pid]
    src = f"status session pid {target_pid}"
else:
    # fallback: records past the largest gap in t_ms (a boot boundary)
    pids = sorted(set(r["pid"] for r in recs))
    window = [r for r in recs if r["pid"] == pids[-1]]
    src = f"max pid {pids[-1]} (status unavailable)"
print(f"bound to {src}: {len(window)} records")
for r in window:
    t = r["tag"]
    if any(k in t for k in ("copyin","io_ctrl","pool","module_start","runtime_start","insession","pad_")):
        print(f"  seq{r['seq']} {t} = {r.get('value_hex', r.get('value'))}")
PYEOF

# Sink verification: hash + first bytes (if present)
if [ -f "$DIR/sink.bin" ] && [ $(stat -f%z "$DIR/sink.bin") -gt 0 ]; then
  echo "=== sink: $(stat -f%z "$DIR/sink.bin")B sha=$(shasum -a 256 "$DIR/sink.bin" | awk '{print substr($1,1,16)}') ==="
  xxd -l 32 "$DIR/sink.bin" | head -2
fi
echo "=== evidence dir: $DIR ==="
