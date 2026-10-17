#!/bin/bash
# run_tests.sh — host tests for the worker-7-installer vertical slice.
#
# Covers the required scenarios:
#   1  multi-mod composition with a conflict (detection + later-wins outcome)
#   2  installed vs running as distinct durable states (+ real activation
#      component consuming the composed root through mfa_activate)
#   3  cancel mid-transaction (during staging; between the swap renames)
#      leaving the previous set byte-for-byte intact
#   4  simulated crash between journal steps, then recovery:
#      every crash point ends at the old set or the new set, never a mix
#   5  rollback path restoring the original root byte-for-byte
#   6  unsupported-requirement refusal with specific unavailable reasons
#   7  FOMOD-style variant choices (defaults, explicit choice, invalid
#      choice, needs-choices)
#   8  uninstall (mod removal and removal-to-empty)
#   9  unmanaged-root ownership guard
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/../build/modinst"
TMP="$HERE/tmp"
PASS=0; FAIL=0

ok()   { PASS=$((PASS+1)); echo "PASS $1"; }
bad()  { FAIL=$((FAIL+1)); echo "FAIL $1"; }
check() { # check <name> <condition-result>
  if [ "$2" -eq 0 ]; then ok "$1"; else bad "$1"; fi
}
has() { # has <name> <needle> <haystack-file>
  if grep -qF "$2" "$3"; then ok "$1"; else bad "$1 (missing: $2 in $(basename "$3"))"; fi
}
hasnt() {
  if grep -qF "$2" "$3"; then bad "$1 (unexpected: $2)"; else ok "$1"; fi
}
treehash() {
  (cd "$1" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256 | shasum -a 256 | cut -d' ' -f1)
}
newsbox() { # fresh sandbox with capability table and fixture bundles
  rm -rf "$TMP/$1"
  mkdir -p "$TMP/$1"/{state/journal,roots,bundles}
  cat > "$TMP/$1/state/caps.tsv" <<'EOF'
# runtime components on PS5 (maturity from vendored engine facts, FORMATS.md)
component pak-loader available
component overlay-ps5 available
component ue4ss-ps5 integrating
component papyrus-vm planned
EOF
  B="$TMP/$1/bundles"; SB="$TMP/$1"
}
M() { "$BIN" --state "$SB/state" --roots "$SB/roots" "$@"; }

echo "== fixtures and scenarios =="

###############################################################################
# Scenario A: three mods, one conflicting destination, full pipeline
###############################################################################
newsbox A
B="$SB/bundles"
"$BIN" make-bundle --id item-spawner --ver 1.2.0 --prio 100 --title PPSA33286 \
  --engine unreal --sdk 0x11000000 --framework ue4ss-ps5 \
  --file lua_mod,'ue4ss/Mods/ItemSpawner/scripts/main.lua'=SPAWNER_MAIN \
  --file config,'ue4ss/Mods/Shared/config.txt'=A_ITEM_CONFIG \
  -o "$B/spawner.mfb" > /dev/null
"$BIN" make-bundle --id hd-skin --ver 2.1.0 --prio 200 --title PPSA33286 \
  --engine unreal --sdk 0x11000000 --framework pak-loader \
  --file pak,'mods/~mods/Skin_P.pak'=B_SKIN_PAK \
  --file config,'ue4ss/Mods/Shared/config.txt'=B_SKIN_CONFIG_WINS \
  -o "$B/skin.mfb" > /dev/null
"$BIN" make-bundle --id low-priority-mod --ver 0.3.0 --prio 50 --title PPSA33286 \
  --engine unreal --sdk 0x11000000 --framework ue4ss-ps5 \
  --file lua_mod,'ue4ss/Mods/Low/scripts/low.lua'=LOW_MAIN \
  -o "$B/low.mfb" > /dev/null

M register-mod --id item-spawner --title PPSA33286 --bundle "$B/spawner.mfb" > "$SB/01.out" 2>&1
M register-mod --id hd-skin --title PPSA33286 --bundle "$B/skin.mfb" >> "$SB/01.out" 2>&1
M register-mod --id low-priority-mod --title PPSA33286 --bundle "$B/low.mfb" >> "$SB/01.out" 2>&1
has "A1 register three mods as downloaded" "state=downloaded" "$SB/01.out"

