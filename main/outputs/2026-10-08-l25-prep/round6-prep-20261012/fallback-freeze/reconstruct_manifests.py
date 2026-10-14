#!/usr/bin/env python3
"""reconstruct_manifests.py -- rebuild the marker manifest each fallback build
was verified with, and write it into THIS directory only.

Mac-only, read-only on every input. harvest/markers-declared.json is never
written (it is opened for reading only), and harvest/add_build_markers.py is
never run: its SETS literal is read with ast.literal_eval.

Matching manifest per build (from each build's own evidence log):
  B1-lookup-nocache          19 markers  evidence/.../B1-lookup-nocache/verify_build.txt
  B2b-error-wrapper-memlog   21 markers  evidence/.../B2b-error-wrapper-memlog/verify_build-with-B2-markers.txt
  B3-iterators               23 markers  evidence/.../B3-iterators/verify_build.txt

Sources compared (all must agree):
  S1 harvest/markers-declared.json.pre-B2markers-20261012 (19) + the B2/B3
     SETS of harvest/add_build_markers.py, appended exactly as its main() does
  S2 harvest/markers-declared.json today (24), first N markers
  S3 the copy of harvest/markers-declared.json inside each build's own source
     snapshot tar (taken just before that build started)
  S4 the marker ids + entry texts printed in each build's evidence verify log

Independence (r2 correction): S1 and S2 are NOT independent of each other.
S2 is add_build_markers.py's own output from the same pre-B2 base and SETS, so
S1 == S2 only shows the reconstruction re-derives the file the script wrote.
Truly independent of S1/S2: S3 (a byte copy taken before each build) and S4
(what verify_build printed at build time). Per build:
  B1  (19): S3 byte-identical + S4 ids/texts       -> 2 independent sources
  B3  (23): S3 byte-identical + S4 ids/texts       -> 2 independent sources
  B2b (21): base 19 = S3 + S4; the 2 B2 markers    -> S4 only (S3 predates them)

Prints how many checks ran; exits 1 if any failed or if zero ran.
"""
import ast
import hashlib
import json
import os
import re
import sys
import tarfile

OUT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"
HARVEST = os.path.join(OUT, "2026-10-08-l25-prep/harvest")
EVID = os.path.join(OUT, "2026-10-08-l25-prep/evidence/builds-20261012")
SNAPS = os.path.join(OUT, "2026-10-08-l26-runtime/snapshots")
HERE = os.path.dirname(os.path.abspath(__file__))
PRE_B2 = os.path.join(HARVEST, "markers-declared.json.pre-B2markers-20261012")
CURRENT = os.path.join(HARVEST, "markers-declared.json")
ADDER = os.path.join(HARVEST, "add_build_markers.py")
MEMBER = "2026-10-08-l25-prep/harvest/markers-declared.json"

BUILDS = [
    # label, marker count, steps appended after the pre-B2 19, evidence log, snapshot ts
    ("B1-lookup-nocache", 19, [], "verify_build.txt", "20261012T032300Z"),
    ("B2b-error-wrapper-memlog", 21, ["B2"], "verify_build-with-B2-markers.txt", "20261012T033928Z"),
    ("B3-iterators", 23, ["B2", "B3"], "verify_build.txt", "20261012T035107Z"),
]

n = 0
failed = []


def ok(cond, what):
    global n
    n += 1
    print("  [%s] %s" % ("PASS" if cond else "FAIL", what))
    if not cond:
        failed.append(what)
    return bool(cond)


def sha(b):
    return hashlib.sha256(b).hexdigest()


def read_sets():
    tree = ast.parse(open(ADDER).read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "SETS" for t in node.targets):
            return ast.literal_eval(node.value)
    return None


def built(step, entries):
    # mirrors add_build_markers.py main(): lines 41-44 of that file
    return [{"id": mid, "fix": "overnight 2026-10-12 labelled build " + step,
             "canonical_marker": text.split(" ")[0],
             "required": [{"text": text, "encodings": [enc], "source": src}],
             "note": "added by add_build_markers.py before build " + step}
            for mid, text, enc, src in entries]


ENTRY_RE = re.compile(r"^\s+entry (?P<r>'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\")\s")
HEAD_RE = re.compile(r"^manifest: .* \(schema (?P<schema>\S+), (?P<n>\d+) declared markers\)")
MARK_RE = re.compile(r"^\[(?P<st>PASS|MISS)\] (?P<id>\S+)\s")


def parse_log(path):
    declared, ids, texts, cur = None, [], [], None
    summary = None
    for line in open(path, encoding="utf-8", errors="replace"):
        m = HEAD_RE.match(line)
        if m:
            declared = int(m.group("n"))
        m = MARK_RE.match(line)
        if m:
            ids.append((m.group("st"), m.group("id")))
            cur = []
            texts.append(cur)
            continue
        m = ENTRY_RE.match(line)
        if m and cur is not None:
            cur.append(ast.literal_eval(m.group("r")))
        if line.startswith("SUMMARY: "):
            summary = line.strip()
    return declared, ids, texts, summary


