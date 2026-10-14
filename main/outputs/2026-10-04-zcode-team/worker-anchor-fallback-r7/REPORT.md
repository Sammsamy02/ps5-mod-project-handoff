# REPORT — worker-anchor-fallback-r7 (2026-10-04)

Proposal-only worker. Nothing outside `outputs/2026-10-04-zcode-team/worker-anchor-fallback-r7/`
was written; `worker-module-start-wiring/` was consumed read-only. No console
contact, no network, no nested subagents. ~44 tool calls (bound 70).
CLAIM.json was written first; this REPORT is the last artifact.

## 0. Deliverables (this directory)

| file | what |
|---|---|
| `CLAIM.json` | ownership, reads, bounds (written first) |
| `FALLBACK-RANKING.md` | evidence ranking: uniqueness recount, igf7 status, span coverage, mutation likelihood |
| `runtime_start_bridge_r7.diff` | ready-to-apply bridge PROPOSAL (r6 → r7 ordered ladder) |
| `runtime_module_entry_r7.diff` | ready-to-apply entry PROPOSAL (monotonic-ms + pid prefix on UE4SS.log failure lines) |
| `host_test_r7/` | ladder host test (`ladder_test.cpp`, binary, `run_output.txt`, `receipt.txt`) |
| `REPORT.md` | this file (last) |
| `scratch/` | syntax-gate stub headers + generated r7 sources (evidence of the gates; not deliverables) |

Apply commands (from `worker-module-start-wiring/`; both diffs were
patch-verified against the pristine r6/r3 files in a temp dir and reproduce
the gated sources byte-exactly):

```
cp runtime_start_bridge_r6.cpp runtime_start_bridge_r7.cpp
patch -p1 < ../../worker-anchor-fallback-r7/runtime_start_bridge_r7.diff

cp runtime_module_entry_r3.c runtime_module_entry_r7.c
patch -p1 < ../../worker-anchor-fallback-r7/runtime_module_entry_r7.diff
```

## 1. Ranking outcome (full evidence in FALLBACK-RANKING.md)

Whole-file recount over the retained eboot (sha `3b6cfad4…622fc`, 194,241,424 B,
re-verified this session): **B/D/E each occur exactly once; C occurs TWICE**
(canonical p_vaddr 0x75cffda → rt 0x79cffda; second copy p_vaddr 0x76c8e58 →
rt 0x7ac8e58, same rodata LOAD); A once, in the flags-0 note segment. The
recount independently agrees with `worker-c2-probe/EXPECTED-VALUES-OFFLINE.json`.

In-game status (cited from `2026-10-04-zcode-controller/console/c2-alternative-results.jsonl`,
build `igf7-3dd949879a0a`): B, C, D, E all `cand_read` OK with live_hex equal
to the offline expected bytes; **A = `cand_refusal "uncovered"`, no
dereference attempted**. **D at rt 0x7f8845d was covered by the single
readable span `[0x787c000, 0x82bc000) prot=1`** (mapspans page 0 `s0` — the
page-rounded rodata LOAD phdr1); B, C1, C2, E all fall in that same span, so
every gated read is boundary-safe. The igf2-era recovery
(`worker-mapping-recovery`) independently corroborates the exec-only first
span `[0x400000, 0x787c000) prot=4` but holds no readable span above
0x787c000 — the data-span fact rests on the igf7 same-invocation records,
which are the same process-state class (module_start/menu) the bridge runs in.

**Ladder: E → D → B → C(both sites) → A(logged, never gates); ONE full match
among E/D/B (or C with BOTH sites matching) anchors base 0x400000.**

## 2. Bridge r7 PROPOSAL (runtime_start_bridge_r7.diff)

- Replaces r6's E-required single anchor with the ordered ladder above. Every
  rung is evaluated on every launch (no short-circuit): the DECISION is the
  ladder gate, the evidence is complete in one launch — exactly what a
  post-launch-5 E-mismatch triage needs (E's `mm_off`/`live_w0` plus D/B/C/A
  verdicts in the same log).