M records > "$SB/02.out" 2>&1
has "A2 downloaded state is durable before prepare" "mod item-spawner PPSA33286 downloaded" "$SB/02.out"
hasnt "A3 nothing claims installed before install" "installed" "$SB/02.out"

M prepare --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod > "$SB/03.out" 2>&1
RC=$?
check "A4 prepare succeeds (exit 0)" $([ $RC -eq 0 ]; echo $?)
has    "A5 conflict detected at the shared destination" "conflict dest=ue4ss/Mods/Shared/config.txt previous=item-spawner winner=hd-skin" "$SB/03.out"
has    "A6 load order is priority then id" "load-order 50 low-priority-mod 0.3.0" "$SB/03.out"
has    "A7 higher priority loads later" "load-order 200 hd-skin 2.1.0" "$SB/03.out"
has    "A8 integrating framework reported as warning, not blocker" "warning framework 'ue4ss-ps5' (mod 'item-spawner') is integrating" "$SB/03.out"
has    "A9 plan reports ready" "ready 1" "$SB/03.out"

M install --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod > "$SB/04.out" 2>&1
RC=$?
check "A10 install succeeds (exit 0)" $([ $RC -eq 0 ]; echo $?)
has "A11 conflict winner content installed" "B_SKIN_CONFIG_WINS" "$SB/roots/PPSA33286/ue4ss/Mods/Shared/config.txt"
if grep -qF "A_ITEM_CONFIG" "$SB/roots/PPSA33286/ue4ss/Mods/Shared/config.txt"; then bad "A12 conflict winner wrong"; else ok "A12 losing content not present"; fi
has "A13 marker written (managed root)" "managed by modforge" "$SB/roots/PPSA33286/.modforge-root"
has "A14 winner file present" "B_SKIN_PAK" "$SB/roots/PPSA33286/mods/~mods/Skin_P.pak"
[ "$(grep -c '^mod	' "$SB/roots/PPSA33286/loadlist.txt")" -eq 3 ]
check "A15 load list has three mods in load order" $?
grep '^mod	' "$SB/roots/PPSA33286/loadlist.txt" | head -1 | grep -q "low-priority-mod"
check "A16 load-list order: lowest priority first" $?
tail -1 "$SB/roots/PPSA33286/loadlist.txt" | grep -q "^file	"
check "A17 file records present (load list written last)" $?
M records > "$SB/05.out" 2>&1
has "A18 mods advance to installed" "mod hd-skin PPSA33286 installed" "$SB/05.out"
has "A19 generation recorded" "generation=1" "$SB/05.out"

###############################################################################
# Scenario B: installed is not running; running comes only from the runtime
###############################################################################
M records > "$SB/10.out" 2>&1
has  "B1 installed game shows runtime not-started" "runtime not-started running=0" "$SB/10.out"
M simulate-runtime --title PPSA33286 --rt-state mods-started > "$SB/11.out" 2>&1
RC=$?
check "B2 real activation component consumes composed root (mfa_activate ok)" $([ $RC -eq 0 ]; echo $?)
has "B3 activation-check reports the installed set" "activation-check ok title=PPSA33286 mods=3" "$SB/11.out"
has "B4 status written through the real mfa_status API" "runtime-status state=mods-started" "$SB/11.out"
M records > "$SB/12.out" 2>&1
has "B5 running only after the runtime acknowledged" "runtime mods-started running=1" "$SB/12.out"
M simulate-runtime --title PPSA33286 --rt-state failed --code 7 --detail "engine-bridge-absent" > "$SB/13.out" 2>&1
M records > "$SB/14.out" 2>&1
has "B6 failure state replaces running with detail" "runtime failed running=0" "$SB/14.out"
has "B7 failure detail surfaced" "detail=engine-bridge-absent" "$SB/14.out"

###############################################################################
# Scenario C: cancel mid-transaction restores byte-for-byte
###############################################################################
newsbox C
cp -R "$TMP/A/bundles/." "$SB/bundles/"
M register-mod --id item-spawner --title PPSA33286 --bundle "$SB/bundles/spawner.mfb" > /dev/null 2>&1
M install --title PPSA33286 --mods item-spawner > "$SB/01.out" 2>&1
OLD=$(treehash "$SB/roots/PPSA33286")
M register-mod --id hd-skin --title PPSA33286 --bundle "$SB/bundles/skin.mfb" > /dev/null 2>&1
M register-mod --id low-priority-mod --title PPSA33286 --bundle "$SB/bundles/low.mfb" > /dev/null 2>&1

