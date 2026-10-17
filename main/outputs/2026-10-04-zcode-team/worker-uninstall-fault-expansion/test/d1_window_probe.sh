#!/bin/sh
# d1_window_probe.sh — worker-uninstall-fault-expansion, 2026-10-04.
#
# Demonstrates defect D1 on the BASELINE (pre-patch) chain binary.
#
# cmd_install committed records AFTER journal_delete (baseline modinst_core.c:
# journal_delete at the tail of the swap phase, records_save in the commit
# block that follows). A death in that window leaves an end state of
# "journal durably gone + records.txt stale". Every later open runs
# recover_all, finds NO journal ("nothing in flight") and never repairs the
# records: `records` claims installed while the removed mod's files are gone
# ("records lie"), and the next uninstall/install rebuilds its surviving set
# from that lie.
#
# The window is unreachable by the stock --crash-at harness (no probe point
# between journal_delete and records_save), so this probe reconstructs that
# exact end state faithfully: run the uninstall to completion, then restore
# records.txt to its pre-uninstall bytes (the stale content a death in the
# window would have left) with the journal already durably deleted.
#
# The probe must OBSERVE the lie (exit 0 + "D1 DEMONSTRATED"). Under the
# patched chain (uf-order-commit) the window no longer exists; the state this
# probe reconstructs is only reachable by external tampering, which is out of
# the single-writer threat model (documented in REPORT.md).
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=${BIN:-$HERE/composition/build_baseline/modinst_composed}
TMP=$HERE/test/run-d1
LOG=$HERE/receipts/d1-window-demo.log
mkdir -p "$(dirname "$LOG")"
: > "$LOG"
note() { echo "== $*" | tee -a "$LOG"; }
run() { note "run: $*"; "$@" >> "$LOG" 2>&1; RC=$?; note "  exit=$RC"; }

T=PPSA33286
S=$TMP/state; R=$TMP/roots
rm -rf "$TMP"; mkdir -p "$TMP"
[ -x "$BIN" ] || { echo "FATAL: baseline binary missing" >&2; exit 2; }

run "$BIN" make-bundle --id skin --ver 1.0.0 --title "$T" --engine unreal \
  --file pak,mods/~mods/Skin_P.pak=SKINPAKBYTES -o "$TMP/skin.mfb"
run "$BIN" make-bundle --id spawner --ver 1.0.0 --title "$T" --engine unreal \
  --file lua_mod,ue4ss/Mods/ItemSpawner/scripts/main.lua=print\(\"spawn\"\) -o "$TMP/spawner.mfb"
run "$BIN" --state "$S" --roots "$R" register-mod --id skin --title "$T" --bundle "$TMP/skin.mfb"
run "$BIN" --state "$S" --roots "$R" register-mod --id spawner --title "$T" --bundle "$TMP/spawner.mfb"
run "$BIN" --state "$S" --roots "$R" install --title "$T" --mods skin,spawner
cp "$S/records.txt" "$S/records.pre.txt"   # pre-uninstall (stale-side) records
SKINFILE="$R/$T/mods/~mods/Skin_P.pak"
[ -f "$SKINFILE" ] || { echo "FATAL: setup failed (skin file missing)" >&2; exit 2; }

# the uninstall whose commit-window crash is being reconstructed
run "$BIN" --state "$S" --roots "$R" install --title "$T" --remove skin
[ "$RC" -eq 0 ] || { echo "FATAL: uninstall failed" >&2; exit 2; }

# reconstruct the window end state: journal already durably deleted (it is),
# records.txt still at its pre-uninstall content (what a death before
# records_save leaves behind)
cp "$S/records.pre.txt" "$S/records.txt"
[ ! -e "$SKINFILE" ] && note "state: skin file gone; records.txt reverted to pre-uninstall bytes"

# the lie: records claim installed while the file is absent, and no journal
# exists for any recovery to repair it
run "$BIN" --state "$S" --roots "$R" records
if grep -q "mod skin $T installed" "$LOG" && [ ! -e "$SKINFILE" ]; then
  note "D1 DEMONSTRATED: records say 'mod skin installed' while $SKINFILE is absent"
  note "and journal/ is empty, so recover_all can never repair this (no 'in flight' evidence)"
  exit 0
fi
note "D1 NOT demonstrated (records/root consistent) — probe invariant broken"
exit 1
