#!/bin/sh
# smoke_host.sh — host smoke test of the composed installer configuration
# (PS5IO_HOST flavor): prepare+install+rollback over a temp tree using a
# saved corpus zip, per CONSOLE-E2E-PLAN.md stages 0-6 minus the real
# runtime/console steps.
#
# Origin: worker-installer-composition, 2026-10-04. Read-only wrt the corpus.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=${BIN:-$HERE/composition/build/host/modinst_composed}
ROOT=$(cd "$HERE/../../.." && pwd)
CORPUS=${CORPUS:-$ROOT/.superpowers/native-observations/nexus-real-mods-2026-10-01/minimap-v3.1.zip}
TMP=${TMPDIR:-/tmp}/mi-smoke.$$
REC=$HERE/composition/receipts
mkdir -p "$TMP" "$REC"
LOG=$REC/smoke-host.log
: > "$LOG"
PASS=0; FAIL=0

note() { echo "== $*" | tee -a "$LOG"; }
check() { # check LABEL PATTERN FILE
  if grep -q -- "$2" "$3"; then PASS=$((PASS+1)); note "PASS $1";
  else FAIL=$((FAIL+1)); note "FAIL $1 (pattern '$2' not in $3)"; fi
}
run() { note "run: $*"; "$@" >> "$LOG" 2>&1; }

note "binary: $BIN"
note "corpus: $CORPUS"
[ -x "$BIN" ] || { echo "FATAL: binary missing (run compose_installer.py --flavor host)" >&2; exit 2; }
[ -f "$CORPUS" ] || { echo "FATAL: corpus zip missing" >&2; exit 2; }

S="$TMP/state"; R="$TMP/roots"
T=PPSA33286

# --- stage 0: baseline records ------------------------------------------------
run "$BIN" --state "$S" --roots "$R" records
if grep -q "^mod " "$LOG"; then FAIL=$((FAIL+1)); note "FAIL stage0 baseline not empty"; else PASS=$((PASS+1)); note "PASS stage0 baseline empty"; fi

# --- fixtures: two bundles (uses the core's own test builder) ------------------
run "$BIN" make-bundle --id skin --ver 1.0.0 --title "$T" --engine unreal \
  --file pak,mods/~mods/Skin_P.pak=SKINPAKBYTES -o "$TMP/skin.mfb"
