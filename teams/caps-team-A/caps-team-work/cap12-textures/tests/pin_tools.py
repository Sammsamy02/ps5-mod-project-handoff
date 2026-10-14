#!/usr/bin/env python3
"""Dev helper (Mac-only): write the sha256 pins of the texture tools into the
staged derive_substitutes.py and paktx.py (PATCH/tools only). --check: exit 1
if any pin is stale (the test runner calls it this way)."""
import hashlib
import os
import re
import sys

T = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'PATCH', 'tools')
FILES = ('bcn_swap.py', 'bcn_decode.py', 'texture_package.py', 'metal_bcn_check.swift', 'ps5_tiling.py', 'ps5_layout_census.json')


def main():
    check = '--check' in sys.argv
    pins = {f: hashlib.sha256(open(os.path.join(T, f), 'rb').read()).hexdigest() for f in FILES}
    stale = 0
    n = 0
    for host in ('derive_substitutes.py', 'paktx.py'):
        p = os.path.join(T, host)
        s = open(p).read()
        out = s
        for f, h in pins.items():
            pat = re.compile(r'("%s": ")([0-9a-fA-Z_]+)(")' % re.escape(f))
            for m in pat.finditer(s):
                n += 1
                if m.group(2) != h:
                    stale += 1
            out = pat.sub(lambda m: m.group(1) + h + m.group(3), out)
        if not check and out != s:
            open(p, 'w').write(out)
    print(f'pin_tools: {n} pins checked, {stale} stale' + ('' if check else ' (rewritten)'))
    return 1 if (n == 0 or (check and stale)) else 0


if __name__ == '__main__':
    sys.exit(main())
