#!/usr/bin/env python3
"""freeze_fallbacks.py -- make read-only frozen copies of the fallback PRXs
(B3-iterators, B2b-error-wrapper-memlog, B1-lookup-nocache), mirroring the
layout of 2026-10-08-l26-frozen/B4-shim-final-stack-d044cf0b/.

Mac-only. No network, no console, no builds. Copies, never moves.

Modes (exactly one):
  --preflight   read-only: every sha / size / magic / snapshot cross-check
                between the task values, BUILD-LEDGER-20261012.md, the build
                evidence (RESULT, snapshot.stdout/stderr, FINAL-RECEIPT) and
                the archived files. Writes nothing.
  --freeze      runs --preflight; only if it passes, creates each NEW frozen
                dir 2026-10-08-l26-frozen/<label>-<sha8>/ (refuses if it
                already exists), copies the 6 source files (copy2 keeps
                mtimes, as the B4 copy did), writes <prx>.sha256 in the B4
                format, verifies every copy byte-for-byte by sha256, then sets
                files 0444 and the dir 0555. Then runs --check.
  --check       read-only: re-verifies the frozen dirs (file set, modes,
                hashes against the sources and the receipts).

Every mode prints how many checks it ran and exits 1 if any check failed or
if the number of checks is zero. Exit 2 = usage error.
"""
import hashlib
import json
import os
import re
import shutil
import stat
import sys

OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"
LEDGER = os.path.join(OUT, "2026-10-08-l25-prep/BUILD-LEDGER-20261012.md")
EVID = os.path.join(OUT, "2026-10-08-l25-prep/evidence/builds-20261012")
ARCH = os.path.join(OUT, "2026-10-08-l26-build-variants/archive-r6")
FROZEN = os.path.join(OUT, "2026-10-08-l26-frozen")
PRX_NAME = "modbridge_supplier_composition.prx"
RECEIPT_NAME = "FINAL-RECEIPT-VARIANT-A.json"
SYMBOLS_NAME = PRX_NAME + ".symbols.json"
MAGIC = "4f153d1d"

# Source 1: the values given in the task (typed by the driver session).
BUILDS = [
    ("B3-iterators",
     "40b8972b58e313c61b84e2665d8c166887b49712753b000ff4f0cf919cfb9175"),
    ("B2b-error-wrapper-memlog",
     "74fab85a5165c17a4d169f8efdb25e0e647fa7b867ed7843a7b4869a5f8c58b2"),
    ("B1-lookup-nocache",
     "b9cc52485c51e5ebbaf1944e01f472f65d19136e28d3a0ef965756b1936aa909"),
]
# Never written by this script: --freeze refuses these as destinations
# (freeze_one). No mode reads them; their before/after fingerprints
# (protected-frozen-fingerprint-*.txt) are taken separately.
PROTECTED = ("B4-shim-final-stack-d044cf0b", "1e49188b-acceptance-pass1")


class Checks:
    def __init__(self):
        self.n = 0
        self.failed = []

    def ok(self, cond, what):
        self.n += 1
        print("  [%s] %s" % ("PASS" if cond else "FAIL", what))
        if not cond:
            self.failed.append(what)
        return bool(cond)

    def finish(self, mode):
        print("SUMMARY %s: checks=%d pass=%d fail=%d"
              % (mode, self.n, self.n - len(self.failed), len(self.failed)))
        if self.n == 0:
            print("FAIL: zero checks ran -- a gate that checks nothing fails")
            return 1
        if self.failed:
            print("FAIL: " + "; ".join(self.failed))
            return 1
        print("RESULT %s: PASS" % mode)
        return 0


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def magic(path):
    with open(path, "rb") as f:
        return f.read(4).hex()


def kv_file(path):
    d = {}
    with open(path) as f:
        for line in f:
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                d[k.strip()] = v.strip()
    return d


def ledger_row(label):
    """Source 2: BUILD-LEDGER-20261012.md row '| <label> | `sha` | snap | id |'."""
    pat = re.compile(r"^\|\s*%s\s*\|\s*`([0-9a-f]{64})`\s*\|\s*([0-9a-f]{8})"
                     r"\s*\|\s*(\S+)\s*\|" % re.escape(label))
    rows = []
    with open(LEDGER) as f:
        for line in f:
            m = pat.match(line)
            if m:
                rows.append(m.groups())
    return rows


