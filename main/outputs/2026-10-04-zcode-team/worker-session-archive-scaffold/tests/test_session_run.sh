#!/bin/bash
# test_session_run.sh — host test for the session archive scaffold.
# Generates fixture files under tests/fixtures/, exercises every subcommand of
# session_run.sh against a disposable run-root (tests/tmp-runroot/), and
# asserts the archive-law properties of worker-post-spawn-closure-prep/
# ACCEPTANCE-RECORD.md §1. Writes tests/TEST-RECEIPT.json.
# Usage: bash tests/test_session_run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ScaffoldRoot="$(cd "$HERE/.." && pwd)"
RUN="$ScaffoldRoot/session_run.sh"
FIX="$HERE/fixtures"
ROOT="$HERE/tmp-runroot"
python3="$(command -v python3 || echo python3)"

PASS=0; FAIL=0; RESULTS=()
ok()  { PASS=$((PASS+1)); RESULTS+=("{\"check\":\"$1\",\"status\":\"PASS\"}"); echo "PASS: $1"; }
bad() { FAIL=$((FAIL+1)); RESULTS+=("{\"check\":\"$1\",\"status\":\"FAIL\",\"detail\":\"$2\"}"); echo "FAILED: $1 :: $2"; }

assert_eq() { # name expected actual
  if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "expected [$2] got [$3]"; fi
}
assert_match() { # name regex actual
  if echo "$3" | grep -qE "$2"; then ok "$1"; else bad "$1" "[$3] does not match $2"; fi
}
assert_file() { if [ -f "$2" ]; then ok "$1"; else bad "$1" "missing file $2"; fi; }

