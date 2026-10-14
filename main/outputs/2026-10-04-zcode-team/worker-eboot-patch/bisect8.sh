#!/usr/bin/env bash
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TEAM="$(dirname "$HERE")"
EBOOT_A1="$TEAM/worker-early-load/eboot-earlyload-A1.bin"
EBOOT_A2="$TEAM/worker-early-load/eboot-earlyload-A2.bin"
EBOOT_ORIG="$TEAM/worker-game-version/console-toplevel-eboot.bin"
MB_PRX="$HERE/mb.prx"
MB_PLAIN="$HERE/mb-plain.prx"
SHA_A1="3374b5ab18f4c5feb58b07a951d845e6462e40dcf7d08bc9b35f6f612b411960"
SHA_A2="cbb4ee4dd2766e649f597490cf77ea0efa69fc90cc463c7415606b4993255e34"
SHA_ORIG="afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e"
SHA_MB="3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
SHA_MB_VIEW="133db40cba5a3008cecd33f724cdb19b3bac867f5e006faa9337363451ba5d5d"
FTP="ftp://ps5-host-not-configured.invalid:2121"
GAME_DIR="/data/homebrew/Silent Hill Townfall PPSA33286"
GAME_DIR_URL="/data/homebrew/Silent%20Hill%20Townfall%20PPSA33286"
EBOOT_REMOTE="$GAME_DIR/eboot.bin"
BAK_REMOTE="$GAME_DIR/eboot.bin.pre-earlyload.bak"
PRX_REMOTE="$GAME_DIR/prx/mb.prx"
SCEMOD_REMOTE="$GAME_DIR/sce_module/mb.prx"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
EVID="$HERE/deploy-evidence-$STAMP"
mkdir -p "$EVID"
url_quote() { python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$1"; }
ftp_get() {
    curl -sS --connect-timeout 10 --max-time 420 --ftp-pasv \
        -o "$2" "$FTP$(url_quote "$1")" 2>"$EVID/curl-retr-$(basename "$2").err"
}
if [[ "$VARIANT" == "A1" ]]; then
    EBOOT_LOCAL="$EBOOT_A1"; SHA_EBOOT="$SHA_A1"
else
    EBOOT_LOCAL="$EBOOT_A2"; SHA_EBOOT="$SHA_A2"
fi
log()  { printf '%s %s\n' "$(date -u +%H:%M:%S)" "$*"; }
die()  { log "STOP: $*"; exit 1; }
step() { log "== $* =="; }
sha_of() { /usr/bin/shasum -a 256 "$1" | awk '{print $1}'; }
step "0. local artifact gate"
for f in "$EBOOT_A1:$SHA_A1" "$EBOOT_A2:$SHA_A2" "$EBOOT_ORIG:$SHA_ORIG" \
         "$MB_PRX:$SHA_MB" "$MB_PLAIN:$SHA_MB_VIEW"; do
    path="${f%%:*}"; want="${f##*:}"
    [[ -f "$path" ]] || die "missing local artifact $path"
    got="$(sha_of "$path")"
    [[ "$got" == "$want" ]] || die "local sha mismatch $path"
    log "  ok $(basename "$path") $want"
done
