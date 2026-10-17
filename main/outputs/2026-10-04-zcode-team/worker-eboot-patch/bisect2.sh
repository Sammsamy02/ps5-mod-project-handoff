#!/usr/bin/env bash
# deploy-earlyload.sh — controller-run deployment of the DT_NEEDED early-load
# patch (worker-early-load design §7 staging ladder, Boot 1).
#
# Prepared by worker-eboot-patch 2026-10-06. THIS SCRIPT WAS NEVER EXECUTED:
# no console contact was made by the preparing worker. Every network step is
# gated on an expected hash; any mismatch STOPS the run (no blind retries —
# reconcile first, per standing dispatch law).
#
# What it does, in order:
#   0. local artifact gate   — sha256 of every input file before any network
#   1. pre-flight (read-only)— LIST game root; RETR root eboot.bin and verify
#                              it is EXACTLY the pinned base (afe0a2a5…);
#                              verify prx/ and sce_module/ exist
#   2. backup                — if eboot.bin.pre-earlyload.bak is absent on the
#                              console, upload the verified pristine base as
#                              the backup, readback-verify it (once ever)
#   3. upload eboot variant  — A1 (default) or A2 → eboot.bin
#   4. upload mb.prx         — FSELF runtime → prx/mb.prx and sce_module/mb.prx
#   5. readback verification — eboot: byte-exact sha (plain-ELF RETR law).
#                              mb.prx: FSELF RETR may return either the raw
#                              container or the decrypted/stripped view
#                              (regression-ledger ftpsrv law); BOTH pinned
#                              candidates are accepted and the applied law is
#                              recorded.
#
# Usage:
#   ./deploy-earlyload.sh            # stage variant A1 (design Boot 1)
#   ./deploy-earlyload.sh A2         # stage variant A2 (design Boot 2/3)
#   ./deploy-earlyload.sh --dry-run  # print the plan, touch nothing
#
# Console prerequisites: ftpsrv anonymous FTP reachable at ps5-host-not-configured.invalid:2121,
# game NOT running, dump dir shadow-mounted as /app0 (mount.lnk). One deploy
# per boot — one variable per boot (EARLY-LOAD-DESIGN.md §7).
set -uo pipefail

VARIANT="A1"
DRY_RUN=1
[[ "${2:-}" == "--dry-run" || "${VARIANT}" == "--dry-run" ]] && DRY_RUN=1
[[ "$VARIANT" == "--dry-run" ]] && VARIANT="A1"
if [[ "$VARIANT" != "A1" && "$VARIANT" != "A2" ]]; then
    echo "usage: $0 [A1|A2] [--dry-run]" >&2; exit 2
fi

# ---------------------------------------------------------------- constants
FTP="ftp://ps5-host-not-configured.invalid:2121"
GAME_DIR="/data/homebrew/Silent Hill Townfall PPSA33286"
GAME_DIR_URL="/data/homebrew/Silent%20Hill%20Townfall%20PPSA33286"
EBOOT_REMOTE="$GAME_DIR/eboot.bin"                    # loader's boot target
BAK_REMOTE="$GAME_DIR/eboot.bin.pre-earlyload.bak"    # pristine backup (once)
PRX_REMOTE="$GAME_DIR/prx/mb.prx"                     # NEEDED search path (fmod precedent)
SCEMOD_REMOTE="$GAME_DIR/sce_module/mb.prx"           # belt-and-braces copy

HERE="$(cd "$(dirname "$0")" && pwd)"
TEAM="$(dirname "$HERE")"
EBOOT_A1="$TEAM/worker-early-load/eboot-earlyload-A1.bin"
EBOOT_A2="$TEAM/worker-earlyload/eboot-earlyload-A2.bin"
EBOOT_ORIG="$TEAM/worker-game-version/console-toplevel-eboot.bin"
MB_PRX="$HERE/mb.prx"
MB_PLAIN="$HERE/mb-plain.prx"          # readback view-hash candidate

SHA_ORIG="afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e"  # 194241424 B
SHA_A1="3374b5ab18f4c5feb58b07a951d845e6462e40dcf7d08bc9b35f6f612b411960"
SHA_A2="cbb4ee4dd2766e649f597490cf77ea0efa69fc90cc463c7415606b4993255e34"
SHA_MB="3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"    # 30151623 B
SHA_MB_VIEW="133db40cba5a3008cecd33f724cdb19b3bac867f5e006faa9337363451ba5d5d" # bare ELF view
BYTES_EBOOT=194241424
BYTES_MB=30151623

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVID="$HERE/deploy-evidence-$STAMP"
mkdir -p "$EVID"