def snapshot_info(label):
    """snapshot.stdout: SNAPSHOT-ID / SNAPSHOT-TAR / SNAPSHOT-MANIFEST."""
    d = {}
    with open(os.path.join(EVID, label, "snapshot.stdout")) as f:
        for line in f:
            parts = line.split(None, 1)
            if len(parts) == 2:
                d[parts[0]] = parts[1].strip()
    return d


def sources(label):
    pkg = os.path.join(ARCH, label, "2026-10-08-l26-package-A")
    s = snapshot_info(label)
    tar = s.get("SNAPSHOT-TAR", "")
    base = tar[:-len(".tar.gz")] if tar.endswith(".tar.gz") else tar
    return {
        "prx": os.path.join(pkg, "pass-1", PRX_NAME),
        "twin": os.path.join(pkg, "pass-2", PRX_NAME),
        "receipt": os.path.join(pkg, RECEIPT_NAME),
        "symbols": os.path.join(pkg, SYMBOLS_NAME),
        "tar": tar,
        "manifest": s.get("SNAPSHOT-MANIFEST", ""),
        "snapsha": base + ".sha256",
        "snapshot_id": s.get("SNAPSHOT-ID", ""),
    }


def dest_dir(label, sha):
    return os.path.join(FROZEN, "%s-%s" % (label, sha[:8]))


def preflight(c):
    print("== PREFLIGHT (read-only) ==")
    for label, exp in BUILDS:
        print("-- %s (task sha %s)" % (label, exp))
        rows = ledger_row(label)
        c.ok(len(rows) == 1, "%s: exactly one ledger row (found %d)" % (label, len(rows)))
        lsha, lsnap, lbid = rows[0] if rows else ("", "", "")
        c.ok(lsha == exp, "%s: ledger sha == task sha (%s)" % (label, lsha))
        res = kv_file(os.path.join(EVID, label, "RESULT"))
        c.ok(res.get("RESULT") == "PASS", "%s: evidence RESULT=PASS" % label)
        c.ok(res.get("LABEL") == label, "%s: evidence RESULT LABEL matches" % label)
        c.ok(res.get("PRX_SHA256") == exp, "%s: evidence RESULT PRX_SHA256 == task sha" % label)
        c.ok(res.get("MAGIC") == MAGIC, "%s: evidence RESULT MAGIC=%s" % (label, MAGIC))
        c.ok(res.get("BUILD_ID") == lbid, "%s: build id RESULT %s == ledger %s"
             % (label, res.get("BUILD_ID"), lbid))
        s = sources(label)
        for k in ("prx", "twin", "receipt", "symbols", "tar", "manifest", "snapsha"):
            c.ok(bool(s[k]) and os.path.isfile(s[k]), "%s: source %s exists: %s" % (label, k, s[k]))
        if not all(os.path.isfile(s[k]) for k in ("prx", "twin", "receipt", "symbols", "tar", "manifest", "snapsha")):
            continue
        psha, tsha = sha256(s["prx"]), sha256(s["twin"])
        size = os.path.getsize(s["prx"])
        c.ok(psha == exp, "%s: sha256(archive pass-1 PRX) == task sha" % label)
        c.ok(tsha == exp, "%s: sha256(archive pass-2 twin PRX) == task sha" % label)
        c.ok(magic(s["prx"]) == MAGIC, "%s: PRX magic %s" % (label, magic(s["prx"])))
        c.ok(str(size) == res.get("BYTES"), "%s: PRX bytes %d == RESULT BYTES %s" % (label, size, res.get("BYTES")))
        rc = json.load(open(s["receipt"]))
        c.ok((rc.get("prx") or {}).get("sha256") == exp, "%s: receipt prx.sha256 == task sha" % label)
        c.ok((rc.get("prx") or {}).get("bytes") == size, "%s: receipt prx.bytes == PRX bytes" % label)
        c.ok((rc.get("twin") or {}).get("sha256") == exp, "%s: receipt twin.sha256 == task sha" % label)
        c.ok((rc.get("variant") or {}).get("key") == "A" and (rc.get("variant") or {}).get("unsafe") is False,
             "%s: receipt variant A, unsafe=false" % label)
        ev_receipt = os.path.join(EVID, label, RECEIPT_NAME)
        c.ok(os.path.isfile(ev_receipt) and sha256(ev_receipt) == sha256(s["receipt"]),
             "%s: archive receipt byte-identical to evidence receipt" % label)
        sc = rc.get("symbols_sidecar") or {}
        c.ok(sc.get("sha256") == sha256(s["symbols"]), "%s: sha256(symbols) == receipt symbols_sidecar.sha256" % label)
        c.ok(sc.get("prx_sha256") == exp, "%s: receipt symbols_sidecar.prx_sha256 == task sha" % label)
        c.ok(sc.get("bytes") == os.path.getsize(s["symbols"]), "%s: symbols bytes == receipt" % label)
        snap_id = s["snapshot_id"]
        c.ok(snap_id == res.get("SNAPSHOT_ID") == lsnap,
             "%s: snapshot id stdout %s == RESULT %s == ledger %s" % (label, snap_id, res.get("SNAPSHOT_ID"), lsnap))
        tar_sha = sha256(s["tar"])
        c.ok(tar_sha.startswith(snap_id), "%s: sha256(snapshot tar) starts with id %s" % (label, snap_id))
        tok = open(s["snapsha"]).read().split()
        c.ok(len(tok) == 2 and tok[0] == tar_sha and tok[1] == os.path.basename(s["tar"]),
             "%s: snapshot .sha256 file == sha256(tar) and names the tar" % label)
        with open(os.path.join(EVID, label, "snapshot.stderr")) as f:
            stderr_txt = f.read()
        c.ok(("(sha256 %s)" % tar_sha) in stderr_txt, "%s: snapshot.stderr records the same tar sha" % label)
        man = json.load(open(s["manifest"]))
        c.ok(man.get("snapshot_id") == snap_id and (man.get("tar") or {}).get("sha256") == tar_sha,
             "%s: snapshot manifest id + tar sha match" % label)
        cs = json.load(open(os.path.join(ARCH, label, "l26-A", "CHANGE-SELECTION.json")))
        bs = cs.get("build_stamp") or {}
        c.ok(bs.get("snapshot_id") == snap_id and bs.get("build_id") == lbid,
             "%s: archived CHANGE-SELECTION build_stamp snapshot %s / build %s"
             % (label, bs.get("snapshot_id"), bs.get("build_id")))
        d = dest_dir(label, exp)
        print("  info: destination %s %s" % (d, "EXISTS" if os.path.exists(d) else "(not yet created)"))


