#!/bin/bash
# test_modmanager.sh -- OFFLINE end-to-end test of modmanager.py.
#
# Agent GG, 2026-10-08-l25-prep/mod-manager. Battery-mode law: NO console
# contact. The reachability probe is exercised only against a guaranteed-dead
# local address (127.0.0.1:9, connection refused instantly on loopback), and
# the no-flag --apply refusal happens before any network call at all.
#
# Flow (T1..T12):
#   T1  --apply without --i-know-console-is-on -> refused (exit 2, no net)
#   T2  --apply with the flag but console down (127.0.0.1:9) -> refused
#   T3  install LineTraceMod (mod-ladder TREE-B) with --enable
#       - staged copy sha == TREE-B original (never-edit-mods law)
#       - pre-flight verdict recorded (LOADS-CLEAN when lua5.5 present)
#   T4  keymap regen includes the mod (F3 <- LineTraceMod row present)
#   T5  loadlist synthesis byte-exact vs fixture (447 B base + 2 rows)
#   T6  audit clean (exit 0)
#   T7  install CheatManagerEnablerMod --enable; synthesis has BOTH mods
#   T8  disable LineTraceMod -> its rows gone, other mod's rows intact
#   T9  uninstall LineTraceMod -> store dir + manifest entry gone;
#       synthesis == base + CheatManager rows (byte-exact, others untouched)
#   T10 tamper with a staged file -> audit flags violation (exit 2)
#   T11 restore tampered file (reinstall) -> audit clean again
#   T12 keymap determinism: regen with no store mods == the pre-install
#       bytes recorded in T4's baseline snapshot (disable-everything path)
#   T13 deploy dry-run smoke; T14 flat zzdiag install + row arithmetic;
#   T15 deploy shared-key conflict refusal + --allow-cofire
#   T16 end-to-end pak-mod install (agent HK): synthetic utoc/ucas trio
#       via the OVERLAY BUILDER'S OWN fixture functions -> staged
#       byte-exact, overlay admission dry-run vs --game-dump-root, builder
#       plan captured into the install receipt, ZERO loadlist rows for
#       asset files, dump mirror untouched by dry-runs, unified plan doc
#   T17 keybind Lua mod composed with the overlay in ONE deploy: keymap
#       regen + --check after the state change, pad row drift re-pinned,
#       deploy-plan.json carries uploads + overlay + rollback refs,
#       audit clean with the overlay verify section
#   T18 package-ID conflict end-to-end: conflicting trio installs (WARN +
#       manifest REFUSED record) -> deploy REFUSED naming
#       mod_conflict_rejected + the --allow-overlay-conflict remedy BEFORE
#       upload planning -> later-wins composes both -> disable recovers

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../.." && pwd)"
MM="$HERE/modmanager.py"
TREEB="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods"
LUA_BIN="${LUA:-lua5.5}"

PASS=0; FAIL=0
ok()   { PASS=$((PASS+1)); echo "  PASS: $1"; }
bad()  { FAIL=$((FAIL+1)); echo "  FAIL: $1"; }
check(){ if [ "$1" = "0" ]; then ok "$2"; else bad "$2"; fi; }

SB="$(mktemp -d /tmp/mm-test.XXXXXX)"
ROOT="$SB/root"
mkdir -p "$ROOT"
trap 'rm -rf "$SB"' EXIT

echo "sandbox: $SB"
echo "lua: $LUA_BIN ($([ -x "$(command -v "$LUA_BIN" 2>/dev/null || true)" ] && echo present || echo MISSING))"

