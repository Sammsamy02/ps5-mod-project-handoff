#!/bin/bash
# worker-e2e-payload-build — gated console payload build per CONFIG-ENTRY-DESIGN.md sec 8.
# Writes ONLY under worker-e2e-payload-build/. STOP (exit 2) on any gate mismatch.
#
# AMENDED GATE (documented, not bypassed): the console link emits a 16-byte
# .note.gnu.build-id descriptor (file offset 0x93D00, section .note.gnu.build-id
# at 0x93CF0) whose value is PER-LINK NONDETERMINISTIC — proven this session by
# relinking the IDENTICAL pristine inputs twice and observing exactly those 16
# bytes differ between MY OWN two links. Byte-equality with the D1 payload
# e3489297... is therefore impossible for any relink, including D1's own.
# Gate form: differing-byte set vs e3489297 must be EXACTLY that 16-byte note
# descriptor, and both images must be bit-identical with those 16 bytes zeroed.
set -euo pipefail

ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM=$ROOT/outputs/2026-10-04-zcode-team
ME=$TEAM/worker-e2e-payload-build
D1=$TEAM/worker-d1-chain-relink/composition
CE=$TEAM/worker-e2e-config-entry

PROSPERO=$ROOT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang
LLVM_NM=$ROOT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/llvm-nm
LLVM_READOBJ=$ROOT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-readobj
RESOURCE=$ROOT/.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21
NATIVE_MBED=$ROOT/outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405
LIBARCHIVE=$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/native/libarchive-build/.libs/libarchive.a
LIBLZMA=$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/native/xz-build/src/liblzma/.libs/liblzma.a
LIBZ=$ROOT/.superpowers/native-dependency-spike/native-zip-experiment-1/native/zlib-build/libz.a
LIBMCRYPTO=$NATIVE_MBED/libmbedcrypto.a
XSCRIPT=$D1/build/console/elf_x86_64.dynamic-only.x

# Same env the D1 composer set (composition_zb_console_inner.py line 542):
export LLVM_CONFIG="$ROOT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"

EXPECTED_PRISTINE_SRC=45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d
EXPECTED_OWNED_SRC=43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0
EXPECTED_D1_CORE_O=dafd4214856d269d9a8462a64df49ac8da4cf16e085af04dca409cd6c7821ac6
EXPECTED_D1_PAYLOAD=e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436
EXPECTED_HOST_O=51b881715d9a43f7c29886622f5d74becee5b38f02a93198f02549688c52ec40
EXPECTED_HOST_BIN=4659c24c4a4d829dc13a59813e7b137bf4d0efeaf0bca45b21e064b589eb2aa1
NOTE_DESC_OFF=605440   # 0x93D00: .note.gnu.build-id desc, 16 bytes (0x93CF0+16)

stop() { echo "GATE-STOP: $*" >&2; exit 2; }
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
chk() { local got="$2" want="$3" what="$1"; if [ "$got" != "$want" ]; then stop "$what sha mismatch: got $got want $want"; fi; echo "GATE-OK $what = $got"; }

# compare two ELFs: per file, locate .note.gnu.build-id via readobj; the ONLY
# allowed differing bytes are the two 16-byte note descriptors, and the images
# must be bit-identical with both descriptors zeroed.
bidcmp() { # $1=a $2=b $3=label
  python3 - "$1" "$2" "$LLVM_READOBJ" "$3" <<'PYEOF'
import sys,subprocess,re
def note_desc(p, ro):
    out=subprocess.run([ro,'--sections',p],capture_output=True,text=True).stdout
    m=re.search(r'Name: \.note\.gnu\.build-id.*?Offset: (0x[0-9A-Fa-f]+).*?Size: (\d+)', out, re.S)
    if not m: print(f"BID-FAIL {sys.argv[4]}: no .note.gnu.build-id in {p}"); sys.exit(1)
    off=int(m.group(1),16); size=int(m.group(2))
    if size!=32: print(f"BID-FAIL {sys.argv[4]}: unexpected note size {size} in {p}"); sys.exit(1)
    return off+16, hex(off+16)
a=open(sys.argv[1],'rb').read(); b=open(sys.argv[2],'rb').read()
oa,ha=note_desc(sys.argv[1],sys.argv[3]); ob,hb=note_desc(sys.argv[2],sys.argv[3])
label=sys.argv[4]
if len(a)!=len(b): print(f"BID-FAIL {label}: length {len(a)} != {len(b)}"); sys.exit(1)
d=[i for i in range(len(a)) if a[i]!=b[i]]
inside=all((oa<=i<oa+16) or (ob<=i<ob+16) for i in d)
za=bytearray(a); zb=bytearray(b); za[oa:oa+16]=b'\x00'*16; zb[ob:ob+16]=b'\x00'*16
identical = bytes(za)==bytes(zb)
print(f"BID {label}: diff_bytes={len(d)} all_in_note_desc={inside} (a@{ha} b@{hb}) zeroed_identical={identical} range={hex(min(d)) if d else '-'}..{hex(max(d)) if d else '-'}")
sys.exit(0 if (d and inside and identical and len(d)==16) else 1)
PYEOF
}

