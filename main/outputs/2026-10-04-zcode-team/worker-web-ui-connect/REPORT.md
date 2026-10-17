# REPORT.md — worker-web-ui-connect

Date: 2026-10-04. Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`.
Write scope honored: only `outputs/2026-10-04-zcode-team/worker-web-ui-connect/`.
No existing file modified anywhere; no network/console; no nested subagents.
Tool calls used: ~24 of ≤50.

## Task results

1. **Read actual state.** `web/app.js` (1045 lines), `web/index.html` (81),
   `native-host/http.c` (939), `mod_service/read_bridge.h` (25),
   `read_bridge.cpp` (36) — read in full. Confirmed AUDIT.md's key facts at
   read time: mod-service routes are GET-only two-canned-request passthroughs
   (`http.c:771-799`, consulted first by dispatch at `http.c:806`);
   `web/app.js:935` calls `GET /api/installed`, which no host route serves;
   state pill map and runtime line live in `renderInstalled()` (app.js ~750-761).

2. **PATCH-SET.md delivered** — combined set with exact apply order
   (p1 read_bridge.h → p2 read_bridge.cpp → p3 http.c → f1 app.js → f2
   index.html), rationale, and the apply commands.

3. **Verification (the load-bearing finding).** worker-8's three diffs **do
   not apply as written**:
   - Patch 1: both hunk headers miscounted → `patch fragment without header`.
   - Patch 2: header old-count 3 vs 4 context lines → `corrupt patch at line 41`.
   - Patch 3 hunk B: header `-771,14 +771,22` vs actual 29 old / 39 new lines →
     `corrupt patch at line 47`.
   - Patch 3 hunk A: fails as written AND fails `--recount` AND fails with
     corrected header counts on fresh trees, although its pre-image was
     verified byte-identical to `http.c:287-293`. Residual root cause not
     isolated (recorded honestly as an unresolved anomaly); the hunk is
     treated as untrusted.
   All added/removed content lines in the originals match the current files —
   the defects are header bookkeeping, not intent.
   Fix: regenerated **canonical patches** with `diff -u` between pristine
   copies and intended-state copies (change content transcribed verbatim from
   worker-8's `+`/`-` lines). Canonical set: 5/5 pass strict `git apply
   --check`, then 5/5 apply sequentially on a fresh tree, final tree
   byte-identical (`diff -r`) to the independently built intended state.
   Evidence preserved under `verify/` (`patches/` originals + fixes,
   `canon/` generated patches + a/b trees, `applytest/`, `fulltest/`).

4. **Conflicts between the two patch sets:** none textual (disjoint files).
   Semantic interactions documented in PATCH-SET.md §4: (a) web assets are
   compiled into the host binary, so frontend patches need an asset rebuild;
   (b) the new POST route does not require Origin/CSRF unlike all other
   mutating POSTs — flagged as a controller decision, not changed; (c)
   capability gating means nothing user-visible changes until the mod service
   archive is linked (`host.c:442-455`, controller-owned build step); (d) the
   dead `/api/installed` call is removed in the UI, and no such route is added.

5. **COMPLETE-DIFF.md delivered** — all five patches verbatim with per-file
   effect table and the post-patch user-visible walkthrough: the new
   `POST /api/mod-service/request` (envelope verbatim, 200/400/405/502/503
   contract), honest mod states (Downloaded/Prepared/Installed/Unknown state),
   and the ack-based running indicator
   (`No runtime ack` / `Running (ack: …)` / `Failed` / `Stopped`, always with
   "Live game process unverified").

## Missing input / fallback (controller action required)

`worker-e2e-records-ui/PATCH-OUTLINE.md` never existed during this session
(directory absent at first read and at every poll, including the final one).
Per the task description ("4 display-only changes to app.js + 1 to
index.html") I derived fallback patches f1/f2 (labeled in both documents).
**If the sibling's outline lands before application, prefer it and skip
`f1`/`f2`; they are independent of p1-p3 and swappable.**

## Honesty bounds

Verification is patch-application-on-copies only. Nothing was compiled, run,
or exercised against a live host; no claims about runtime behavior beyond what
the code read shows. No installation-support claims: checkpoints
`installation_supported:false` / `console_executed:false` stand; linking and
payload binding remain controller-owned steps per worker-8 PATCHES.md.