# ---- fixtures -------------------------------------------------------------
# Documented base composition (TEST-PLAN.md; l23 receipt: 12 lines, 447 B).
# mk_fixture <pad-size> [extra row lines...] -> byte-exact expected loadlist
mk_fixture() {
  local padsize="$1"; shift
  printf '%s\n' \
    'modforge-loadlist	1' \
    'title	PPSA33286' \
    'engine	unreal' \
    'framework	ue4ss-ps5' \
    'mod	TownfallItemSpawner	1.0.0' \
    'file	config	ue4ss/UE4SS-settings.ini	1947' \
    'file	config	ue4ss/MemberVariableLayout.ini	588' \
    'file	config	ue4ss/VTableLayout.ini	216' \
    "file	config	ue4ss/PS5Pad.ini	$padsize" \
    'file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415' \
    'file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0' \
    'file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	9962' \
    "$@"
}
mk_base_fixture() { mk_fixture 3718; }
mk_base_fixture > "$SB/fixture-base.txt"
# base + LineTraceMod rows (main.lua 2642 B per mod-ladder/MANIFEST.md #1);
# pad size 1728 = FV-deterministic output for the {spawner, LineTraceMod}
# set (key-identity hashing; re-derive only if keymap-gen.py changes)
mk_fixture 1728 \
  'file	lua_mod	ue4ss/Mods/LineTraceMod/Scripts/main.lua	2642' \
  'file	lua_mod	ue4ss/Mods/LineTraceMod/enabled.txt	0' \
  > "$SB/fixture-linetrace.txt"

# sanity: the base fixture itself must be 447 B (receipt-verified shape)
[ "$(wc -c < "$SB/fixture-base.txt" | tr -d ' ')" = "447" ] \
  && ok "base fixture is 447 B (matches l23 receipt)" \
  || bad "base fixture is NOT 447 B -- fixture wrong"

# ---- T1: --apply refused without the acknowledgment flag (no network) ----
echo "== T1: --apply without --i-know-console-is-on =="
OUT="$("$MM" --root "$ROOT" deploy --apply 2>&1)"; RC=$?
echo "$OUT" | grep -q "i-know-console-is-on" && ok "refusal names the flag" || bad "refusal message wrong"
[ "$RC" = "2" ] && ok "exit 2 (refused, rc=$RC)" || bad "expected exit 2, got $RC"

# ---- T2: --apply with flag, console down (dead loopback address) --------
echo "== T2: --apply with flag, console unreachable (127.0.0.1:9) =="
OUT="$("$MM" --root "$ROOT" deploy --apply --i-know-console-is-on \
        --console-ip 127.0.0.1 --http-port 9 2>&1)"; RC=$?
echo "$OUT" | grep -q "reachability" && ok "refusal names the reachability probe" || bad "refusal message wrong"
[ "$RC" = "2" ] && ok "exit 2 (refused, rc=$RC)" || bad "expected exit 2, got $RC"

# ---- baseline keymap with an empty store (for T12 determinism) ----------
echo "== T3a: baseline keymap regen with empty store =="
"$MM" --root "$ROOT" keymap > "$SB/t3a.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "keymap regen clean (rc=0)" || { bad "keymap regen rc=$RC"; sed -n '1,20p' "$SB/t3a.log"; }
cp "$ROOT/staging/PS5Pad.ini" "$SB/baseline-pad.ini"
PAD_A=$(shasum -a 256 "$SB/baseline-pad.ini" | cut -d' ' -f1)
echo "  baseline pad sha256: $PAD_A"

# ---- T3: install LineTraceMod from TREE-B -------------------------------
echo "== T3: install LineTraceMod (TREE-B) --enable =="
ORIG="$TREEB/LineTraceMod/Scripts/main.lua"
ORIG_SHA=$(shasum -a 256 "$ORIG" | cut -d' ' -f1)
"$MM" --root "$ROOT" install "$TREEB/LineTraceMod" --enable > "$SB/t3.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "install rc=0" || { bad "install rc=$RC"; sed -n '1,30p' "$SB/t3.log"; }
STAGED_SHA=$(shasum -a 256 "$ROOT/store/LineTraceMod/Scripts/main.lua" | cut -d' ' -f1)
[ "$STAGED_SHA" = "$ORIG_SHA" ] \
  && ok "never-edit-mods: staged sha == TREE-B original sha" \
  || bad "staged sha $STAGED_SHA != original $ORIG_SHA"
[ -f "$ROOT/store/manifest.json" ] && ok "manifest.json written" || bad "manifest.json missing"
grep -q '"TownfallItemSpawner"' "$ROOT/store/manifest.json" 2>/dev/null \
  && bad "manifest should not contain resident spawner as installed" \
  || ok "resident spawner not (incorrectly) installed into the store"
