#!/usr/bin/env python3
"""Re-run keymap-gen.py --check (read-only) on the pinned PS5Pad.ini and on the
future-only candidate, with all 7 pinned Lua mods (sha-verified copies).
Mac-only. Writes nothing; prints how many mods and files it checked and exits
non-zero when any count is zero or a check result differs from the expected
(pinned: 0 violations / 0 warnings; candidate: 0 violations / 8 orphan warnings).

Run: python3 -I rerun_keymap_checks.py [--root WORKTREE]
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PINS = "outputs/2026-10-08-l25-prep/morning-launch-20261012/rowcheck-pins.json"
GEN = "outputs/2026-10-08-l25-prep/keymap/keymap-gen.py"
PINNED_PAD = "outputs/2026-10-08-l25-prep/ueselftest/PS5Pad-round4-proposed.ini"
CANDIDATE = "outputs/2026-10-08-l25-prep/round6-prep-20261012/minimap-keys/PS5Pad.FUTURE-minimap-DO-NOT-UPLOAD.ini"
# keymap-gen's --pin-pad input: the l23 console-harvested 3718 B profile.
PIN_PAD = ("outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/"
           "harvest-20261008T174239Z-25929d/PS5Pad.ini")
PIN_PAD_SHA_PREFIX = "07808703f0fe130b"
ROOTS = ["outputs/2026-10-08-l25-prep/morning-launch-20261012/upload",
         "outputs/2026-10-08-l25-prep/evidence/morning-readonly-20261012T114008Z/spool/rows",
         ".superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/minimap-v3.1"]
EXPECT = {"pinned": (0, 0), "candidate": (0, 8)}


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(HERE.parents[4]))
    root = Path(ap.parse_args().root).resolve()
    fails = []
    pins = json.loads((root / PINS).read_text())
    mods = []
    for rel, p in pins.items():
        if not rel.endswith("/Scripts/main.lua"):
            continue
        for r in ROOTS:
            f = root / r / rel
            if f.is_file():
                b = f.read_bytes()
                if len(b) == p["size"] and sha(b) == p["sha256"]:
                    mods.append((rel.split("/")[2], f))
                    break
        else:
            fails.append(f"no sha-verified local copy of {rel}")
    print(f"mods with a verified copy: {len(mods)}")
    pp = (root / PIN_PAD).read_bytes()
    if not sha(pp).startswith(PIN_PAD_SHA_PREFIX) or len(pp) != 3718:
        fails.append("pin-pad input is not the 3718 B l23 profile")
    checked = 0
    for label, ini, size in (("pinned", PINNED_PAD, 1736), ("candidate", CANDIDATE, 1669)):
        argv = [sys.executable, "-I", str(root / GEN), "--check", str(root / ini),
                "--pin-pad", str(root / PIN_PAD), "--expect-size", str(size)]
        for name, f in mods:
            argv += ["--mod", f"{name}={f}"]
        out = subprocess.run(argv, capture_output=True, text=True, cwd=str(root))
        print(f"## {label}: keymap-gen.py --check {ini} (exit {out.returncode})")
        print(out.stdout.rstrip())
        if out.stderr.strip():
            print(out.stderr.rstrip())
        m = re.search(r"check: (\d+) violations?, (\d+) warnings?", out.stdout)
        checked += 1
        if not m:
            fails.append(f"{label}: no 'check:' summary line")
        elif (int(m.group(1)), int(m.group(2))) != EXPECT[label]:
            fails.append(f"{label}: got {m.group(0)}, expected {EXPECT[label]}")
    print(f"files checked: {checked}; mods: {len(mods)}")
    if len(mods) != 7:
        fails.append(f"expected 7 mods, found {len(mods)}")
    for f in fails:
        print(f"FAIL: {f}")
    ok = checked > 0 and len(mods) > 0 and not fails
    print("overall", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