def freeze_one(c, label, exp):
    s = sources(label)
    d = dest_dir(label, exp)
    if os.path.basename(d) in PROTECTED:
        c.ok(False, "%s: refusing protected destination %s" % (label, d))
        return
    if os.path.exists(d):
        c.ok(False, "%s: destination already exists, not overwritten: %s" % (label, d))
        return
    os.mkdir(d, 0o755)
    plan = [(s["prx"], PRX_NAME), (s["receipt"], RECEIPT_NAME), (s["symbols"], SYMBOLS_NAME),
            (s["tar"], os.path.basename(s["tar"])), (s["manifest"], os.path.basename(s["manifest"])),
            (s["snapsha"], os.path.basename(s["snapsha"]))]
    for src, name in plan:
        dst = os.path.join(d, name)
        shutil.copy2(src, dst)  # copy, never move; keeps mtime like the B4 copy
        c.ok(sha256(dst) == sha256(src), "%s: copied %s (sha256 identical to source)" % (label, name))
    with open(os.path.join(d, PRX_NAME + ".sha256"), "w") as f:
        f.write("%s  %s\n" % (exp, PRX_NAME))  # same format as the B4 frozen dir
    c.ok(open(os.path.join(d, PRX_NAME + ".sha256")).read() == "%s  %s\n" % (exp, PRX_NAME),
         "%s: wrote %s.sha256" % (label, PRX_NAME))
    for name in os.listdir(d):
        os.chmod(os.path.join(d, name), 0o444)
    os.chmod(d, 0o555)
    print("  froze %s" % d)


