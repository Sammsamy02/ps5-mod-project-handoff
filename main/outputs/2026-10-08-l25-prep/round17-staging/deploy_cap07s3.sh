#!/bin/bash
# deploy_cap07s3.sh -- integrator: the #7 step 2 game-paks source (round17-staging/cap07s3-gamepaks).
# The runtime lists <game>/<Project>/Content/Paks/~mods (launch K/L: the configured pakmods is not visible).
#   empty  : create an EMPTY ~mods folder (proves the listing route: expect "[PAKMOUNT] source=game-paks" then
#            "pre-call refuse: zero selected containers"). Works with any pak state.
#   leon   : put the unchanged Leon 2K trio in ~mods (full sha readback of all 3). Refuses unless the game paks are
#            PRISTINE (a #12 overlay would be shadowed by the mounted mod and spoil both tests).
#   remove : delete the trio (if present) and the ~mods folder. Game files are never touched.
# Refuses if the game runs. Evidence -> $2 (a new dir).
set -u
MODE="${1:?usage: deploy_cap07s3.sh <empty|leon|remove> <new evidence dir>}"
EV="${2:?usage: deploy_cap07s3.sh <empty|leon|remove> <new evidence dir>}"
C=ps5-host-not-configured.invalid
F="ftp://$C:2121"
PAKS="/data/homebrew/Silent Hill Townfall PPSA33286/townfall/content/paks"
PAKS_URL="$F$(printf '%s' "$PAKS" | sed 's/ /%20/g')"
PREP=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep
LEON=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted
SUMS="$PREP/evidence/cap07s2-pakmods-deploy-20261013T230337Z/SHA256SUMS.source"
die() { echo "FAIL: $*"; exit 1; }
mkdir -p "$EV" || die "mkdir"
[ "$(curl -s --max-time 8 "http://$C:8084/processes_list" | grep -c eboot)" = "0" ] || die "game running"
curl -s --max-time 20 "$PAKS_URL/" | awk '{print $NF}' | grep -vx "\.\|\.\." > "$EV/paks.list.before.txt"; [ "${PIPESTATUS[0]}" = "0" ] && [ -s "$EV/paks.list.before.txt" ] || die "paks listing"
HAVE=$(grep -cx '~mods' "$EV/paks.list.before.txt")
case "$MODE" in
  empty)
    if [ "$HAVE" = "0" ]; then curl -s --max-time 20 -Q "MKD $PAKS/~mods" "$PAKS_URL/" -o /dev/null || die "MKD"; fi
    curl -s --max-time 20 "$PAKS_URL/~mods/" | awk '{print $NF}' | grep -vx "\.\|\.\." > "$EV/tilde.list.txt"; [ "${PIPESTATUS[0]}" = "0" ] || die "~mods listing"
    [ "$(grep -c . "$EV/tilde.list.txt")" = "0" ] || die "~mods is not empty: $(tr '\n' ' ' < "$EV/tilde.list.txt")"
    echo "DEPLOY-CAP07S3 empty PASS (~mods exists, 0 entries)";;
  leon)
    (cd "$PREP/pak-overlay" && python3 -I paktx.py check --record records/townfall.record.json --ftp "$C:2121") > "$EV/paktx-check.txt" 2>&1
    grep -q PRISTINE "$EV/paktx-check.txt" || die "game paks are not PRISTINE (restore the #12 install first)"
    (cd "$LEON" && shasum -a 256 -c "$SUMS") > "$EV/source-check.txt" 2>&1 || die "local Leon trio sha"
    [ "$HAVE" = "0" ] && { curl -s --max-time 20 -Q "MKD $PAKS/~mods" "$PAKS_URL/" -o /dev/null || die "MKD"; }
    for n in zzz_Leon_Townfall_2K_P.pak zzz_Leon_Townfall_2K_P.utoc zzz_Leon_Townfall_2K_P.ucas; do
      curl -s --max-time 900 -T "$LEON/$n" "$PAKS_URL/~mods/$n" || die "upload $n"
    done
    : > "$EV/SHA256SUMS.readback"
    for n in zzz_Leon_Townfall_2K_P.pak zzz_Leon_Townfall_2K_P.utoc zzz_Leon_Townfall_2K_P.ucas; do
      s=$(curl -s --max-time 900 "$PAKS_URL/~mods/$n" | shasum -a 256 | cut -d' ' -f1)
      echo "$s  $n" >> "$EV/SHA256SUMS.readback"
    done
    cmp -s "$SUMS" "$EV/SHA256SUMS.readback" || die "readback sha differs (see $EV/SHA256SUMS.readback)"
    echo "DEPLOY-CAP07S3 leon PASS (3/3 full sha readback)";;
  remove)
    if [ "$HAVE" != "0" ]; then
      curl -s --max-time 20 "$PAKS_URL/~mods/" | awk '{print $NF}' | grep -vx "\.\|\.\." > "$EV/tilde.list.before.txt"; [ "${PIPESTATUS[0]}" = "0" ] || die "~mods listing"
      while IFS= read -r n; do
        [ -n "$n" ] || continue
        case "$n" in zzz_Leon_Townfall_2K_P.pak|zzz_Leon_Townfall_2K_P.utoc|zzz_Leon_Townfall_2K_P.ucas) ;;
          *) die "~mods holds an unexpected entry: $n (left untouched)";; esac
        curl -s --max-time 20 -Q "DELE $PAKS/~mods/$n" "$PAKS_URL/" -o /dev/null || die "DELE $n"
      done < "$EV/tilde.list.before.txt"
      curl -s --max-time 20 -Q "RMD $PAKS/~mods" "$PAKS_URL/" -o /dev/null || die "RMD"
    fi
    curl -s --max-time 20 "$PAKS_URL/" | awk '{print $NF}' | grep -vx "\.\|\.\." > "$EV/paks.list.after.txt"; [ "${PIPESTATUS[0]}" = "0" ] || die "paks listing"
    [ "$(grep -cx '~mods' "$EV/paks.list.after.txt")" = "0" ] || die "~mods still present"
    echo "DEPLOY-CAP07S3 remove PASS";;
  *) die "unknown mode $MODE";;
esac
