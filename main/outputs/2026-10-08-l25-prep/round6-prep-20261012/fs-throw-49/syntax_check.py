#!/usr/bin/env python3
"""syntax_check.py -- fs-throw-49 track: -fsyntax-only checks of the staged
PosixFile.cpp copies (P1 patched + unpatched control). Mac-only. Writes ONLY
into this directory (syntax-check/); produces no object files.

Two modes, run ONE compile at a time (no parallelism):
  target  the EXACT argv the current l26-A build used for the posix-file unit
          (outputs/2026-10-08-l26-build-variants/l26-A/pass1/objects/
          posix-file.command.json: prospero-clang++, same -I/-D/-ivfsoverlay),
          with the source swapped to the staged copy, '-c -o X -MD -MF Y'
          removed and '-fsyntax-only' added.
  host    /usr/bin/clang++ with the project's existing host recipe for this
          TU (logwriter-impl-verify/build_and_run.sh:87-95,115:
          -DRC_FILE_USE_POSIX_BACKEND ... ), '-fsyntax-only' instead of '-c'.

Before every compile: vm_stat 'Pages free' must be >= 2 GiB, else the compile
is SKIPPED and counted as a failure (never silently passed).
Prints how many compiles ran; exits 1 if any compile failed or was skipped,
2 if ZERO compiles ran.
"""
import json
import os
import re
import subprocess
import sys

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'syntax-check')
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/File/FileType/PosixFile.cpp'
CMD_JSON = os.path.join(WHAT, 'outputs/2026-10-08-l26-build-variants/l26-A/pass1/objects/posix-file.command.json')
STAGED = {'P1': os.path.join(HERE, 'patched', REL), 'CONTROL': os.path.join(HERE, 'control', REL)}
PORT = os.path.join(WHAT, '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port')
DEPS = os.path.join(WHAT, '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first')
VFMT = os.path.join(WHAT, '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/fmt/include')
CAND_INC = os.path.join(WHAT, 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include')
HOST_FLAGS = ['-std=c++23', '-O0', '-fPIC',
              '-DRC_FILE_USE_POSIX_BACKEND', '-DFMT_HEADER_ONLY', '-DFORCE_U16',
              '-DRC_DYNAMIC_OUTPUT_BUILD_STATIC', '-DRC_FILE_BUILD_STATIC', '-DRC_HELPERS_BUILD_STATIC',
              '-Wno-unknown-pragmas', '-Wno-unused-parameter', '-Wno-unused-variable',
              '-include', 'algorithm',
              '-I', CAND_INC,
              '-I', PORT + '/first/include', '-I', PORT + '/fmt/include', '-I', VFMT,
              '-I', DEPS + '/DynamicOutput/include', '-I', DEPS + '/File/include', '-I', DEPS + '/Helpers/include',
              '-I', DEPS + '/String/include', '-I', DEPS + '/Constructs/include']
MIN_FREE = 2 * 1024 ** 3
LLVM_CONFIG = os.path.join(WHAT, 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')


def free_bytes():
    txt = subprocess.run(['vm_stat'], capture_output=True, text=True).stdout
    page = int(re.search(r'page size of (\d+) bytes', txt).group(1))
    free = int(re.search(r'Pages free:\s+(\d+)', txt).group(1))
    return free * page


def target_argv(src):
    argv = json.load(open(CMD_JSON))['argv']
    orig = os.path.join(WHAT, REL)
    out, i, swapped = [], 0, 0
    while i < len(argv):
        a = argv[i]
        if a in ('-o', '-MF'):
            i += 2
            continue
        if a in ('-c', '-MD'):
            i += 1
            continue
        if a == orig:
            out.append(src)
            swapped += 1
        else:
            out.append(a)
        i += 1
    if swapped != 1:
        raise SystemExit(f'SYNTAX-CHECK: ERROR source path found {swapped}x in {CMD_JSON}')
    return out + ['-fsyntax-only']


def make_negative():
    """NEGATIVE control: the P1 copy with one undeclared identifier injected
    INSIDE the new helper. Both modes must REJECT it, proving the check really
    compiles the patched code (a check that cannot fail is not a check)."""
    src = open(STAGED['P1']).read()
    anchor = 'std::filesystem::create_directories(parent, ec);'
    if src.count(anchor) != 1:
        raise SystemExit('SYNTAX-CHECK: ERROR negative-control anchor not unique')
    neg_dir = os.path.join(OUT, 'negative-control')
    os.makedirs(neg_dir, exist_ok=True)
    path = os.path.join(neg_dir, 'PosixFile.cpp')
    with open(path, 'w') as f:
        f.write(src.replace(anchor, anchor + ' fs49_negative_control_undeclared_identifier;'))
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    plan = []
    STAGED['NEGATIVE'] = make_negative()
    for label in ('CONTROL', 'P1', 'NEGATIVE'):
        src = STAGED[label]
        if not os.path.isfile(src):
            print(f'SYNTAX-CHECK: ERROR staged file missing: {src}')
            return 2
        plan.append(('target', label, target_argv(src)))
        plan.append(('host', label, ['/usr/bin/clang++'] + HOST_FLAGS + ['-fsyntax-only', src]))
    ran = failed = 0
    for mode, label, argv in plan:
        fb = free_bytes()
        tag = f'{mode}-{label}'
        with open(os.path.join(OUT, f'{tag}.argv.json'), 'w') as f:
            json.dump(argv, f, indent=1)
        if fb < MIN_FREE:
            print(f'SYNTAX-CHECK: {tag}: SKIPPED (Pages free {fb / 2**30:.2f} GiB < 2 GiB) -> counted as FAIL')
            failed += 1
            continue
        # Same toolchain env the build driver uses (build_runtime.py:98).
        env = dict(os.environ, CCACHE_DISABLE='1', LLVM_CONFIG=LLVM_CONFIG)
        r = subprocess.run(argv, capture_output=True, text=True, env=env, cwd=OUT)
        ran += 1
        with open(os.path.join(OUT, f'{tag}.log'), 'w') as f:
            f.write(f'# free before compile: {fb / 2**30:.2f} GiB\n# exit: {r.returncode}\n')
            f.write('# --- stdout ---\n' + r.stdout + '# --- stderr ---\n' + r.stderr)
        nwarn = r.stderr.count('warning:')
        nerr = r.stderr.count('error:')
        if label == 'NEGATIVE':
            # must be rejected, and for the injected identifier specifically
            ok = r.returncode != 0 and 'fs49_negative_control_undeclared_identifier' in r.stderr
            verdict = 'PASS (rejected as expected)' if ok else 'FAIL (broken copy was NOT rejected)'
        else:
            ok = r.returncode == 0 and nerr == 0
            verdict = 'PASS' if ok else 'FAIL'
        failed += not ok
        print(f'SYNTAX-CHECK: {tag}: exit={r.returncode} errors={nerr} warnings={nwarn} '
              f'free_before={fb / 2**30:.2f}GiB -> {verdict}')
    print(f'SYNTAX-CHECK: ran {ran} of {len(plan)} compiles, {failed} failed/skipped')
    if ran == 0:
        print('SYNTAX-CHECK: FAIL -- ZERO compiles ran')
        return 2
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