M install --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod --cancel-after 1 > "$SB/02.out" 2>&1
RC=$?
check "C1 cancel during staging exits cancelled" $([ $RC -eq 5 ]; echo $?)
has "C2 cancel reported with position" "cancelled during staging after 1 file(s)" "$SB/02.out"
[ "$(treehash "$SB/roots/PPSA33286")" = "$OLD" ]
check "C3 previous root byte-for-byte unchanged after cancel" $?
[ -z "$(ls -A "$SB/state/journal" 2>/dev/null)" ]
check "C4 journal cleaned after cancel" $?
[ ! -e "$SB/roots/PPSA33286.next" ] && [ ! -e "$SB/roots/PPSA33286.prev" ]
check "C5 no staging leftovers after cancel" $?

M install --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod --cancel-at rename1 > "$SB/03.out" 2>&1
RC=$?
check "C6 cancel between renames exits cancelled" $([ $RC -eq 5 ]; echo $?)
has "C7 rollback between renames reported" "cancelled between renames; rolled back" "$SB/03.out"
[ "$(treehash "$SB/roots/PPSA33286")" = "$OLD" ]
check "C8 previous root byte-for-byte unchanged after mid-swap cancel" $?
[ -z "$(ls -A "$SB/state/journal" 2>/dev/null)" ] && [ ! -e "$SB/roots/PPSA33286.prev" ]
check "C9 rollback cleanup complete" $?

###############################################################################
# Scenario D: crash between journal steps -> recovery (old or new, never mix)
###############################################################################
crash_case() { # <name> <point> <expect: old|new> <from-A-sandbox:0|1>
  local NAME="$1" POINT="$2" EXPECT="$3"
  newsbox "D_$NAME"
  cp -R "$TMP/A/bundles/." "$SB/bundles/"
  M register-mod --id item-spawner --title PPSA33286 --bundle "$SB/bundles/spawner.mfb" > /dev/null 2>&1
  M register-mod --id hd-skin --title PPSA33286 --bundle "$SB/bundles/skin.mfb" > /dev/null 2>&1
  M register-mod --id low-priority-mod --title PPSA33286 --bundle "$SB/bundles/low.mfb" > /dev/null 2>&1
  M install --title PPSA33286 --mods item-spawner > /dev/null 2>&1
  local OLD NEW
  OLD=$(treehash "$SB/roots/PPSA33286")
  # compute what the new set would hash to (a completed reference install)
  cp -R "$SB" "$TMP/D_${NAME}_ref"
  REF="$TMP/D_${NAME}_ref"
  "$BIN" --state "$REF/state" --roots "$REF/roots" install --title PPSA33286 \
      --mods item-spawner,hd-skin,low-priority-mod > /dev/null 2>&1
  NEW=$(treehash "$REF/roots/PPSA33286")
  rm -rf "$TMP/D_${NAME}_ref"

  M install --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod --crash-at "$POINT" > "$SB/crash.out" 2>&1
  local RC=$?
  check "D-$NAME-1 crash at $POINT exits 86" $([ $RC -eq 86 ]; echo $?)
  M records > "$SB/rec.out" 2>&1   # opening the store runs recovery
  has "D-$NAME-2 recovery ran" "recover title=PPSA33286" "$SB/rec.out"
  local H; H=$(treehash "$SB/roots/PPSA33286")
  if [ "$EXPECT" = "old" ]; then
    [ "$H" = "$OLD" ]; check "D-$NAME-3 root is exactly the old set" $?
    has "D-$NAME-4 records still at old generation" "generation=1" "$SB/rec.out"
  else
    [ "$H" = "$NEW" ]; check "D-$NAME-3 root is exactly the new set" $?
    has "D-$NAME-4 records advanced to new generation" "generation=2" "$SB/rec.out"
  fi
  [ -z "$(ls -A "$SB/state/journal" 2>/dev/null)" ]
  check "D-$NAME-5 journal removed after recovery" $?
  [ ! -e "$SB/roots/PPSA33286.next" ] && [ ! -e "$SB/roots/PPSA33286.prev" ]
  check "D-$NAME-6 no .next/.prev leftovers" $?
}
crash_case stagejournal stage-journal old
crash_case staged       staged        old
crash_case swapjournal  swap-journal  old
crash_case rename1      rename1       new
crash_case rename2      rename2       new
crash_case prevdel      prevdel       new

