#!/bin/sh
# run_bridge_tests.sh — VERBATIM copy of worker-zip-to-mfb test/run_bridge_tests.sh (worker-zip-bridge-console-link regression; only this header comment differs; BIN auto-resolves to THIS dir's composed host binary)
# (AUTO-BRIDGE) -> install -> ack -> uninstall on 3 worker-6 corpus zips
# (Lua mod, pak mod, ini-fragment+pak), plus a negative case (no recognizable
# mod content refuses with a specific reason) and a --no-auto-bundle opt-out
# case (reporting-only behavior preserved). Records states checked at every
# step. Origin: worker-zip-to-mfb, 2026-10-04. Host-only; corpus read-only.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=$HERE/composition/build/host/modinst_composed
ROOT=$(cd "$HERE/../../.." && pwd)
OBS=$ROOT/.superpowers/native-observations
T=PPSA33286
RUN=$HERE/test/run
LOG=$HERE/test/bridge-e2e.log
SUM=$HERE/test/bridge-e2e-summary.log
mkdir -p "$HERE/test" "$RUN"
: > "$LOG"; : > "$SUM"
PASS=0; FAIL=0
note() { echo "== $*" | tee -a "$LOG"; }
ok()   { PASS=$((PASS+1)); echo "PASS $*" | tee -a "$SUM" "$LOG"; }
bad()  { FAIL=$((FAIL+1)); echo "FAIL $*" | tee -a "$SUM" "$LOG"; }
run() { note "run: $*"; "$@" >> "$LOG" 2>&1; RC=$?; note "  exit=$RC"; }
stage() { echo "" | tee -a "$LOG"; note "### $*"; }
stage "T0 binary + corpus"
[ -x "$BIN" ] && ok "T0 composed binary present" || { echo "FATAL: binary missing (run compose_bridge.py)" >&2; exit 2; }
check_sha() { # file pinned
  GOT=$(shasum -a 256 "$1" | cut -d' ' -f1)
  [ "$GOT" = "$2" ] && ok "corpus sha256 matches worker-6 pin: $(basename "$1")" \
    || bad "$(basename "$1") sha $GOT != pinned $2"
}
check_sha "$OBS/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip" 61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5
check_sha "$OBS/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip" 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad
check_sha "$OBS/performance-pak-real-mod-intake-1/original.zip" 27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e

# flow ID ZIPPATH PINNEDSHA [expected installed file under roots/$T]...
flow() {
  ID=$1; ZIP=$2; SHA=$3; shift 3
  stage "case $ID: download -> prepare(auto-bridge) -> install -> ack -> uninstall"
  S=$RUN/$ID/state; R=$RUN/$ID/roots; I=$RUN/$ID/intake
  rm -rf "$RUN/$ID"; mkdir -p "$RUN/$ID"
  printf 'id\t%s\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nsha256\t%s\n' \
    "$ID" "$T" "$ZIP" "$I" "$SHA" > "$RUN/$ID/job.tsv"
  run "$BIN" --state "$S" --roots "$R" download --job "$RUN/$ID/job.tsv"
  [ "$RC" -eq 0 ] && ok "$ID download exit 0" || bad "$ID download exit=$RC"
  grep -q "downloaded mod=$ID title=$T state=downloaded" "$LOG" && ok "$ID record downloaded" || bad "$ID record downloaded missing"
  run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
    --archive "$I/$ID.zip" --quarantine "$I/$ID.q" --mod "$ID"
  [ "$RC" -eq 0 ] && ok "$ID prepare-archive exit 0" || bad "$ID prepare-archive exit=$RC"
  grep -q "bridge bundle .*/bundles/$ID.mfb id=$ID files=" "$LOG" && ok "$ID auto-bridge composed mfb" || bad "$ID auto-bridge line missing"
  [ -f "$S/bundles/$ID.mfb" ] && ok "$ID bundle artifact present" || bad "$ID bundle artifact missing"
  [ "$(head -c 8 "$S/bundles/$ID.mfb")" = "MFBUNDL1" ] && ok "$ID bundle is MFBUNDL1" || bad "$ID bundle magic wrong"
  grep -q "records prepared mod=$ID title=$T" "$LOG" && ok "$ID records prepared" || bad "$ID records prepared missing"
  [ -f "$I/$ID.q/manifest-v1.bin" ] && ok "$ID quarantine retained" || bad "$ID quarantine missing"
  run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods "$ID"
  [ "$RC" -eq 0 ] && ok "$ID install exit 0" || bad "$ID install exit=$RC (bridge failed to make it installable)"
  grep -q "^installed title=$T generation=1 mods=$ID" "$LOG" && ok "$ID installed line" || bad "$ID installed line missing"
  for f in "$@"; do
    [ -f "$R/$T/$f" ] && ok "$ID staged file $f" || bad "$ID staged file $f missing"
  done
  [ -f "$R/$T/loadlist.txt" ] && ok "$ID loadlist written" || bad "$ID loadlist missing"
  run "$BIN" --state "$S" --roots "$R" records
  grep -q "mod $ID $T installed" "$LOG" && ok "$ID record installed" || bad "$ID record not installed"
  run "$BIN" --state "$S" --roots "$R" simulate-runtime --title "$T" --rt-state mods-started
  run "$BIN" --state "$S" --roots "$R" records
  grep -q "runtime mods-started running=1" "$LOG" && ok "$ID runtime ack running=1" || bad "$ID running=1 missing"
  run "$BIN" --state "$S" --roots "$R" install --title "$T" --remove "$ID"
  [ "$RC" -eq 0 ] && ok "$ID uninstall exit 0" || bad "$ID uninstall exit=$RC"
  run "$BIN" --state "$S" --roots "$R" records
  grep -q "mod $ID $T prepared" "$LOG" && ok "$ID record back to prepared" || bad "$ID record not prepared after remove"
  for f in "$@"; do
    [ ! -e "$R/$T/$f" ] && ok "$ID removed file $f" || bad "$ID file $f survived uninstall"
  done
}

