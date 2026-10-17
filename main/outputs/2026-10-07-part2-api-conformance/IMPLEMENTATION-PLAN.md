# Local Part 2 conformance tooling

Goal: preserve TownfallItemSpawner V1.0 exactly as downloaded while preparing an inert Lua API probe and a strict future PC/PS5 record comparator. The parent task already authorizes this implementation locally; no deployment, original-mod edits, runtime changes, account/network access, or commits are in scope.

Files: `conformance.lua` returns an opt-in runner and has no load-time engine side effects; `compare.py` reads JSONL/TSV records and writes a JSON/TSV verdict; `manifest.json` pins the cases, 17-surface census, 33 reflected-method census and source/release provenance; `tests/` exercises the actual tools locally; `README.md` and `REPORT.md` state usage, ordering and pending evidence.

- [x] Verify release/console hashes and five selected binding source hashes against the saved l16 receipt.
- [x] Write comparator tests first. Equal complete engine records may match; missing/extra/duplicate cases, host fixtures, skipped/unavailable/observed behavior, unknown normalization and metadata mismatch cannot pass.
- [x] Implement the comparator using only Python's standard library. Normalize only an explicit whitelist inside `volatile`; retain all substantive values and errors verbatim.
- [x] Write runner safety tests first. Loading invokes zero engine APIs; default/off or missing fixtures, risk permissions, cleanup context and independently witnessed game-thread context cannot invoke engine mutations.
- [x] Implement the single Lua script. Emit binding presence separately from behavior; preserve original signatures and call-flow order; buffer asynchronous callback evidence until explicit finish; mark local doubles as fixture-only.
- [x] Run the focused suite and syntax checks; review the owned-folder diff and rehash immutable source/mod files. Save local evidence and the PC reference plan without claiming engine parity.

Risks covered: incomplete streams, substantive errors containing addresses, callback registration without delivery, aliasing of writable nested structs, cleanup failure, and host doubles incorrectly promoted to engine proof. Same-game PC evidence is absent. All 17 API surfaces and all 33 reflected calls remain engine-unproved after l16's pre-constructor refusal.
