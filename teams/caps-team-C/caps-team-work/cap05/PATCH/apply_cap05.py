#!/usr/bin/env python3
"""apply_cap05.py - stage the cap05 crash-test insertion into a COPY of LuaMod.cpp.

Never edits its input. Refuses unless each anchor occurs exactly once and the
markers are not already present. Writes the patched copy and a unified diff.

  python3 -I apply_cap05.py --base <candidate>/src/LuaMod.cpp \
      --out-file /path/LuaMod.cpp.cap05 --out-diff /path/LuaMod.cpp.cap05.diff

Exit 0 = both insertions applied (count printed); 2 = refused.
"""
import argparse
import difflib
import hashlib
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cap05_crashtest_snippets as S  # noqa: E402


def stage(text):
    if "[CAP05-CRASHTEST-BEGIN]" in text or "ModBridgeCrashTest" in text:
        raise ValueError("cap05 insertion already present - refusing to double-apply")
    for name, anchor in (("ANCHOR_CORE", S.ANCHOR_CORE), ("ANCHOR_LUA", S.ANCHOR_LUA)):
        n = text.count(anchor)
        if n != 1:
            raise ValueError("%s occurs %d times (need exactly 1) - refusing" % (name, n))
    if text.index(S.ANCHOR_CORE) > text.index(S.ANCHOR_LUA):
        raise ValueError("anchor order unexpected (core helper must precede the Lua registration)")
    out = text.replace(S.ANCHOR_CORE, S.CORE + S.ANCHOR_CORE, 1)
    out = out.replace(S.ANCHOR_LUA, S.LUA + S.ANCHOR_LUA, 1)
    # inverse check: removing the insertions restores the input byte for byte
    if out.replace(S.CORE, "", 1).replace(S.LUA, "", 1) != text:
        raise ValueError("inverse check failed")
    return out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--out-file", required=True)
    ap.add_argument("--out-diff", required=True)
    ap.add_argument("--label", default="candidate/src/LuaMod.cpp")
    a = ap.parse_args(argv)
    for p in (a.out_file, a.out_diff):
        if os.path.exists(p):
            print("apply_cap05: refusing to overwrite %s" % p, file=sys.stderr)
            return 2
    with open(a.base, "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")
    try:
        out = stage(text)
    except ValueError as e:
        print("apply_cap05: REFUSED: %s" % e, file=sys.stderr)
        return 2
    with open(a.out_file, "x", encoding="utf-8", newline="") as f:
        f.write(out)
    diff = difflib.unified_diff(text.splitlines(True), out.splitlines(True),
                                fromfile="a/" + a.label, tofile="b/" + a.label)
    with open(a.out_diff, "x", encoding="utf-8", newline="") as f:
        f.writelines(diff)
    print("apply_cap05: PASS anchors=2 insertions=2 base_sha256=%s out_sha256=%s added_lines=%d"
          % (hashlib.sha256(raw).hexdigest(), hashlib.sha256(out.encode()).hexdigest(),
             out.count("\n") - text.count("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