# Rollback recovery path: crash after the first rename, staging root lost
newsbox D_rollback
cp -R "$TMP/A/bundles/." "$SB/bundles/"
M register-mod --id item-spawner --title PPSA33286 --bundle "$SB/bundles/spawner.mfb" > /dev/null 2>&1
M register-mod --id hd-skin --title PPSA33286 --bundle "$SB/bundles/skin.mfb" > /dev/null 2>&1
M install --title PPSA33286 --mods item-spawner > /dev/null 2>&1
OLD=$(treehash "$SB/roots/PPSA33286")
M install --title PPSA33286 --mods item-spawner,hd-skin --crash-at rename1 > /dev/null 2>&1
rm -rf "$SB/roots/PPSA33286.next"    # staging root destroyed after the crash
M records > "$SB/01.out" 2>&1
has  "DR1 recovery announces rollback" "rolled back swap" "$SB/01.out"
[ "$(treehash "$SB/roots/PPSA33286")" = "$OLD" ]
check "DR2 original root restored byte-for-byte from .prev" $?
has  "DR3 records unchanged (old set still installed)" "mod item-spawner PPSA33286 installed" "$SB/01.out"
hasnt "DR4 replaced mod not installed after rollback" "mod hd-skin PPSA33286 installed" "$SB/01.out"

###############################################################################
# Scenario E: unsupported requirements -> specific unavailable reasons
###############################################################################
newsbox E
B="$SB/bundles"
"$BIN" make-bundle --id orphan-dependent --ver 1.0.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework ue4ss-ps5 --requires missing-mod=1.0.0 \
  --file lua_mod,'ue4ss/Mods/Orphan/main.lua'=ORPHAN \
  -o "$B/orphan.mfb" > /dev/null
"$BIN" make-bundle --id windows-dll-mod --ver 1.0.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework windows-pe-backend \
  --file native_plugin,'plugins/hook.dll'=MZ_WINDOWS_DLL \
  -o "$B/dll.mfb" > /dev/null
"$BIN" make-bundle --id old-spawner --ver 0.1.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework ue4ss-ps5 \
  --file lua_mod,'ue4ss/Mods/Old/main.lua'=OLD_MOD \
  -o "$B/old.mfb" > /dev/null
"$BIN" make-bundle --id needs-spawner --ver 1.0.0 --prio 200 --title PPSA33286 \
  --engine unreal --framework ue4ss-ps5 --requires old-spawner=2.0.0 \
  --file lua_mod,'ue4ss/Mods/Needs/main.lua'=NEEDS \
  -o "$B/needs.mfb" > /dev/null
"$BIN" make-bundle --id papyrus-script-mod --ver 1.0.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework papyrus-vm \
  --file script,'Scripts/quest.pex'=PEX_BYTES \
  -o "$B/papyrus.mfb" > /dev/null
M register-mod --id orphan-dependent --title PPSA33286 --bundle "$B/orphan.mfb" > /dev/null 2>&1
M register-mod --id windows-dll-mod --title PPSA33286 --bundle "$B/dll.mfb" > /dev/null 2>&1
M register-mod --id old-spawner --title PPSA33286 --bundle "$B/old.mfb" > /dev/null 2>&1
M register-mod --id needs-spawner --title PPSA33286 --bundle "$B/needs.mfb" > /dev/null 2>&1
M register-mod --id papyrus-script-mod --title PPSA33286 --bundle "$B/papyrus.mfb" > /dev/null 2>&1

M prepare --title PPSA33286 --mods orphan-dependent > "$SB/01.out" 2>&1
RC=$?
check "E1 missing-mod dependency refuses (exit 3)" $([ $RC -eq 3 ]; echo $?)
has "E2 specific reason names the missing mod and version" "blocker requirement-unavailable mod 'orphan-dependent' requires mod 'missing-mod' >= 1.0.0 which is not selected" "$SB/01.out"
M install --title PPSA33286 --mods orphan-dependent > "$SB/02.out" 2>&1
RC=$?
check "E3 install of unmet dependency refused" $([ $RC -eq 3 ]; echo $?)
[ ! -e "$SB/roots/PPSA33286" ]
check "E4 nothing installed for refused mod" $?

