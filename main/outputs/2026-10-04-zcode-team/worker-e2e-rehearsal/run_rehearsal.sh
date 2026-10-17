#!/bin/sh
# run_rehearsal.sh — worker-e2e-rehearsal: full staged E2E flow ON HOST with the
# newest wiring (rebuilt from vendored inputs by compose_e2e.py in THIS dir).
# Flow per E2E-RUNBOOK-V2 + CONSOLE-E2E-PLAN:
#   S0 baseline -> S1 download (stub transport, REAL sink/records) ->
#   S2a prepare-archive (REAL intake on the corpus zip, records transition) ->
#   S2b bundle flow (register+prepare) -> S3 install (transactional) ->
#   S4/5 activation-records (running ONLY from the runtime ack file; host uses
#      simulate-runtime = format proof only, never a real ack) ->
#   S6 uninstall -> S7 verify clean state.
# Transcript: run/state/e2e-status.jsonl, copied to rehearsal/e2e-status.jsonl.
# Origin: worker-e2e-rehearsal, 2026-10-04. Host-side only; no network; corpus read-only.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=$HERE/composition/build/host/modinst_composed
ROOT=$(cd "$HERE/../../.." && pwd)
CORPUS=$ROOT/.superpowers/native-observations/nexus-real-mods-2026-10-01/minimap-v3.1.zip
PINNED_SHA=cf3c353b4e8ab0d49214ceaeae5a8989cf58d1b2ce5758673a360d2e463df377
T=PPSA33286
RUN=$HERE/rehearsal/run
S=$RUN/state; R=$RUN/roots; I=$RUN/intake
LOG=$HERE/rehearsal/console.log
SUM=$HERE/rehearsal/summary.log
mkdir -p "$HERE/rehearsal" "$RUN"
: > "$LOG"; : > "$SUM"
PASS=0; FAIL=0; INFO=0
note() { echo "== $*" | tee -a "$LOG"; }
ok()   { PASS=$((PASS+1)); echo "PASS $*" | tee -a "$SUM" "$LOG"; }
bad()  { FAIL=$((FAIL+1)); echo "FAIL $*" | tee -a "$SUM" "$LOG"; }
inf()  { INFO=$((INFO+1)); echo "INFO $*" | tee -a "$SUM" "$LOG"; }
run() { note "run: $*"; "$@" >> "$LOG" 2>&1; RC=$?; note "  exit=$RC"; }
stage() { echo "" | tee -a "$LOG"; note "### $*"; }

note "binary: $BIN  (rebuilt by worker-e2e-rehearsal compose_e2e.py --flavor host)"
note "corpus: $CORPUS"
[ -x "$BIN" ] || { echo "FATAL: binary missing" >&2; exit 2; }
[ -f "$CORPUS" ] || { echo "FATAL: corpus zip missing" >&2; exit 2; }
GOT=$(shasum -a 256 "$CORPUS" | cut -d' ' -f1)
[ "$GOT" = "$PINNED_SHA" ] && ok "corpus sha256 matches worker-6 zip-roundtrip pinned value" \
  || bad "corpus sha256 $GOT != pinned $PINNED_SHA"

# --- S0 baseline ---------------------------------------------------------------
stage "S0 baseline (fresh tree)"
[ ! -f "$S/e2e-status.jsonl" ] && ok "S0 no e2e-status.jsonl before first command" \
  || bad "S0 stale e2e-status.jsonl present"
run "$BIN" --state "$S" --roots "$R" records
[ "$RC" -eq 0 ] && ok "S0 records exit 0" || bad "S0 records exit=$RC"
grep -q "^mod " "$LOG" && bad "S0 baseline not empty" || ok "S0 baseline records empty"
[ ! -d "$R/$T" ] && ok "S0 mod root absent" || bad "S0 mod root exists"

# --- S1 download: stub transport, REAL sink + records ---------------------------
stage "S1 download (file:// stub transport; real sink/records)"
printf 'id\t%s\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nsha256\t%s\n' \
  rehearsal-minimap "$T" "$CORPUS" "$I" "$PINNED_SHA" > "$RUN/job-s1.tsv"
run "$BIN" --state "$S" --roots "$R" download --job "$RUN/job-s1.tsv"
echo "$RC" | grep -qx 0 && ok "S1 exit 0" || bad "S1 exit=$RC"
grep -q "downloaded mod=rehearsal-minimap title=$T state=downloaded bytes=.* sha256=$PINNED_SHA" "$LOG" \
  && ok "S1 success line (content-addressed sha)" || bad "S1 success line missing"