if [[ "$VARIANT" == "A1" ]]; then
    EBOOT_LOCAL="$EBOOT_A1"; SHA_EBOOT="$SHA_A1"
else
    EBOOT_LOCAL="$EBOOT_A2"; SHA_EBOOT="$SHA_A2"
fi

# ---------------------------------------------------------------- helpers
log()  { printf '%s %s\n' "$(date -u +%H:%M:%S)" "$*"; }
die()  { log "STOP: $*"; log "evidence: $EVID"; exit 1; }

sha_of() { /usr/bin/shasum -a 256 "$1" | awk '{print $1}'; }

ftp_get() {  # ftp_get <remote-path> <local-out>  (path may contain spaces)
    local remote="$1" out="$2"
    local url="$FTP$(python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$remote")"
    curl -sS --connect-timeout 10 --max-time 420 --ftp-pasv -o "$out" "$url" \
        2>"$EVID/curl-retr-$(basename "$out").err"
}

ftp_put() {  # ftp_put <local-file> <remote-path>
    local localf="$1" remote="$2"
    local url="$FTP$(python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$remote")"
    curl -sS --connect-timeout 10 --max-time 420 --ftp-pasv -T "$localf" "$url" \
        2>"$EVID/curl-stor-$(basename "$localf").err"
}

ftp_list() {  # ftp_list <remote-dir>
    local dir="$1"
    local url="$FTP$(python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$dir")/"
    curl -sS --connect-timeout 10 --max-time 60 --ftp-pasv "$url" 2>/dev/null
}

step() { log "== $* =="; }

# ---------------------------------------------------------------- 0. local gate
step "0. local artifact gate"
for f in "$EBOOT_A1:$SHA_A1" "$EBOOT_A2:$SHA_A2" "$EBOOT_ORIG:$SHA_ORIG" \
         "$MB_PRX:$SHA_MB" "$MB_PLAIN:$SHA_MB_VIEW"; do
    path="${f%%:*}"; want="${f##*:}"
    [[ -f "$path" ]] || die "missing local artifact $path"
    got="$(sha_of "$path")"
    [[ "$got" == "$want" ]] || die "local sha mismatch $path: $got != $want"
    log "  ok $(basename "$path") $want"
done
log "variant selected: $VARIANT (sha $SHA_EBOOT)"
if (( DRY_RUN )); then
    log "dry run — plan only, no console contact"; exit 0
fi

# ---------------------------------------------------------------- 1. pre-flight
step "1. pre-flight (read-only)"
ROOT_LS="$EVID/ftp-list-game-root.txt"
ftp_list "$GAME_DIR" > "$ROOT_LS" || die "FTP LIST $GAME_DIR failed"
grep -q "eboot.bin" "$ROOT_LS" || die "no eboot.bin in game root listing"
grep -qE "(^| )prx$" "$ROOT_LS" || die "no prx/ dir in game root listing"
grep -qE "(^| )sce_module$" "$ROOT_LS" || die "no sce_module/ dir in game root listing"
log "  LIST ok: eboot.bin, prx/, sce_module/ present"

RB="$EVID/readback-preflight-eboot.bin"
ftp_get "$EBOOT_REMOTE" "$RB" || die "preflight RETR eboot.bin failed"
RB_SHA="$(sha_of "$RB")"; RB_SZ=$(stat -f%z "$RB")
echo "$RB_SHA $RB_SZ" > "$EVID/preflight-eboot.sha"
[[ "$RB_SZ" == "$BYTES_EBOOT" ]] || die "console eboot.bin size $RB_SZ != $BYTES_EBOOT"
[[ "$RB_SHA" == "$SHA_ORIG" ]] || \
    die "console eboot.bin is NOT the pinned base (got $RB_SHA, want $SHA_ORIG) — offsets must be re-derived; do not deploy"

# ---------------------------------------------------------------- 2. backup
step "2. backup of original eboot"
BAK_LS_LINE="$(grep -o "eboot.bin.pre-earlyload.bak" "$ROOT_LS" || true)"
if [[ -n "$BAK_LS_LINE" ]]; then
    RB_BAK="$EVID/readback-existing-bak.bin"
    ftp_get "$BAK_REMOTE" "$RB_BAK" || die "existing backup RETR failed"
    if [[ "$(sha_of "$RB_BAK")" == "$SHA_ORIG" ]]; then
        log "  existing remote backup verified (afe0a2a5…) — not overwriting"
    else
        die "existing remote backup has wrong sha — do not overwrite; reconcile"
    fi