mkdir -p "$ME/receipts/gate-byte-repro-host" "$ME/receipts/gate-console-repro/pristine" \
         "$ME/receipts/gate-console-repro/pristine-link" "$ME/receipts/gate-console-repro/pristine-link2" \
         "$ME/build-r1" "$ME/build-r2"

echo "== Gate 8.1: patch apply =="
chk "pristine-base-src" "$(sha "$D1/vendored/modinst_core.c")" "$EXPECTED_PRISTINE_SRC"
chk "owned-src" "$(sha "$CE/owned/modinst_core.c")" "$EXPECTED_OWNED_SRC"
chk "my-patched-src" "$(sha "$ME/src/modinst_core.c")" "$EXPECTED_OWNED_SRC"
cmp "$ME/src/modinst_core.c" "$CE/owned/modinst_core.c" || stop "8.1: my patched src differs from owned"
{ echo "pristine $D1/vendored/modinst_core.c = $(sha "$D1/vendored/modinst_core.c") (expect $EXPECTED_PRISTINE_SRC)";
  echo "diff applied: $CE/patch/modinst_core-config-entry.diff (sha $(sha "$CE/patch/modinst_core-config-entry.diff")) via: (cd src && patch -p1 modinst_core.c < diff)";
  echo "result src/modinst_core.c = $(sha "$ME/src/modinst_core.c") (expect $EXPECTED_OWNED_SRC)";
  echo "cmp vs owned/modinst_core.c: BYTE-EXACT"; } > "$ME/receipts/gate-8.1-patch-apply.log"

console_compile() { # $1=src $2=outdir
  local SRC=$1 OUT=$2
  "$PROSPERO" -std=c11 -O2 -Wall -Wextra -resource-dir "$RESOURCE" -DMB_ARCHIVE_BUILTIN_XZ=1 \
    -I"$D1/vendored" -I"$TEAM/worker-d1-chain-relink/owned" -I"$D1/vendored/archive" \
    -I"$NATIVE_MBED/sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include" -I"$NATIVE_MBED/config" \
    -MD -MF "$OUT/modinst_core.d" -c "$SRC" -o "$OUT/modinst_core.ps5.o"
}
console_link() { # $1=core_o $2=outdir $3=mapname
  local CORE_O=$1 OUT=$2 MAP=$3
  "$PROSPERO" -resource-dir "$RESOURCE" \
    "$CORE_O" \
    "$D1/build/console/ps5_io.ps5.o" \
    "$D1/build/console/ps5_io_dir.ps5.o" \
    "$D1/build/console/ps5_prepare_bridge.ps5.o" \
    "$D1/build/console/modinst_archive_prepare.ps5.o" \
    "$D1/build/console/e2e_status.ps5.o" \
    "$D1/build/console/modforge_activation.ps5.o" \
    "$D1/build/console/intake.ps5.o" \
    "$D1/build/console/posix_compat.ps5.o" \
    "$D1/build/console/modinst_zip_bridge.ps5.o" \
    "$LIBARCHIVE" "$LIBLZMA" "$LIBZ" "$LIBMCRYPTO" \
    -Wl,--no-undefined,-z,now \
    -Wl,-T,"$XSCRIPT" \
    -Wl,-Map="$OUT/$MAP" \
    -o "$OUT/modbridge-installer.payload.elf"
}