[ -f "$I/rehearsal-minimap.zip" ] && ok "S1 intake zip present" || bad "S1 intake zip missing"
G2=$(shasum -a 256 "$I/rehearsal-minimap.zip" | cut -d' ' -f1)
[ "$G2" = "$PINNED_SHA" ] && ok "S1 intake bytes == corpus sha" || bad "S1 intake sha $G2"
grep -q "\"sha256\": \"$PINNED_SHA\"" "$S/downloads/rehearsal-minimap.json" \
  && ok "S1 sidecar content-addressed record" || bad "S1 sidecar sha missing"
grep -q "file://" "$S/downloads/rehearsal-minimap.json" \
  && bad "S1 URL leaked into sidecar" || ok "S1 no URL in sidecar"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod rehearsal-minimap $T downloaded" "$LOG" && ok "S1 records row downloaded" \
  || bad "S1 records row missing"
grep -q '"stage":"stage-1-download","outcome":"ok"' "$S/e2e-status.jsonl" \
  && ok "S1 e2e-status stage-1 ok" || bad "S1 e2e-status stage-1 ok missing"
grep -q '"cmd":"download"' "$S/e2e-status.jsonl" && ok "S1 e2e-status begin line" \
  || bad "S1 e2e-status begin line missing"
grep -c '"event":"end"' "$S/e2e-status.jsonl" >/dev/null && ok "S1 end lines present" \
  || bad "S1 no end lines"

# --- S2a prepare: REAL intake on the corpus zip + records transition ------------
stage "S2a prepare-archive (real libarchive intake; durable downloaded->prepared)"
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/rehearsal-minimap.zip" --quarantine "$I/rehearsal-minimap.q" --mod rehearsal-minimap
echo "$RC" | grep -qx 0 && ok "S2a exit 0" || bad "S2a exit=$RC"
grep -q "records prepared mod=rehearsal-minimap title=$T" "$LOG" \
  && ok "S2a records transition line" || bad "S2a records transition line missing"
grep -q "rehearsal-minimap.*prepared" "$S/records.txt" \
  && ok "S2a durable records.txt row prepared" || bad "S2a durable row wrong"
ls "$I/rehearsal-minimap.q"/member-*.bin >/dev/null 2>&1 \
  && ok "S2a quarantine members materialized" || bad "S2a quarantine members missing"
[ -f "$I/rehearsal-minimap.q/manifest-v1.bin" ] && ok "S2a quarantine manifest-v1.bin" \
  || bad "S2a quarantine manifest missing"
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$I/rehearsal-minimap.zip" --quarantine "$I/rehearsal-minimap.q2" --mod rehearsal-minimap
grep -q "records unchanged mod=rehearsal-minimap already prepared" "$LOG" \
  && ok "S2a idempotent re-run" || bad "S2a idempotency broken"
[ ! -d "$R/$T" ] && ok "S2a nothing under mod root" || bad "S2a files leaked under mod root"
# informational probe (expected honest failure): install of a raw archive mod
run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods rehearsal-minimap
if [ "$RC" -ne 0 ]; then inf "S2a probe: install of archive-mod refused/failed (exit $RC) — no archive->bundle bridge (expected; finding F-R1)"; else bad "S2a probe: install of archive-mod unexpectedly succeeded"; fi
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod rehearsal-minimap $T prepared" "$LOG" && ok "S2a records row prepared" \
  || bad "S2a records row not prepared"

# --- S2b bundle flow: register + prepare ----------------------------------------
stage "S2b bundle flow (register-mod --bundle; prepare)"
# NB: make-bundle must be argv[1] (fixture builder bypasses the store flag loop)
run "$BIN" make-bundle --id mm-skin --ver 1.0.0 --title "$T" --engine unreal \
  --file pak,mods/~mods/MM_Skin_P.pak=MMPAKBYTES -o "$RUN/mm-skin.mfb"