else
    ftp_put "$EBOOT_ORIG" "$BAK_REMOTE" || die "backup upload failed"
    RB_BAK="$EVID/readback-new-bak.bin"
    ftp_get "$BAK_REMOTE" "$RB_BAK" || die "backup readback RETR failed"
    [[ "$(sha_of "$RB_BAK")" == "$SHA_ORIG" ]] || die "backup readback sha mismatch"
    log "  remote backup created + verified: $BAK_REMOTE (afe0a2a5…)"
fi

# ---------------------------------------------------------------- 3. eboot
step "3. upload patched eboot ($VARIANT)"
ftp_put "$EBOOT_LOCAL" "$EBOOT_REMOTE" || die "eboot upload failed"
RB_E="$EVID/readback-eboot-$VARIANT.bin"
ftp_get "$EBOOT_REMOTE" "$RB_E" || die "eboot readback RETR failed"
RB_E_SHA="$(sha_of "$RB_E")"; RB_E_SZ=$(stat -f%z "$RB_E")
echo "$RB_E_SHA $RB_E_SZ" > "$EVID/readback-eboot-$VARIANT.sha"
[[ "$RB_E_SZ" == "$BYTES_EBOOT" ]] || die "eboot readback size $RB_E_SZ != $BYTES_EBOOT"
[[ "$RB_E_SHA" == "$SHA_EBOOT" ]] || die "eboot readback sha mismatch ($RB_E_SHA != $SHA_EBOOT)"
log "  eboot.bin staged + byte-exact readback verified ($SHA_EBOOT)"

# ---------------------------------------------------------------- 4. mb.prx
step "4. upload mb.prx (prx/ + sce_module/)"
ftp_put "$MB_PRX" "$PRX_REMOTE" || die "mb.prx upload to prx/ failed"
ftp_put "$MB_PRX" "$SCEMOD_REMOTE" || die "mb.prx upload to sce_module/ failed"

# ---------------------------------------------------------------- 5. mb readback
step "5. mb.prx readback verification (raw-or-view law)"
MB_LAWS=""
for dest in "$PRX_REMOTE" "$SCEMOD_REMOTE"; do
    tag="$(basename "$(dirname "$dest")")"
    RB_M="$EVID/readback-mb-$tag.prx"
    ftp_get "$dest" "$RB_M" || die "mb.prx readback RETR ($tag) failed"
    RB_M_SHA="$(sha_of "$RB_M")"; RB_M_SZ=$(stat -f%z "$RB_M")
    echo "$RB_M_SHA $RB_M_SZ" > "$EVID/readback-mb-$tag.sha"
    LAW="none"
    if [[ "$RB_M_SHA" == "$SHA_MB" ]]; then
        LAW="raw-bytes"
    elif [[ "$RB_M_SHA" == "$SHA_MB_VIEW" ]]; then
        LAW="decrypted-view"
    fi
    [[ "$LAW" != "none" ]] || \
        die "mb.prx readback ($tag) matches NEITHER raw sha $SHA_MB NOR view sha $SHA_MB_VIEW (got $RB_M_SHA, $RB_M_SZ bytes) — reconcile before boot"
    log "  $tag/mb.prx verified ($LAW law, $RB_M_SHA)"
    MB_LAWS="$MB_LAWS$tag=$LAW "
done

# ---------------------------------------------------------------- summary
cat > "$EVID/deploy-summary.txt" <<EOF
variant:        $VARIANT
eboot sha256:   $SHA_EBOOT
mb.prx sha256:  $SHA_MB
backup:         $BAK_REMOTE ($SHA_ORIG)
eboot readback: byte-exact PASS
mb.prx readback laws: $MB_LAWS
timestamp utc:  $(date -u +%Y-%m-%dT%H:%M:%SZ)
next: boot the title once; acceptance = a module_start_entered line in
/data/townfall_runtime_aligned_entry.log timestamped BEFORE the earliest
engine log line (design sec 7). If boot aborts: run rollback-earlyload.sh.
EOF
log "DEPLOY COMPLETE — evidence in $EVID"
log "next: one boot; acceptance = our module_start log line strictly before first engine line"
