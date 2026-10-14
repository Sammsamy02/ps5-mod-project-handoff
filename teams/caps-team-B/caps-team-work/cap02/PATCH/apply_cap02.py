#!/usr/bin/env python3
"""cap02 apply script for the INTEGRATOR (review F1 + F2).

Applies PATCH/01 (log-only shadow) to the candidate tree AND does the two
bookkeeping steps the build driver needs, so build_variants.py gate
G2-part-change-pins passes:

  1. copy the two new headers into <typescan-root>/candidate/include/
  2. patch -p1 --fuzz=0 -d <typescan-root>/candidate < PATCH/01-shadow-launch-ready.diff
  3. CHANGES.json row sync: every `files` row whose candidate is
     candidate/src/UE4SSProgram.cpp gets the new candidate_sha256, plus a
     `cap02_row_sync` record (same shape as r11_row_sync)
  4. add both headers to `added_headers` and `added_header_hashes`

Default is a DRY RUN (prints what it would do, writes nothing). Pass
--execute to write. Every input is sha-pinned (PATCH/MANIFEST.json and the
expected before/after sha of UE4SSProgram.cpp); any mismatch stops before
anything is written. PATCH/02 is NOT SHIPPABLE and is never applied here.

--typescan-root lets the host tests run this against a scratch copy; the
CHANGES.json rows keep their real absolute paths, and rows are matched by
their path relative to the typescan-r4 root.

Prints how many rows/headers it checked; exits non-zero when that is zero or
on any failure.
"""
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REAL_ROOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4")
REL_SRC = "candidate/src/UE4SSProgram.cpp"
BEFORE_SHA = "fe9fe7bc1f856f2268290336dc65c4f5273aa27f8766426535ccfca0a317e45a"  # R13c (R11 + R12 + cap #4)
AFTER_SHA = "ad3ae621a7c86d771f4ed56e54eddf3330b8db6a1525c5a73a8466a6c57078cf"   # + PATCH/01
DIFF = HERE / "01-shadow-launch-ready.diff"
HEADERS = ["AutoProfileGenerator.hpp", "AutoLayoutPacks.generated.hpp"]
SYNC_KEY = "cap02_row_sync"


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def fail(msg):
    print(f"FAIL: {msg}")
    sys.exit(1)


