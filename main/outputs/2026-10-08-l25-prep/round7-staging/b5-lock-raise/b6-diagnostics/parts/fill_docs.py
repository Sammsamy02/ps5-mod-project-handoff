#!/usr/bin/env python3
"""Rewrites the sha lists (SHA256SUMS-*.txt) and the sha mentions in CHANGES-ROWS.md from the current work/ base/ b6.diff.
Prints how many files it hashed; exits 1 on zero."""
import hashlib, os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()
files = ['include/LuaMadeSimple/LuaMadeSimple.hpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp', 'src/LuaMod.cpp', 'src/LuaPathShim.cpp']
n = 0
for d in ('base', 'work'):
    open(os.path.join(root, 'SHA256SUMS-%s.txt' % d), 'w').write(''.join('%s  ./%s\n' % (sha(os.path.join(root, d, f)), f) for f in files)); n += len(files)
open(os.path.join(root, 'SHA256SUMS-diff.txt'), 'w').write('%s  b6.diff\n' % sha(os.path.join(root, 'b6.diff'))); n += 1
p = os.path.join(root, 'CHANGES-ROWS.md'); s = open(p).read()
w = {f: sha(os.path.join(root, 'work', f)) for f in files}
for key, f in (('LuaMod.cpp', 'src/LuaMod.cpp'), ('LuaMadeSimple.cpp', 'src/LuaMadeSimple/LuaMadeSimple.cpp'), ('LuaMadeSimple.hpp', 'include/LuaMadeSimple/LuaMadeSimple.hpp'), ('LuaPathShim.cpp', 'src/LuaPathShim.cpp')):
    s = re.sub(r'(candidate/(?:src|include)/(?:LuaMadeSimple/)?%s", "candidate_sha256": "<fill[^>]*?staged copy is )[0-9a-f]{8}' % re.escape(key), lambda m: m.group(1) + w[f][:8], s)
open(p, 'w').write(s)
print('fill_docs: hashed %d files' % n)
sys.exit(0 if n else 1)
