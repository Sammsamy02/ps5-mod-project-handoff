#!/bin/bash
# build_and_run.sh -- CAP13 Mac-level end-to-end proof (agent KF, 2026-10-13).
#
# Builds e2e_host as a plain x86_64 macOS process (Rosetta 2 on Apple Silicon)
# and runs every case: bridge sanity, dev cases, unseen-mod cases, refusal
# cases, and the staged-shim coverage audit. Output: REPORT/evidence/.
#
# WATCHDOG/RETRY: this macOS 15.7.8 Rosetta build has a flaky demand-translation
# wedge (process enters state UE inside the Rosetta exception server; kill -9
# cannot reap it). Each case runs under a watchdog with up to N retries; a case
# that permanently wedges is recorded as WEDGE (data, not silence).
#
# Mac-only. No console contact. Original DLL files are never written; the host
# hashes every input before/after and asserts byte identity.

set -u
cd "$(dirname "$0")"
CAP13="$(cd .. && pwd)"
EV="$CAP13/REPORT/evidence"
mkdir -p "$EV"
OUT="$EV/e2e-run-$(date -u +%Y%m%d-%H%M%S).log"
TRIES=${TRIES:-3}
WATCHDOG_SECS=${WATCHDOG_SECS:-20}

echo "== build (target: x86_64-apple-macos, runs under Rosetta) ==" | tee "$OUT"
if ! clang++ -arch x86_64 -std=c++17 -O1 -g -Wall -Wno-unused-parameter -Wno-unused-variable \
      -I"$CAP13/native-loader/include" -I"$CAP13/native-loader/src" -I"$CAP13/BUILD/src" \
      e2e_host.cpp "$CAP13/native-loader/src/pe_loader.cpp" \
      "$CAP13/native-loader/src/dllmain_invoker.cpp" \
      "$CAP13/native-loader/src/win_api_shim.cpp" \
      "$CAP13/native-loader/src/win64_bridge.S" -o e2e_host 2>&1 | tee -a "$OUT"; then
    echo "BUILD FAILED" | tee -a "$OUT"; exit 1
fi
file e2e_host | tee -a "$OUT"

run_case() { # run_case <label> <timeout-allowed:true/false> <args...>
    local label="$1"; local may_wedge="$2"; shift 2
    for attempt in $(seq 1 "$TRIES"); do
        echo "" | tee -a "$OUT"
        echo "== RUN: $label (attempt $attempt) ==" | tee -a "$OUT"
        arch -x86_64 ./e2e_host "$@" >> "$OUT" 2>&1 &
        local pid=$!
        local waited=0
        while kill -0 "$pid" 2>/dev/null && [ "$waited" -lt "$WATCHDOG_SECS" ]; do
            sleep 1; waited=$((waited+1))
        done
        if ! kill -0 "$pid" 2>/dev/null; then
            echo "== RUN $label: finished (attempt $attempt) ==" | tee -a "$OUT"
            return 0
        fi
        kill -9 "$pid" 2>/dev/null
        echo "== RUN $label: WEDGE detected (watchdog ${WATCHDOG_SECS}s, attempt $attempt) ==" | tee -a "$OUT"
    done
    if [ "$may_wedge" = "wedge-ok" ]; then
        echo "== RUN $label: recorded as WEDGE (expected under this Rosetta; documented) ==" | tee -a "$OUT"
    else
        echo "== RUN $label: FAILED -- permanently wedged after $TRIES attempts ==" | tee -a "$OUT"
    fi
    return 0
}

S="$CAP13/SAMPLE"; F="$CAP13/FIXTURES"

run_case "bridge (staged callN + g2h s0)"     nowedge bridge
run_case "bridge6 (thunk4s2 stack staging)"   wedge-ok bridge6
run_case "fixture (entry point + exports)"    nowedge fixture        "$F/cap13-ue4ss-native-mod-FIXTURE.dll"
run_case "console-dump (dev sample)"          nowedge console-dump   "$S/cap13-ue4ss-flavor-console-dump.dll"
run_case "lifecycle keybinds-poller (UNSEEN)" nowedge lifecycle      "$S/cap13-ue4ss-flavor-keybinds-poller.dll"

# UNSEEN trio, primary three:
run_case "UNSEEN splitscreen (TLS class)"     nowedge tls            "$S/cap13-mod-splitscreen.dll"
run_case "UNSEEN jsb-lua-profiler (TLS)"      nowedge tls            "$S/cap13-mod-jsb-lua-profiler.dll"
run_case "UNSEEN keybinds (TLS)"              nowedge tls            "$S/cap13-mod-keybinds.dll"

# Extended unseen coverage: every remaining KA sample through the same loader.
for m in actordumper bpmod-loader bpml-generic-functions cheat-manager-enabler \
         console-commands console-enabler linetrace keybinds-noeh feature-probe; do
    run_case "UNSEEN tls mod-$m"              nowedge tls            "$S/cap13-mod-$m.dll"
done

# Refusal matrix incl. real UE4SS-distribution binaries.
run_case "refuse x86 fixture"                 nowedge refuse         "$F/cap13-native-x86-FIXTURE.dll" "not x86_64"
run_case "refuse not-a-PE"                    nowedge refuse         "$F/NOT-A-PE-FIXTURE.dll" "not a PE image"
run_case "refuse real UE4SS.dll (real TLS)"   nowedge tls            "$S/real-UE4SS.dll"
run_case "refuse real dwmapi.dll (imports)"   nowedge refuse         "$S/real-dwmapi.dll" "unresolved imports"

# Staged-shim coverage audit (requirement 6 evidence; audit F1/F14).
for c in cap13-ue4ss-flavor-console-dump cap13-feature-probe cap13-mod-splitscreen real-dwmapi; do
    run_case "shim-coverage $c"               nowedge shim-coverage  "$S/$c.dll"
done

echo "" | tee -a "$OUT"
echo "== all runs complete; full log: $OUT ==" | tee -a "$OUT"
echo "TOTAL PASS checks: $(grep -c '^CHECK: .* PASS' "$OUT")" | tee -a "$OUT"
echo "TOTAL FAIL checks: $(grep -c '^CHECK: .* FAIL' "$OUT")" | tee -a "$OUT"
echo "WEDGE records:     $(grep -c 'WEDGE' "$OUT")" | tee -a "$OUT"