flow zb-lua "$OBS/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip" \
  61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5 \
  "ue4ss/Mods/Infinite Stamina/Scripts/main.lua" "ue4ss/Mods/Infinite Stamina/enabled.txt"

flow zb-pak "$OBS/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip" \
  3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad \
  "mods/~mods/PlayerMovementSpeed_2x_P.pak" "mods/~mods/PlayerMovementSpeed_2x_P.ucas" "mods/~mods/PlayerMovementSpeed_2x_P.utoc"

flow zb-ini "$OBS/performance-pak-real-mod-intake-1/original.zip" \
  27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e \
  "mods/~mods/zzz_UniversalPerformance_P.pak" "config/Windows/Engine.ini"

stage "case zb-neg: no recognizable mod content refuses with a specific reason"
NEG=$RUN/neg/notamod.zip
rm -rf "$RUN/neg"
mkdir -p "$RUN/neg"
python3 - "$NEG" <<'EOF'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("ReadMe.txt", "documentation only, no mod payload\n")
    z.writestr("screenshots/shot.png", b"\x89PNG\r\n\x1a\nfake")
print("negative fixture:", sys.argv[1])
EOF
NEGSHA=$(shasum -a 256 "$NEG" | cut -d' ' -f1)
S=$RUN/neg/state; R=$RUN/neg/roots; I=$RUN/neg/intake
printf 'id\t%s\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nsha256\t%s\n' \
  zb-neg "$T" "$NEG" "$I" "$NEGSHA" > "$RUN/neg/job.tsv"
run "$BIN" --state "$S" --roots "$R" download --job "$RUN/neg/job.tsv"
[ "$RC" -eq 0 ] && ok "zb-neg download exit 0" || bad "zb-neg download exit=$RC"
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/zb-neg.zip" --quarantine "$I/zb-neg.q" --mod zb-neg
[ "$RC" -ne 0 ] && ok "zb-neg prepare-archive refuses (exit $RC)" || bad "zb-neg unexpectedly bridged"
grep -q "error bad_request auto-bundle refused: no mod content recognized" "$LOG" \
  && ok "zb-neg specific refusal reason" || bad "zb-neg refusal reason missing/wrong"
grep -q "quarantine retained" "$LOG" && ok "zb-neg quarantine retained on refusal" || bad "zb-neg quarantine line missing"
[ -f "$I/zb-neg.q/manifest-v1.bin" ] && ok "zb-neg quarantine still present" || bad "zb-neg quarantine erased"
[ ! -f "$S/bundles/zb-neg.mfb" ] && ok "zb-neg no bundle written" || bad "zb-neg bundle written despite refusal"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod zb-neg $T downloaded" "$LOG" && ok "zb-neg record stays downloaded" || bad "zb-neg record state changed"

stage "case zb-off: --no-auto-bundle restores reporting-only (F-R1 semantics)"
S=$RUN/off/state; R=$RUN/off/roots; I=$RUN/off/intake
rm -rf "$RUN/off"; mkdir -p "$RUN/off"
printf 'id\t%s\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nsha256\t%s\n' \
  zb-off "$T" "$OBS/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip" "$I" \
  61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5 > "$RUN/off/job.tsv"
run "$BIN" --state "$S" --roots "$R" download --job "$RUN/off/job.tsv"
OFFMARK=$(wc -l < "$LOG" | tr -d ' ')
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/zb-off.zip" --quarantine "$I/zb-off.q" --mod zb-off --no-auto-bundle
[ "$RC" -eq 0 ] && ok "zb-off prepare-archive (reporting-only) exit 0" || bad "zb-off prepare-archive exit=$RC"
tail -n +"$OFFMARK" "$LOG" | grep -q "bridge bundle" \
  && bad "zb-off bridge ran despite --no-auto-bundle" || ok "zb-off bridge skipped"
[ ! -f "$S/bundles/zb-off.mfb" ] && ok "zb-off no bundle written" || bad "zb-off bundle written"
grep -q "records prepared mod=zb-off title=$T" "$LOG" && ok "zb-off records prepared" || bad "zb-off records missing"
run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods zb-off
if [ "$RC" -ne 0 ]; then
  grep -q "not a modforge bundle" "$LOG" && ok "zb-off install refuses with the original F-R1 reason" || bad "zb-off wrong refusal"
else
  bad "zb-off install unexpectedly succeeded"
fi

stage "transcript sanity (every begin has an end; no failed ends)"
for d in "$RUN"/zb-lua "$RUN"/zb-pak "$RUN"/zb-ini "$RUN"/neg "$RUN"/off; do
  E=$(python3 - "$d/state/e2e-status.jsonl" <<'EOF'
import json,sys
b=e=f=0
try: lines=[l for l in open(sys.argv[1]) if l.strip()]
except FileNotFoundError: print("missing"); sys.exit()
for l in lines:
    d=json.loads(l)
    if d.get("event")=="begin": b+=1
    elif d.get("event")=="end":
        e+=1
        if d.get("exit",0)!=0: f+=1
print(f"{b} begins, {e} ends, {f} failed ends, {b-e} unmatched")
EOF
) ; echo "$d: $E" | tee -a "$SUM" "$LOG"; done
echo "BRIDGE E2E RESULT: PASS=$PASS FAIL=$FAIL (log: test/bridge-e2e.log)" | tee -a "$SUM" "$LOG"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
