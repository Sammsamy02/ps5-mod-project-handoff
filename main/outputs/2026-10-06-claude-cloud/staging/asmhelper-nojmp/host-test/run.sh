#!/bin/bash
# Host test: old (694af016) vs new walkguard helper, PROSPERO and host modes.
# Needs clang++, Zydis 4 and fmt headers/libs (Ubuntu: libzydis-dev libfmt-dev; macOS: brew install zydis fmt).
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$H/../../../../.." && pwd)"
OLD="$ROOT/outputs/2026-10-04-zcode-team/worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp"
NEW="$H/../ASMHelper.guarded.nojmp.cpp"
OUT="$(mktemp -d)"
EXTRA=""
command -v brew >/dev/null 2>&1 && EXTRA="-I$(brew --prefix)/include -L$(brew --prefix)/lib"
for v in old new; do for m in prospero host; do
  src=$OLD; [ $v = new ] && src=$NEW
  def=""; [ $m = prospero ] && def="-D__PROSPERO__"
  echo "=== $v/$m"
  clang++ -std=c++20 -O0 -w $def $EXTRA -I"$H/shims" "$src" "$H/test.cpp" -lZydis -lfmt -o "$OUT/t-$v-$m"
  "$OUT/t-$v-$m" | grep RESULT
done; done