if command -v "$LUA_BIN" >/dev/null 2>&1; then
  grep -q '"verdict": "LOADS-CLEAN"' "$ROOT/store/manifest.json" \
    && ok "pre-flight verdict LOADS-CLEAN recorded" \
    || { bad "pre-flight verdict not LOADS-CLEAN"; grep -n 'verdict' "$ROOT/store/manifest.json"; }
else
  grep -q '"verdict": "UNAVAILABLE"' "$ROOT/store/manifest.json" \
    && ok "pre-flight UNAVAILABLE recorded (no lua binary)" \
    || bad "pre-flight verdict missing"
fi

# ---- T4: keymap includes the mod -----------------------------------------
echo "== T4: keymap regen includes LineTraceMod =="
grep -q "F3 <- LineTraceMod" "$ROOT/staging/PS5Pad.ini" \
  && ok "generated pad lists 'F3 <- LineTraceMod'" \
  || bad "generated pad lacks the LineTraceMod F3 note"
grep -qE '^[A-Z_0-9+]+ = F3$' "$ROOT/staging/PS5Pad.ini" \
  && ok "an auto row maps some chord to F3" \
  || bad "no row maps to F3"

# ---- T5: loadlist synthesis byte-exact vs fixture ------------------------
echo "== T5: synthesized loadlist byte-exact vs fixture =="
cmp -s "$ROOT/staging/loadlist.txt" "$SB/fixture-linetrace.txt" \
  && ok "staging/loadlist.txt byte-identical to base+LineTrace fixture" \
  || { bad "synthesis differs from fixture"; diff "$SB/fixture-linetrace.txt" "$ROOT/staging/loadlist.txt" | head -10; }
PINNED=$(grep -c 'file	config	ue4ss/PS5Pad.ini	' "$ROOT/staging/loadlist.txt")
[ "$PINNED" = "1" ] && ok "exactly one re-pinned PS5Pad.ini row" || bad "pad row count=$PINNED"
NEWSIZE=$(grep 'file	config	ue4ss/PS5Pad.ini	' "$ROOT/staging/loadlist.txt" | cut -f4)
ACTSIZE=$(wc -c < "$ROOT/staging/PS5Pad.ini" | tr -d ' ')
[ "$NEWSIZE" = "$ACTSIZE" ] \
  && ok "pad row byte count == wc -c of regenerated pad ($ACTSIZE)" \
  || bad "pad row pins $NEWSIZE but pad is $ACTSIZE B"

# ---- T6: audit clean ------------------------------------------------------
echo "== T6: audit (offline) =="
"$MM" --root "$ROOT" audit > "$SB/t6.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "audit exit 0" || { bad "audit rc=$RC"; cat "$SB/t6.log"; }

# ---- T7: second mod -------------------------------------------------------
echo "== T7: install CheatManagerEnablerMod --enable =="
"$MM" --root "$ROOT" install "$TREEB/CheatManagerEnablerMod" --enable > "$SB/t7.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "install rc=0" || { bad "install rc=$RC"; sed -n '1,30p' "$SB/t7.log"; }
# pad size for the {spawner, LineTrace, Cheat} set is FV-deterministic but
# set-dependent (inputs-sha256 header) -- take it from the regenerated pad;
# the row==wc -c chain is separately enforced by audit + FV --check.
PAD_BOTH=$(wc -c < "$ROOT/staging/PS5Pad.ini" | tr -d ' ')
mk_fixture "$PAD_BOTH" \
  'file	lua_mod	ue4ss/Mods/LineTraceMod/Scripts/main.lua	2642' \
  'file	lua_mod	ue4ss/Mods/LineTraceMod/enabled.txt	0' \
  'file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua	1407' \
  'file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/enabled.txt	0' \
  > "$SB/fixture-both.txt"
cmp -s "$ROOT/staging/loadlist.txt" "$SB/fixture-both.txt" \
  && ok "synthesis byte-identical to base+both fixture" \
  || { bad "synthesis differs"; diff "$SB/fixture-both.txt" "$ROOT/staging/loadlist.txt" | head; }

# ---- T8: disable LineTraceMod --------------------------------------------
echo "== T8: disable LineTraceMod =="
"$MM" --root "$ROOT" disable LineTraceMod > "$SB/t8.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "disable rc=0" || bad "disable rc=$RC"
grep -q "LineTraceMod" "$ROOT/staging/loadlist.txt" \
  && bad "disabled mod rows still present" \
  || ok "LineTraceMod rows gone from synthesis"
