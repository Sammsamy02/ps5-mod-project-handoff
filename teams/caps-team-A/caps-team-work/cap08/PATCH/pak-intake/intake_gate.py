#!/usr/bin/env python3
"""Pre-install gate for the #7 mount step: a pak mod may be installed only with a
fresh chunk_intake report for EXACTLY these mod files.  Mac-only.

PASS (exit 0) when the report's mod utoc/ucas sha256 equal the files about to be
installed, the report was made against EXACTLY the game base files given here
(every base .utoc and the ScriptObjects .utoc, by sha256; a report made before
a game update therefore fails), the report checked a non-zero number of items,
and the verdict is ADMITTED, or ADMITTED-WITH-VISUAL-LOSS with
--accept-visual-loss (the user's recorded acknowledgement).  Anything else
exits 2.  Every run (PASS or FAIL) writes a JSON record to --record, so the
acceptance is kept in the evidence tree, not only printed."""
import argparse
import datetime
import hashlib
import json
import os
import sys

sys.dont_write_bytecode = True


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--report", required=True)
    ap.add_argument("--mod-utoc", required=True)
    ap.add_argument("--mod-ucas", required=True)
    ap.add_argument("--base-utoc", action="append", required=True,
                    help="each game base .utoc the report must have been made with (repeat)")
    ap.add_argument("--script-objects-utoc", required=True, help="the game's global .utoc (ScriptObjects)")
    ap.add_argument("--accept-visual-loss", metavar="WHO_AND_WHEN",
                    help="text recording who accepted the disclosed visual loss and when")
    ap.add_argument("--record", required=True, help="write the gate record (JSON) here, e.g. in the evidence tree")
    a = ap.parse_args(argv)
    with open(a.report) as f:
        r = json.load(f)
    problems = []
    mod = r.get("inputs", {}).get("mod", {})
    if mod.get("utoc_sha256") != sha(a.mod_utoc):
        problems.append("report is for a different .utoc")
    if mod.get("ucas_sha256") != sha(a.mod_ucas):
        problems.append("report is for a different .ucas")
    want = sorted(b.get("sha256") or "" for b in r.get("inputs", {}).get("base_utoc", []))
    have = sorted(sha(p) for p in a.base_utoc)
    if not want:
        problems.append("report lists zero base files")
    elif want != have:
        problems.append("report was made with other game base files (%d in report, %d given, %d match)"
                        % (len(want), len(have), len(set(want) & set(have))))
    so = r.get("inputs", {}).get("script_objects", {}).get("utoc_sha256")
    if so != sha(a.script_objects_utoc):
        problems.append("report was made with another ScriptObjects file (or an older tool without that record)")
    n = r.get("counts", {}).get("items_checked_total", 0)
    if not n:
        problems.append("report checked zero items")
    v = r.get("verdict")
    if v == "ADMITTED-WITH-VISUAL-LOSS" and not a.accept_visual_loss:
        problems.append("visual loss disclosed but not accepted (--accept-visual-loss)")
    elif v not in ("ADMITTED", "ADMITTED-WITH-VISUAL-LOSS"):
        problems.append("intake verdict is %s" % v)
    print("intake gate: items checked in report = %d, base files checked = %d, verdict = %s" % (n, len(have), v))
    for p in problems:
        print("FAIL: " + p)
    if not problems:
        print("PASS" + (" (visual loss accepted: %s)" % a.accept_visual_loss if a.accept_visual_loss else ""))
    rec = {"label": "Mac-only", "gate": "intake_gate", "result": "PASS" if not problems else "FAIL",
           "problems": problems, "time_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
           "report": os.path.abspath(a.report), "report_sha256": sha(a.report), "verdict": v, "items_checked": n,
           "mod_utoc_sha256": mod.get("utoc_sha256"), "mod_ucas_sha256": mod.get("ucas_sha256"),
           "base_utoc_sha256": have, "script_objects_utoc_sha256": so,
           "visual_loss_accepted_by": a.accept_visual_loss if v == "ADMITTED-WITH-VISUAL-LOSS" else None}
    with open(a.record, "w") as f:
        json.dump(rec, f, indent=1, sort_keys=True)
    print("record written: %s" % a.record)
    return 0 if not problems else 2


if __name__ == "__main__":
    sys.exit(main())