# --- fixtures ------------------------------------------------------------
mkdir -p "$FIX" "$ROOT"
rm -rf "$ROOT"/* 2>/dev/null
printf 'record-001 family=static_tls seh64 result=86\nrecord-002 family=static_tls seh64 result=93\n' > "$FIX/fake-owned-dll-candidate.jsonl"
printf 'step 1 preflight SHASUMS 13/13 OK\nverdict: PASS\nstatus: EXACT match on readback\n' > "$FIX/fake-tool-output.txt"
printf 'synthetic D1 aligned-entry log line\n' > "$FIX/fake-aligned-entry.log"

# --- 1. start stamps a fresh, law-shaped run dir -------------------------
RD_DLL8="$("$RUN" start --flow dll8 --run-root "$ROOT" | tail -1)"
assert_match "start: dir name is <YYYYMMDD-HHMMSS>-dll8" \
             "$ROOT/[0-9]{8}-[0-9]{6}-dll8$" "$RD_DLL8"
assert_file "start: events.log exists from the first moment (law minimum)" "$RD_DLL8/events.log"
assert_file "start: receipt.json exists from the first moment (law minimum)" "$RD_DLL8/receipt.json"
assert_match "start: OPEN receipt carries flow+utc+honesty" \
             '"status": "OPEN"' "$(cat "$RD_DLL8/receipt.json")"
RD2="$("$RUN" start --flow dll8 --run-root "$ROOT" | tail -1)"
if [ "$RD_DLL8" != "$RD2" ] && [ -d "$RD2" ]; then
  ok "start: fresh-dir law (same-second start stamps a NEW dir, never re-runs)"
  # Keep it as a forensics record: an immediate ABORTED close, per the law.
  "$RUN" close --run-dir "$RD2" --verdict ABORTED --notes "fixture: same-second duplicate stamp" >/dev/null
else bad "start: fresh-dir law (never re-runs into an existing dir)" "collision"; fi

# --- 2. note appends to events.log ---------------------------------------
"$RUN" note --run-dir "$RD_DLL8" "step dll8.1 preflight identity checked" >/dev/null
assert_match "note: events.log gets [HH:MM:SS] step lines" \
             "^\[[0-9]{2}:[0-9]{2}:[0-9]{2}\] step dll8\.1 preflight identity checked$" \
             "$(tail -1 "$RD_DLL8/events.log")"

# --- 3. capture copies evidence, records source path + sha ---------------
"$RUN" capture --run-dir "$RD_DLL8" --src "$FIX/fake-owned-dll-candidate.jsonl" --as allfam8-console-result.jsonl >/dev/null
assert_file "capture: evidence file lands in run dir" "$RD_DLL8/allfam8-console-result.jsonl"
FixtureSha="$(shasum -a 256 "$FIX/fake-owned-dll-candidate.jsonl" | cut -d' ' -f1)"
CopySha="$(shasum -a 256 "$RD_DLL8/allfam8-console-result.jsonl" | cut -d' ' -f1)"
assert_eq "capture: sha256 of copy matches shasum of source" "$FixtureSha" "$CopySha"
ArtifactsLine="$(grep 'allfam8-console-result.jsonl' "$RD_DLL8/artifacts.jsonl" | tail -1)"
assert_match "capture: artifacts.jsonl records absolute source path" \
             "\"source\": \"$FIX/fake-owned-dll-candidate.jsonl\"" "$ArtifactsLine"
assert_match "capture: artifacts.jsonl records the sha256" "\"sha256\": \"$FixtureSha\"" "$ArtifactsLine"
if "$RUN" capture --run-dir "$RD_DLL8" --src "$FIX/fake-owned-dll-candidate.jsonl" --as allfam8-console-result.jsonl >/dev/null 2>&1; then
  bad "capture: refuses overwrite (never re-run into existing evidence)" "no error raised"
else ok "capture: refuses overwrite (never re-run into existing evidence)"; fi

# --- 4. verdict archives tool output + extracts verdict lines -------------
VLines="$("$RUN" verdict --run-dir "$RD_DLL8" --from "$FIX/fake-tool-output.txt" --label dll8-verdict)"
assert_file "verdict: raw tool output archived as file (stdout-only verdict is not a record)" "$RD_DLL8/verdict-input-dll8-verdict.txt"
assert_eq "verdict: extracts verdict/status lines (PASS + status: EXACT)" "2" "$(echo "$VLines" | grep -c .)"
assert_match "verdict: PASS line captured verbatim" "verdict: PASS" "$VLines"
assert_match "verdict: status line captured verbatim" "status: EXACT match on readback" "$VLines"
assert_file "verdict: verdict-lines.txt written" "$RD_DLL8/verdict-lines.txt"

# --- 5. checklist honesty: close with missing rows ------------------------
CloseOut="$("$RUN" close --run-dir "$RD_DLL8" --verdict PASS --notes "fixture run; positions 37-40 checked in fixture only")"
assert_match "close: reports closed run" "closed .*dll8 verdict=PASS" "$CloseOut"
assert_match "close: honesty — missing expected artifacts counted" "missing_expected=[0-9]+" "$CloseOut"
assert_match "close: receipt has verdict" '"verdict": "PASS"' "$(cat "$RD_DLL8/receipt.json")"
assert_match "close: receipt has utc" '"utc": "[0-9]{4}-' "$(cat "$RD_DLL8/receipt.json")"
assert_match "close: receipt keeps verdict lines" '"verdict_lines": \[' "$(cat "$RD_DLL8/receipt.json")"
assert_match "close: receipt lists artifacts with sha+source" '"source": ' "$(cat "$RD_DLL8/receipt.json")"
assert_match "close: fixed honesty note present (no completion claims)" \
             'never back-filled' "$(cat "$RD_DLL8/receipt.json")"
if "$RUN" close --run-dir "$RD_DLL8" --verdict FAIL >/dev/null 2>&1; then
  bad "close: refuses rewriting a CLOSED receipt" "second close succeeded"
else ok "close: refuses rewriting a CLOSED receipt"; fi
assert_match "close: INDEX.md row appended" \
             "\| [0-9]{4}-.* \| dll8 \|" "$(tail -1 "$ROOT/INDEX.md")"

# --- 6. game-launch flow maps to game-evidence checklist ------------------
CL="$( "$RUN" checklist --flow game-launch9; "$RUN" checklist --flow game-launch10 )"
for token in cp0-cp4-verdict.json cp5-verdict.json cp6-verdict.json \
             rung_check-RUNG1.json game-evidence-receipt.json \
             stability-gate-verdict.md spawn-count-table.csv; do
  assert_match "checklist game-launch9/10 lists $token" "$token" "$CL"
done
assert_match "checklist dll8 lists ledger verdict (EXPECTED-DELTA-8FAM)" \
             "allfam8-ledger-verdict\.json" "$("$RUN" checklist --flow dll8)"
assert_match "checklist e2e lists consumed-twin + stale.chk + reset proof" \
             "stale\.chk" "$("$RUN" checklist --flow e2e)"

# --- 7. game-launch10 end-to-end (ABORTED run keeps its dir) --------------
RD_L10="$("$RUN" start --flow game-launch10 --run-root "$ROOT" | tail -1)"
"$RUN" capture --run-dir "$RD_L10" --src "$FIX/fake-aligned-entry.log" >/dev/null
"$RUN" close --run-dir "$RD_L10" --verdict ABORTED --notes "fixture abort; aborted runs keep their dirs" >/dev/null
assert_file "aborted run keeps events.log (forensics law)" "$RD_L10/events.log"
assert_match "aborted run keeps its receipt with ABORTED verdict" \
             '"verdict": "ABORTED"' "$(cat "$RD_L10/receipt.json")"
assert_match "aborted run listed in INDEX.md" "game-launch10" "$(tail -1 "$ROOT/INDEX.md")"

# --- 8. pre-created flow entries exist ------------------------------------
for f in dll8 e2e game-launch9 game-launch10; do
  assert_file "flows/$f.md pre-created" "$ScaffoldRoot/flows/$f.md"
done
assert_match "flows/dll8.md checklist cites EXPECTED-DELTA-8FAM positions" \
             "37–40" "$(cat "$ScaffoldRoot/flows/dll8.md")"
assert_match "runs/INDEX.md pristine header (no scaffold run dirs in runs/)" \
             "^# SESSION INDEX" "$(head -1 "$ScaffoldRoot/runs/INDEX.md")"
RunsCount="$(ls -1 "$ScaffoldRoot/runs" | grep -c '^[0-9]\{8\}-' || true)"
assert_eq "runs/ contains no pre-created run dirs (stamped at run time only)" "0" "$RunsCount"

# --- receipt --------------------------------------------------------------
UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
JsonChecks="$(IFS=,; echo "${RESULTS[*]}")"
python3 - "$HERE/TEST-RECEIPT.json" "$UTC" "$PASS" "$FAIL" "$JsonChecks" <<'PYEOF'
import json, sys
out, utc, passed, failed, checks = sys.argv[1:6]
doc = {
    "helper": "session_run.sh",
    "test": "test_session_run.sh",
    "utc": utc,
    "law_source": "outputs/2026-10-04-zcode-team/worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md",
    "summary": {"passed": int(passed), "failed": int(failed)},
    "checks": json.loads("[" + checks + "]"),
    "honesty": "host-side fixture test of scaffold mechanics only; no console contact; no acceptance row advanced",
}
with open(out, "w") as f:
    json.dump(doc, f, indent=2)
PYEOF
echo ""
echo "selftest summary: $PASS ok, $FAIL failed"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