grep -q "CheatManagerEnablerMod/Scripts/main.lua	1407" "$ROOT/staging/loadlist.txt" \
  && ok "OTHER mod rows untouched by the disable" \
  || bad "disable disturbed another mod's rows"
grep -q "TownfallItemSpawner/Scripts/main.lua	21415" "$ROOT/staging/loadlist.txt" \
  && ok "base rows untouched by the disable" \
  || bad "disable disturbed base rows"

# ---- T9: uninstall LineTraceMod ------------------------------------------
echo "== T9: uninstall LineTraceMod =="
"$MM" --root "$ROOT" uninstall LineTraceMod --yes > "$SB/t9.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "uninstall rc=0" || bad "uninstall rc=$RC"
[ -d "$ROOT/store/LineTraceMod" ] && bad "store dir still exists" || ok "store dir deleted"
grep -q '"LineTraceMod"' "$ROOT/store/manifest.json" \
  && bad "manifest still lists LineTraceMod" \
  || ok "manifest entry removed"
PAD_CHEAT=$(wc -c < "$ROOT/staging/PS5Pad.ini" | tr -d ' ')
mk_fixture "$PAD_CHEAT" \
  'file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua	1407' \
  'file	lua_mod	ue4ss/Mods/CheatManagerEnablerMod/enabled.txt	0' \
  > "$SB/fixture-cheatonly.txt"
cmp -s "$ROOT/staging/loadlist.txt" "$SB/fixture-cheatonly.txt" \
  && ok "synthesis byte-identical to base+CheatManager fixture" \
  || { bad "post-uninstall synthesis differs"; diff "$SB/fixture-cheatonly.txt" "$ROOT/staging/loadlist.txt" | head; }

# ---- T10: tamper detection ------------------------------------------------
echo "== T10: tamper with a staged file -> audit violation =="
printf -- '-- tampered\n' >> "$ROOT/store/CheatManagerEnablerMod/Scripts/main.lua"
"$MM" --root "$ROOT" audit > "$SB/t10.log" 2>&1; RC=$?
[ "$RC" = "2" ] && ok "audit exit 2 on store tampering" || bad "audit rc=$RC (expected 2)"
grep -q "DRIFTED" "$SB/t10.log" && ok "audit names the drifted file" || bad "no DRIFTED line"

# ---- T11: restore by reinstall -------------------------------------------
echo "== T11: reinstall restores a clean store =="
"$MM" --root "$ROOT" uninstall CheatManagerEnablerMod --yes > "$SB/t11.log" 2>&1
"$MM" --root "$ROOT" install "$TREEB/CheatManagerEnablerMod" --enable > "$SB/t11b.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "reinstall rc=0" || { bad "reinstall rc=$RC"; sed -n '1,20p' "$SB/t11b.log"; }
"$MM" --root "$ROOT" audit > "$SB/t11c.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "audit exit 0 after reinstall" || { bad "audit rc=$RC"; cat "$SB/t11c.log"; }
cmp -s "$ROOT/staging/loadlist.txt" "$SB/fixture-cheatonly.txt" \
  && ok "synthesis byte-exact again" \
  || bad "post-reinstall synthesis differs"

# ---- T12: keymap determinism (disable everything -> baseline bytes) ------
echo "== T12: keymap determinism =="
"$MM" --root "$ROOT" disable CheatManagerEnablerMod > "$SB/t12.log" 2>&1
PAD_B=$(shasum -a 256 "$ROOT/staging/PS5Pad.ini" | cut -d' ' -f1)
[ "$PAD_A" = "$PAD_B" ] \
  && ok "empty-set regen reproduces the pre-install pad byte-identically" \
  || bad "pad determinism broken: $PAD_A != $PAD_B"

# ---- deploy dry-run smoke (must be exit 0, zero network) -----------------
echo "== T13: deploy dry-run smoke =="
"$MM" --root "$ROOT" deploy > "$SB/t13.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy dry-run rc=0" || { bad "deploy dry-run rc=$RC"; cat "$SB/t13.log"; }
grep -q "DRY-RUN: no console writes" "$SB/t13.log" && ok "dry-run banner present" || bad "dry-run banner missing"
grep -q "loadlist.txt last" "$SB/t13.log" && ok "upload order law printed" || bad "order law not printed"
grep -q "REFUSED" "$SB/t13.log" && bad "unexpected refusal in dry-run" || ok "no refusal in dry-run"

