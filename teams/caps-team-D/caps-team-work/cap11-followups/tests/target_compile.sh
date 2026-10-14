#!/bin/zsh
# TARGET compile of the changed unit (cppmod = CppMod.cpp + the two .inc files + the two headers) with R15e's own
# compile command (prospero-clang++, LLVM 21.1.8), and the import delta against R15e's runtime-l26-A.o.
#   baseline : the LIVE CppMod.cpp, unpatched (its symbols are allowed: they are not added by this patch)
#   patched  : a scratch copy of the LIVE CppMod.cpp (byte-identical) next to PATCH/post's two .inc files, with
#              PATCH/post/include first on the include path
# Two compiles, one after the other (the cap is 2 parallel; one at a time keeps memory low). Pauses while less
# than 4 GB is available or an integrator prospero-clang compile is running. Read-only on every project tree.
set -u
HERE=${0:A:h}
FU=${HERE:h}
LIVE=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4
OUT=$HERE/output/target
STAGE=${TMPDIR:-/tmp}/cap11-fu-stage-$$
mkdir -p $OUT $STAGE/src/Mod
avail_gb() { vm_stat | awk '/page size of/ {ps=$8} /Pages free/ {f=$3} /Pages inactive/ {i=$3} /Pages speculative/ {s=$3} /Pages purgeable/ {p=$3} END {printf "%d", (f+i+s+p)*ps/1073741824}' }
tries=0
while true; do
  g=$(avail_gb)
  if (( g >= 4 )) && ! ps -axo command | grep -v grep | grep -q "prospero-clang"; then break; fi
  tries=$((tries+1)); (( tries > 40 )) && { print "PAUSED: ${g} GB available or integrator compile running; giving up"; exit 3 }
  python3 -I -c "import time; time.sleep(15)"
done
cp $LIVE/candidate/src/Mod/CppMod.cpp $STAGE/src/Mod/CppMod.cpp
cp $FU/PATCH/post/candidate/src/Mod/NativeGuiEngineBackend.inc $FU/PATCH/post/candidate/src/Mod/NativeModEngineServices.inc $STAGE/src/Mod/
{
  print "stage (sha256):"
  shasum -a 256 $LIVE/candidate/src/Mod/CppMod.cpp $STAGE/src/Mod/CppMod.cpp $STAGE/src/Mod/*.inc $FU/PATCH/post/include/Platform/*.hpp
} > $OUT/stage.txt
python3 -I $HERE/target_compile_r15e.py --baseline-unit cppmod=$LIVE/candidate/src/Mod/CppMod.cpp --unit cppmod=$STAGE/src/Mod/CppMod.cpp \
  --include $FU/PATCH/post/include --baseline-plain --out $OUT/objects > $OUT/target_compile.log 2>&1
rc=$?
cat $OUT/stage.txt $OUT/target_compile.log
rm -rf $STAGE
exit $rc
