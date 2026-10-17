#!/usr/bin/env python3
"""Poll /tmp/sideproject for module outputs. Mac-only host polling.

Tracks which of the 8 target modules have produced files and when their
content last changed (mtime+size snapshot), so review starts as soon as a
module appears and re-reviews when it keeps changing.
"""

import hashlib
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from framework import MODULES, module_dir, module_files  # noqa: E402

STATE = "/tmp/sideproject/tester/module_state.json"


def snapshot(name):
    d = module_dir(name)
    if not os.path.isdir(d):
        return None
    files = module_files(name)
    if not files:
        return {"empty_dir": True, "files": []}
    h = hashlib.sha256()
    meta = []
    for rel in files:
        p = os.path.join(d, rel)
        try:
            st = os.stat(p)
        except OSError:
            continue
        h.update(rel.encode())
        h.update(str(st.st_size).encode())
        h.update(str(int(st.st_mtime)).encode())
        meta.append({"file": rel, "size": st.st_size, "mtime": int(st.st_mtime)})
    return {"empty_dir": False, "count": len(meta), "digest": h.hexdigest()[:16],
            "files": [m["file"] for m in meta]}


def status(all_modules=False):
    out = {}
    for name in MODULES:
        snap = snapshot(name)
        out[name] = snap
    return out


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    wait = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    for r in range(rounds):
        st = status()
        line = []
        for name, snap in st.items():
            if snap is None:
                line.append(f"{name}:absent")
            elif snap.get("empty_dir"):
                line.append(f"{name}:empty")
            else:
                line.append(f"{name}:{snap['count']}files/{snap['digest'][:8]}")
        print(f"[{time.strftime('%H:%M:%S')}] " + " ".join(line))
        if r < rounds - 1:
            time.sleep(wait)
    # persist
    with open(STATE, "w") as f:
        json.dump({k: (v and {kk: vv for kk, vv in v.items() if kk != "files"})
                   for k, v in st.items()}, f, indent=1)


if __name__ == "__main__":
    main()