run "$BIN" make-bundle --id spawner --ver 1.0.0 --title "$T" --engine unreal \
  --file lua_mod,ue4ss/Mods/ItemSpawner/scripts/main.lua=print\(\"spawn\"\) \
  -o "$TMP/spawner.mfb"
check "fixture skin" "^bundle .*id=skin" "$LOG"
check "fixture spawner" "^bundle .*id=spawner" "$LOG"

# --- stage 1: register (downloaded) --------------------------------------------
run "$BIN" --state "$S" --roots "$R" register-mod --id skin --title "$T" --bundle "$TMP/skin.mfb"
run "$BIN" --state "$S" --roots "$R" register-mod --id spawner --title "$T" --bundle "$TMP/spawner.mfb"
run "$BIN" --state "$S" --roots "$R" records
check "stage1 downloaded" "mod skin $T downloaded" "$LOG"

# --- stage 2a: prepare-archive over the REAL pinned intake closure -------------
run "$BIN" --state "$S" --roots "$R" prepare-archive --title "$T" \
  --archive "$CORPUS" --quarantine "$TMP/quarantine"
check "stage2a prepared" "^prepared title=$T" "$LOG"
if ls "$TMP/quarantine"/member-*.bin >/dev/null 2>&1; then PASS=$((PASS+1)); note "PASS stage2a quarantine members";
else FAIL=$((FAIL+1)); note "FAIL stage2a quarantine members"; fi
if [ -f "$TMP/quarantine/manifest-v1.bin" ]; then PASS=$((PASS+1)); note "PASS stage2a manifest";
else FAIL=$((FAIL+1)); note "FAIL stage2a manifest"; fi

# --- stage 2b: prepare (plan; requirement satisfied) ---------------------------
run "$BIN" --state "$S" --roots "$R" prepare --title "$T" --mods skin,spawner
check "stage2b ready" "^ready 1" "$LOG"
run "$BIN" --state "$S" --roots "$R" records
check "stage2b prepared-state" "mod skin $T prepared" "$LOG"

# --- stage 3: install ----------------------------------------------------------
run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods skin,spawner
check "stage3 installed" "^installed title=$T" "$LOG"
[ -f "$R/$T/mods/~mods/Skin_P.pak" ] && { PASS=$((PASS+1)); note "PASS stage3 staged file"; } \
  || { FAIL=$((FAIL+1)); note "FAIL stage3 staged file"; }
[ -f "$R/$T/loadlist.txt" ] && { PASS=$((PASS+1)); note "PASS stage3 loadlist"; } \
  || { FAIL=$((FAIL+1)); note "FAIL stage3 loadlist"; }
[ ! -f "$S/journal/$T.json" ] && { PASS=$((PASS+1)); note "PASS stage3 journal gone"; } \
  || { FAIL=$((FAIL+1)); note "FAIL stage3 journal gone"; }

# --- stages 4/5: runtime ack (simulated; running derived only from the file) ----
run "$BIN" --state "$S" --roots "$R" simulate-runtime --title "$T" --rt-state activated
run "$BIN" --state "$S" --roots "$R" records
check "stage5 not-running-yet" "runtime activated running=0" "$LOG"
run "$BIN" --state "$S" --roots "$R" simulate-runtime --title "$T" --rt-state mods-started
run "$BIN" --state "$S" --roots "$R" records
check "stage5 running" "runtime mods-started running=1" "$LOG"

# --- rollback probes (fresh trees) ---------------------------------------------
S2="$TMP/state-rb"; R2="$TMP/roots-rb"
run "$BIN" --state "$S2" --roots "$R2" register-mod --id skin --title "$T" --bundle "$TMP/skin.mfb"
run "$BIN" --state "$S2" --roots "$R2" register-mod --id spawner --title "$T" --bundle "$TMP/spawner.mfb"
run "$BIN" --state "$S2" --roots "$R2" install --title "$T" --mods skin,spawner
# rollback 1: graceful cancel between renames -> previous root restored
run "$BIN" --state "$S2" --roots "$R2" install --title "$T" --mods skin --cancel-at rename1
check "rollback cancel" "cancelled between renames; rolled back" "$LOG"
[ -f "$R2/$T/mods/~mods/Skin_P.pak" ] && [ -f "$R2/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua" ] \
  && { PASS=$((PASS+1)); note "PASS rollback cancel old set intact"; } \
  || { FAIL=$((FAIL+1)); note "FAIL rollback cancel old set intact"; }
# rollback 2: hard crash during staging -> recovery rolls the staging back
run "$BIN" --state "$S2" --roots "$R2" install --title "$T" --mods skin --crash-at staged
[ $? -eq 86 ] && { PASS=$((PASS+1)); note "PASS rollback crash exit 86"; } || { FAIL=$((FAIL+1)); note "FAIL rollback crash exit"; }
run "$BIN" --state "$S2" --roots "$R2" recover
check "rollback recover" "rolled back staging" "$LOG"
[ ! -d "$R2/$T.next" ] && { PASS=$((PASS+1)); note "PASS rollback .next gone"; } \
  || { FAIL=$((FAIL+1)); note "FAIL rollback .next gone"; }

# --- stage 6: uninstall ---------------------------------------------------------
run "$BIN" --state "$S" --roots "$R" install --title "$T" --remove spawner
check "stage6 uninstall" "^installed title=$T" "$LOG"
[ ! -f "$R/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua" ] && { PASS=$((PASS+1)); note "PASS stage6 file removed"; } \
  || { FAIL=$((FAIL+1)); note "FAIL stage6 file removed"; }
run "$BIN" --state "$S" --roots "$R" records
check "stage6 state-prepared" "mod spawner $T prepared" "$LOG"

# --- bounded JSONL status interface ---------------------------------------------
if [ -f "$S/e2e-status.jsonl" ]; then
  PASS=$((PASS+1)); note "PASS e2e jsonl present"
  grep -q '"stage":"stage-3-install"' "$S/e2e-status.jsonl" && { PASS=$((PASS+1)); note "PASS e2e stage3 line"; } \
    || { FAIL=$((FAIL+1)); note "FAIL e2e stage3 line"; }
  grep -q '"event":"end"' "$S/e2e-status.jsonl" && { PASS=$((PASS+1)); note "PASS e2e end lines"; } \
    || { FAIL=$((FAIL+1)); note "FAIL e2e end lines"; }
else
  FAIL=$((FAIL+1)); note "FAIL e2e jsonl present"
fi

echo "SMOKE RESULT: PASS=$PASS FAIL=$FAIL (log: composition/receipts/smoke-host.log, tree: $TMP)" | tee -a "$LOG"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
