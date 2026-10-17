#!/bin/sh
# run_tests.sh — build + run the worker-8 host tests against the REAL composed
# installer binary (worker-zip-to-mfb lineage) and a REAL corpus zip.
# Host-side only; no network; no console; no original file modified.
#
# Env: TEAM_DIR may override the team output root; LOGDIR receives the log.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/.." && pwd)
DEFAULT_TEAM=$(cd "$SRC/../../../.." && pwd)/2026-10-04-zcode-team
TEAM=${TEAM_DIR:-$DEFAULT_TEAM}
LOGDIR=${LOGDIR:-$HERE}
WORK=$(mktemp -d /tmp/mfw8-test.XXXXXX)

INSTALLER="$TEAM/worker-zip-to-mfb/composition/build/host/modinst_composed"
if [ ! -x "$INSTALLER" ]; then
  INSTALLER="$TEAM/worker-e2e-final/composition/build/host/modinst_composed"
fi
if [ ! -x "$INSTALLER" ]; then
  echo "FAIL no composed installer binary found (expected worker-zip-to-mfb or worker-e2e-final build)"
  exit 2
fi
CORPUS=""
for cand in \
  "$TEAM/worker-zip-to-mfb/test/run/zb-lua/infinite-stamina-v1.zip" \
  "$TEAM/worker-zip-to-mfb/test/run/zb-lua/intake/infinite-stamina-v1.zip"; do
  [ -f "$cand" ] && CORPUS="$cand" && break
done
if [ -z "$CORPUS" ]; then
  CORPUS=$(find "$TEAM/worker-zip-to-mfb/test" -name '*.zip' -print 2>/dev/null | head -1)
fi
if [ -z "$CORPUS" ] || [ ! -f "$CORPUS" ]; then
  echo "FAIL no real corpus zip found under worker-zip-to-mfb/test"
  exit 2
fi
echo "== installer: $INSTALLER"
echo "== corpus:   $CORPUS"
echo "== work:     $WORK"

CC=${CC:-cc}
echo "== build"
"$CC" -std=c11 -Wall -Wextra -O2 -pthread \
  -I"$SRC" -I"$SRC/../../mod_service/vendor" \
  -o "$WORK/mfw8_test" \
  "$HERE/mfw8_test.c" \
  "$SRC/mfw8_records.c" "$SRC/mfw8_actions.c" "$SRC/mfw8_caps.c" "$SRC/mfw8_service.c" \
  || exit 2

echo "== run"
LOG="$LOGDIR/mfw8-test.log"
{
  echo "installer: $INSTALLER"
  echo "installer sha256: $(shasum -a 256 "$INSTALLER" | cut -d' ' -f1)"
  echo "corpus: $CORPUS"
  echo "corpus sha256: $(shasum -a 256 "$CORPUS" | cut -d' ' -f1)"
} > "$LOG"
"$WORK/mfw8_test" "$INSTALLER" "$WORK" "$CORPUS" 2>&1 | tee -a "$LOG"
rc=$?
{
  echo ""
  echo "== durable records.txt after tests:"
  cat "$WORK/state/records.txt" 2>/dev/null
  echo "== identity.tsv after tests:"
  cat "$WORK/state/identity.tsv" 2>/dev/null
} >> "$LOG"
echo "== log: $LOG"
exit $rc
