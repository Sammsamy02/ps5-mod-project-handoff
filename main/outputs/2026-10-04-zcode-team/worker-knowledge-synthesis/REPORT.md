# REPORT — worker-knowledge-synthesis (2026-10-05)

## Method

1. Extracted all 335 `zcode_*` entries from `.superpowers/native-observations/transport-experiments-progress.json`
   (the ledger's key order is the session chronology, 2026-10-04T15:15Z takeover through
   2026-10-05T17:38Z full-surface mobilization) into a working dump; read in full or in targeted part
   ~55 entries spanning the takeover, Flash capacity trial, return-session consolidation, launch-9,
   the launch-10 build/staging/runs 1-4, and the L11 chain.
2. Read the primary worker reports the ledger points to for the load-bearing numbers:
   `worker-real-timing/TIMING.md` (per-stage table, C_comp, gates, clock identity),
   `worker-ploader-70s/GAP-ANALYSIS.md` (tick-budget account),
   `worker-geometry-reference/GEOMETRY.md` (slide model, phdr table, seed corrections),
   `worker-fold1-preverify/FOLD1-READY.md` (basename law),
   `worker-launch10-staging-packet/REPORT.md` + `DIFF.md` (the codified ftpsrv readback law),
   `worker-initializer-integration/REPLACE-ARGV.md` (frozen-argv replacement mechanics).
3. Read `README-RETURN.md` (the disk-reverified return checklist) and the companion
   `worker-knowledge-consolidation/KNOWLEDGE-BASE.md` to complement rather than duplicate: that
   document owns durable TARGET facts; this one owns the session's earned LESSONS and the
   state-pointer map for the return session.
4. Wrote `KNOWLEDGE-BASE.md` in the five mission-ordered sections, then `CLAIM.json` (15 claims,
   each with its citation set) and this report.

## Coverage of the mission brief

| Mission item | Where landed |
|---|---|
| +0x400000 slide vs 0x4000000 slip (extent 0xbc602c8+0x400000=0xc0602c8, never 0xfc602c8) | KB §1.1 |
| LOAD0 +0x4000 file-offset==vaddr trap | KB §1.2 |
| Hex-digit-width errors as a CLASS + check-against-python-hex() habit | KB §1.3 (4 documented instances incl. the pad-code correction and the 0x37ac8c0 tick constant) |
| ftpsrv readback law (segment-coverage, SHT+tail; never byte-compare, never path-only-trust) | KB §2.1 (+ the rc-9 silent STOR fact) |
| Basename embedding in objects | KB §2.2 |
| Minified-script edit discipline (single-line replacements only) | KB §2.3 (single-line tagged edits, one-token flips, sha-pinned stage-header/replace-argv mechanisms, frozen-env one-liner class) |
| args-scope / cwd-reset classes | KB §2.4 (+ §2.5 root-shadowing and stale-pin hazards, same "tooling law" family) |
| Four-run arc with gates ASCII → size → pad-ALREADY_OPENED | KB §3.1 (per-run table incl. pid, classifier label, fix class; run4 repro; stale-defaults lesson) |
| C_comp = 0.8 s, early-refusal-path scope | KB §3.2 |
| 71 s tick-budget account | KB §3.3 (refuted failure-path hypothesis, per-plugin re-payment, PATCH 02+05 proposal-only, rejected ini-order mitigation) |
| Pad ownership question | KB §3.4 (ALREADY_OPENED semantics, duplicate-init hypothesis, A/B outcomes) |
| Pump structural fact (marshal-only, scoped) | KB §3.5 |
| Geometry fix's predicted-pair 3x confirmation | KB §3.6 (+ sdiag accumulation / clock identity / never-race-status reading rules) |
| Evidence-ladder discipline | KB §4.1 |
| Distinct-outcome labeling (pad A/B) | KB §4.2 (+ candidate/synthetic provenance labeling) |
| Comparator chains with byte-matched controls | KB §4.3 |
| Fold discipline (worker dirs vs controller final-units) | KB §4.4 |
| Two-snapshot race class | KB §4.5 |
| State pointers | KB §5 (ledger, runbooks, tools, build/candidate identities, in-flight roster of 9, live hazards) |

## Honesty notes

- Synthesis only. Every KB line carries its ledger key (`L<key>` = `zcode_<key>`) and/or an on-disk
  artifact path; no new measurements, verdicts, or facts were introduced by this worker. Where the
  ledger self-corrected mid-session (extent prose, pad error code, "single global bias" reversal),
  the correction chain is presented as a correction — the superseded value is shown as the
  known-wrong figure it is, because those figures still circulate in older prose and are exactly the
  traps the next reader must not step on.
- Cross-checks performed while synthesizing (no new claims produced): the corrected pad code
  0x80920004 equals TIMING.md's decimal -2137915388 (2's-complement verified), and the 2958/2960
  size-gate figures agree between the ledger entry and TIMING.md's mfa_activate_failed row; the two
  documents frame the same gate from different angles (loadlist-declared vs staged size) and both
  are cited.
- The known-wrong figures themselves (0xfc602c8, 0x80A40004, 0x37ac8c0) appear in this deliverable
  only as labeled anti-examples, matching the README-RETURN §5 practice.
- `worker-knowledge-consolidation/KNOWLEDGE-BASE.md` is NOT superseded: it holds target facts and
  target methods; this doc is organized by cost-of-lessons and points to it in the header. Any
  conflict between older prose and this doc resolves per §1.1's rule: recompute from pinned inputs,
  trust the code's expression, then fix the prose.
- No console contact, no probe execution, no writes outside
  `outputs/2026-10-04-zcode-team/worker-knowledge-synthesis/`, no ledger mutation.

## Deliverables

- `CLAIM.json` — scope, primary sources, 15 claims (KS-1…KS-15) with evidence pointers, non-claims.
- `KNOWLEDGE-BASE.md` — the read-second accelerator (5 sections, all cited).
- `REPORT.md` — this file.
