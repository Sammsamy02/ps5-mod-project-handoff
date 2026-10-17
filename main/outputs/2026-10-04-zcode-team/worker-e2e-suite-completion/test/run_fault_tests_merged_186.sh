#!/bin/sh
# run_fault_tests_merged.sh — worker-d1-chain-relink merged-chain revision (51-tuple uf+r2 chain; sole delta vs worker-uninstall-fault-expansion run_fault_tests.sh: uf-p5-poison-pill expected-unmatched 1->0, documented in header tail)
#
# Adversarial uninstall / crash-recovery coverage on top of the delivered
# F-R1 chain (worker-zip-to-mfb binary, 49-patch chain incl. uf-order-commit):
#   - crash at EVERY journal/transaction boundary of UNINSTALL:
#     stage-journal, staged, swap-journal, rename1, rename2, prevdel, and the
#     new uf probe point pre-journal-del (post-fix window);
#   - install->uninstall interleavings (install crash then remove, remove
#     crash then install, double-uninstall, remove-during-prepare,
#     back-to-back crashes across two transactions);
#   - kill-during-enumeration emulation (partial .prev deletion);
#   - fsync-ordering / power-loss sims via faithful state surgery (journal
#     rename lost, .next dir entry lost, stray .tmp, unknown-phase journal);
#   - quarantine-retained-on-refusal pin.
#
# Invariants asserted after EVERY crash + recovery:
#   I1 tree-hash old-or-current: the live root tree hash equals the full-set
#      tree or the post-uninstall tree (computed from clean references) —
#      never a mix;
#   I2 records never lie: a mod's record state is installed iff its staged
#      files are present in the live root;
#   I3 journal/ is empty once recovery is done, and recovery is idempotent;
#   I4 the next transaction after recovery succeeds and converges.
#
# Origin: worker-uninstall-fault-expansion, 2026-10-04. Host-only; no
# network; no console; all trees inside test/run-*.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=$HERE/composition/build/host/modinst_composed
RUN=$HERE/test/run-fault
LOG=$HERE/test/fault-186-e2e.log
SUM=$HERE/test/fault-186-e2e-summary.log
T=PPSA33286
mkdir -p "$HERE/test" "$RUN"
rm -rf "$RUN"
: > "$LOG"; : > "$SUM"
PASS=0; FAIL=0
note() { echo "== $*" | tee -a "$LOG"; }
ok()   { PASS=$((PASS+1)); echo "PASS $*" | tee -a "$SUM" "$LOG"; }
bad()  { FAIL=$((FAIL+1)); echo "FAIL $*" | tee -a "$SUM" "$LOG"; }
run()  { note "run: $*"; "$@" >> "$LOG" 2>&1; RC=$?; note "  exit=$RC"; }
stage() { echo "" | tee -a "$LOG"; note "### $*"; }
mark() { wc -l < "$LOG" | tr -d ' '; }
seg()  { tail -n +"$1" "$LOG"; }
tree_hash() { # tree_hash DIR -> content+relpath hash (fixtures are newline-free)
  find "$1" -type f | LC_ALL=C sort | while IFS= read -r f; do
    printf '%s  %s\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "${f#"$1"/}"
  done | shasum -a 256 | cut -d' ' -f1
}
journal_empty() { # journal_empty CASEDIR
  [ -z "$(ls -A "$1/state/journal" 2>/dev/null)" ]
}
expect_records() { # expect_records CASEDIR EXPECTED_SKIN EXPECTED_SPAWNER
  local d=$1 want_skin=$2 want_spawner=$3 m
  m=$(mark)
  run "$BIN" --state "$d/state" --roots "$d/roots" records
  seg "$m" | grep -q "mod skin $T $want_skin" && ok "$CN records skin=$want_skin" || bad "$CN records skin=$want_skin"
  seg "$m" | grep -q "mod spawner $T $want_spawner" && ok "$CN records spawner=$want_spawner" || bad "$CN records spawner=$want_spawner"
}
invariants() { # invariants CASEDIR  — I1+I2 against the live tree
  local d=$1 th skinf spawnerf
  th=$(tree_hash "$d/roots/$T")
  if [ "$th" = "$TREE_FULL" ] || [ "$th" = "$TREE_RM" ]; then
    ok "$CN tree-hash is old-or-current, never mixed"
  else
    bad "$CN tree-hash MIXED ($th not in {FULL,RM})"
  fi
  skinf="$d/roots/$T/mods/~mods/Skin_P.pak"
  spawnerf="$d/roots/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua"
  if [ -e "$skinf" ]; then
    expect_records "$d" installed installed
  else
    expect_records "$d" prepared installed
  fi
  [ -e "$spawnerf" ] && ok "$CN spawner file present" || bad "$CN spawner file present"
}
finish_next_op() { # finish_next_op CASEDIR — I4: complete the interrupted uninstall
  local d=$1 m skinf spawnerf
  skinf="$d/roots/$T/mods/~mods/Skin_P.pak"
  spawnerf="$d/roots/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua"
  if [ -e "$skinf" ]; then
    note "$CN next-op: recovery rolled back (both installed) -> finish by removing spawner"
    run "$BIN" --state "$d/state" --roots "$d/roots" install --title "$T" --remove spawner
    [ "$RC" -eq 0 ] && ok "$CN next-op remove exit 0" || bad "$CN next-op remove exit=$RC"
    [ -e "$skinf" ] && ok "$CN next-op skin file present" || bad "$CN next-op skin file present"
    [ ! -e "$spawnerf" ] && ok "$CN next-op spawner file gone" || bad "$CN next-op spawner file gone"
    m=$(mark)
    run "$BIN" --state "$d/state" --roots "$d/roots" records
    seg "$m" | grep -q "mod skin $T installed" && ok "$CN next-op skin installed" || bad "$CN next-op skin installed"
  else
    note "$CN next-op: recovery committed the uninstall (skin prepared) -> removing spawner empties the set"
    run "$BIN" --state "$d/state" --roots "$d/roots" install --title "$T" --remove spawner
    [ "$RC" -eq 0 ] && ok "$CN next-op remove exit 0" || bad "$CN next-op remove exit=$RC"
    [ "$(find "$d/roots/$T" -type f 2>/dev/null | wc -l | tr -d ' ')" = "2" ] \
      && ok "$CN next-op empty-set tree (marker + loadlist only)" || bad "$CN next-op empty-set tree"
    expect_records "$d" prepared prepared
  fi
  journal_empty "$d" && ok "$CN next-op journal empty" || bad "$CN next-op journal empty"
}
setup_full() { # setup_full CASEDIR -> registers skin+spawner, prepares, installs
  local d=$1
  mkdir -p "$d"
  run "$BIN" --state "$d/state" --roots "$d/roots" register-mod --id skin --title "$T" --bundle "$FIX/skin.mfb"
  run "$BIN" --state "$d/state" --roots "$d/roots" register-mod --id spawner --title "$T" --bundle "$FIX/spawner.mfb"
  run "$BIN" --state "$d/state" --roots "$d/roots" prepare --title "$T" --mods skin,spawner
  run "$BIN" --state "$d/state" --roots "$d/roots" install --title "$T" --mods skin,spawner
  [ "$RC" -eq 0 ] || { echo "FATAL: setup_full install failed" >&2; exit 2; }
}
recover_expect() { # recover_expect CASEDIR PHRASE LABEL
  local d=$1 phrase=$2 label=$3 m
  m=$(mark)
  run "$BIN" --state "$d/state" --roots "$d/roots" recover
  seg "$m" | grep -q "$phrase" && ok "$CN recover: $label" || bad "$CN recover expected '$phrase' ($label)"
}
crash_run() { # crash_run CASEDIR ARGS... — expect exit 86, tally unmatched
  local d=$1; shift
  echo 1 >> "$d/expected-unmatched"
  run "$BIN" --state "$d/state" --roots "$d/roots" "$@"
  if [ "$RC" -eq 86 ]; then ok "$CN crash exit 86 at $1"; else bad "$CN crash exit=$RC (want 86) at $1"; fi
}

