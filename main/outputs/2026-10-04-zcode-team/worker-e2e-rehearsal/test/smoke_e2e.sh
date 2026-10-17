#!/bin/sh
# smoke_e2e.sh — worker-e2e-final host suite: proves the two closed gaps.
#   gap-2: download --job -> exclusive-owner transport ABI v1 -> REAL file sink
#          -> intake/<id>.zip -> durable records row `downloaded` (+ failures)
#   gap-1: prepare-archive --mod -> durable downloaded->prepared transition
# Run test/smoke_host.sh first for the original 26/26 regression.
# Origin: worker-e2e-final, 2026-10-04. Host-side only (stub transport).
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=${BIN:-$HERE/composition/build/host/modinst_composed}
ROOT=$(cd "$HERE/../../.." && pwd)
CORPUS=${CORPUS:-$ROOT/.superpowers/native-observations/nexus-real-mods-2026-10-01/minimap-v3.1.zip}
TMP=${TMPDIR:-/tmp}/mi-e2e.$$
REC=$HERE/composition/receipts
mkdir -p "$TMP" "$REC"
LOG=$REC/smoke-e2e.log
: > "$LOG"
PASS=0; FAIL=0
note() { echo "== $*" | tee -a "$LOG"; }
check() { if grep -q -- "$2" "$3"; then PASS=$((PASS+1)); note "PASS $1";
  else FAIL=$((FAIL+1)); note "FAIL $1 (pattern '$2' not in $3)"; fi }
run() { note "run: $*"; "$@" >> "$LOG" 2>&1; RC=$?; note "  exit=$RC"; }

note "binary: $BIN"
[ -x "$BIN" ] || { echo "FATAL: binary missing (run compose_e2e.py --flavor host)" >&2; exit 2; }
[ -f "$CORPUS" ] || { echo "FATAL: corpus zip missing" >&2; exit 2; }

S="$TMP/state"; R="$TMP/roots"; I="$TMP/intake"; T=PPSA33286
SHA=$(shasum -a 256 "$CORPUS" | cut -d' ' -f1)
mkdir -p "$TMP/fx"
cp "$CORPUS" "$TMP/fx/refused.stub-refuse.zip"
cp "$CORPUS" "$TMP/fx/cut.stub-cut.zip"
cp "$CORPUS" "$TMP/fx/slow.stub-timeout.zip"