M prepare --title PPSA33286 --mods windows-dll-mod > "$SB/03.out" 2>&1
has "E5 unknown runtime capability named specifically" "blocker requirement-unavailable framework 'windows-pe-backend' required by mod 'windows-dll-mod': no runtime component provides this capability on PS5" "$SB/03.out"
M install --title PPSA33286 --mods windows-dll-mod > "$SB/04.out" 2>&1
RC=$?
check "E6 install refused for unknown capability" $([ $RC -eq 3 ]; echo $?)

M prepare --title PPSA33286 --mods papyrus-script-mod > "$SB/05.out" 2>&1
has "E7 planned component refusal is specific" "runtime component is planned (not ported)" "$SB/05.out"

M prepare --title PPSA33286 --mods needs-spawner,old-spawner > "$SB/06.out" 2>&1
RC=$?
check "E8 too-old dependency refuses (exit 3)" $([ $RC -eq 3 ]; echo $?)
has "E9 version shortfall named" "requires 'old-spawner' >= 2.0.0 but selected version is 0.1.0" "$SB/06.out"

###############################################################################
# Scenario F: FOMOD-style variant choices
###############################################################################
newsbox F
B="$SB/bundles"
"$BIN" make-bundle --id texture-pack --ver 1.0.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework pak-loader \
  --file pak,'mods/~mods/armor.pak'=TEX_2K \
  -o "$B/tex2k.mfb" > /dev/null
"$BIN" make-bundle --id texture-pack --ver 1.0.0 --prio 100 --title PPSA33286 \
  --engine unreal --framework pak-loader \
  --file pak,'mods/~mods/armor.pak'=TEX_4K \
  -o "$B/tex4k.mfb" > /dev/null
"$BIN" make-bundle --id quest-expansion --ver 2.0.0 --prio 120 --title PPSA33286 \
  --engine unreal --framework ue4ss-ps5 \
  --file lua_mod,'ue4ss/Mods/Quest/main.lua'=QUEST_FULL \
  -o "$B/quest_full.mfb" > /dev/null
"$BIN" make-bundle --id quest-expansion --ver 2.0.0 --prio 120 --title PPSA33286 \
  --engine unreal --framework ue4ss-ps5 \
  --file lua_mod,'ue4ss/Mods/Quest/main.lua'=QUEST_LITE \
  -o "$B/quest_lite.mfb" > /dev/null
M register-mod --id texture-pack --title PPSA33286 \
  --variant Resolution/2K="$B/tex2k.mfb" --variant Resolution/4K="$B/tex4k.mfb" \
  --defaults Resolution=2K > "$SB/00.out" 2>&1
has "F1 variant mod registered as downloaded" "state=downloaded variants=2" "$SB/00.out"
M register-mod --id quest-expansion --title PPSA33286 \
  --variant Install/Lite="$B/quest_lite.mfb" --variant Install/Full="$B/quest_full.mfb" \
  > /dev/null 2>&1

M prepare --title PPSA33286 --mods texture-pack > "$SB/01.out" 2>&1
RC=$?
check "F2 default variant chosen without --choices (exit 0)" $([ $RC -eq 0 ]; echo $?)
has "F3 default choice visible in plan" "Resolution=2K (default)" "$SB/01.out"
M install --title PPSA33286 --mods texture-pack > /dev/null 2>&1
has "F4 default variant content installed" "TEX_2K" "$SB/roots/PPSA33286/mods/~mods/armor.pak"

M install --title PPSA33286 --mods texture-pack --choices Resolution=4K > "$SB/02.out" 2>&1
RC=$?
check "F5 explicit choice install succeeds" $([ $RC -eq 0 ]; echo $?)
has "F6 chosen variant content installed" "TEX_4K" "$SB/roots/PPSA33286/mods/~mods/armor.pak"
[ "$(cat "$SB/roots/PPSA33286/mods/~mods/armor.pak")" = "TEX_4K" ]
check "F7 exactly the chosen variant, not a merge" $?

