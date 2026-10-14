# l19 approved local plan

User spec: current October7 approval, bound Townfall Shipping/Stats facts, collect-all build/layout inventory, XOM answer, full two-build/import/package pipeline, l18 timeline and speedup proposals only. No console contact or deployment.

1. Implement generic identity-gated build fact resolution using per-game evidence/profile; strict missing/conflicting/wrong identity refusal. TDD on actual initializer boundary.
2. Inventory all actual reachable layout and build queries. Resolve independently evidenced facts; record unknowns together and refuse before engine calls rather than guess.
3. Review unchanged XOM paths and build timeline from saved logs, respecting missing clocks/popups.
4. Freeze sources and evidence; bind isolated pipeline; fresh NativeLayoutBinding/header consumer, exact provider/ODR/source/dependency/import gates; double-build composition/PRX determinism.
5. Publish local report, fact table, receipts and honest remaining blockers. Upload HOLD.

Ruling: user approval is the design authority; no repeated design approval or new worktree needed. This worktree is already managed and isolated. All old artifacts/user files preserved.
Ruling: unresolved offsets cannot be made facts to satisfy an all-resolved request. Continue local source/static-evidence work; collect all unresolved prerequisites in the artifact and report them if proof remains absent.

Interface review: implementation produces inline BuildFactsChecks/RuntimeBuildProfile + staged layout inventory accessor; full pipeline consumes initializer and native-layoutbinding TU + Platform header. Inventory supplies evidence/presence, no shared writes. Timing supplies saved-log analysis only. Root owns pipeline/report.

Task1: in progress (profile_impl). Task2: in progress (fact_inventory). Task3: in progress (timing_xom). Task4: pipeline preparation. Task5: pending.