job() { # job FILE id title url [sha]
  printf 'id\t%s\ntitle\t%s\nurl\t%s\nintake\t%s\n' "$2" "$3" "$4" "$I" > "$1"
  [ $# -gt 4 ] && printf 'sha256\t%s\n' "$5" >> "$1"
}
note "corpus sha256: $SHA"

# --- A: job -> transport -> real sink -> intake -> record (downloaded) -------
job "$TMP/ok.tsv" e2eminimap "$T" "file://$CORPUS" "$SHA"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/ok.tsv"
check "A1 download ok" "downloaded mod=e2eminimap title=$T state=downloaded bytes=.* sha256=$SHA" "$LOG"
[ "$RC" -eq 0 ] && { PASS=$((PASS+1)); note "PASS A1 exit 0"; } || { FAIL=$((FAIL+1)); note "FAIL A1 exit"; }
[ -f "$I/e2eminimap.zip" ] && { PASS=$((PASS+1)); note "PASS A2 intake file"; } \
  || { FAIL=$((FAIL+1)); note "FAIL A2 intake file"; }
GOT=$(shasum -a 256 "$I/e2eminimap.zip" | cut -d' ' -f1)
[ "$GOT" = "$SHA" ] && { PASS=$((PASS+1)); note "PASS A3 content-addressed sha"; } \
  || { FAIL=$((FAIL+1)); note "FAIL A3 sha ($GOT)"; }
grep -q "\"sha256\": \"$SHA\"" "$S/downloads/e2eminimap.json" && { PASS=$((PASS+1)); note "PASS A3b sidecar record"; } \
  || { FAIL=$((FAIL+1)); note "FAIL A3b sidecar record"; }
grep -q "file://" "$S/downloads/e2eminimap.json" && { FAIL=$((FAIL+1)); note "FAIL A3c URL leaked into record"; } \
  || { PASS=$((PASS+1)); note "PASS A3c no URL in record"; }
run "$BIN" --state "$S" --roots "$R" records
check "A4 records downloaded" "mod e2eminimap $T downloaded" "$LOG"
check "A5 e2e stage-1 ok" '"stage":"stage-1-download","outcome":"ok"' "$S/e2e-status.jsonl"
grep -q '"cmd":"download"' "$S/e2e-status.jsonl" && { PASS=$((PASS+1)); note "PASS A5b begin line"; } \
  || { FAIL=$((FAIL+1)); note "FAIL A5b begin line"; }

# --- B: gap-1 prepare-archive -> durable prepared transition -----------------
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/e2eminimap.zip" --quarantine "$TMP/quarantine" --mod e2eminimap
check "B1 transition" "records prepared mod=e2eminimap title=$T" "$LOG"
run "$BIN" --state "$S" --roots "$R" records
check "B2 records prepared" "mod e2eminimap $T prepared" "$LOG"
grep -q "e2eminimap.*prepared" "$S/records.txt" && { PASS=$((PASS+1)); note "PASS B2b durable records.txt"; } \
  || { FAIL=$((FAIL+1)); note "FAIL B2b durable records.txt"; }
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/e2eminimap.zip" --quarantine "$TMP/quarantine2" --mod e2eminimap
check "B3 idempotent" "records unchanged mod=e2eminimap already prepared" "$LOG"
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/e2eminimap.zip" --quarantine "$TMP/q-ghost" --mod ghostmod
[ "$RC" -eq 2 ] && { PASS=$((PASS+1)); note "PASS B4 exit 2"; } || { FAIL=$((FAIL+1)); note "FAIL B4 exit=$RC"; }
check "B4 unknown mod" "unknown mod 'ghostmod' for prepare-archive (quarantine retained)" "$LOG"
[ -f "$TMP/q-ghost/manifest-v1.bin" ] && { PASS=$((PASS+1)); note "PASS B4b quarantine retained"; } \
  || { FAIL=$((FAIL+1)); note "FAIL B4b quarantine retained"; }

# --- C: failure paths (no record, no leftovers) ------------------------------
job "$TMP/mm.tsv" mm "$T" "file://$CORPUS" 0000000000000000000000000000000000000000000000000000000000000000
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/mm.tsv"
[ "$RC" -eq 1 ] && { PASS=$((PASS+1)); note "PASS C1 sha-mismatch exit 1"; } || { FAIL=$((FAIL+1)); note "FAIL C1 exit=$RC"; }
check "C1 honest error" "payload sha256 mismatch" "$LOG"
[ ! -f "$I/mm.zip" ] && { PASS=$((PASS+1)); note "PASS C1 no intake file"; } || { FAIL=$((FAIL+1)); note "FAIL C1 intake file"; }
job "$TMP/rf.tsv" rf "$T" "file://$TMP/fx/refused.stub-refuse.zip"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/rf.tsv"
check "C2 response-refused" "status=7(response-refused)" "$LOG"
[ "$RC" -eq 1 ] && { PASS=$((PASS+1)); note "PASS C2 exit 1"; } || { FAIL=$((FAIL+1)); note "FAIL C2 exit=$RC"; }
job "$TMP/ct.tsv" ct "$T" "file://$TMP/fx/cut.stub-cut.zip"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/ct.tsv"
check "C3 mid-stream cut" "status=6(transport-failed)" "$LOG"
[ ! -f "$I/ct.zip" ] && { PASS=$((PASS+1)); note "PASS C3 no intake file"; } || { FAIL=$((FAIL+1)); note "FAIL C3 intake file"; }
job "$TMP/to.tsv" to "$T" "file://$TMP/fx/slow.stub-timeout.zip"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/to.tsv"
check "C4 timeout" "status=12(timeout)" "$LOG"
printf 'id\tbk\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nbogus\t1\n' "$T" "$CORPUS" "$I" > "$TMP/bk.tsv"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/bk.tsv"
[ "$RC" -eq 2 ] && { PASS=$((PASS+1)); note "PASS C5 unknown-key exit 2"; } || { FAIL=$((FAIL+1)); note "FAIL C5 exit=$RC"; }
check "C5 explicit-config-only" "unknown job key 'bogus'" "$LOG"
printf 'id\tnourl\ntitle\t%s\nintake\t%s\n' "$T" "$I" > "$TMP/nl.tsv"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/nl.tsv"
[ "$RC" -eq 2 ] && { PASS=$((PASS+1)); note "PASS C6 missing-url exit 2"; } || { FAIL=$((FAIL+1)); note "FAIL C6 exit=$RC"; }
job "$TMP/hs.tsv" hs "$T" "http://$CORPUS"
run "$BIN" --state "$S" --roots "$R" download --job "$TMP/hs.tsv"
check "C7 non-owner scheme refused" "status=7(response-refused)" "$LOG"
ls "$I"/*.tmp >/dev/null 2>&1 && { FAIL=$((FAIL+1)); note "FAIL C8 tmp leftovers"; } \
  || { PASS=$((PASS+1)); note "PASS C8 no tmp leftovers"; }
N=$(grep -c "^mod	" "$S/records.txt" 2>/dev/null)
N=${N:-0}
[ "$N" -eq 1 ] && { PASS=$((PASS+1)); note "PASS C9 records intact (1 mod)"; } \
  || { FAIL=$((FAIL+1)); note "FAIL C9 records mod count=$N"; }
check "C10 fail events" '"stage":"stage-1-download","outcome":"fail"' "$S/e2e-status.jsonl"

echo "E2E RESULT: PASS=$PASS FAIL=$FAIL (log: composition/receipts/smoke-e2e.log, tree: $TMP)" | tee -a "$LOG"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
