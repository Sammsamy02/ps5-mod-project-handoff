#!/usr/bin/env bash
# rollback-earlyload.sh — restore the pristine game-root eboot.bin after a
# deploy-earlyload.sh stage (EARLY-LOAD-DESIGN.md §7 rollback lane).
#
# Prepared by worker-eboot-patch 2026-10-06. NEVER EXECUTED by the preparing
# worker (no console contact). Hash-gated like the deploy script; any mismatch
# STOPS.
#
# What it does:
#   0. local gate  — pristine base file must hash afe0a2a5… exactly
#   1. observe     — RETR current console eboot.bin, record what is staged
#                    (informational; works from any prior state)
#   2. restore     — upload the pristine base → eboot.bin
#   3. verify      — readback sha must equal afe0a2a5… byte-exact
#   4. --remove-prx (optional flag): also delete prx/mb.prx and
#      sce_module/mb.prx via FTP DELE and verify deletion by LIST.
#      Default does NOT delete them: mb.prx is inert once the eboot no longer
#      DT_NEEDEDs it (design §7: "mb.prx alone is inert once the eboot is
#      restored — no residual risk").
#
# Usage:
#   ./rollback-earlyload.sh                # restore eboot only (safe default)
#   ./rollback-earlyload.sh --remove-prx   # also delete the staged mb.prx files
#   ./rollback-earlyload.sh --dry-run      # print the plan, touch nothing
set -uo pipefail

REMOVE_PRX=0
DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        --remove-prx) REMOVE_PRX=1 ;;
        --dry-run)    DRY_RUN=1 ;;
        *) echo "usage: $0 [--remove-prx] [--dry-run]" >&2; exit 2 ;;
    esac
done

# ---------------------------------------------------------------- constants
FTP="ftp://ps5-host-not-configured.invalid:2121"
GAME_DIR="/data/homebrew/Silent Hill Townfall PPSA33286"
EBOOT_REMOTE="$GAME_DIR/eboot.bin"
PRX_REMOTE="$GAME_DIR/prx/mb.prx"
SCEMOD_REMOTE="$GAME_DIR/sce_module/mb.prx"

HERE="$(cd "$(dirname "$0")" && pwd)"
TEAM="$(dirname "$HERE")"
EBOOT_ORIG="$TEAM/worker-game-version/console-toplevel-eboot.bin"

SHA_ORIG="afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e"
BYTES_EBOOT=194241424
SHA_A1="3374b5ab18f4c5feb58b07a951d845e6462e40dcf7d08bc9b35f6f612b411960"
SHA_A2="cbb4ee4dd2766e649f597490cf77ea0efa69fc90cc463c7415606b4993255e34"

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVID="$HERE/rollback-evidence-$STAMP"
mkdir -p "$EVID"

log()  { printf '%s %s\n' "$(date -u +%H:%M:%S)" "$*"; }
die()  { log "STOP: $*"; log "evidence: $EVID"; exit 1; }
step() { log "== $* =="; }
sha_of() { /usr/bin/shasum -a 256 "$1" | awk '{print $1}'; }

url_quote() { python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$1"; }
ftp_get() {
    curl -sS --connect-timeout 10 --max-time 420 --ftp-pasv \
        -o "$2" "$FTP$(url_quote "$1")" 2>"$EVID/curl-retr-$(basename "$2").err"
}
ftp_put() {
    curl -sS --connect-timeout 10 --max-time 420 --ftp-pasv -T "$1" \
        "$FTP$(url_quote "$2")" 2>"$EVID/curl-stor-$(basename "$1").err"
}
ftp_list() {
    curl -sS --connect-timeout 10 --max-time 60 --ftp-pasv \
        "$FTP$(url_quote "$1")/" 2>/dev/null
}
ftp_dele() {
    curl -sS --connect-timeout 10 --max-time 60 --ftp-pasv \
        -Q "-DELE $1" "$FTP/" 2>"$EVID/curl-dele.err"
}

# ---------------------------------------------------------------- 0. local gate
step "0. local artifact gate"
[[ -f "$EBOOT_ORIG" ]] || die "missing pristine base $EBOOT_ORIG"
GOT="$(sha_of "$EBOOT_ORIG")"
[[ "$GOT" == "$SHA_ORIG" ]] || die "local pristine base sha mismatch: $GOT"
log "  ok pristine base $SHA_ORIG"
if (( DRY_RUN )); then log "dry run — plan only, no console contact"; exit 0; fi

# ---------------------------------------------------------------- 1. observe
step "1. observe current console eboot (informational)"
RB_NOW="$EVID/readback-before-rollback.bin"
if ftp_get "$EBOOT_REMOTE" "$RB_NOW"; then
    NOW_SHA="$(sha_of "$RB_NOW")"; NOW_SZ=$(stat -f%z "$RB_NOW")
    echo "$NOW_SHA $NOW_SZ" > "$EVID/before-rollback.sha"
    STATE="unknown"
    [[ "$NOW_SHA" == "$SHA_A1" ]] && STATE="A1-staged"
    [[ "$NOW_SHA" == "$SHA_A2" ]] && STATE="A2-staged"
    [[ "$NOW_SHA" == "$SHA_ORIG" ]] && STATE="already-pristine"
    log "  current console eboot: $NOW_SHA ($STATE, $NOW_SZ bytes)"
else
    log "  WARN: could not RETR current eboot (continuing — restore is base-locked)"
fi

# ---------------------------------------------------------------- 2. restore
step "2. restore pristine eboot.bin"
ftp_put "$EBOOT_ORIG" "$EBOOT_REMOTE" || die "restore upload failed"

# ---------------------------------------------------------------- 3. verify
step "3. readback verification"
RB_R="$EVID/readback-restored-eboot.bin"
ftp_get "$EBOOT_REMOTE" "$RB_R" || die "restore readback RETR failed"
RB_SHA="$(sha_of "$RB_R")"; RB_SZ=$(stat -f%z "$RB_R")
echo "$RB_SHA $RB_SZ" > "$EVID/restored-eboot.sha"
[[ "$RB_SZ" == "$BYTES_EBOOT" ]] || die "restored eboot size $RB_SZ != $BYTES_EBOOT"
[[ "$RB_SHA" == "$SHA_ORIG" ]] || die "restored eboot sha $RB_SHA != $SHA_ORIG"
log "  eboot.bin restored + byte-exact verified ($SHA_ORIG)"

# ---------------------------------------------------------------- 4. optional prx cleanup
if (( REMOVE_PRX )); then
    step "4. remove staged mb.prx files (--remove-prx)"
    for f in "$PRX_REMOTE" "$SCEMOD_REMOTE"; do
        ftp_dele "$f" || log "  WARN: DELE $f returned nonzero"
    done
    for d in "$GAME_DIR/prx" "$GAME_DIR/sce_module"; do
        if ftp_list "$d" | grep -q "mb.prx"; then
            log "  WARN: $d/mb.prx still listed after DELE — verify manually"
        else
            log "  $d/mb.prx removed (LIST-verified)"
        fi
    done
else
    step "4. mb.prx files left in place (inert without the patched eboot)"
    log "  pass --remove-prx to also delete prx/mb.prx and sce_module/mb.prx"
fi

cat > "$EVID/rollback-summary.txt" <<EOF
restored eboot sha256: $SHA_ORIG ($BYTES_EBOOT bytes)
readback: byte-exact PASS
mb.prx removal: $([[ $REMOVE_PRX -eq 1 ]] && echo performed || echo skipped)
timestamp utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF
log "ROLLBACK COMPLETE — evidence in $EVID"
