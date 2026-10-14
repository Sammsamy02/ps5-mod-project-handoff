# REPORT — worker-idle-work-survey (2026-10-05)

Deliverable order honored: `CLAIM.json` (first) → `TRAMPOLINE-REVIEW.md` (the one
REMAINING item, executed) → this `REPORT.md` (last). Read-only survey; no console
contact; no nested subagents; writes only under this worker directory.

## Answer up front

**One genuinely useful, already-approved, host-executable item remained, and it is now
done: the SEH64 PS5-trampoline pre-execution review** — the DLL track's explicitly
named "one named open item" (`worker-dll-track-closure/DLL-TRACK-STATE.md` §6),
required "before any reliance on SEH64 semantics beyond the exercised fixture census".
Not a duplicate: no delivered worker owns it (grep over all worker dirs shows it only
ever referenced as an open item). It is not console-bound (§6: "Nothing in the queue is
console-blocking"). Deliverable: `TRAMPOLINE-REVIEW.md` in this directory — reviews the
naked trio (`call_at_frame_win64` / `resume_win64` / `svc_capture_context`) and all six
required divergence topics. Headline result: **finding F-1** — the guard-frame call
enters filters/`__finally` funclets with entry rsp ≡ 0 (mod 16) instead of the Win64
required ≡ 8 (off-by-8 alignment inversion whenever `frame_rsp ≡ 0`, which is the
general case given `dctx->frame_rsp = context->rsp` provenance); a real-world filter
with an aligned SSE stack spill would `#GP`. Invisible to the shipped fixture census
(globals-only bodies, no aligned spills) and to Rosetta host validation; the review
specifies the discharging probe and the 0x28→0x30 guard-zone fix shape (owning lineage,
new ids). Also: F-2 (captured `Context.Rax` holds the EFlags image, not "caller rax as
found" — comment-semantics fix), plus recorded verdicts that the other four divergences
(signal shim, vectored handlers, C++ EH, integer-only restore) are fail-closed or
ABI-sound as shipped, and that the PS5 SysV-entry route fact is receipt-corroborated
(`win64cc_defs: 0` / 7 win64cc entries).

## Survey — everything else, classified

Method: read `worker-index-final-edition/INDEX-FINAL.md` (ledger open-questions tables
(a)/(b)), the launch-10/11 matrices, app/DLL/installer track closure docs, the
readiness certification chain, and the newest worker dirs (newer than INDEX-FINAL's
freeze, which itself is slightly stale — see "Swept-late" below). Every candidate was
checked against delivered worker dirs before classification.

| Candidate (source) | Classification | Evidence |
|---|---|---|
| Comp6 staging, dll8 run, e2e MODE-B run, ridealong/settings staging, launches 9/10, cc1 probe, pad probe, igf7 (INDEX-FINAL table (a) #1–9) | CONSOLE-BOUND | All require console reachability; console DOWN at close; first act on return is the reachability check |
| FName FC1 exec-side probe (table (a) #10) | CONSOLE-BOUND (HELD) | Record-corrected: never wired; data-side fc1 rides launch 9 |
| App-track console closure (table (a) #11) | CONSOLE-BOUND + user-only parts | MODE=B consumption on console; real OAuth client_id is user-only |
| Exec-writability probe, vtable swap/D-1, R3 carrier fact F-1, verdict-gating, GMalloc V1/V2, seed slots 2/4, Item Spawner acceptance (table (b)) | CONSOLE-BOUND | All gated on launch evidence or in-game observation |
| PLH Prospero layer (U-SWAP-4, "REPORT pending" in LAUNCH11-MATRIX) | ALREADY-DONE (swept-late) | `worker-polyhook-prospero-layer/REPORT.md` + RECEIPT.md landed 02:47 (after the matrix read); all 14 symbol families, host 71/71, compile-gated |
| No-exec gate integration (U-SWAP-5) | CONSOLE-BOUND (contingent) | Diff set delivered as proposal; integration only if BOTH_FAIL/FATAL verdict — launch-10 evidence decides |
| DIFF-2 rebase (LAUNCH11-MATRIX §4-d "mandatory before co-staging") | ALREADY-DONE (swept-late) | `worker-r3-diff2-rebase/` exists with `MATRIX-NOTE-SKEW-CLOSED.md`, rebased diffs, compile gate, receipts |
| Frozen-script arming edition (LAUNCH11-MATRIX §4-g) | CONSOLE-BOUND (integration-time decision) | Matrix: "integration-time decision required"; L11-4 cannot launch until a controller takes it — not pre-buildable host-side without a verdict |
| cc1 9th-ini entry adoption (LAUNCH11-MATRIX §4-i) | OWNER-DECISION-GATED | "adoption is a separate owner decision" (layout owner + batch tool owner under new id); rides a console window |
| dll8 console run (DLL TRACK-COMPLETE criterion 4) | CONSOLE-BOUND | The only blocking criterion; 7-family fallback equally console-bound |
| App P1/P2/P4 durable stand-up + queue load (`APP-SESSION-INTEGRATION.md` §2) | CONSOLE-WINDOW-BOUND / controller-owned | P0 re-pin APPLIED (controller update in-doc + INDEX); P1–P4 are pre-window prep requiring a durably running server + durable state dirs — value materializes only at the console window and a persistent server is a controller/host-process decision, not idle-work for a survey pass |
| MSVCP140 9th family (DLL-TRACK-STATE §6) | NOT-REQUIRED successor | "explicitly not required for this track's completion"; new family authoring, below the one named open item in value and explicitly deprioritized by its own owner doc |
| Pre-session inventory cross-check (INDEX-FINAL "Incomplete: CLAIM only") | ALREADY-DONE (swept-late) | `worker-pre-session-inventory/REPORT.md` + PRE-SESSION-INVENTORY.md landed 01:43–01:44 (after INDEX-FINAL's 01:21 read); 1,306 citations audited, F1–F5 findings |
| Cert residual: EVE-STATE L11 size cell "400" (worker-cert-fourth-pass disclosure) | ALREADY-DONE (swept-late) | EVE-STATE.md line 38 now reads 440; recomputed grep finds no "400" residue; 9/9 stamp corroborated by worker-cert-fourth-pass |
| Pre-session-inventory F1 (decision-brief wrong v3 path) / F2 (app doc sha-size conflation) / F5 (master-plan pointer) | ALREADY-DONE (swept-late) | Decision-brief no longer cites `engineversion-ini/package-v3`; APP-SESSION-INTEGRATION.md:108 now carries the corrected 1448/447 split; master-plan line 4 carries the v3 note |
| Doc-chain items f.1–f.6 of READINESS-CERT | ALREADY-DONE | Fixed across commits 4e6c995/7bc652e/5404fd3/d24a628; fourth-pass independent verification: 9/9 CORROBORATED |
| **SEH64 PS5-trampoline pre-execution review (DLL-TRACK-STATE §6)** | **REMAINING → DONE by this worker** | See above; `TRAMPOLINE-REVIEW.md` |

## Swept-late note (state newer than INDEX-FINAL)

INDEX-FINAL froze at ~01:45; between then and this survey the following landed and were
accounted for above: `worker-polyhook-prospero-layer` (REPORT), `worker-launch11-*`
(matrix + vtable-prebuild + dry-compose), `worker-r3-diffs-authoring/-rebase`,
`worker-walk-site-transitions`, `worker-no-exec-mode-gate`, `worker-carrier-probe-build`,
`worker-menu-visibility-runbook`, `worker-runbook-eighth-sink`,
`worker-post-launch-tools`, `worker-session-handoff-readme`,
`worker-ledger-archive-snapshot`, the completion of `worker-pre-session-inventory`, and
the full certification chain through the 9/9 fourth-pass corroboration. Any survey
relying on INDEX-FINAL alone would have wrongly flagged several of these as remaining.

## Honesty

- This worker executed one review task (read + analysis + one review document); it did
  not modify any source, tool, or doc outside this directory, did not execute code, and
  made no console contact.
- F-1 is an open parity RISK established by static ABI analysis with the exact
  arithmetic in `TRAMPOLINE-REVIEW.md` §1.1; it is not a demonstrated target fault and
  is not claimed as one. Its discharge (probe + fix + both-route compile) belongs to
  the SEH64 lineage under new ids.
- No quality score, no completion claim beyond this survey's own deliverables; the
  9/9 host-readiness certification stands as recorded (console remains the only missing
  piece for the launch path).