def check_dir(c, label, exp, compare_sources=True):
    d = dest_dir(label, exp) if label else None
    print("-- check %s" % d)
    if not os.path.isdir(d):
        c.ok(False, "%s: frozen dir exists" % label)
        return
    s = sources(label) if compare_sources else None
    names = sorted(os.listdir(d))
    snap_names = [n for n in names if n.startswith("snapshot-")]
    want = sorted([PRX_NAME, PRX_NAME + ".sha256", SYMBOLS_NAME, RECEIPT_NAME] + snap_names)
    c.ok(len(names) == 7 and names == want and len(snap_names) == 3,
         "%s: exactly the 7 B4-layout files (%d present)" % (label, len(names)))
    c.ok(stat.S_IMODE(os.stat(d).st_mode) == 0o555, "%s: dir mode 555 (%o)" % (label, stat.S_IMODE(os.stat(d).st_mode)))
    for n in names:
        m = stat.S_IMODE(os.stat(os.path.join(d, n)).st_mode)
        c.ok(m == 0o444, "%s: %s mode 444 (%o)" % (label, n, m))
    prx = os.path.join(d, PRX_NAME)
    psha = sha256(prx)
    c.ok(psha == exp, "%s: frozen PRX sha256 == expected" % label)
    c.ok(magic(prx) == MAGIC, "%s: frozen PRX magic %s" % (label, magic(prx)))
    tok = open(prx + ".sha256").read().split()
    c.ok(tok == [exp, PRX_NAME], "%s: .prx.sha256 content == expected sha + name" % label)
    rc = json.load(open(os.path.join(d, RECEIPT_NAME)))
    c.ok((rc.get("prx") or {}).get("sha256") == exp and (rc.get("prx") or {}).get("bytes") == os.path.getsize(prx),
         "%s: frozen receipt prx block matches the frozen PRX" % label)
    c.ok((rc.get("symbols_sidecar") or {}).get("sha256") == sha256(os.path.join(d, SYMBOLS_NAME)),
         "%s: frozen symbols sha == receipt symbols_sidecar.sha256" % label)
    tars = [n for n in snap_names if n.endswith(".tar.gz")]
    shas = [n for n in snap_names if n.endswith(".sha256")]
    mans = [n for n in snap_names if n.endswith(".manifest.json")]
    if len(tars) == 1 and len(shas) == 1 and len(mans) == 1:
        tsha = sha256(os.path.join(d, tars[0]))
        tok2 = open(os.path.join(d, shas[0])).read().split()
        c.ok(tok2 == [tsha, tars[0]], "%s: snapshot .sha256 == sha256(frozen tar)" % label)
        man = json.load(open(os.path.join(d, mans[0])))
        c.ok((man.get("tar") or {}).get("sha256") == tsha and tsha.startswith(man.get("snapshot_id", "-")),
             "%s: snapshot manifest matches frozen tar (id %s)" % (label, man.get("snapshot_id")))
    else:
        c.ok(False, "%s: one tar + one sha256 + one manifest" % label)
    if compare_sources:
        pairs = [(s["prx"], PRX_NAME), (s["receipt"], RECEIPT_NAME), (s["symbols"], SYMBOLS_NAME),
                 (s["tar"], os.path.basename(s["tar"])), (s["manifest"], os.path.basename(s["manifest"])),
                 (s["snapsha"], os.path.basename(s["snapsha"]))]
        for src, name in pairs:
            c.ok(os.path.isfile(os.path.join(d, name)) and sha256(src) == sha256(os.path.join(d, name)),
                 "%s: frozen %s byte-identical to its source" % (label, name))


def main(argv):
    if len(argv) != 1 or argv[0] not in ("--preflight", "--freeze", "--check"):
        print(__doc__)
        return 2
    mode = argv[0]
    c = Checks()
    if mode in ("--preflight", "--freeze"):
        preflight(c)
        if mode == "--preflight":
            return c.finish("preflight")
        if c.failed or c.n == 0:
            print("preflight FAILED -- nothing frozen")
            return c.finish("preflight")
        print("== FREEZE ==")
        for label, exp in BUILDS:
            freeze_one(c, label, exp)
    print("== CHECK (read-only) ==")
    for label, exp in BUILDS:
        check_dir(c, label, exp)
    return c.finish(mode.lstrip("-"))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
