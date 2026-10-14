#!/usr/bin/env python3
"""run_p1_host_test.py -- builds and runs the P1 host test (Mac-only).

Compiles with the host /usr/bin/clang++ and the project's existing host recipe
for PosixFile.cpp (logwriter-impl-verify/build_and_run.sh:87-95), ONE compile at
a time, after checking vm_stat 'Pages free' >= 2 GiB before each step (a step
below the limit is skipped and counted as a failure). Everything it writes stays
under this host-test/ directory (build/, run-CONTROL/, run-P1/).

Two binaries: CONTROL (unpatched PosixFile.cpp copy) and P1 (patched copy),
each linked with the real port File.cpp and test_p1_posixfile.cpp.
Prints how many binaries ran and their check counts; exit 0 = both pass,
1 = a step or check failed, 2 = ZERO test binaries ran.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRACK = os.path.dirname(HERE)
WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'
REL = 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/File/FileType/PosixFile.cpp'
SRC = {'CONTROL': os.path.join(TRACK, 'control', REL), 'P1': os.path.join(TRACK, 'patched', REL)}
PORT = os.path.join(WHAT, '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port')
DEPS = os.path.join(WHAT, '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first')
VFMT = os.path.join(WHAT, '.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/fmt/include')
CAND_INC = os.path.join(WHAT, 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/include')
CXX = '/usr/bin/clang++'
FLAGS = ['-std=c++23', '-O0', '-fPIC',
         '-DRC_FILE_USE_POSIX_BACKEND', '-DFMT_HEADER_ONLY', '-DFORCE_U16',
         '-DRC_DYNAMIC_OUTPUT_BUILD_STATIC', '-DRC_FILE_BUILD_STATIC', '-DRC_HELPERS_BUILD_STATIC',
         '-Wno-unknown-pragmas', '-Wno-unused-parameter', '-Wno-unused-variable',
         '-include', 'algorithm',
         '-I', CAND_INC,
         '-I', PORT + '/first/include', '-I', PORT + '/fmt/include', '-I', VFMT,
         '-I', DEPS + '/DynamicOutput/include', '-I', DEPS + '/File/include', '-I', DEPS + '/Helpers/include',
         '-I', DEPS + '/String/include', '-I', DEPS + '/Constructs/include']
MIN_FREE = 2 * 1024 ** 3
LOG = []


def free_bytes():
    txt = subprocess.run(['vm_stat'], capture_output=True, text=True).stdout
    page = int(re.search(r'page size of (\d+) bytes', txt).group(1))
    return int(re.search(r'Pages free:\s+(\d+)', txt).group(1)) * page


def step(name, argv):
    fb = free_bytes()
    if fb < MIN_FREE:
        LOG.append(f'{name}: SKIPPED (Pages free {fb / 2**30:.2f} GiB < 2 GiB)')
        print(LOG[-1])
        return False
    r = subprocess.run(argv, capture_output=True, text=True, cwd=HERE, env=dict(os.environ, CCACHE_DISABLE='1'))
    ok = r.returncode == 0
    LOG.append(f'{name}: exit={r.returncode} free_before={fb / 2**30:.2f}GiB {"ok" if ok else "FAILED"}')
    print(LOG[-1])
    if r.stdout.strip():
        print(r.stdout.rstrip())
    if r.stderr.strip():
        print(r.stderr.rstrip()[-4000:])
    return ok


def main():
    build = os.path.join(HERE, 'build')
    os.makedirs(build, exist_ok=True)
    bad = 0
    ran = 0
    if not step('compile File.cpp', [CXX] + FLAGS + ['-c', PORT + '/first/src/File/File.cpp', '-o', build + '/File.o']):
        print('P1-HOST-RUN: FAIL -- File.cpp did not compile')
        return 1
    for label in ('CONTROL', 'P1'):
        extra = ['-DEXPECT_P1'] if label == 'P1' else []
        ok = step(f'compile PosixFile.cpp [{label}]', [CXX] + FLAGS + ['-c', SRC[label], '-o', f'{build}/PosixFile-{label}.o'])
        ok = ok and step(f'compile test [{label}]', [CXX] + FLAGS + extra + ['-c', HERE + '/test_p1_posixfile.cpp', '-o', f'{build}/test-{label}.o'])
        ok = ok and step(f'link [{label}]', [CXX, f'{build}/test-{label}.o', f'{build}/PosixFile-{label}.o', build + '/File.o', '-o', f'{build}/test-{label}'])
        if not ok:
            bad += 1
            continue
        scratch = os.path.join(HERE, f'run-{label}')
        shutil.rmtree(scratch, ignore_errors=True)
        os.makedirs(scratch)
        r = subprocess.run([f'{build}/test-{label}', scratch], capture_output=True, text=True)
        ran += 1
        print(r.stdout.rstrip())
        if r.stderr.strip():
            print(r.stderr.rstrip())
        print(f'P1-HOST-RUN: test-{label} exit={r.returncode}')
        bad += r.returncode != 0
    print(f'P1-HOST-RUN: ran {ran} of 2 test binaries; {bad} failed')
    if ran == 0:
        print('P1-HOST-RUN: FAIL -- ZERO test binaries ran')
        return 2
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
