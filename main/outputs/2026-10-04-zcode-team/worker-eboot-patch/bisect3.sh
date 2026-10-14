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
for f in "$EBOOT_A1:$SHA_A1" "$EBOOT_A2:$SHA_A2" "$EBOOT_ORIG:$SHA_ORIG" \
         "$MB_PRX:$SHA_MB" "$MB_PLAIN:$SHA_MB_VIEW"; do
    path="${f%%:*}"
    if [[ -f "$path" ]]; then echo "ok $(basename "$path")"; else echo "MISSING $path"; fi
done
