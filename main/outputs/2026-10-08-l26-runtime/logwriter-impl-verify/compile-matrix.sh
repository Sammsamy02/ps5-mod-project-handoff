#!/bin/zsh
# compile-matrix.sh — logwriter F1-F4 candidates (agent CL, 2026-10-08)
#
# Matrix per touched TU (units: output, posix-file, lua-library; plus a
# FileDevice.hpp consumer probe = the lua-library TU itself, whose dep set
# includes the header):
#   host      /usr/bin/clang++  -fsyntax-only and -c   (Z13 smoke recipe:
#             -D__PROSPERO__ -D__SCE__ -fshort-wchar, stubs + l24 vfs overlay
#             kept, source swapped to the staged variant, candidate/include
#             prepended FIRST so the candidate FileDevice.hpp wins)
#   prospero  ps5-payload-sdk prospero-clang++ -fsyntax-only (exact l24
#             receipt argv, source swapped, candidate/include prepended)
# Both VARIANTS (original staged, candidate staged) are checked.
set -uo pipefail

WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
T4=$WHAT/outputs/2026-10-08-l26-runtime/typescan-r4
VDIR=$WHAT/outputs/2026-10-08-l26-runtime/logwriter-impl-verify
RECEIPT=$WHAT/outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json
STUBS=/tmp/l25-combined-smoke/stubs
OVERLAY=$WHAT/outputs/2026-10-08-l24-build-r2/headers.vfs.json
HOST_CXX=/usr/bin/clang++
PROSPERO=$WHAT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++
OUT=$VDIR/COMPILE-MATRIX.txt

mkdir -p $VDIR/matrix-obj

unit_source_map() {
  case "$1" in
    output)      echo "$T4/REPLACE/src/DynamicOutput/Output.cpp" ;;
    posix-file)  echo "$T4/REPLACE/src/File/FileType/PosixFile.cpp" ;;
    lua-library) echo "$T4/REPLACE/src/LuaLibrary.cpp" ;;
  esac
}

python3 - "$RECEIPT" "$T4" "$VDIR" "$STUBS" "$OVERLAY" "$HOST_CXX" "$PROSPERO" > $OUT <<'PYEOF'
import json, subprocess, sys, os
from pathlib import Path
receipt=json.loads(Path(sys.argv[1]).read_text())
T4=Path(sys.argv[2]); VDIR=Path(sys.argv[3]); STUBS=Path(sys.argv[4]); OVERLAY=Path(sys.argv[5])
HOST=sys.argv[6]; PROSPERO=sys.argv[7]
ENV=dict(os.environ, LLVM_CONFIG='/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config', CCACHE_DISABLE='1')
units={u['name']:u for u in receipt['selected_units'] if u['name'] in ('output','posix-file','lua-library')}
def staged(kind,unit):
    m={'output':'src/DynamicOutput/Output.cpp','posix-file':'src/File/FileType/PosixFile.cpp','lua-library':'src/LuaLibrary.cpp'}[unit]
    return str(T4/kind/m)
def adapt(unit,kind,compiler):
    u=units[unit]
    argv=list(u['argv'])
    src=str(Path(u['source']).resolve())
    out=[]
    i=0
    replaced=False
    while i < len(argv):
        a=argv[i]
        if a==src:
            out.append(staged(kind,unit)); replaced=True; i+=1; continue
        if a=='-MD': i+=1; continue
        if a=='-MF': i+=2; continue
        if a=='-o': i+=2; continue
        if a=='-c' and compiler!=PROSPERO: i+=1; continue  # host: caller adds mode flags
        out.append(a); i+=1
    assert replaced, unit
    if compiler==PROSPERO:
        pre=['-fsyntax-only','-I',str(T4/kind/'include')]
        return [compiler]+pre+out
    pre=['-D__PROSPERO__','-D__SCE__','-fshort-wchar',
         '-I',str(STUBS),'-I',str(T4/kind/'include'),'-ivfsoverlay',str(OVERLAY)]
    return [HOST]+pre+out
results=[]
for unit in ('output','posix-file','lua-library'):
    for kind in ('original','candidate'):
        for label,comp,extra in (('host-syntax',HOST,['-fsyntax-only']),
                                 ('host-c',HOST,['-c','-o',str(VDIR/'matrix-obj'/f'{unit}-{kind}.o')]),
                                 ('prospero-syntax',PROSPERO,['-fsyntax-only'])):
            argv=adapt(unit,kind,comp)
            # host mode flags replace the -c we may have kept for prospero; strip trailing prospero -fsyntax-only for host
            if comp==HOST:
                argv=[a for a in argv if a!='-fsyntax-only']
                argv=argv[:1]+extra+argv[1:]
            r=subprocess.run(argv,capture_output=True,text=True,cwd=str(VDIR),env=ENV,timeout=900)
            err=(r.stderr or '')
            errs=[l for l in err.splitlines() if ' error' in l or l.startswith('error')]
            status='PASS' if r.returncode==0 else 'FAIL'
            results.append((unit,kind,label,status,r.returncode,len(errs),errs[:3]))
            (VDIR/'matrix-obj'/f'{unit}-{kind}-{label}.log').write_text('CMD: '+' '.join(argv)+'\n\n'+r.stdout+r.stderr)
            print(f"{unit:12s} {kind:9s} {label:16s} {status} exit={r.returncode} errors={len(errs)}")
            for e in errs[:3]: print("    ",e[:200])
ok=all(r[3]=='PASS' for r in results)
print('MATRIX:', 'ALL PASS' if ok else 'FAILURES PRESENT', f'({len(results)} checks)')
sys.exit(0 if ok else 1)
PYEOF
echo "matrix exit=$? (see $OUT)"
