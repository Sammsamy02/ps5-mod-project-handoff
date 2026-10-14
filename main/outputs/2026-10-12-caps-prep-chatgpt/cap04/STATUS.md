# Capability 4 — early loading

Label: Mac-only audit / partly proven by historical console receipts. No fresh console verdict.

The 2026-10-10 status table supersedes PHASEC-EARLY-LOADING-DESIGN.md's broader SATISFIED-ALREADY wording: runtime residency during initialization is receipted historically, but admission before first LoadMap is not. Neither startup before item scan nor a later map callback establishes the first-map ordering.

Fresh marker-source-audit/ supersedes the earlier line-range observation: current FL-MAP helpers emit real pre/post callback markers, but a post-only callback can allocate ordinal1, so that ordinal does not establish process-first entry. Deferred paired registration has no successful-side rollback in its partial-failure branch; mixed real/fallback observation is a source-level risk, not a reproduced runtime failure. WORLD-EV remains inferred/delayed/partial. The reviewed ready seam has no shared run/build/clock marker. Six exact source hashes were stable and root-rechecked; this is source evidence, not built/deployed behavior. See MARKER-CONTRACT.md for exact lines and minimum proof instrumentation.

New deferred-arm-stage/ supplies an exact-source, exact-once staged correction for partial native Lua delivery and startup publication ordering. Two PROSPERO callback guards admit only eager0/full2; partial native registration stays inert for pending1/refused3 while the existing fallback remains. State/route initialization now precedes publishing pending, so startup cannot overwrite the armer's terminal state. The refusal log describes the remaining inert registration accurately. No synchronous unregister is attempted from a callback. Six source-level checks pass; one independent review has R1 resolved and no open critical/important findings in this narrow scope. Source unchanged; no apply, compile or deployment. See deferred-arm-stage/RUN.md and final hash records in REVIEW.json.

Next step: the runtime owner may integrate this reviewed stage later; B7/build/launch does not wait. The combined #4+#5 authorized launch uses `MILESTONE-PROOF.md`. Three passes apply only to kernel capabilities #1/#3 per latest human order; the older Phase C three-pass proposal does not add a requirement. First-map and actual partial-arm behavior remain unproved.

## Implemented offline prep (2026-10-12)

verify_milestones.py + test_milestones.py: 8/8 behavior tests pass. README.md defines the normalized evidence contract and runnable commands. Existing preserved raw log replay, decoded through the project's decoder, correctly remains INCONCLUSIVE; preserved-log-audit.json records hashes and search-term references. No existing marker is invented and no capture coverage is inferred from a late callback. PASS checks supplied declarations, not their truth; source/coverage attestation remains a review requirement.

deferred-arm-stage/test_stage.py adds 6/6 source/staging checks, for 14 scoped #4 tests total. These do not execute C++ or reproduce a runtime failure.
