#!/usr/bin/env python3
"""Genuine public target configuration and static-runtime build, max one job.

No PRX packaging, target execution, deployment, fake OS macros, or cached
feature answers. Each command and result is recorded including failed stages.
Source staging/copying is separate so this script never overwrites sources.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parents[1]
local_file = HERE / 'LOCAL-INPUTS.json'
local = json.loads(local_file.read_text()) if local_file.exists() else {}
parser = argparse.ArgumentParser()
parser.add_argument('stage', choices=['autogen', 'configure', 'build'])
parser.add_argument('--sdk', type=Path, default=local.get('sdk'), required='sdk' not in local)
parser.add_argument('--llvm-bin', type=Path, default=local.get('llvm_bin'), required='llvm_bin' not in local)
parser.add_argument('--build-tools', type=Path, default=local.get('build_tools'), required='build_tools' not in local)
parser.add_argument('--archive-bin', type=Path, default=local.get('archive_bin'), required='archive_bin' not in local,
                    help='Complete LLVM 21.1.8 llvm-ar/llvm-ranlib directory; compiler extraction is partial')
parser.add_argument('--label', default='TARGET')
parser.add_argument('--part', action='append', choices=['eglib', 'arch', 'utils', 'cil', 'zlib', 'sgen', 'metadata', 'mini', 'runtime-archive', 'native'])
args = parser.parse_args()
work = HERE / 'target-work'
env = os.environ.copy()
for required in [args.sdk / 'bin/prospero-clang', args.sdk / 'bin/prospero-lld',
                 args.llvm_bin / 'llvm-config', args.llvm_bin / 'clang-21',
                 args.llvm_bin / 'llvm-nm', args.build_tools / 'bin/autoconf']:
    if not required.is_file():
        raise SystemExit('Required build tool missing: ' + str(required))
archive_tool = args.archive_bin / 'llvm-ar'
if hashlib.sha256(archive_tool.read_bytes()).hexdigest() != 'c2640fc0a747413fe27818531d4a22f979d91b7e299723a68225917c42574b50':
    raise SystemExit('LLVM 21.1.8 archive tool hash differs from pinned public archive manifest')
for key in ['MONO_PATH', 'MONO_ENV_OPTIONS', 'MONO_AOT_CACHE', 'CONFIG_SITE', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS']:
    env.pop(key, None)
env.update(PATH=str(args.build_tools / 'bin') + ':/usr/bin:/bin:/usr/sbin:/sbin',
           LLVM_CONFIG=str(args.llvm_bin / 'llvm-config'),
           CC=str(args.sdk / 'bin/prospero-clang'), CXX=str(args.sdk / 'bin/prospero-clang++'),
           AR=str(archive_tool), RANLIB=str(args.archive_bin / 'llvm-ranlib'),
           NM=str(args.sdk / 'bin/prospero-nm'), STRIP=str(args.sdk / 'bin/prospero-strip'),
           LD=str(args.sdk / 'bin/prospero-lld'), AS=str(args.sdk / 'bin/prospero-as'),
           CFLAGS='-O1', CXXFLAGS='-O1',
           MONO_BRANCH='pinned-public-source', MONO_BUILD_REVISION='0cbf0e290c31adb476f9de0fa44b1d8829affa40')


def run(label, command, cwd=work):
    logfile = HERE / 'logs' / (label + '.log')
    with logfile.open('w') as log:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)
    receipt = dict(command=command, cwd=str(cwd), exit_code=result.returncode,
                   environment={k: env[k] for k in ['PATH', 'LLVM_CONFIG', 'CC', 'CXX', 'AR', 'RANLIB', 'NM', 'LD', 'AS', 'CFLAGS', 'CXXFLAGS', 'MONO_BUILD_REVISION']},
                   max_make_jobs=1, full_target_execution=False, prx_packaging=False,
                   configure_ac_sha256=hashlib.sha256((work / 'configure.ac').read_bytes()).hexdigest(),
                   log_sha256=hashlib.sha256(logfile.read_bytes()).hexdigest())
    (HERE / 'evidence' / (label + '.json')).write_text(json.dumps(receipt, indent=2) + '\n')
    print(label, result.returncode, flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)


if args.stage == 'autogen':
    env['NOCONFIGURE'] = '1'
    env['ACLOCAL_FLAGS'] = '-I /opt/homebrew/share/aclocal'
    run(args.label + '-AUTOGEN', ['./autogen.sh', '--disable-boehm'])
elif args.stage == 'configure':
    build = subprocess.check_output(['/bin/sh', str(work / 'config.guess')], text=True).strip()
    command = ['./configure', '--prefix=' + str(HERE / 'target-install'), '--build=' + build,
               '--host=x86_64-sie-ps5', '--disable-shared', '--enable-static',
               '--disable-mcs-build', '--disable-boehm', '--with-sgen=yes', '--enable-minimal=jit',
               '--with-tls=pthread', '--enable-cooperative-suspend', '--disable-hybrid-suspend',
               '--disable-btls', '--disable-btls-lib', '--disable-nls', '--disable-support-build',
               '--with-ikvm-native=no', '--with-mcs-docs=no', '--disable-system-aot', '--disable-dtrace', '--disable-llvm']
    run(args.label + '-CONFIGURE', command)
else:
    # The tiny Probe owner does not request Mono.Native; build that optional
    # wider class-library surface explicitly with --part native.
    for part in (args.part or ['eglib', 'arch', 'utils', 'cil', 'zlib', 'sgen', 'metadata', 'mini', 'runtime-archive']):
        if part == 'runtime-archive':
            # Upstream disables the aggregate install library with shared=no;
            # its genuine convenience-archive target remains available.
            run(args.label + '-RUNTIME-ARCHIVE', ['/usr/bin/make', '-j1', 'V=1', 'libmonosgen-2.0.la'], work / 'mono/mini')
        else:
            run(args.label + '-' + part.upper(), ['/usr/bin/make', '-j1', 'V=1'], work / 'mono' / part)