# ---- T14: diag-style flat source (zzdiag shape) + row arithmetic ---------
echo "== T14: install zzdiag-v11 (flat dir) --name zzdiag --enable =="
ZZ="$WHAT/outputs/2026-10-08-l25-prep/zzdiag-v11"
"$MM" --root "$ROOT" install "$ZZ" --name zzdiag --enable > "$SB/t14.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "flat-dir install rc=0" || { bad "flat install rc=$RC"; sed -n '1,20p' "$SB/t14.log"; }
[ -f "$ROOT/store/zzdiag/Scripts/main.lua" ] \
  && ok "flat source staged at Scripts/main.lua (layout only)" \
  || bad "staged flat layout wrong"
grep -q "shared key: F7 fires for TownfallItemSpawner, zzdiag" "$SB/t14.log" \
  && ok "FV WARN surfaces at install (F7 shared with the spawner)" \
  || bad "no shared-key WARN at install"
ZB=$(wc -c < ../zzdiag-v11/main.lua | tr -d ' ')
grep -q "ue4ss/Mods/zzdiag/Scripts/main.lua	$ZB" "$ROOT/staging/loadlist.txt" \
  && ok "zzdiag main.lua row pins live size ($ZB B)" \
  || bad "zzdiag main row wrong/missing"
grep -q "ue4ss/Mods/zzdiag/enabled.txt	0" "$ROOT/staging/loadlist.txt" \
  && ok "zzdiag enabled.txt row pins 0 B" \
  || bad "zzdiag enabled row wrong/missing"
# row arithmetic from the v7 sidecar law: 447 base + 53 + 45 = 545 B
TOTSZ=$(wc -c < "$ROOT/staging/loadlist.txt" | tr -d ' ')
[ "$TOTSZ" = "545" ] && ok "synthesis total 545 B (matches zzdiag sidecar arithmetic)" \
  || bad "synthesis total $TOTSZ B (expected 545)"
TAIL2=$(tail -2 "$ROOT/staging/loadlist.txt")
echo "$TAIL2" | head -1 | grep -q "^file\tlua_mod\tue4ss/Mods/zzdiag/Scripts/main.lua\t$ZB$" \
  && echo "$TAIL2" | tail -1 | grep -q '^file	lua_mod	ue4ss/Mods/zzdiag/enabled.txt	0$' \
  && ok "zzdiag rows appended LAST (zzdiag-v7 LOADLIST-ROW.txt shape)" \
  || bad "zzdiag rows not in the documented position"

# ---- T15: deploy refuses on the shared-key conflict ----------------------
echo "== T15: deploy conflict refusal + --allow-cofire =="
"$MM" --root "$ROOT" deploy > "$SB/t15.log" 2>&1; RC=$?
[ "$RC" = "2" ] && ok "deploy REFUSED (rc=2) while the F7 conflict is unresolved" \
  || bad "deploy rc=$RC (expected refusal)"
grep -q "shared-key conflict" "$SB/t15.log" && ok "refusal explains the co-fire policy" \
  || bad "refusal message wrong"
grep -q "ue4ss/Mods/zzdiag/Scripts/main.lua" "$SB/t15.log" \
  && bad "refusal happened after the upload plan (gate ordering wrong)" \
  || ok "conflict gate fires before any upload planning"
"$MM" --root "$ROOT" deploy --allow-cofire > "$SB/t15b.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy --allow-cofire dry-run rc=0" || { bad "allow-cofire rc=$RC"; tail -5 "$SB/t15b.log"; }
grep -q "ue4ss/Mods/zzdiag/Scripts/main.lua" "$SB/t15b.log" \
  && ok "upload plan includes the diag mod files" \
  || bad "upload plan lacks zzdiag files"

