#!/usr/bin/env python3
"""compare_regression.py — v2-vs-v1 regression proof for analyzer_v2.py.

worker-runbook-v2 · 2026-10-06. Reads two results.json documents (v1 and v2)
produced from the SAME run dir with the SAME arguments and proves that v2
classifies everything v1 covered IDENTICALLY. Deltas allowed ONLY where the
task explicitly adds vocabulary:
  - tool / version / generated_utc / runbook strings
  - NEW keys (v2-only) — ignored
  - observables: v1 rows must appear in v2 with identical status AND evidence
    (v2 may add rows; order of v1 rows preserved)
  - greps: v1 keys identical; v2 may add keys
  - guards: v1 strings must be a prefix subsequence of v2's (v2 appends)
Exit 0 = REGRESSION-PROOF, 1 = DIVERGENCE.

Usage: compare_regression.py V1_RESULTS V2_RESULTS
"""
import json
import sys

IGNORE_TOP = {"tool", "version", "generated_utc", "runbook"}


def fail(msg):
    print("DIVERGENCE: %s" % msg)
    sys.exit(1)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    v1 = json.load(open(sys.argv[1]))
    v2 = json.load(open(sys.argv[2]))

    # 1. every shared top-level key must be identical, except the ignored set
    for k in sorted(set(v1) | set(v2)):
        in1, in2 = k in v1, k in v2
        if in1 and not in2:
            fail("v2 DROPPED key %r" % k)
        if k in IGNORE_TOP:
            continue
        if in1 and in2:
            if k == "observables":
                continue  # compared structurally below
            if k == "greps":
                for gk in v1[k]:
                    if gk not in v2[k]:
                        fail("v2 DROPPED grep %r" % gk)
                    if v1[k][gk] != v2[k][gk]:
                        fail("grep %r differs" % gk)
                continue
            if k == "guards":
                if v1[k] != v2[k][:len(v1[k])]:
                    fail("guards prefix changed")
                continue
            # additive law: every v1 sub-key of a shared dict must survive
            # verbatim in v2; v2 may ADD sub-keys (new vocabulary), never
            # change or drop v1 content.
            if isinstance(v1[k], dict) and isinstance(v2[k], dict):
                for sk, sv in v1[k].items():
                    if sk not in v2[k]:
                        fail("v2 DROPPED %s.%s" % (k, sk))
                    if v2[k][sk] != sv:
                        fail("%s.%s differs: v1=%r v2=%r" % (k, sk, sv, v2[k][sk]))
                continue
            if v1[k] != v2[k]:
                fail("key %r differs:\n  v1=%r\n  v2=%r" % (k, v1[k], v2[k]))

    # 2. observables: every v1 row must exist verbatim in v2 (same rung,
    #    observable, status, evidence), in the same relative order.
    def key(r):
        return (r["rung"], r["observable"], r["status"], r["evidence"])

    v2_rows = [key(r) for r in v2["observables"]]
    pos = 0
    for r in v1["observables"]:
        k = key(r)
        try:
            idx = v2_rows.index(k, pos)
        except ValueError:
            # a not-reached annotation may legitimately differ ONLY if the
            # cascade stop changed — which is a divergence by definition here
            fail("v1 observable missing/changed in v2: %r" % (k,))
        pos = idx + 1
    n_added = len(v2["observables"]) - len(v1["observables"])

    # 3. the archived v1 classification (rungs / cascade / verdicts) identical
    for k in ("rungs", "load_question_verdict", "cascade_stop",
              "failure_table_row_hint", "pool_range_verdict_fresh",
              "pool_verdict_meaning", "max_pid", "max_pid_source",
              "baseline_bytes"):
        a, b = v1.get(k), v2.get(k)
        if k == "rungs":
            # additive law: v1 rung statuses identical; v2 may add GI
            for rk, rv in a.items():
                if b.get(rk) != rv:
                    fail("rung %r differs: v1=%r v2=%r" % (rk, rv, b.get(rk)))
            continue
        if a != b:
            fail("classification key %r differs: v1=%r v2=%r" % (k, a, b))

    print("REGRESSION-PROOF: %d/%d v1 observables verbatim in v2 "
          "(+ %d new v2 rows), %d greps identical, rungs/cascade/verdicts identical"
          % (len(v1["observables"]), len(v2["observables"]), n_added,
             len(v1["greps"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