for f in "$LIBARCHIVE" "$LIBLZMA" "$LIBZ" "$LIBMCRYPTO" "$XSCRIPT" \
         "$D1/build/console/ps5_io.ps5.o" "$D1/build/console/ps5_io_dir.ps5.o" \
         "$D1/build/console/ps5_prepare_bridge.ps5.o" "$D1/build/console/modinst_archive_prepare.ps5.o" \
         "$D1/build/console/e2e_status.ps5.o" "$D1/build/console/modforge_activation.ps5.o" \
         "$D1/build/console/intake.ps5.o" "$D1/build/console/posix_compat.ps5.o" \
         "$D1/build/console/modinst_zip_bridge.ps5.o"; do
  [ -f "$f" ] || stop "missing pinned link input: $f"
done
chk "D1-console-payload-baseline" "$(sha "$D1/build/console/modbridge-installer.payload.elf")" "$EXPECTED_D1_PAYLOAD"
chk "D1-console-core-o-baseline" "$(sha "$D1/build/console/modinst_core.ps5.o")" "$EXPECTED_D1_CORE_O"

echo "== Gate: host-test binary byte-reproduction (D1 host-build.json argv + config-entry host link) =="
H=$ME/receipts/gate-byte-repro-host
/usr/bin/clang -std=c11 -O2 -Wall -Wextra -DPS5IO_HOST=1 -DMI_HAS_DOWNLOAD=1 \
  -I"$D1/vendored" -I"$TEAM/worker-d1-chain-relink/owned" -I"$D1/vendored/archive" \
  -I"$ROOT/outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8/sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include" \
  -I"$ROOT/outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8/config" \
  -c "$ME/src/modinst_core.c" -o "$H/modinst_core_cfgentry.o" 2> "$H/compile-stderr.log"
chk "host-cfgentry-o" "$(sha "$H/modinst_core_cfgentry.o")" "$EXPECTED_HOST_O"
/usr/bin/clang "$H/modinst_core_cfgentry.o" \
  "$D1/build/host/ps5_io.o" "$D1/build/host/ps5_io_dir.o" "$D1/build/host/ps5_prepare_bridge.o" \
  "$D1/build/host/modinst_archive_prepare.o" "$D1/build/host/e2e_status.o" "$D1/build/host/modforge_activation.o" \
  "$D1/build/host/intake.o" "$D1/build/host/posix_compat.o" "$D1/build/host/file_sink.o" \
  "$D1/build/host/modinst_download.o" "$D1/build/host/mb_curl_stub_host.o" "$D1/build/host/modinst_zip_bridge.o" \
  "$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/host/libarchive-build/.libs/libarchive.a" \
  "$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/host/xz-build/src/liblzma/.libs/liblzma.a" \
  "$ROOT/.superpowers/native-dependency-spike/native-zip-experiment-1/host/zlib-build/libz.a" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8/libmbedcrypto.a" \
  -o "$H/modinst_composed_cfgentry" 2> "$H/link-stderr.log"
chk "host-cfgentry-binary" "$(sha "$H/modinst_composed_cfgentry")" "$EXPECTED_HOST_BIN"