# ---- T16..T18: end-to-end overlay wiring (agent HK, round-7) ------------
# Synthetic game dump + IoStore mod trios built with the OVERLAY BUILDER'S
# OWN fixture functions (test_overlay_builder.py synth_base/synth_mod; the
# manager subprocess-calls overlay_builder.py, never imports/edits it).
# Distinct salts keep per-mod added package ids unique; shared replace
# index 1 (package 0x1001) makes AlphaTex vs GammaDup a real conflict.
echo "== T16: end-to-end pak-mod install (synthetic utoc/ucas trio) =="
PAKO="$WHAT/outputs/2026-10-08-l25-prep/pak-overlay"
PYFIX="$SB/hk-overlay"
ROOT2="$SB/root2"
mkdir -p "$ROOT2"
python3 - "$PAKO" "$PYFIX" <<'PYEOF' > "$SB/t16fix.log" 2>&1
import os, sys
pak_overlay, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, pak_overlay)
import test_overlay_builder as tob   # fixture functions only
os.makedirs(out, exist_ok=True)
tob.synth_base(out)                             # <out>/paks/pakchunk0-ps5.*
tob.synth_mod(out, "AlphaTex", [1], salt=1)     # replaces package 0x1001
tob.synth_mod(out, "BetaTex", [2], salt=2)      # replaces package 0x1002
tob.synth_mod(out, "GammaDup", [1], salt=3)     # ALSO 0x1001 -> conflict
print("fixtures-ok")
PYEOF
grep -q "fixtures-ok" "$SB/t16fix.log" \
  && ok "synthetic dump + 3 mod trios built (builder fixture approach)" \
  || { bad "fixture build failed"; cat "$SB/t16fix.log"; }
cat > "$ROOT2/config.json" <<EOF
{
  "game_dump_root": "$PYFIX/paks",
  "overlay_index": "pakchunk0-ps5.utoc",
  "console_ip": "127.0.0.1",
  "http_port": 9
}
EOF
"$MM" --root "$ROOT2" install "$PYFIX/mods/AlphaTex" --enable > "$SB/t16.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "asset-mod install rc=0" || { bad "asset install rc=$RC"; sed -n '1,30p' "$SB/t16.log"; }
ORIG_UTOC_SHA=$(shasum -a 256 "$PYFIX/mods/AlphaTex/AlphaTex.utoc" | cut -d' ' -f1)
STAGED_UTOC_SHA=$(shasum -a 256 "$ROOT2/store/AlphaTex/AlphaTex.utoc" | cut -d' ' -f1)
[ "$ORIG_UTOC_SHA" = "$STAGED_UTOC_SHA" ] \
  && ok "never-edit-mods: staged utoc sha == original (binary assets)" \
  || bad "staged utoc sha drift: $STAGED_UTOC_SHA != $ORIG_UTOC_SHA"
grep -q '"verdict": "NOT-APPLICABLE"' "$ROOT2/store/manifest.json" \
  && ok "preflight NOT-APPLICABLE recorded (asset-only mod)" \
  || bad "asset preflight verdict wrong"
grep -q "overlay admission dry-run OK" "$SB/t16.log" \
  && ok "overlay admission dry-run ran at install" \
  || { bad "no overlay admission at install"; grep -n "overlay" "$SB/t16.log" | head -5; }
grep -q '"verdict": "PLANNED"' "$ROOT2/store/manifest.json" \
  && ok "manifest overlay verdict PLANNED" || bad "overlay verdict not PLANNED"
grep -q '"AlphaTex--AlphaTex"' "$ROOT2/store/manifest.json" \
  && ok "manifest records overlay mod id AlphaTex--AlphaTex" \
  || bad "overlay mod id missing from manifest"
INSTALL_RCPT=$(ls -d "$ROOT2"/receipts/install-* 2>/dev/null | tail -1)
[ -n "$INSTALL_RCPT" ] && [ -f "$INSTALL_RCPT/overlay-plan.json" ] \
  && ok "install receipt captures the builder plan (overlay-plan.json)" \
  || bad "no install receipt with overlay-plan.json"
grep -q '"result": "dry-run"' "$INSTALL_RCPT/overlay-plan.json" 2>/dev/null \
  && grep -q '"derived_index_sha256"' "$INSTALL_RCPT/overlay-plan.json" 2>/dev/null \
  && ok "builder plan/ledger JSON captured verbatim" \
  || bad "overlay-plan.json does not carry the builder dry-run plan"
