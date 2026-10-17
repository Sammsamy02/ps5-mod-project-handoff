# Local UE4SS conformance tooling

This folder contains one inert Lua runner and a local Python comparator for the unmodified TownfallItemSpawner V1.0 release (Nexus mod95/file218). It is separate from the release. It has not been installed, enabled, uploaded, or run in either game. Loading `conformance.lua` returns a module and invokes zero engine APIs; `new()` returns `nil, "disabled"` unless `enabled=true` is explicit.

`manifest.json` pins the 24 cases, 17 API surfaces, exact runner/release/helper hashes, saved l16 binding-source hashes, and separate platform provenance. The PS5 runtime pin is the tested l16 baseline, not proof that its APIs ran. A future approved runtime needs an explicitly updated manifest/profile pin. PC pins remain pending. The two downloaded release files and saved console copies are immutable.

Run local verification from the worktree root:

```sh
python3 -m unittest discover -s outputs/2026-10-07-part2-api-conformance/tests -v
```

The tests exercise the actual Python CLI and standalone Lua artifact. Lua doubles are marked `host-fixture`/`fixture-pass`; they do not establish UE4SS, Townfall, or PS5 behavior. The available standalone interpreter is Lua 5.5.1; syntax uses features available in Lua 5.4, but no target Lua 5.4 execution was performed here.

The comparator reads JSONL or TSV with a single lossless `record_json` column. Each stream contains one `run` header, the ordered binding inventory, all 24 ordered terminal `case` records, and one complete `end` record. Partial case selection still emits explicit skipped rows and cannot pass full-suite comparison.

```sh
python3 outputs/2026-10-07-part2-api-conformance/compare.py \
  --reference /absolute/path/to/future-pc.jsonl \
  --candidate /absolute/path/to/future-ps5.jsonl \
  --json /absolute/path/to/comparison.json \
  --tsv /absolute/path/to/comparison.tsv
```

Omitting `--reference` returns `reference_pending`/exit2. Exit0 means the complete case suite matches with the supplied proved engine evidence; exit1 means substantive differences or failed behavior; exit2 means invalid records, incomplete proof, or a missing reference. Equal failed cases still fail. A match never establishes that the original mod's full menu/spawn behavior worked.

Only declared scalar fields inside the top-level `volatile` object can be removed: `identity`, `object_address`, `thread_id`, `known_game_thread_id`, `proof_id`, `duration_ms`, and `timestamp_utc`. Unknown volatile fields are rejected. Values, argument/return counts, callback order/counts, writable readbacks, cleanup results, errors and skip reasons remain exact. Addresses embedded in errors are not rewritten. JSON duplicate keys, nonfinite numbers, unknown statuses, wrong types, boolean order/counts, missing/duplicate/extra/out-of-order cases, and mismatched manifest hashes cannot pass.

PC and PS5 executable/runtime hashes are each recorded as provenance; they are not required equal across platforms. Suite, runner, immutable release, helper, game content build, scenario and controlled object context must match. Manifest platform pins are checked individually. `engine`, `proved`, thread/cleanup witnesses and attestation labels are declarations, not authentication: collect independent runtime/game/log receipts, verify their hashes, and retain them beside the records. A user can forge a JSON label; this parser cannot authenticate it.

For a future separately authorized engine run, call `runner.new(config)`, `session:run()` once, and `session:finish()` once after the approved manual observations. There is no driver, auto-loader, network sink, PC orchestration or PS5 deployment here. A file/console loader, trustworthy JSONL sink, native thread witness and controlled object fixtures must actually exist first. No supplied fixture fabricates those interfaces.

Required configuration contracts:

- `enabled=true`, `evidence="engine"` or `"host-fixture"`, `side="pc"`, `"ps5"`, or `"host"`; explicit `emit(json_line)` sink; optional `select={case_ids}` in manifest order. Every risk permission defaults off in `allow`.
- `metadata` supplies run ID, content build, scenario/context IDs, runtime build/SHA-256, platform game-binary SHA-256, runner SHA-256, release/helper SHA-256, and four independently receipted attestations: real engine, immutable release, independent thread witness, controlled object context.
- `thread_guard()` must read an independent native witness for the current invocation and return `{kind="independent-native", in_game_thread=true, thread_id=..., known_game_thread_id=..., proof_id=...}`. Equal IDs alone and a Lua boolean are not an interface or proof. Every engine case and asynchronous callback checks the guard. The l16 runtime exposes no verified Lua witness used by this tool, so current engine runs are pending.
- Fixture objects must be native `userdata` for engine evidence. Lua tables are permitted only in `host-fixture` runs, which never emit engine `proved` records. Bad API results after invocation fail; missing explicit inputs skip before invocation.
- Callback registration/dispatch needs `context.registration_plan_id` for an isolated, reviewed lifetime/teardown session. It may affect bindings/hooks and is never enabled by default. Registration acceptance is separate from actual key, game-thread and map-prehook delivery. The runner schedules no map transitions or synthetic key events; it only records approved manual events. Callbacks become inert at finish; removing native registrations still requires the isolated mod/session teardown.
- Construction and asset loading need exact owned class/outer/FName or cooked-path fixtures, `cleanup_plan_id`, and `cleanup(kind, context, object)` returning an independently verified cleanup receipt. Missing cleanup skips before calling; failed cleanup fails. The asset fallback requires an initially invalid lookup and preserves all returned object/found/loaded results plus the second lookup. No actor-spawn API is called by this runner.
- Property writes need an exclusively owned TextBlock fixture and `textblock_owned=true`. The runner snapshots scalar ShadowOffset X/Y and nested Font.Size values, performs the release-shaped writes, validates readback, restores the original scalars and validates restoration even after a write/read error. This does not exercise all UMG/color/margin/anchor layouts.
- Trace needs explicit native pawn/system objects, frozen start/end vectors and exact expected OUT point/normal values. It passes the original 12 arguments, ignored-pawn array and writable hit table. It never computes a spawn location, loads an actor class or spawns an actor automatically.
- Native string conversions need explicit FName/FText/FString userdata and expected text; name-pool and FText construction permissions additionally require fixture/lifetime context. Replace2 is observation-only because the PC contract/reference is pending. Loader inventory is observation-only because counting searchers does not prove disk/C-loading behavior. `print_capture(message)` must independently verify the exact output string before print behavior can be proved.

The first 17 cases cover the 17 surfaces in release flow order; later cases test actual callback delivery and known differences. The runner is a probe, not the original menu. Follow the release's real ladder and the pending reference plan in `REPORT.md`.
