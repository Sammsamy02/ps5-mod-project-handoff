#!/usr/bin/env python3
"""Stage exact, invertible source changes; input trees are read-only."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil

HERE = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--autogen-source', type=Path, required=True)
args = parser.parse_args()
bindings = []


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def stage(path, raw, expected_hash, edits):
    if sha(raw) != expected_hash:
        raise ValueError(str(path) + ': input hash drift')
    original = raw.decode()
    changed = original
    for old, new in edits:
        if changed.count(old) != 1:
            raise ValueError(str(path) + ': nonunique or missing exact anchor')
        changed = changed.replace(old, new, 1)
    inverse = changed
    for old, new in reversed(edits):
        if inverse.count(new) != 1:
            raise ValueError(str(path) + ': inverse anchor drift')
        inverse = inverse.replace(new, old, 1)
    assert inverse == original
    for directory, data in [('pristine', original), ('derived', changed)]:
        (HERE / directory / path).parent.mkdir(exist_ok=True, parents=True)
        (HERE / directory / path).write_text(data)
    patch = ''.join(difflib.unified_diff(original.splitlines(True), changed.splitlines(True),
                                       fromfile='a/' + str(path), tofile='b/' + str(path)))
    (HERE / 'patches' / (path.name + '.patch')).write_text(patch)
    bindings.append(dict(path=str(path), source_sha256=sha(raw), derived_sha256=sha(changed.encode()),
                         patch_sha256=sha(patch.encode()), exact_inverse=True))


# The earlier narrow TLS stage remains byte-for-byte the first edit.
original = (args.source / 'configure.ac').read_bytes()
old_tls = 'AC_ARG_WITH(tls, [  --with-tls=__thread,pthread    select Thread Local Storage implementation (defaults to __thread)],[],[with_tls=__thread])\n'
new_tls = (HERE / 'tools/tls-policy.m4').read_text()
assert sha(new_tls.encode()) == 'afd927b24a7dfcc27e5bd7c8f6c3fd9c4964db4d6fd6b1da7f2fa3b8fdd95da5'
platform_anchor = 'build_darwin=no\n\ncase "$host" in\n\twasm32*)'
platform = '''build_darwin=no

case "$host" in
\tx86_64-sie-ps5)
\t\t# Prospero platform characteristics: begin
\t\t# The public compiler supplies the FreeBSD ABI macros. Do not invent
\t\t# OS macros or inherit a desktop /usr/local search path or -lutil.
\t\tCPPFLAGS="$CPPFLAGS -DHOST_BSD -DGC_FREEBSD_THREADS -D_WITH_GETLINE"
\t\tlibgc_threads=pthreads
\t\tneed_link_unlink=yes
\t\tuse_sigposix=yes
\t\tAC_DEFINE(PTHREAD_POINTER_ID, 1, [pthread is a pointer])
\t\t# Prospero platform characteristics: end
\t\t;;
\twasm32*)'''
guard_anchor = 'AC_MSG_RESULT([$host_prospero])\n'
guard = '''AC_MSG_RESULT([$host_prospero])

# Prospero compiler identity guard: begin
case "$host" in
    x86_64-sie-ps5)
        if test "x$host_prospero" != "xyes"; then
            AC_MSG_ERROR([x86_64-sie-ps5 requires a genuine Prospero compiler])
        fi
        ;;
    *-*-ps5*)
        AC_MSG_ERROR([only x86_64-sie-ps5 is configured for the public target])
        ;;
    *)
        if test "x$host_prospero" = "xyes"; then
            AC_MSG_ERROR([use --host=x86_64-sie-ps5 with the Prospero compiler])
        fi
        ;;
esac
# Prospero compiler identity guard: end
'''
native_anchor = 'elif case $host_os in freebsd*) true;; *) false;; esac; then\n'
native = '''elif test "x$host" = "xx86_64-sie-ps5"; then
\t# Build the existing Unix/FreeBSD source set with genuine SDK headers.
\t# Do not report the unassigned Prospero platform as desktop FreeBSD.
\tmono_native_text="Prospero (public FreeBSD ABI source set)"
\tMONO_NATIVE_CC=$CC
\tMONO_NATIVE_CXX=$CXX
\tMONO_NATIVE_CPPFLAGS=$CPPFLAGS
\tMONO_NATIVE_CXXFLAGS=$CXXFLAGS
\tMONO_NATIVE_CFLAGS=$CFLAGS
\tMONO_NATIVE_LDFLAGS=$LDFLAGS
\tmono_native=yes
\tmono_native_compat=no
\tMONO_NATIVE_PLATFORM=freebsd
\tMONO_NATIVE_PLATFORM_TYPE="MONO_NATIVE_PLATFORM_TYPE_UNKNOWN"
elif case $host_os in freebsd*) true;; *) false;; esac; then
'''
math_anchor = 'AC_SEARCH_LIBS(sqrtf, m)\n\n# nanosleep may not be part of libc, also search it in other libraries\n'
math = '''AC_SEARCH_LIBS(sqrtf, m)

# Keep established target flags, but use the measured dependency on Prospero.
# The public compiler supplies math through its default C runtime libraries.
MONO_MATH_LIBS="-lm"
if test "x$host_prospero" = "xyes"; then
    case "$ac_cv_search_sqrtf" in
        "none required") MONO_MATH_LIBS= ;;
        no) AC_MSG_ERROR([Prospero compiler cannot link the required sqrtf function]) ;;
        *) MONO_MATH_LIBS="$ac_cv_search_sqrtf" ;;
    esac
fi
AC_SUBST(MONO_MATH_LIBS)

# nanosleep may not be part of libc, also search it in other libraries
'''
name_anchor = '\tAC_CHECK_FUNCS(pthread_getattr_np pthread_attr_get_np pthread_getname_np pthread_setname_np pthread_cond_timedwait_relative_np)\n'
name_check = name_anchor + '''\t# A symbol-only import stub does not establish the public function ABI.
\tAC_CHECK_DECLS([pthread_getname_np], [], [], [[
#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
]])
'''
stage(Path('configure.ac'), original, '982a753d6163aa07b7b6ba5dacf3eb73b01379031f43c44c3bbca5ec7ce68754',
      [(old_tls, new_tls), (platform_anchor, platform), (guard_anchor, guard), (native_anchor, native),
       (math_anchor, math), (name_anchor, name_check),
       ('\tAC_CHECK_FUNCS(dl_iterate_phdr)\n', '\tAC_CHECK_FUNCS(dl_iterate_phdr)\n\tAC_CHECK_FUNCS(kinfo_getfile)\n')])
stage(Path('mono/utils/mono-threads-posix.c'), (args.source / 'mono/utils/mono-threads-posix.c').read_bytes(),
      '25c492611b7f75dea4125554ccec86df650f05c10e76c00f2fea5a28554de0bd',
      [('#include <pthread.h>\n', '#include <pthread.h>\n#if defined(__PROSPERO__) && defined(HAVE_PTHREAD_NP_H)\n#include <pthread_np.h>\n#endif\n'),
       ('#ifdef HAVE_PTHREAD_GETNAME_NP\n', '#if defined(HAVE_PTHREAD_GETNAME_NP) && (!defined(__PROSPERO__) || HAVE_DECL_PTHREAD_GETNAME_NP)\n')])
stage(Path('mono/metadata/w32process-unix.c'), (args.source / 'mono/metadata/w32process-unix.c').read_bytes(),
      '6a40d8af54a0689acd88ab0ce44a3f611deced69f839db4c209990bfa0ab6d85',
      [("#elif defined (__FreeBSD__)\n\t/* FreeBSD lets us get a list of FDs.",
        "#elif defined (__FreeBSD__) && (!defined (__PROSPERO__) || defined (HAVE_KINFO_GETFILE))\n\t/* FreeBSD lets us get a list of FDs.")])
stage(Path('mono/metadata/w32process-unix-internals.h'), (args.source / 'mono/metadata/w32process-unix-internals.h').read_bytes(),
      'f4fe5a7172746d4a1eebda9cfa10dbefc33ce6771775e695004299523a6bbcfc',
      [("#elif (defined(__OpenBSD__) || defined(__FreeBSD__)) && defined(HAVE_LINK_H)",
        "#elif (defined(__OpenBSD__) || defined(__FreeBSD__)) && defined(HAVE_LINK_H) && (!defined(__PROSPERO__) || defined(HAVE_DL_ITERATE_PHDR))")])
for filename in ['mono/eglib/Makefile.am', 'mono/mini/Makefile.am.in']:
    raw = (args.source / filename).read_bytes()
    if filename.endswith('eglib/Makefile.am'):
        anchor = 'libeglib_la_LIBADD = -lm $(LTLIBICONV)'
        replacement = 'libeglib_la_LIBADD = $(MONO_MATH_LIBS) $(LTLIBICONV)'
        edits = [(anchor, replacement)]
    else:
        # Both complete executable link blocks are anchored independently.
        edits = []
        for executable in ['mono_boehm', 'mono_sgen']:
            start = raw.decode().index(executable + '_LDADD =')
            end = raw.decode().index('$(MONO_DTRACE_OBJECT)', start) + len('$(MONO_DTRACE_OBJECT)')
            block = raw.decode()[start:end]
            assert block.count('-lm') == 1
            edits.append((block, block.replace('-lm', '$(MONO_MATH_LIBS)')))
    expected = {'mono/eglib/Makefile.am': 'b1ce70dc38eb7501ec38676b803de9ffcf8dd13b51fd5f4e4ebebb95cd35170e',
                'mono/mini/Makefile.am.in': '760b6a3627746ffae79fe4c6e3b290011462cad4a736ca06d20b82d951f4ae82'}
    stage(Path(filename), raw, expected[filename], edits)
# config.sub is an Automake-generated auxiliary, not tracked by this Mono pin.
stage(Path('config.sub'), (args.autogen_source / 'config.sub').read_bytes(),
      'd611751fba98e807c9684d253bb02aa73d6825fe0e0b9ae3cbf258a59171c9b0',
      [('| ekkobsd* | freebsd* | riscix*', '| ekkobsd* | freebsd* | ps5 | riscix*')])
# Only regeneration inputs are copied, never the mutable host build/config.h.
for name in ['aclocal.m4', 'acinclude.m4']:
    shutil.copyfile(args.autogen_source / name, HERE / 'derived' / name)
shutil.copytree(args.autogen_source / 'm4', HERE / 'derived/m4', dirs_exist_ok=True)
receipt = dict(mono_commit='0cbf0e290c31adb476f9de0fa44b1d8829affa40',
               config_sub_provider='GNU Automake 1.16.5', bindings=bindings,
               scope='exact invertible portability source staging; full build and runtime behavior evidence are separate')
(HERE / 'evidence/CONFIGURATION-STAGE.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
