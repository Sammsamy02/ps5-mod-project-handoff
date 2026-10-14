#!/usr/bin/env python3
"""worker-gameid-unit host-test extractor.

Extracts EXACTLY the marker-bounded regions of the STAGED bridge TU:
  1. gameid_sha256.inc  — [gameid-sha256-BEGIN] .. [gameid-sha256-END]
                         (the shipped sha256 bytes, verbatim)
  2. gameid_hunk.inc    — the whole gameid section, from the section header
                         comment to the line before the next pre-existing
                         section comment ('// v5: fresh per-address RANGE'),
                         verbatim, for the host syntax check.
Fail loudly (exit 2) if any marker is missing or duplicated: the tests are
meaningless unless they test the real staged bytes.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UNIT = os.path.dirname(HERE)
STAGED = os.path.join(UNIT, "staging", "runtime_start_bridge_r14_diag_spanpub.cpp")
SHA_BEGIN = "// [gameid-sha256-BEGIN]"
SHA_END = "// [gameid-sha256-END]"
HUNK_BEGIN = "// ---- gameid (worker-gameid-unit staged NEXT candidate, 2026-10-05)"
HUNK_END = "// v5: fresh per-address RANGE mapped-verdict probe for addresses the"


def fail(msg):
    sys.stderr.write("EXTRACT-FAIL: %s\n" % msg)
    sys.exit(2)


def main():
    lines = open(STAGED, encoding="utf-8").read().split("\n")
    b = [i for i, l in enumerate(lines) if l.strip() == SHA_BEGIN]
    e = [i for i, l in enumerate(lines) if l.strip() == SHA_END]
    if len(b) != 1 or len(e) != 1 or e[0] <= b[0]:
        fail("sha256 markers not unique/ordered: begin=%r end=%r" % (b, e))
    sha_inc = "\n".join(lines[b[0] + 1:e[0]]) + "\n"
    open(os.path.join(HERE, "gameid_sha256.inc"), "w", encoding="utf-8").write(sha_inc)

    hb = [i for i, l in enumerate(lines) if l.startswith(HUNK_BEGIN)]
    he = [i for i, l in enumerate(lines) if l.startswith(HUNK_END)]
    if len(hb) != 1 or len(he) != 1 or he[0] <= hb[0]:
        fail("hunk markers not unique/ordered: begin=%r end=%r" % (hb, he))
    hunk = "\n".join(lines[hb[0]:he[0]]) + "\n"
    open(os.path.join(HERE, "gameid_hunk.inc"), "w", encoding="utf-8").write(hunk)
    print("extracted: sha256 impl %d lines, hunk %d lines"
          % (e[0] - b[0] - 1, he[0] - hb[0]))


if __name__ == "__main__":
    main()