# asset-only mod: ZERO loadlist rows (overlay surface, not ue4ss/Mods)
PAD0=$(wc -c < "$ROOT2/staging/PS5Pad.ini" | tr -d ' ')
mk_fixture "$PAD0" > "$SB/fixture-assetbase.txt"
cmp -s "$ROOT2/staging/loadlist.txt" "$SB/fixture-assetbase.txt" \
  && ok "loadlist has NO asset-mod rows (base composition byte-exact)" \
  || { bad "asset mod leaked into loadlist"; diff "$SB/fixture-assetbase.txt" "$ROOT2/staging/loadlist.txt" | head -5; }
# dry-runs never touch the dump mirror: only the 2 base files remain
NPAKS=$(ls -A "$PYFIX/paks" | wc -l | tr -d ' ')
[ "$NPAKS" = "2" ] && ok "game dump mirror untouched by planning (2 base files)" \
  || bad "dump mirror mutated during dry-runs ($NPAKS entries)"
"$MM" --root "$ROOT2" deploy > "$SB/t16deploy.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy dry-run rc=0 with overlay mods enabled" \
  || { bad "deploy dry-run rc=$RC"; tail -10 "$SB/t16deploy.log"; }
grep -q "unified deploy plan" "$SB/t16deploy.log" \
  && ok "ONE unified plan document printed" || bad "unified plan missing"
grep -q "overlay     status=planned" "$SB/t16deploy.log" \
  && ok "unified plan shows overlay status=planned" \
  || bad "unified plan overlay line wrong"
grep -q "ue4ss/Mods/AlphaTex" "$SB/t16deploy.log" && bad "asset files leaked into the upload plan" \
  || ok "upload plan contains no asset files (scope law)"
DEPLOY_PLAN=$(ls -t "$ROOT2"/receipts/deploy-dry-*/deploy-plan.json 2>/dev/null | head -1)
[ -n "$DEPLOY_PLAN" ] && grep -q '"overlay"' "$DEPLOY_PLAN" && grep -q '"rollback"' "$DEPLOY_PLAN" \
  && ok "deploy-plan.json saved (uploads + overlay + pad + loadlist + rollback refs)" \
  || bad "deploy-plan.json missing/incomplete"

# ---- T17: keybind Lua mod composed with the overlay ----------------------
echo "== T17: keybind Lua mod + overlay in one deploy =="
"$MM" --root "$ROOT2" install "$TREEB/LineTraceMod" --enable > "$SB/t17.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "LineTraceMod install rc=0 alongside asset mod" \
  || { bad "linetrace install rc=$RC"; sed -n '1,20p' "$SB/t17.log"; }
grep -q "F3 <- LineTraceMod" "$ROOT2/staging/PS5Pad.ini" \
  && ok "keymap includes the lua mod (F3 <- LineTraceMod)" \
  || bad "keymap lacks the lua mod binding"
grep -q "ue4ss/Mods/LineTraceMod/Scripts/main.lua	2642" "$ROOT2/staging/loadlist.txt" \
  && ok "lua mod loadlist rows present" || bad "lua mod rows missing"
NEWSZ=$(grep 'file	config	ue4ss/PS5Pad.ini	' "$ROOT2/staging/loadlist.txt" | cut -f4)
ACTSZ=$(wc -c < "$ROOT2/staging/PS5Pad.ini" | tr -d ' ')
[ "$NEWSZ" = "$ACTSZ" ] \
  && ok "pad row drift re-pinned to the fresh pad size ($ACTSZ B)" \
  || bad "pad row pins $NEWSZ but pad is $ACTSZ B"
grep -qE "FV --check after (install|enable|disable|uninstall)[^:]*: [0-9]+ violation" "$SB/t17.log" \
  && ok "FV --check runs after every install/enable/disable" \
  || bad "no --check after state change"
"$MM" --root "$ROOT2" deploy > "$SB/t17deploy.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy dry-run rc=0 (uploads + overlay in one document)" \
  || { bad "deploy rc=$RC"; tail -10 "$SB/t17deploy.log"; }
grep -q "ue4ss/Mods/LineTraceMod/Scripts/main.lua" "$SB/t17deploy.log" \
  && ok "upload plan includes the lua mod files" \
  || bad "upload plan lacks lua mod files"