M prepare --title PPSA33286 --mods texture-pack --choices Resolution=8K > "$SB/03.out" 2>&1
RC=$?
check "F8 invalid choice rejected (exit 2)" $([ $RC -eq 2 ]; echo $?)
has "F9 invalid choice names the valid options" "error fomod_invalid_choice mod 'texture-pack' group 'Resolution': '8K' is not an option (valid: 2K,4K)" "$SB/03.out"

M prepare --title PPSA33286 --mods quest-expansion > "$SB/04.out" 2>&1
RC=$?
check "F10 group without default demands a choice (exit 4)" $([ $RC -eq 4 ]; echo $?)
has "F11 needs_choices lists the open group" "needs_choices mod 'quest-expansion' group 'Install' has no default" "$SB/04.out"

###############################################################################
# Scenario G: uninstall (removal and removal-to-empty)
###############################################################################
newsbox G
cp -R "$TMP/A/bundles/." "$SB/bundles/"
M register-mod --id item-spawner --title PPSA33286 --bundle "$SB/bundles/spawner.mfb" > /dev/null 2>&1
M register-mod --id hd-skin --title PPSA33286 --bundle "$SB/bundles/skin.mfb" > /dev/null 2>&1
M register-mod --id low-priority-mod --title PPSA33286 --bundle "$SB/bundles/low.mfb" > /dev/null 2>&1
M install --title PPSA33286 --mods item-spawner,hd-skin,low-priority-mod > /dev/null 2>&1
has "G1 conflict winner present before uninstall" "B_SKIN_CONFIG_WINS" "$SB/roots/PPSA33286/ue4ss/Mods/Shared/config.txt"
M install --title PPSA33286 --remove hd-skin > "$SB/01.out" 2>&1
RC=$?
check "G2 uninstall transaction succeeds" $([ $RC -eq 0 ]; echo $?)
has  "G3 remaining mod wins the old conflict again" "A_ITEM_CONFIG" "$SB/roots/PPSA33286/ue4ss/Mods/Shared/config.txt"
[ ! -e "$SB/roots/PPSA33286/mods/~mods/Skin_P.pak" ]
check "G4 removed mod's files gone" $?
M records > "$SB/02.out" 2>&1
has  "G5 removed mod drops back to prepared" "mod hd-skin PPSA33286 prepared" "$SB/02.out"
has  "G6 survivors stay installed" "mod item-spawner PPSA33286 installed" "$SB/02.out"
has  "G7 generation advanced" "generation=2" "$SB/02.out"
M install --title PPSA33286 --remove item-spawner,low-priority-mod > "$SB/03.out" 2>&1
RC=$?
check "G8 uninstall-to-empty succeeds" $([ $RC -eq 0 ]; echo $?)
[ -f "$SB/roots/PPSA33286/loadlist.txt" ] && [ -f "$SB/roots/PPSA33286/.modforge-root" ] \
  && [ -z "$(find "$SB/roots/PPSA33286" -type f ! -name loadlist.txt ! -name .modforge-root)" ]
check "G9 empty root keeps only marker + load list" $?
M simulate-runtime --title PPSA33286 --rt-state mods-started > "$SB/04.out" 2>&1
has "G10 empty root still activates cleanly" "activation-check ok title=PPSA33286 mods=0" "$SB/04.out"

###############################################################################
# Scenario H: ownership guard
###############################################################################
newsbox H
mkdir -p "$SB/roots/PPSA99999"
echo "precious save data" > "$SB/roots/PPSA99999/saves.dat"
"$BIN" make-bundle --id stray --ver 1.0.0 --prio 100 --title PPSA99999 \
  --engine unreal --framework pak-loader \
  --file pak,'mods/~mods/x.pak'=X \
  -o "$SB/bundles/../stray.mfb" > /dev/null
M register-mod --id stray --title PPSA99999 --bundle "$SB/stray.mfb" > /dev/null 2>&1
M install --title PPSA99999 --mods stray > "$SB/01.out" 2>&1
RC=$?
check "H1 unmanaged root refused (exit 2)" $([ $RC -eq 2 ]; echo $?)
has "H2 refusal is specific about the marker" "refusing to replace unmanaged mod root" "$SB/01.out"
has "H3 foreign file untouched" "precious save data" "$SB/roots/PPSA99999/saves.dat"

###############################################################################
echo
echo "RESULT pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