def rel_of(abs_path):
    """Path of a CHANGES.json entry relative to the typescan-r4 root, or None."""
    s = str(abs_path)
    marker = "/typescan-r4/"
    return s.split(marker, 1)[1] if marker in s else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--typescan-root", default=str(REAL_ROOT))
    ap.add_argument("--execute", action="store_true", help="write changes (default: dry run)")
    ap.add_argument("--date", default="2026-10-13")
    a = ap.parse_args()
    root = Path(a.typescan_root).resolve()
    changes_path = root / "CHANGES.json"
    src = root / REL_SRC
    inc = root / "candidate/include"
    checked = 0

    # --- pins: the patch set itself (MANIFEST.json written when the patch was reviewed)
    manifest = json.loads((HERE / "MANIFEST.json").read_text())
    for name, digest in manifest["sha256"].items():
        p = HERE / name
        if not p.exists() or sha(p) != digest:
            fail(f"patch file {name} does not match PATCH/MANIFEST.json")
        checked += 1
    # --- pins: the candidate file and its CHANGES row
    if not src.exists():
        fail(f"missing {src}")
    cur = sha(src)
    if cur != BEFORE_SHA:
        fail(f"{REL_SRC} sha {cur[:12]} != expected base {BEFORE_SHA[:12]} (tree moved; regenerate PATCH/01)")
    d = json.loads(changes_path.read_text())
    if SYNC_KEY in d:
        fail(f"CHANGES.json already has {SYNC_KEY} (already applied?)")
    rows = [r for r in d["files"] if rel_of(r["candidate"]) == REL_SRC]
    if not rows:
        fail(f"zero CHANGES.json rows for {REL_SRC}")
    for r in rows:
        if r["candidate_sha256"] != BEFORE_SHA:
            fail(f"CHANGES row pin {r['candidate_sha256'][:12]} != base {BEFORE_SHA[:12]}")
        checked += 1
    for h in HEADERS:
        if (inc / h).exists():
            fail(f"{inc / h} already exists")
        if any(Path(x).name == h for x in d.get("added_headers", [])):
            fail(f"{h} already listed in added_headers")
        checked += 1
    if not inc.is_dir():
        fail(f"missing include dir {inc}")
    # --- the diff must apply exactly (no fuzz) before anything is written
    dry = subprocess.run(["patch", "-p1", "--fuzz=0", "--dry-run", "-d", str(root / "candidate"), "-i", str(DIFF)],
                         capture_output=True, text=True)
    if dry.returncode != 0:
        fail("patch --dry-run failed:\n" + dry.stdout + dry.stderr)
    checked += 1
    if checked == 0:
        fail("checked zero items")
    print(f"pre-checks PASS: checked {checked} items ({len(rows)} CHANGES row(s), {len(HEADERS)} headers, "
          f"{len(manifest['sha256'])} pinned patch files, patch dry-run)")
    if not a.execute:
        print("DRY RUN: nothing written. Re-run with --execute to apply.")
        return 0

    # --- execute
    backup = root / f"CHANGES.json.pre-cap02-{a.date.replace('-', '')}"
    if backup.exists():
        fail(f"backup {backup} already exists")
    shutil.copy2(changes_path, backup)
    for h in HEADERS:
        shutil.copy2(HERE / "new-files" / h, inc / h)
    run = subprocess.run(["patch", "-p1", "--fuzz=0", "-d", str(root / "candidate"),
                          "-i", str(DIFF)], capture_output=True, text=True)
    if run.returncode != 0:
        fail("patch failed:\n" + run.stdout + run.stderr)
    for leftover in (Path(str(src) + ".orig"), Path(str(src) + ".rej")):
        if leftover.exists():
            fail(f"patch left {leftover.name} in the candidate tree")
    new = sha(src)
    if new != AFTER_SHA:
        fail(f"patched sha {new[:12]} != expected {AFTER_SHA[:12]}")
    for r in rows:
        r["candidate_sha256"] = new
    hdr_paths = [str(inc / h) for h in HEADERS]
    d.setdefault("added_headers", []).extend(hdr_paths)
    d.setdefault("added_header_hashes", {}).update({p: sha(p) for p in hdr_paths})
    d[SYNC_KEY] = {
        "date": a.date,
        "by": "integrator (cap02 apply_cap02.py)",
        "content": "cap02 log-only shadow: [AutoProfile] shadow line on the compiled-declaration path "
                   "(non-arming, exceptions caught); two new headers AutoProfileGenerator.hpp + "
                   "AutoLayoutPacks.generated.hpp in candidate/include.",
        "candidate_sha256": {REL_SRC.replace("candidate/", "", 1): new},
        "added_headers": {Path(p).name: sha(p) for p in hdr_paths},
        "backup": backup.name,
    }
    changes_path.write_text(json.dumps(d, indent=2) + "\n")
    # --- post-check (same rule as build_variants G2: candidate sha == row pin)
    d2 = json.loads(changes_path.read_text())
    post = 0
    for r in d2["files"]:
        if rel_of(r["candidate"]) == REL_SRC:
            if sha(src) != r["candidate_sha256"]:
                fail("post-check: row pin != file sha")
            post += 1
    for p in hdr_paths:
        if d2["added_header_hashes"].get(p) != sha(p):
            fail(f"post-check: header hash mismatch {p}")
        post += 1
    if post == 0:
        fail("post-check checked zero items")
    print(f"APPLIED: {REL_SRC} {BEFORE_SHA[:12]} -> {new[:12]}; {len(rows)} row(s) synced; "
          f"{len(hdr_paths)} headers added; backup {backup.name}; post-check {post} items PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
