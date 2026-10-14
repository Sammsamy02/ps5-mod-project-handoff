# One-launch milestone receipt (design-only)

Target assertion: runtime is resident and the milestone observer is armed before the engine's **first LoadMap entry**. One authorized launch alongside #5; do not delay B7/hooks/15-fields to add this prep.

Required record, all from the same launch/build:

| Record | Required meaning |
|---|---|
| Build SHA / ID; module_start_entered | Identifies injected artifact and residency. |
| Observer admission/armed marker | Validates data-only route, verified values, active observation before first entry. A request queued for later arming is insufficient. |
| First LoadMap entry marker + sequence | Actual entry, not return, and explicit first-entry coverage from process startup. |
| LoadMap exit; BeginPlay | Orders downstream milestones; preserves every refusal. |
| Mod-ready, playable scene, clean exit | Completes useful operation with no crash/watchdog signature. |

PASS only if module entry < observer armed < first LoadMap entry < downstream BeginPlay in a shared monotonic clock or one ordered sink, with coverage established before that first entry. If timers differ, preserve synchronization bounds; overlapping bounds are inconclusive. A second map load cannot replace the first.

INCONCLUSIVE if the hook arms late, deferred delivery is refused, sequence is absent, the first entry may have been missed, or coarse timestamps cannot order events. FAIL only when complete coverage positively shows late residency/arming. Preserve raw logs and quote decoded line references; use harvest/decode_session_log.py for NUL-interleaved logs after the game is confirmed closed. No raw/plain grep as evidence.

If B7's available instrumentation cannot observe first entry without new runtime work, retain the partial historical label and report the exact coverage gap. No executable modification or new kernel dependency is justified by this receipt task.
