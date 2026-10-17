# copyin-probe part — wiring for the l25 build (item 7)

Author: agent-Q, 2026-10-08. Observation-only logging; no behavior change.
Full analysis: `outputs/2026-10-08-l25-prep/agent-Q-copyin-probe.md`.

## Why this is delivered as overlay files, not a build_runtime.py part

`build_runtime.py` (outputs/2026-10-08-l25-runtime/build_runtime.py:41-47) maps each
`files` row one-hop: `replacements[original] = candidate`, and the compile step resolves
a unit's source with a single lookup (build_runtime.py:106). Two parts therefore cannot
both modify the same unit source: the row whose `original` is the parent composition's
pinned path wins, the other is silently never compiled.

The l25 `typescan-r4` part (owned by another agent) already declares
`original = outputs/2026-10-08-l24-runtime/typescan-r3/candidate/src/UnrealInitializer.cpp`
(sha 78c16fb7…) → `candidate = outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
(sha c4364dd7…) in its `files`. A second row from me against the typescan-r3 original
would trip the duplicate-original assert (build_runtime.py:45); a row against the
typescan-r4 candidate would be dead (one-hop lookup never reaches it). Per the task's
explicit fallback ("write the patch as a new overlay file + the exact wiring instructions
rather than guessing"), the UE4SS-side change is delivered as a pure-addition patch on
top of typescan-r4's candidate, and the bridge-side change as a packaging-lineage
overlay (runtime_start_bridge.cpp is not a unit of the runtime-object build at all; it
is linked at the packaging step, cf. `bridge_copyin_initializer_enter` present in
outputs/2026-10-08-l24-build-r2/pass1/undefined.stdout:23-30).

## File 1 — UE4SS side (unit `unreal-unrealinitializer`)

- Base: `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
  (sha256 c4364dd734019969be12491ffd8a76bed24f5e3a862ba331179f3e53c650cd4e)
- Patch: `patches/unrealinitializer-copyin-probe-liveness.patch` (pure additions)
- Ready-made candidate: `candidate/src/UnrealInitializer.cpp`
  (sha256 125a04718b52ef3b2314ca5c7bcae846ae5fc7c0c4d55fcb1ee38a2bd5c3765f)

Exact wiring (owner of typescan-r4 or coordinator):

1. Verify the base still matches:
   `shasum -a 256 outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UnrealInitializer.cpp`
   (must equal c4364dd7…; if typescan-r4 revised their file, rebase the three hunks:
   probe() hook, CopyinProbeLiveness helper, post-complete() call).
2. Adopt the whole candidate: replace the typescan-r4 candidate file with
   `copyin-probe/candidate/src/UnrealInitializer.cpp` — this needs NO build_runtime.py
   change, because typescan-r4's existing `files` row already maps the unit source to
   that path; only its `candidate_sha256` must be updated to 125a0471… (and the row in
   the `changes` key kept in sync, plus their copied_original stays c4364dd7-recorded).
   Alternatively apply the patch in place and update `candidate_sha256` to the result.
3. No include-directory or added-header changes are needed.

Import safety: the only external symbol added to the unit is the `diagnostic_event`
DECLARATION (already undefined in the baseline object, undefined.stdout:38) — the
import-delta assertion (build_runtime.py:143-144) is untouched.

## File 2 — bridge side (packaging lineage)

- Base: `outputs/2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp`
  (sha256 89e4b64344fe0acd5cb5cf30156420f3f73c994f7ccdc949ab66b50a1a6f0eac)
- Patch: `patches/runtime_start_bridge-copyin-probe-liveness.patch` (72 added lines)
- Ready-made candidate: `candidate/lifecycle/runtime_start_bridge.cpp`
  (sha256 f510c01484e3edcc21960fb0bb5c23562e548522b30cc629e43baf3dd1911712)

Exact wiring at the l25 packaging step (when it re-composes the lineage, as the l24
packaging did from `lineage/`):

1. Verify the l25 packaging's `lineage/lifecycle/runtime_start_bridge.cpp` still hashes
   89e4b643… (the base this patch was diffed against).
2. Replace that lineage file with `copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp`
   (or apply the patch), and record the new sha in the packaging receipt.
3. No new imports/exports; all additions are `static` state + `::diagnostic_event` calls
   inside existing exported functions (`bridge_copyin_initializer_complete`,
   `bridge_copyin_initializer_leave`, `bridge_kernel_write_copyin_probe_until`).

If the coordinator prefers the bridge change to ride the NEXT packaging (not l25's),
the UnrealInitializer-side patch is still safe to adopt alone: it emits the one-time
state line from admission-side facts, and its `note_probe_call` throttle works even
without the bridge-side counters (they are independent observations of the same
dispatch class).

## What the logging emits (expected shapes)

Post-admission success (the crash-session configuration, oracle-probes=0):

- diag: `copyin_probe_state 3` (gate armed | bank retired), `copyin_probe_gate_armed 1`,
  `copyin_probe_bank_retired 1`, `copyin_probe_parent_fd_open 0`, `copyin_probe_sinks_open 0`,
  `copyin_probe_nonce_used N`, `copyin_probe_attempts_total N`
- UE4SS.log: `[Seeds][L25] copyin probe state after admission: kernel-write dispatch STOPPED …`
- shutdown: `copyin_probe_gate_released 1`, `copyin_probe_attempts_late_total 0`

Only if something unexpected fires later (none exists in the composed runtime today):

- diag + UE4SS.log: `copyin_probe_fire_post_admission …` + addr/len fields, throttled
  to one note per 10 s, plus the bridge's own refuse-path telemetry
  (`pool_copyin_probe_fresh 2` = Unavailable, no kernel write).

## Verification hooks (no console needed for review)

- `grep -c "^-[^-]" patches/*.patch` → 0 (pure additions).
- Brace/paren balance of both candidates equals their bases (checked; see report).
- Import set: `llvm-nm -u` on a rebuilt runtime object must equal the l24 baseline
  (build_runtime.py enforces this itself at build time).