def main():
    print("== inputs (read-only) ==")
    pre_b = open(PRE_B2, "rb").read()
    cur_b = open(CURRENT, "rb").read()
    P, C = json.loads(pre_b), json.loads(cur_b)
    print("  pre-B2  %s sha256=%s markers=%d" % (PRE_B2, sha(pre_b), len(P["markers"])))
    print("  current %s sha256=%s markers=%d" % (CURRENT, sha(cur_b), len(C["markers"])))
    ok(len(P["markers"]) == 19, "pre-B2 backup has 19 markers")
    ok(len(C["markers"]) == 24, "current harvest manifest has 24 markers")
    ok({k: v for k, v in P.items() if k != "markers"} == {k: v for k, v in C.items() if k != "markers"},
       "pre-B2 and current agree on every non-marker key (%s)" % ", ".join(k for k in P if k != "markers"))
    ok(C["markers"][:19] == P["markers"], "current markers[0:19] == pre-B2 markers (S1 == S2 for the base 19)")
    SETS = read_sets()
    ok(isinstance(SETS, dict) and sorted(SETS) == ["B2", "B3", "B4"],
       "add_build_markers.py SETS literal read via ast (keys %s)" % (sorted(SETS) if SETS else None))
    ext = {k: built(k, v) for k, v in (SETS or {}).items()}
    ok(C["markers"][19:21] == ext.get("B2"), "current markers[19:21] == SETS['B2'] as appended by the script")
    ok(C["markers"][21:23] == ext.get("B3"), "current markers[21:23] == SETS['B3'] as appended by the script")
    ok(C["markers"][23:24] == ext.get("B4"), "current markers[23:24] == SETS['B4'] (the B4-only marker)")
    ok(json.dumps(C, indent=1).encode() == cur_b,
       "json.dumps(indent=1) reproduces the current file byte-for-byte (same writer format as the script)")

    provenance = {"schema": "fallback-marker-manifests/1", "mac_only": True, "console_contact": False,
                  "inputs": {"pre_b2": {"path": PRE_B2, "sha256": sha(pre_b)},
                             "current": {"path": CURRENT, "sha256": sha(cur_b)},
                             "add_build_markers": {"path": ADDER, "sha256": sha(open(ADDER, "rb").read())}},
                  "manifests": []}
    for label, count, steps, logname, ts in BUILDS:
        print("== %s (%d markers) ==" % (label, count))
        s1 = dict(P)
        s1["markers"] = list(P["markers"]) + [m for st in steps for m in ext.get(st, [])]
        s2 = dict(C)
        s2["markers"] = C["markers"][:count]
        ok(len(s1["markers"]) == count, "%s: S1 (pre-B2 + %s) has %d markers" % (label, "+".join(steps) or "nothing", count))
        ok(s1 == s2, "%s: S1 == S2 (current manifest, first %d)" % (label, count))
        with tarfile.open(os.path.join(SNAPS, "snapshot-%s.tar.gz" % ts)) as t:
            snap_b = t.extractfile(MEMBER).read()
        S3 = json.loads(snap_b)
        print("  snapshot-%s member sha256=%s markers=%d" % (ts, sha(snap_b), len(S3["markers"])))
        if len(S3["markers"]) == count:
            ok(S3 == s1, "%s: S3 (its own snapshot copy) == S1 exactly" % label)
        else:
            # B2b: its snapshot was taken before the B2 markers were added (19 markers);
            # its base must still equal S1's base, and the B2 additions come from S1/S2/S4.
            ok(S3["markers"] == s1["markers"][:len(S3["markers"])] and len(S3["markers"]) == 19,
               "%s: S3 snapshot copy holds the 19-marker base (taken before the B2 markers were added) == S1 base" % label)
        log = os.path.join(EVID, label, logname)
        declared, ids, texts, summary = parse_log(log)
        print("  evidence log %s: declared=%s markers_listed=%d %s" % (log, declared, len(ids), summary))
        ok(declared == count, "%s: S4 evidence log header says %s declared" % (label, declared))
        ok([i for _, i in ids] == [m["id"] for m in s1["markers"]], "%s: S4 marker ids (in order) == reconstructed" % label)
        ok(all(st == "PASS" for st, _ in ids) and len(ids) == count, "%s: S4 every marker PASS in the original verify" % label)
        ok(texts == [[r["text"] for r in m["required"]] for m in s1["markers"]],
           "%s: S4 required entry texts (per marker) == reconstructed" % label)
        out_b = json.dumps(s1, indent=1).encode()
        if len(S3["markers"]) == count:
            ok(out_b == snap_b, "%s: written bytes == its snapshot copy byte-for-byte" % label)
        if not steps:
            ok(out_b == pre_b, "%s: written bytes == pre-B2 backup byte-for-byte" % label)
        dst = os.path.join(HERE, "markers-%s-%d.json" % (label, count))
        if os.path.exists(dst):
            ok(open(dst, "rb").read() == out_b, "%s: existing %s already identical (not rewritten)" % (label, dst))
        else:
            with open(dst, "wb") as f:
                f.write(out_b)
            ok(open(dst, "rb").read() == out_b, "%s: wrote %s" % (label, dst))
        indep = (["S3 snapshot copy (byte-identical)", "S4 evidence verify log (ids + entry texts)"]
                 if len(S3["markers"]) == count else
                 ["base 19: S3 snapshot copy + S4 evidence verify log",
                  "the %d added markers: S4 evidence verify log only (S3 predates them)" % (count - len(S3["markers"]))])
        provenance["manifests"].append({"label": label, "path": dst, "markers": count, "sha256": sha(out_b),
                                        "independent_sources": indep,
                                        "not_independent": "S1 (pre-B2 + SETS) and S2 (today's file) share inputs: "
                                                           "S2 was written by add_build_markers.py from the same base + SETS",
                                        "ids": [m["id"] for m in s1["markers"]],
                                        "evidence_log": log, "snapshot_member_sha256": sha(snap_b),
                                        "snapshot_member_markers": len(S3["markers"])})
    with open(os.path.join(HERE, "MANIFESTS-PROVENANCE.json"), "w") as f:
        json.dump(provenance, f, indent=1)
    print("SUMMARY reconstruct: checks=%d pass=%d fail=%d" % (n, n - len(failed), len(failed)))
    if n == 0:
        print("FAIL: zero checks ran")
        return 1
    if failed:
        print("FAIL: " + "; ".join(failed))
        return 1
    print("RESULT reconstruct: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