[ -x "$BIN" ] || { echo "FATAL: patched binary missing (run compose_fault.py)" >&2; exit 2; }

stage "T0 fixtures (stateless make-bundle, reused across cases)"
FIX=$RUN/fixtures
mkdir -p "$FIX"
run "$BIN" make-bundle --id skin --ver 1.0.0 --title "$T" --engine unreal \
  --file pak,mods/~mods/Skin_P.pak=SKINPAKBYTES -o "$FIX/skin.mfb"
[ "$RC" -eq 0 ] && ok "fixture skin.mfb" || bad "fixture skin.mfb"
run "$BIN" make-bundle --id spawner --ver 1.0.0 --title "$T" --engine unreal \
  --file lua_mod,ue4ss/Mods/ItemSpawner/scripts/main.lua=print\(\"spawn\"\) -o "$FIX/spawner.mfb"
[ "$RC" -eq 0 ] && ok "fixture spawner.mfb" || bad "fixture spawner.mfb"
run "$BIN" make-bundle --id pakmod --ver 1.0.0 --title "$T" --engine unreal \
  --file pak,mods/~mods/Pakmod_P.pak=PAKMODBYTES -o "$FIX/pakmod.mfb"
[ "$RC" -eq 0 ] && ok "fixture pakmod.mfb" || bad "fixture pakmod.mfb"

stage "A reference trees (FULL / RM) from clean runs"
BASE=$RUN/uf-base; setup_full "$BASE"; CN=uf-base
TREE_FULL=$(tree_hash "$BASE/roots/$T")
[ -n "$TREE_FULL" ] && ok "reference FULL tree captured ($TREE_FULL)" || bad "reference FULL tree"
REF=$RUN/uf-ref; mkdir -p "$REF"
cp -R "$BASE/state" "$REF/state"; cp -R "$BASE/roots" "$REF/roots"
run "$BIN" --state "$REF/state" --roots "$REF/roots" install --title "$T" --remove skin
[ "$RC" -eq 0 ] && ok "reference clean uninstall exit 0" || bad "reference clean uninstall exit=$RC"
TREE_RM=$(tree_hash "$REF/roots/$T")
[ -n "$TREE_RM" ] && [ "$TREE_RM" != "$TREE_FULL" ] && ok "reference RM tree captured ($TREE_RM)" || bad "reference RM tree"
CN=uf-ref
[ ! -e "$REF/roots/$T/mods/~mods/Skin_P.pak" ] && ok "ref skin file gone" || bad "ref skin file gone"
[ -e "$REF/roots/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua" ] && ok "ref spawner file kept" || bad "ref spawner file kept"
expect_records "$REF" prepared installed

stage "B crash matrix: uninstall at every journal/transaction boundary"
# each case: setup -> uninstall --crash-at P (exit 86) -> recover (phrase) ->
# invariants (I1+I2) -> journal empty -> recover again (idempotent, I3) ->
# next-op completes the interrupted uninstall (I4). expected-unmatched=1.
mx() { # mx CASEID CRASHPOINT RECOVER_PHRASE LABEL
  local id=$1 point=$2 phrase=$3 label=$4 m
  local d=$RUN/$id
  CN=$id; setup_full "$d"
  crash_run "$d" install --title "$T" --remove skin --crash-at "$point"
  recover_expect "$d" "$phrase" "$label"
  invariants "$d"
  journal_empty "$d" && ok "$CN journal empty after recovery" || bad "$CN journal empty after recovery"
  m=$(mark)
  run "$BIN" --state "$d/state" --roots "$d/roots" recover
  [ "$RC" -eq 0 ] && ok "$CN second recover exit 0 (no-op)" || bad "$CN second recover exit=$RC"
  [ -z "$(seg "$m" | grep 'recover title=')" ] && ok "$CN second recover silent (idempotent)" || bad "$CN second recover not silent"
  finish_next_op "$d"
}
mx uf-stage-journal stage-journal "rolled back staging" "stage journal crash rolls back"
mx uf-staged          staged          "rolled back staging" "staged crash rolls back"
mx uf-swap-journal    swap-journal    "rolled back before swap" "swap-journal crash rolls back pre-swap"
mx uf-rename1         rename1         "finished swap forward" "rename1 crash finishes swap forward"
mx uf-rename2         rename2         "committed" "rename2 crash commits"
mx uf-prevdel         prevdel         "committed" "prevdel crash commits"

stage "C interleavings"
CN=uf-i1-installcrash-then-remove; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --mods skin,spawner --crash-at swap-journal
note "i1: uninstall immediately after an install crash (recovery + new transaction, one open)"
run "$BIN" --state "$D/state" --roots "$D/roots" install --title "$T" --remove skin
[ "$RC" -eq 0 ] && ok "$CN remove after install-crash exit 0" || bad "$CN remove after install-crash exit=$RC"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_RM" ] \
  && ok "$CN tree RM (install rollback kept both installed; remove then dropped skin)" \
  || bad "$CN tree RM (got $(tree_hash "$D/roots/$T"))"
expect_records "$D" prepared installed
journal_empty "$D" && ok "$CN journal empty" || bad "$CN journal empty"

CN=uf-i2-removecrash-then-install; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at rename2
note "i2: install immediately after an uninstall crash (recovery commits gen2, install becomes gen3); full re-selection restores the pair"
run "$BIN" --state "$D/state" --roots "$D/roots" install --title "$T" --mods skin,spawner
[ "$RC" -eq 0 ] && ok "$CN install after remove-crash exit 0" || bad "$CN install after remove-crash exit=$RC"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_FULL" ] && ok "$CN tree back to FULL" || bad "$CN tree back to FULL"
expect_records "$D" installed installed
journal_empty "$D" && ok "$CN journal empty" || bad "$CN journal empty"

CN=uf-i3-double-uninstall; D=$RUN/$CN; setup_full "$D"
run "$BIN" --state "$D/state" --roots "$D/roots" install --title "$T" --remove skin
[ "$RC" -eq 0 ] && ok "$CN first remove exit 0" || bad "$CN first remove exit=$RC"
run "$BIN" --state "$D/state" --roots "$D/roots" install --title "$T" --remove skin
[ "$RC" -eq 0 ] && ok "$CN second remove exit 0 (benign no-op transaction)" || bad "$CN second remove exit=$RC"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_RM" ] && ok "$CN tree unchanged (RM)" || bad "$CN tree unchanged (RM)"
expect_records "$D" prepared installed
journal_empty "$D" && ok "$CN journal empty" || bad "$CN journal empty"

CN=uf-i4-remove-during-prepare; D=$RUN/$CN
run "$BIN" --state "$D/state" --roots "$D/roots" register-mod --id pakmod --title "$T" --bundle "$FIX/pakmod.mfb"
setup_full "$D"
run "$BIN" --state "$D/state" --roots "$D/roots" prepare --title "$T" --mods pakmod
crash_run "$D" install --title "$T" --remove skin --crash-at rename2
recover_expect "$D" "committed" "recovery commits with a prepared bystander"
CN=$CN
[ -e "$D/roots/$T/ue4ss/Mods/ItemSpawner/scripts/main.lua" ] && ok "$CN spawner kept" || bad "$CN spawner kept"
[ ! -e "$D/roots/$T/mods/~mods/Skin_P.pak" ] && ok "$CN skin gone" || bad "$CN skin gone"
[ ! -e "$D/roots/$T/mods/~mods/Pakmod_P.pak" ] && ok "$CN pakmod never staged" || bad "$CN pakmod never staged"
m=$(mark)
run "$BIN" --state "$D/state" --roots "$D/roots" records
seg "$m" | grep -q "mod pakmod $T prepared" && ok "$CN pakmod stays prepared (records never lie)" || bad "$CN pakmod stays prepared"
seg "$m" | grep -q "mod skin $T prepared" && ok "$CN skin prepared" || bad "$CN skin prepared"
seg "$m" | grep -q "mod spawner $T installed" && ok "$CN spawner installed" || bad "$CN spawner installed"

CN=uf-i5-new-window-probe; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at pre-journal-del
recover_expect "$D" "committed" "post-records window repaired idempotently (uf-order-commit)"
invariants "$D"
journal_empty "$D" && ok "$CN journal empty after recovery" || bad "$CN journal empty after recovery"
finish_next_op "$D"

CN=uf-i6-partial-prev; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at rename2
PREV="$D/roots/$T.prev"
[ -d "$PREV" ] && ok "$CN .prev present pre-recovery" || bad "$CN .prev present pre-recovery"
note "i6: emulate kill during prevdel enumeration — one file of .prev already gone"
FIRSTF=$(find "$PREV" -type f | LC_ALL=C sort | head -1)
rm -f "$FIRSTF"
recover_expect "$D" "committed" "recovery finishes partial .prev deletion"
[ ! -d "$PREV" ] && ok "$CN .prev fully gone" || bad "$CN .prev fully gone"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_RM" ] && ok "$CN tree RM" || bad "$CN tree RM"
expect_records "$D" prepared installed

CN=uf-i7-back-to-back-crashes; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at swap-journal
crash_run "$D" install --title "$T" --mods skin,spawner --crash-at staged
recover_expect "$D" "rolled back staging" "second crash recovers on its own journal"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_FULL" ] && ok "$CN tree FULL (uninstall rolled back, restage dropped)" || bad "$CN tree FULL"
expect_records "$D" installed installed
journal_empty "$D" && ok "$CN journal empty" || bad "$CN journal empty"

stage "D power-loss / fsync-ordering sims (faithful state surgery)"
CN=uf-p1-journal-lost; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at swap-journal
rm -f "$D/state/journal/$T.json"   # journal rename never reached the disk
note "p1: stale .next present with NO journal — next transaction must clear it, never mix"
run "$BIN" --state "$D/state" --roots "$D/roots" install --title "$T" --mods skin,spawner
[ "$RC" -eq 0 ] && ok "$CN install over stale .next exit 0" || bad "$CN install over stale .next exit=$RC"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_FULL" ] && ok "$CN tree FULL (stale staged RM-set discarded)" || bad "$CN tree FULL (stale staged RM-set discarded)"
expect_records "$D" installed installed

CN=uf-p2-next-lost; D=$RUN/$CN; setup_full "$D"
crash_run "$D" install --title "$T" --remove skin --crash-at rename1
rm -rf "$D/roots/$T.next"          # .next dir entry lost by the power cut
recover_expect "$D" "rolled back swap (previous set restored byte-for-byte)" "rollback from .prev"
[ "$(tree_hash "$D/roots/$T")" = "$TREE_FULL" ] && ok "$CN tree FULL" || bad "$CN tree FULL"
expect_records "$D" installed installed
journal_empty "$D" && ok "$CN journal empty" || bad "$CN journal empty"

CN=uf-p4-tmp-leftover; D=$RUN/$CN
mkdir -p "$D/state/journal" "$D/roots"
printf '{"schema": "modforge-journal/1", "phase": "swap", "generation": 9, "title": "%s", "ll_sha": "deadbeef"}\n' "$T" \
  > "$D/state/journal/$T.json.tmp"
m=$(mark)
run "$BIN" --state "$D/state" --roots "$D/roots" records
[ "$RC" -eq 0 ] && ok "$CN records exit 0 with .tmp leftover" || bad "$CN records exit=$RC with .tmp leftover"
[ -z "$(seg "$m" | grep 'recover title=')" ] && ok "$CN .tmp ignored (json filter)" || bad "$CN .tmp was consumed"
[ -f "$D/state/journal/$T.json.tmp" ] && ok "$CN .tmp untouched" || bad "$CN .tmp touched"

CN=uf-p5-poison-pill; D=$RUN/$CN
mkdir -p "$D/state/journal" "$D/roots"
printf '{"schema": "modforge-journal/1", "phase": "bogus", "generation": 1, "title": "OTHER"}\n' > "$D/state/journal/OTHER.json"
: > "$D/expected-unmatched"   # merged-chain revision worker-d1-chain-relink: F-R2 r2-exit-shim routes the controlled die() exit through e2e_status_end, so the refused records command emits a MATCHED failed end (transcript showed begin+{"event":"end","exit":1}); _exit(86) crash probes and real signals still leave unmatched begins
m=$(mark)
run "$BIN" --state "$D/state" --roots "$D/roots" records
if [ "$RC" -eq 1 ] && seg "$m" | grep -q "unknown journal phase"; then
  ok "$CN unknown-phase journal fails loudly (no guessing, contract fail-safe)"
else
  bad "$CN unknown-phase journal not refused (exit=$RC)"
fi
rm "$D/state/journal/OTHER.json"
run "$BIN" --state "$D/state" --roots "$D/roots" records
[ "$RC" -eq 0 ] && ok "$CN healthy after poison removed" || bad "$CN healthy after poison removed exit=$RC"

stage "E quarantine retained on refusal (invariant pin)"
CN=uf-q-quarantine; D=$RUN/$CN; mkdir -p "$D/intake"
python3 - "$D/intake/notamod.zip" <<'EOF'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.writestr("ReadMe.txt", "documentation only, no mod payload\n")
    z.writestr("screenshots/shot.png", b"\x89PNG\r\n\x1a\nfake")
EOF
printf 'id\t%s\ntitle\t%s\nurl\tfile://%s\nintake\t%s\nsha256\t%s\n' \
  uf-q "$T" "$D/intake/notamod.zip" "$D/intake" \
  "$(shasum -a 256 "$D/intake/notamod.zip" | cut -d' ' -f1)" > "$D/job.tsv"
run "$BIN" --state "$D/state" --roots "$D/roots" download --job "$D/job.tsv"
[ "$RC" -eq 0 ] && ok "$CN download exit 0" || bad "$CN download exit=$RC"
run "$BIN" --state "$D/state" --roots "$D/roots" prepare-archive --title "$T" \
  --archive "$D/intake/uf-q.zip" --quarantine "$D/intake/uf-q.q" --mod uf-q
[ "$RC" -ne 0 ] && ok "$CN prepare-archive refuses" || bad "$CN prepare-archive refused nothing"
[ -f "$D/intake/uf-q.q/manifest-v1.bin" ] && ok "$CN quarantine retained on refusal" || bad "$CN quarantine erased on refusal"
m=$(mark)
run "$BIN" --state "$D/state" --roots "$D/roots" records
seg "$m" | grep -q "mod uf-q $T downloaded" && ok "$CN record stays downloaded" || bad "$CN record state changed"

stage "F transcript sanity (unmatched begins == crash runs; no unexpected failed ends)"
TOT_UNEXP=0
for d in "$RUN"/*/; do
  id=$(basename "$d")
  [ -f "$d/state/e2e-status.jsonl" ] || continue
  EXP=$(cat "$d/expected-unmatched" 2>/dev/null | wc -l | tr -d ' ')
  GOT=$(python3 - "$d/state/e2e-status.jsonl" <<'EOF'
import json,sys
b=e=0
for l in open(sys.argv[1]):
    l=l.strip()
    if not l: continue
    d=json.loads(l)
    if d.get("event")=="begin": b+=1
    elif d.get("event")=="end": e+=1
print(b-e)
EOF
)
  if [ "$EXP" = "$GOT" ]; then ok "sanity $id unmatched=$GOT expected=$EXP"; else bad "sanity $id unmatched=$GOT expected=$EXP"; fi
  [ "$EXP" = "0" ] || TOT_UNEXP=$((TOT_UNEXP+1))
done

echo "FAULT SUITE RESULT: PASS=$PASS FAIL=$FAIL (log: test/fault-e2e.log)" | tee -a "$SUM" "$LOG"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
