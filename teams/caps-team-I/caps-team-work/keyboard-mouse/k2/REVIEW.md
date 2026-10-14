# K2 independent review (one reviewer, 2026-10-14, Mac-only)

Reviewer: one independent agent, read-only, given the patch, DESIGN.md section 5, the live runtime sources and
the project rules. It ran `tests/run.sh` itself (ALL PASS at that time: 41/41 tests, 1494 checks; TSan
10000/10000) and checked the gates and the target check.

**Verdict: SHIP-WITH-FIXES, 0 blockers.** Items 1-3 to fix before the one build. Answers to the focus questions:
the IsInputKeyDown call is safe (zeroed frame of at least ParmsSize+16 bytes, every offset bounded, by-value Key
never constructed so no refcount leak, any write into the Key stops K2 for good); threading correct (SPSC ring, all
shared fields atomic, re-entry blocked); pad behaviour unchanged (deferred attach, activate/deactivate results,
terminate path, pad-only events identical); nothing throws out of any entry point; logs correct and bounded;
nothing game-specific, UE4 and UE5 Key layouts match; the LuaMod change is minimal and in the right place.
Not provable on the Mac: whether the PS5 build feeds the USB keyboard into PlayerInput at all ("supported=1" is not
proof of that), and UI-only input modes hide keys.

## Findings and what was done (all changes re-tested: 49 tests / 5537 checks, TSan, target check, all gates PASS)

| # | Tag | Finding | Done |
|---|---|---|---|
| 1 | SHOULD-FIX | Controller search (FindAllOf every 500 ms) runs even with no key bound, is not budget-counted, and repeats forever if no local controller is ever found; a full object walk is 30-86 ms without the R9 index. | **Fixed.** No engine call at all while no key with an engine name is bound (the work list is built before `begin_frame`). Search back-off 0.5, 1, 2, 4, 8 s (cap 8 s, reset after a find); a search that took >= 4 ms waits the full 8 s at once. NoController frames are counted in `account()`. Tests: `zero_bound_keys_means_zero_engine_calls`, `controller_search_backs_off_and_resets_after_a_find`, `slow_controller_search_waits_the_maximum`. |
| 2 | SHOULD-FIX | `is_live(m_pc)` read `m_pc->GetInternalIndex()` from a cached pointer that may have been freed. | **Fixed.** The object index is stored when the controller is cached; liveness is `IndexToObject(saved)->GetUObject() == m_pc && !IsUnreachable()`, which never reads the object. Test: `cached_controller_never_read_after_it_was_freed` (0 reads of the freed object). |
| 3 | SHOULD-FIX | Only the marshal drain calls K2, not `VmtEngineTickDrain` (CAP09 / cap07 use both). | **Fixed.** `KbmGameThreadFrame()` also after `NativeModsGameThreadFrame(1)`; the 8 ms gate keeps sampling once per frame. |
| 4 | NIT | No line if no controller is ever found; Merged swallows child exceptions silently. | **Partly.** New one-time line at the first minute when nothing was decided: `[kbm] engine-keys waiting reason=<no-binds\|no-controller> bound=<n>` (test `waiting_line_when_nothing_was_decided_in_the_first_minute`). The SUM line format is left exactly as DESIGN.md; the child-exception count is not logged (the update thread has no log sink here). Open. |
| 5 | NIT | The slowdown divisor never recovers. | **Fixed.** 60 sampled frames in a row under budget halve it. Test `divisor_recovers_after_a_slow_patch`. |
| 6 | NIT | Keyboard presses now emit `pad_key_edge` tags and use the pad's 60-tag budget. | **Documented** in APPLY.md and LAUNCH-CARD.md (those tags are not pad proof in K2 and later reports). |
| 7 | NIT | No off switch without a rebuild. | **Not done** (needs a SettingsManager change, another unit). Fail-closed + rollback build. Documented in APPLY.md. |
| 8 | NIT | Tests: glue untested on host; frame slack hides small overruns; no zero-bound test; no failed-deactivate test. | **Fixed.** The glue is now an engine-free `EngineKeysRuntime` in the header (the `.inc` only creates one) and is host-tested: compose once, failure keeps the input, frame before compose, re-entry samples nothing, retire stops (`runtime_glue_compose_once_failure_keeps_input_retire_reentry`). The fake engine counts every access at or past ParmsSize (0 in all layouts). Added `failed_pad_deactivate_still_reported_through_merged`. |

The fixes were not sent back to the reviewer for a second pass (project rule: one independent review, no reviews of
reviews). The integrator may ask for a delta look at the items above before the build.