echo "== Gate: console toolchain reproduction (pristine source, exact D1 argv) =="
C=$ME/receipts/gate-console-repro
console_compile "$D1/vendored/modinst_core.c" "$C/pristine" 2> "$C/pristine-compile.stderr"
grep -c "warning:" "$C/pristine-compile.stderr" > "$C/pristine-warning-count.txt" || true
[ "$(cat "$C/pristine-warning-count.txt")" = "3" ] || stop "pristine console compile warnings != 3"
chk "console-pristine-core-o" "$(sha "$C/pristine/modinst_core.ps5.o")" "$EXPECTED_D1_CORE_O"
console_link "$D1/build/console/modinst_core.ps5.o" "$C/pristine-link" link1.map || stop "pristine link1 failed"
console_link "$D1/build/console/modinst_core.ps5.o" "$C/pristine-link2" link2.map || stop "pristine link2 failed"
P1=$C/pristine-link/modbridge-installer.payload.elf
P2=$C/pristine-link2/modbridge-installer.payload.elf
echo "pristine link1 sha = $(sha "$P1")"
echo "pristine link2 sha = $(sha "$P2")"
bidcmp "$P1" "$P2" "pristine-link1-vs-link2" || stop "lld link nondeterminism EXCEEDS the 16-byte build-id note (see BID line)"
bidcmp "$P1" "$D1/build/console/modbridge-installer.payload.elf" "pristine-link1-vs-D1-payload" || stop "pristine relink differs from D1 payload beyond the 16-byte build-id note"
echo "GATE-OK console link reproduction: every non-build-id byte (681512/681528) identical to D1 e3489297...; build-id is per-link random (proved by link1-vs-link2)"

echo "== Build r1: revised payload (patched source) =="
console_compile "$ME/src/modinst_core.c" "$ME/build-r1" 2> "$ME/receipts/console-compile-modinst_core-r1.stderr"
grep -c "warning:" "$ME/receipts/console-compile-modinst_core-r1.stderr" > "$ME/receipts/r1-warning-count.txt" || true
[ "$(cat "$ME/receipts/r1-warning-count.txt")" = "3" ] || stop "r1 console compile warnings != 3 (got $(cat "$ME/receipts/r1-warning-count.txt"))"
console_link "$ME/build-r1/modinst_core.ps5.o" "$ME/build-r1" modbridge-installer.payload.map || stop "r1 link failed"
R1_ELF=$(sha "$ME/build-r1/modbridge-installer.payload.elf")
R1_BYTES=$(stat -f%z "$ME/build-r1/modbridge-installer.payload.elf")
echo "r1 payload sha = $R1_ELF bytes = $R1_BYTES"
if cmp -s "$ME/build-r1/modinst_core.ps5.o" "$D1/build/console/modinst_core.ps5.o"; then stop "r1 .o equals D1 pristine .o - patch not reflected?"; fi

echo "== Build r2: independent rebuild for determinism =="
console_compile "$ME/src/modinst_core.c" "$ME/build-r2" 2> "$ME/receipts/console-compile-modinst_core-r2.stderr"
grep -c "warning:" "$ME/receipts/console-compile-modinst_core-r2.stderr" > "$ME/receipts/r2-warning-count.txt" || true
[ "$(cat "$ME/receipts/r2-warning-count.txt")" = "3" ] || stop "r2 console compile warnings != 3 (got $(cat "$ME/receipts/r2-warning-count.txt"))"
console_link "$ME/build-r2/modinst_core.ps5.o" "$ME/build-r2" modbridge-installer.payload.map || stop "r2 link failed"
R2_ELF=$(sha "$ME/build-r2/modbridge-installer.payload.elf")
R2_BYTES=$(stat -f%z "$ME/build-r2/modbridge-installer.payload.elf")
echo "r2 payload sha = $R2_ELF bytes = $R2_BYTES"