DEPLOY_PLAN=$(ls -t "$ROOT2"/receipts/deploy-dry-*/deploy-plan.json 2>/dev/null | head -1)
grep -q '"AlphaTex--AlphaTex"' "$DEPLOY_PLAN" 2>/dev/null \
  && ok "deploy-plan.json overlay composes the asset mod" \
  || bad "deploy plan lacks the overlay mod"
"$MM" --root "$ROOT2" audit > "$SB/t17audit.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "audit exit 0 with lua+asset mods (overlay verify clean)" \
  || { bad "audit rc=$RC"; cat "$SB/t17audit.log"; }
grep -q "overlay verify:" "$SB/t17audit.log" \
  && ok "audit reports the overlay journal state" \
  || bad "audit lacks the overlay section"

# ---- T18: package-ID conflict refusal end-to-end -------------------------
echo "== T18: overlay package conflict blocks deploy (+ later-wins + recover) =="
"$MM" --root "$ROOT2" install "$PYFIX/mods/GammaDup" --enable > "$SB/t18.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "conflicting mod INSTALL succeeds (recorded, not fatal)" \
  || { bad "GammaDup install rc=$RC"; sed -n '1,20p' "$SB/t18.log"; }
grep -q "overlay builder REFUSED this composition" "$SB/t18.log" \
  && ok "install surfaces the builder refusal (WARN)" \
  || bad "install hid the overlay refusal"
grep -q '"verdict": "REFUSED"' "$ROOT2/store/manifest.json" \
  && grep -q '"code": "mod_conflict_rejected"' "$ROOT2/store/manifest.json" \
  && ok "manifest records REFUSED + mod_conflict_rejected" \
  || bad "manifest refusal record wrong"
"$MM" --root "$ROOT2" deploy > "$SB/t18deploy.log" 2>&1; RC=$?
[ "$RC" = "2" ] && ok "deploy REFUSED (rc=2) on the package-ID conflict" \
  || bad "deploy rc=$RC (expected 2)"
grep -q "mod_conflict_rejected" "$SB/t18deploy.log" \
  && ok "refusal names the builder conflict code" \
  || bad "refusal lacks the conflict code"
grep -q -- "--allow-overlay-conflict" "$SB/t18deploy.log" \
  && ok "refusal gives the actionable resolution flag" \
  || bad "refusal remedy missing"
grep -q "=== upload plan" "$SB/t18deploy.log" \
  && bad "refusal happened after upload planning (gate order wrong)" \
  || ok "overlay conflict gate fires before any upload planning"
"$MM" --root "$ROOT2" deploy --allow-overlay-conflict > "$SB/t18b.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy --allow-overlay-conflict dry-run rc=0 (later-wins)" \
  || { bad "allow-overlay-conflict rc=$RC"; tail -8 "$SB/t18b.log"; }
DEPLOY_PLAN=$(ls -t "$ROOT2"/receipts/deploy-dry-*/deploy-plan.json 2>/dev/null | head -1)
grep -q '"GammaDup--GammaDup"' "$DEPLOY_PLAN" 2>/dev/null && \
  grep -q '"AlphaTex--AlphaTex"' "$DEPLOY_PLAN" 2>/dev/null \
  && ok "later-wins plan composes BOTH conflicting mods" \
  || bad "later-wins plan missing a conflicting mod"
"$MM" --root "$ROOT2" disable GammaDup > "$SB/t18c.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "disable GammaDup rc=0" || bad "disable rc=$RC"
"$MM" --root "$ROOT2" deploy > "$SB/t18d.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "deploy clean again after disabling the conflicting mod" \
  || { bad "post-disable deploy rc=$RC"; tail -8 "$SB/t18d.log"; }
grep -q "GammaDup" "$SB/t18d.log" && bad "disabled conflict mod still referenced" \
  || ok "conflict mod fully out of the composition"
"$MM" --root "$ROOT2" audit > "$SB/t18audit.log" 2>&1; RC=$?
[ "$RC" = "0" ] && ok "audit exit 0 after recovery" \
  || { bad "audit rc=$RC after recovery"; cat "$SB/t18audit.log"; }

echo ""
echo "======================================"
echo "PASS=$PASS FAIL=$FAIL (sandbox $SB removed on exit)"
[ "$FAIL" = "0" ]