- r6 diagnostics preserved verbatim: `anchor_E_covered/match/mm_off/live_w0`
  and `anchor_A_*` keep their names, order, and logged-not-required A
  semantics. New tags: `anchor_D_*`, `anchor_B_*`, `anchor_C1_*`,
  `anchor_C2_*`, aggregate `anchor_C_match` (1 only if BOTH C sites covered+
  matched — resolves C's 2-occurrence ambiguity structurally; the second
  site has never been read in-game, so C is the weakest gate rung and D/B
  cover its failure).
- Coverage gate first on every site (read-risk rule; igf7's A refusal proved
  the path). Read cost bounded ~200 B across all rungs.
- **Zero new imports/includes**: only the existing `diagnostic_event`, the
  existing `lmf::OsVmMapping`, new static const tables, and a `bool gates`/
  site-table restructure. `kCandidateBase`/`kImageExtentVaddr` unchanged.

Gates: `clang++ -std=c++17 -fsyntax-only -Wall -Wextra` with minimal stub
composition headers (in `scratch/stub/`, documented) → **0 errors, 0
warnings**; the pristine r6 baseline under the same stubs also passes, and
two early stub-side mismatches (noexcept, by-value `MakeNativeProgramContext`,
`KERN_PROC_VMMAP` not on host) were fixed in the STUBS, not the proposal.

## 3. Entry r7 PROPOSAL (runtime_module_entry_r7.diff)

- Prefixes every UE4SS.log failure line with `[t=<monotonic ms>ms pid=<pid>]`
  before the existing `modbridge module_start failed:` text (playbook
  freshness recommendation: orderable, attributable failure lines).
- Uses the SAME `clock_gettime(CLOCK_MONOTONIC)` + `getpid()` pair
  `write_status_failed` already links — **zero new imports** (time.h/unistd.h
  already included). Adds one bounded `log_append_u64` helper (no formatting
  imports; 256-byte line budget absorbs the ≤ ~40-char prefix; detail
  truncation behavior unchanged).
- Gates: C99 syntax gate clean for BOTH r7 and the r3 baseline
  (`-Wall -Wextra`, stub headers in `scratch/`); `entry_u64_test` exercises
  the actual file's `log_append_u64` (same-TU include): 0 / 12345 / 2^32 /
  2^64−1 exact plus capacity clamp — ALL PASS. One test-side expectation was
  corrected during this (the helper writes up to `capacity` bytes; the
  production code was never wrong).

## 4. Ladder host test (host_test_r7/)

Standalone extraction in the `host_test_r6/walk_test.cpp` pattern: same
structs/tables/values, two shims (registry byte lookups that COUNT
unregistered reads; captured diagnostic events). **34/34 PASS, exit 0**
(`run_output.txt`), covering all mandated cases: E-ok, E-fail-D-ok, all-fail
(+ A-match-only still refuses), C-ambiguous with both occurrence sites
checked, coverage-refused-then-next (E under a prot-4 span: refused unread —
zero unregistered reads — ladder falls through to D), plus a menu-like igf7
span-shape case (A uncovered: logged, never read, never gates).

Two test-side bugs were found and fixed during authoring (a precedence
parenthesis, and an inverted page-drop in the menu-like case); each fix was
driven by an observed FAIL, and no production-code change was implied by any
failure.

## 5. Honest limits

- Everything here is host evidence; no target fact was produced by this
  worker. igf7 in-game facts are cited from the console capture, not re-run.
- C2 (second C occurrence) has never been read in-game; the ladder treats it
  as a bonus rung only. First r7-bridge launch supplies that fact.
- The syntax gates use minimal stub headers, NOT the composed unit headers;
  they prove the proposal's internal well-formedness, not a link.
- No claim that the ladder survives a game UPDATE: all windows change
  together there, and refusal is the correct outcome.
- Launch-5 remains imminent and untouched; its `anchor_E_*` diagnostics keep
  their exact names under this proposal.