cmp "$ME/build-r1/modinst_core.ps5.o" "$ME/build-r2/modinst_core.ps5.o" || stop "DETERMINISM: .o r1 != r2 (objects must be fully deterministic)"
bidcmp "$ME/build-r1/modbridge-installer.payload.elf" "$ME/build-r2/modbridge-installer.payload.elf" "r1-vs-r2" || stop "DETERMINISM: r1 vs r2 differ beyond the 16-byte build-id note"
echo "GATE-OK determinism: r1==r2 in all non-build-id bytes; both shas recorded (note desc differs per link by design)"
echo "NOTE r1 (681888 B) vs D1 pristine (681528 B): the +360 B delta IS the config-entry revision (checked at the .o level: r1 core .o != D1 core .o; census gate below proves import-set identity)"
python3 - "$ME/build-r1/modbridge-installer.payload.elf" "$LLVM_READOBJ" > "$ME/receipts/r1-note-offset.txt" <<'PYEOF'
import sys,subprocess,re
out=subprocess.run([sys.argv[2],'--sections',sys.argv[1]],capture_output=True,text=True).stdout
m=re.search(r'Name: \.note\.gnu\.build-id.*?Offset: (0x[0-9A-Fa-f]+)', out, re.S)
print("r1 .note.gnu.build-id section offset:", m.group(1), "desc at", hex(int(m.group(1),16)+16))
PYEOF
python3 - "$ME/build-r1/modbridge-installer.payload.elf" "$ME/receipts/r1-zeroed-buildid.sha" "$LLVM_READOBJ" <<'PYEOF'
import hashlib,sys,subprocess,re
b=bytearray(open(sys.argv[1],'rb').read())
out=subprocess.run([sys.argv[3],'--sections',sys.argv[1]],capture_output=True,text=True).stdout
m=re.search(r'Name: \.note\.gnu\.build-id.*?Offset: (0x[0-9A-Fa-f]+)', out, re.S)
off=int(m.group(1),16)+16
b[off:off+16]=b'\x00'*16
open(sys.argv[2],'w').write(hashlib.sha256(bytes(b)).hexdigest()+"\n")
PYEOF
echo "r1 zeroed-build-id canonical sha = $(cat "$ME/receipts/r1-zeroed-buildid.sha")"

echo "== Gate 8.4: 116-import census re-proof =="
$LLVM_NM -D --undefined-only "$ME/build-r1/modbridge-installer.payload.elf" > "$ME/receipts/census-new-payload.stdout" 2> "$ME/receipts/census-new-payload.stderr"
$LLVM_NM -D --undefined-only "$D1/build/console/modbridge-installer.payload.elf" > "$ME/receipts/census-d1-payload.stdout" 2> "$ME/receipts/census-d1-payload.stderr"
NEW_N=$(wc -l < "$ME/receipts/census-new-payload.stdout" | tr -d ' ')
D1_N=$(wc -l < "$ME/receipts/census-d1-payload.stdout" | tr -d ' ')
echo "new ELF imports=$NEW_N  D1 ELF imports=$D1_N" | tee "$ME/receipts/census-counts.txt"
[ "$NEW_N" = "116" ] || stop "CENSUS: new payload dynamic import count != 116 (got $NEW_N)"
[ "$D1_N" = "116" ] || stop "CENSUS: D1 baseline import count != 116 (got $D1_N)"
diff "$ME/receipts/census-d1-payload.stdout" "$ME/receipts/census-new-payload.stdout" > "$ME/receipts/census-diff.txt" || stop "CENSUS DELTA: import set differs from pristine D1 payload"
diff "$D1/receipts/console-imports.stdout" "$ME/receipts/census-new-payload.stdout" > "$ME/receipts/census-diff-vs-receipt.txt" || stop "CENSUS DELTA: differs from D1 console-imports.stdout receipt"
if awk '{print $NF}' "$ME/receipts/census-new-payload.stdout" | grep -qx -e "_strtol" -e "strtol"; then stop "CENSUS: plain strtol present"; fi
echo "GATE-OK census: 116=116, import set identical to D1 payload AND to console-imports.stdout receipt, no strtol; missing=[] follows from name-set identity + D1 provider receipt (console-import-providers.json)"

echo "== Dynamic layout re-check (readobj) =="
$LLVM_READOBJ --file-headers --program-headers --dynamic-table "$ME/build-r1/modbridge-installer.payload.elf" > "$ME/receipts/console-elf-readobj-r1.stdout" 2> "$ME/receipts/console-elf-readobj-r1.stderr"
diff <(sed 's/[0-9A-F]\{16\}//g' "$D1/receipts/console-elf-readobj.stdout") <(sed 's/[0-9A-F]\{16\}//g' "$ME/receipts/console-elf-readobj-r1.stdout") > "$ME/receipts/readobj-diff-vs-d1.txt" || echo "NOTE: readobj textual diff vs D1 receipt saved (expect revision-related address/size deltas only; inspect)"

echo "ALL GATES PASSED. r1=$R1_ELF ($R1_BYTES bytes) r2=$R2_ELF ($R2_BYTES bytes)"
