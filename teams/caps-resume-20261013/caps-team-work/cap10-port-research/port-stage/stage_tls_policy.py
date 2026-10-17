#!/usr/bin/env python3
"""Stage one pinned Mono configure fix; never modifies the input tree."""
import difflib
import hashlib
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
SOURCE = HERE.parent / 'public-source/configure.ac'
SOURCE_SHA = '982a753d6163aa07b7b6ba5dacf3eb73b01379031f43c44c3bbca5ec7ce68754'
OLD = 'AC_ARG_WITH(tls, [  --with-tls=__thread,pthread    select Thread Local Storage implementation (defaults to __thread)],[],[with_tls=__thread])\n'
NEW = '''mono_tls_explicit=no
AC_ARG_WITH(tls, [  --with-tls=__thread,pthread    select Thread Local Storage implementation (defaults to __thread; pthread on Prospero)], [mono_tls_explicit=yes], [with_tls=__thread])

AC_MSG_CHECKING([whether the compiler targets Prospero])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#ifndef __PROSPERO__
#error not a Prospero compiler
#endif
int main (void) { return 0; }
]])], [host_prospero=yes], [host_prospero=no])
AC_MSG_RESULT([$host_prospero])

# Prospero TLS policy: begin
if test "x$host_prospero" = "xyes"; then
    # The public target wrapper uses emulated TLS, which has no FS-relative
    # offsets. Use Mono's existing pthread-key implementation instead.
    if test "x$mono_tls_explicit" = "xyes" -a "x$with_tls" != "xpthread"; then
        AC_MSG_ERROR([Prospero requires --with-tls=pthread])
    fi
    with_tls=pthread
fi
# Prospero TLS policy: end
'''


def transform(raw):
    if hashlib.sha256(raw).hexdigest() != SOURCE_SHA:
        raise ValueError('pinned Mono configure source drifted')
    text = raw.decode()
    if text.count(OLD) != 1:
        raise ValueError('TLS selection anchor is not unique')
    return text.replace(OLD, NEW, 1).encode()


def inverse(raw):
    text = raw.decode()
    if text.count(NEW) != 1:
        raise ValueError('staged TLS selection anchor is not unique')
    original = text.replace(NEW, OLD, 1).encode()
    if hashlib.sha256(original).hexdigest() != SOURCE_SHA:
        raise ValueError('inverse does not match pinned Mono source')
    return original


if __name__ == '__main__':
    original = SOURCE.read_bytes()
    staged = transform(original)
    if inverse(staged) != original:
        raise ValueError('inverse differs')
    derived = HERE / 'derived/configure.ac'
    derived.parent.mkdir(parents=True, exist_ok=True)
    derived.write_bytes(staged)
    patch = ''.join(difflib.unified_diff(original.decode().splitlines(True), staged.decode().splitlines(True),
                                         fromfile='a/configure.ac', tofile='b/configure.ac'))
    (HERE / 'prospero-pthread-tls.patch').write_text(patch)
    receipt = {'scope': 'Mac-only source stage; not a target runtime or admission',
               'mono_commit': '0cbf0e290c31adb476f9de0fa44b1d8829affa40',
               'source_sha256': SOURCE_SHA, 'derived_sha256': hashlib.sha256(staged).hexdigest(),
               'patch_sha256': hashlib.sha256(patch.encode()).hexdigest(),
               'inverse_matches': True, 'source_files_checked': 1}
    (HERE / 'STAGE-RECEIPT.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))
