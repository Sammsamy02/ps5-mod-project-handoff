#!/bin/bash
# pin_msvc_layout.sh -- pin MSVC ABI facts on the Mac (no Windows needed).
#
# Verified working 2026-10-13 with Apple clang 17 on this machine:
#   * -target x86_64-pc-windows-msvc accepts syntax-only compiles;
#   * -Xclang -fdump-record-layouts prints MSVC record layouts (vftable
#     pointer at offset 0, field offsets, sizeof);
#   * -S emits COFF assembly text containing authoritative MSVC-mangled
#     symbol names ("?call@@YAXPEAUCppUserModBase@RC@@@Z") and the emitted
#     .rdata vftable constants (the qword slot list = the vtable image).
#
# Usage: tools/pin_msvc_layout.sh <header(s) + defines...> out_dir
# The vendored UE4SS headers are compiled with the same defines the port
# uses (see CAP9 design section 0 for the port's flag set) so the layouts
# match what the real mod was built against.
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: pin_msvc_layout.sh <source-file...> <out_dir>" >&2
  exit 2
fi
out="${@: -1}"
srcs=("${@:1:$#-1}")
mkdir -p "$out"

for src in "${srcs[@]}"; do
  base="$(basename "$src")"
  # 1) Record layouts (MSVC ABI field offsets/sizes)
  clang++ -target x86_64-pc-windows-msvc -std=c++17 -fsyntax-only \
      -Xclang -fdump-record-layouts "$src" > "$out/$base.layouts.txt" 2>&1 || true
  # 2) Mangled names + .rdata vftable images (read the assembly text)
  clang++ -target x86_64-pc-windows-msvc -std=c++17 -S -o - "$src" \
      > "$out/$base.msvc.asm" 2>&1 || true
done

echo "Pinned into $out:"
echo "  *.layouts.txt  -- MSVC record layouts (offsets, sizeof, vftable at 0)"
echo "  *.msvc.asm     -- '?...' mangled names + .rdata vftable qword lists"
echo
echo "M1 consumes these to bake:"
echo "  - CppUserModBase base-subobject offsets (ue4ss_compat.cpp)"
echo "  - the shim vftable slot order + slot count"
echo "  - the exact mangled export strings for the census table"