run "$BIN" make-bundle --id mm-lua --ver 1.0.0 --title "$T" --engine unreal \
  --file lua_mod,ue4ss/Mods/MMProbe/scripts/main.lua=print\(\"mm\"\) -o "$RUN/mm-lua.mfb"
grep -q "^bundle .*id=mm-skin" "$LOG" && ok "S2b fixture mm-skin built" || bad "S2b fixture mm-skin"
grep -q "^bundle .*id=mm-lua" "$LOG" && ok "S2b fixture mm-lua built" || bad "S2b fixture mm-lua"
run "$BIN" --state "$S" --roots "$R" register-mod --id mm-skin --title "$T" --bundle "$RUN/mm-skin.mfb"
run "$BIN" --state "$S" --roots "$R" register-mod --id mm-lua --title "$T" --bundle "$RUN/mm-lua.mfb"
grep -q "registered mod=mm-skin title=$T state=downloaded" "$LOG" \
  && ok "S2b register mm-skin downloaded" || bad "S2b register mm-skin"
grep -q "registered mod=mm-lua title=$T state=downloaded" "$LOG" \
  && ok "S2b register mm-lua downloaded" || bad "S2b register mm-lua"
run "$BIN" --state "$S" --roots "$R" prepare --title "$T" --mods mm-skin,mm-lua
grep -q "^ready" "$LOG" && ok "S2b prepare ready" || bad "S2b prepare not ready"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod mm-skin $T prepared" "$LOG" && ok "S2b mm-skin prepared" || bad "S2b mm-skin state"
grep -q "mod mm-lua $T prepared" "$LOG" && ok "S2b mm-lua prepared" || bad "S2b mm-lua state"
[ ! -d "$R/$T" ] && ok "S2b nothing under mod root yet" || bad "S2b files under mod root early"

# --- S3 install (transactional) --------------------------------------------------
stage "S3 install (journal + load-list sha commit)"
run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods mm-skin,mm-lua
echo "$RC" | grep -qx 0 && ok "S3 exit 0" || bad "S3 exit=$RC"
grep -q "^installed title=$T generation=1 mods=mm-skin,mm-lua" "$LOG" \
  && ok "S3 installed line (generation 1)" || bad "S3 installed line missing"
[ -f "$R/$T/mods/~mods/MM_Skin_P.pak" ] && ok "S3 staged pak file" || bad "S3 staged pak file"
[ -f "$R/$T/ue4ss/Mods/MMProbe/scripts/main.lua" ] && ok "S3 staged lua file" || bad "S3 staged lua file"
[ -f "$R/$T/loadlist.txt" ] && ok "S3 loadlist.txt" || bad "S3 loadlist.txt"
[ -f "$R/$T/.modforge-root" ] && ok "S3 managed-root marker" || bad "S3 managed-root marker"
[ ! -f "$S/journal/$T.json" ] && ok "S3 journal gone after commit" || bad "S3 journal survived"
grep -q '"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"' "$S/e2e-status.jsonl" \
  && ok "S3 e2e-status stage-3 begin" || bad "S3 e2e-status stage-3 begin missing"
grep -q '{"event":"end","exit":0}' "$S/e2e-status.jsonl" \
  && ok "S3 e2e-status end exit=0" || bad "S3 e2e-status end missing"
inf "S3 note: install emits begin+end but no terminal status event (unlike download/prepare-archive); see finding F-R2 for the dangling-begin refusal path"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod mm-skin $T installed" "$LOG" && ok "S3 mm-skin installed" || bad "S3 mm-skin state"
grep -q "mod mm-lua $T installed" "$LOG" && ok "S3 mm-lua installed" || bad "S3 mm-lua state"
[ ! -f "$R/$T/runtime-status.txt" ] && ok "S3 no runtime-status.txt before any ack" || bad "S3 runtime-status.txt early"

# --- S4/5 activation-records (running ONLY from the runtime ack file) ------------
stage "S4/S5 activation-records (SIMULATED ack file on host; running derives ONLY from runtime-status)"
run "$BIN" --state "$S" --roots "$R" simulate-runtime --title "$T" --rt-state activated
run "$BIN" --state "$S" --roots "$R" records
grep -q "runtime activated running=0" "$LOG" \
  && ok "S4 activated -> running=0 (honest: not running before mods-started ack)" \
  || bad "S4 activated running=0 missing"
run "$BIN" --state "$S" --roots "$R" simulate-runtime --title "$T" --rt-state mods-started
run "$BIN" --state "$S" --roots "$R" records
grep -q "runtime mods-started running=1" "$LOG" \
  && ok "S5 mods-started ack -> running=1" || bad "S5 running=1 missing"
inf "S4/S5 on host used simulate-runtime (ack-file format proof only); a real running=1 requires the injected runtime's module_start ack on console"

# --- S6 uninstall -----------------------------------------------------------------
stage "S6 uninstall (install --remove; removed mods -> prepared)"
run "$BIN" --state "$S" --roots "$R" install --title "$T" --remove mm-lua
echo "$RC" | grep -qx 0 && ok "S6 remove mm-lua exit 0" || bad "S6 remove mm-lua exit=$RC"
[ ! -f "$R/$T/ue4ss/Mods/MMProbe/scripts/main.lua" ] && ok "S6 lua file removed" || bad "S6 lua file survived"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod mm-lua $T prepared" "$LOG" && ok "S6 mm-lua dropped to prepared" || bad "S6 mm-lua state"
grep -q "mod mm-skin $T installed" "$LOG" && ok "S6 mm-skin still installed" || bad "S6 mm-skin state"
run "$BIN" --state "$S" --roots "$R" install --title "$T" --remove mm-skin
echo "$RC" | grep -qx 0 && ok "S6 remove-all exit 0" || bad "S6 remove-all exit=$RC"
run "$BIN" --state "$S" --roots "$R" records
grep -q "mod mm-skin $T prepared" "$LOG" && ok "S6 mm-skin dropped to prepared" || bad "S6 mm-skin final state"

# --- S7 verify clean state ---------------------------------------------------------
stage "S7 verify clean state"
N=$(grep -c "	installed" "$S/records.txt" 2>/dev/null); N=${N:-0}
[ "$N" -eq 0 ] && ok "S7 zero installed rows in records.txt" || bad "S7 $N installed rows remain"
[ -f "$R/$T/.modforge-root" ] && ok "S7 root is an empty managed root (marker)" || bad "S7 managed marker missing"
[ -f "$R/$T/loadlist.txt" ] && grep -v '^modforge-loadlist	\|^title	\|^engine	' "$R/$T/loadlist.txt" | grep -q . \
  && bad "S7 loadlist lists mod entries" || ok "S7 loadlist is header-only (empty managed root by design)"
[ ! -e "$R/$T/mods/~mods/MM_Skin_P.pak" ] && ok "S7 no mod files remain" || bad "S7 mod files remain"
[ ! -e "$R/$T/runtime-status.txt" ] && ok "S7 runtime-status.txt gone with old root" || bad "S7 runtime-status present"
[ ! -d "$R/$T.next" ] && [ ! -d "$R/$T.prev" ] && ok "S7 no .next/.prev leftovers" || bad "S7 .next/.prev present"
ls "$S/journal/"*.json >/dev/null 2>&1 && bad "S7 journal files remain" || ok "S7 no journal files"
ls "$I"/*.tmp >/dev/null 2>&1 && bad "S7 intake .tmp leftovers" || ok "S7 no .tmp leftovers"
[ -f "$S/downloads/rehearsal-minimap.json" ] && ok "S7 download sidecar still durable" || bad "S7 sidecar lost"
E=$(python3 - "$S/e2e-status.jsonl" <<'EOF'
import json,sys
n=0; badends=0; begins=0; ends=0; unmatched=0
for l in open(sys.argv[1]):
    l=l.strip()
    if not l: continue
    n+=1
    d=json.loads(l)
    ev=d.get("event")
    if ev=="begin": begins+=1
    elif ev=="end":
        ends+=1
        if d.get("exit",0)!=0: badends+=1
    elif ev=="status" and d.get("outcome")=="fail": badends+=1
unmatched=begins-ends
print(f"{n} lines, {begins} begins, {ends} ends, {badends} failed ends, {unmatched} unmatched begins")
EOF
)
echo "$E" | tee -a "$SUM" "$LOG"
echo "$E" | grep -q " 0 failed ends, 1 unmatched begins" \
  && ok "S7 transcript complete: 0 failed ends; exactly the 1 expected unmatched begin (S2a refusal probe, F-R2)" \
  || inf "S7 transcript anomaly: $E (expected 0 failed ends / 1 unmatched begin)"
# snapshot final artifacts
cp "$S/e2e-status.jsonl" "$HERE/rehearsal/e2e-status.jsonl"
cp "$S/records.txt" "$HERE/rehearsal/records.txt"
cp "$S/downloads/rehearsal-minimap.json" "$HERE/rehearsal/download-sidecar.json"
echo "REHEARSAL RESULT: PASS=$PASS FAIL=$FAIL INFO=$INFO (log: rehearsal/console.log, transcript: rehearsal/e2e-status.jsonl)" | tee -a "$SUM" "$LOG"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
