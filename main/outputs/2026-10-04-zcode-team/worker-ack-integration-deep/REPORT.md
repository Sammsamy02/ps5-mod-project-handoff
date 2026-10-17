# REPORT — worker-ack-integration-deep

Deliverable: the worker-runtime-ack emission points are now integrated,
host-verified, and packaged as a controller-ready patch set so the NEXT
game-bound module built on the composition lineage carries them. Everything
here is host evidence; nothing ran on a console. All writes are under
`outputs/2026-10-04-zcode-team/worker-ack-integration-deep/`.

## 1. Vendored ack-enabled bootstrap (task 1)

- `vendored/bootstrap/` — full copy of the CURRENT
  `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/` (8 files + tests)
  with `runtime-ack.diff` applied via `patch -p1` in a worktree-layout replica
  (`vendored/APPLY-RECEIPT.txt`). All four patched files are byte-identical to
  worker-runtime-ack's own apply result (APPLY-MATCH ×4).
- Drift gate (suite step 1): fresh copy of current sources + diff reproduces
  the vendored tree byte-for-byte on every apply, every run. Current sources
  were unchanged since the diff's base (the 4 affected files hashed identical;
  the tree has since gained `activated_mod_source.*` + `tests/`, which the
  diff correctly leaves untouched).

## 2. Host suite re-run against CURRENT sources (task 1, task 3)

`suite/build.sh` (transcript: `suite/TRANSCRIPT.txt`, exit 0):

| Suite | Result |
|---|---|
| Drift gate (current+diff == vendored, 8 files + tests) | PASS |
| T1–T8 original ack suite (worker-runtime-ack, byte-copied, provenance hashed) | PASS |
| A1–A9 NEW adversarial pass (below) | PASS |
| Unchanged `tests/runtime_owner_test.cpp` vs patched owner | PASS |
| ASan/UBSan variants of both suites | PASS |

Adversarial corners (new, `suite/adversarial_ack_test.cpp`): A1 double-start
refused/no dup lines; A2 stop-before-start (`activated,stopped`, running=0);
A3 triple-stop + destructor (exactly one `stopped`); A4 module re-entry
(sequential sessions: file replaced, fresh header/pid, seq restarts at 1);
A5 stale-session re-open interleaving (FINDING: corrupts mid-file, reader sees
no state — prevented upstream by `module_entry.c` entry_guard, one session per
process); A6 ENOENT open failure (inert sink, lifecycle completes); A7
`failed(0)` normalized to code 1; A8 empty config inert with recorded reason;
A9 double close idempotent, post-close emits recorded no-ops. Required
scenarios re-verified: engine-start failure (T3/T4/T5 + A2), double-stop
(T2/A3), ack-sink open failure (T6/T8/A6/A8), module re-entry (A4/A5).
Drift found and fixed during iteration: one wrong assertion in my own A9
(test bug — the emit-once guard makes a post-close `stopped()` a silent
no-op), not a source defect.

## 3. Composition-2 wiring (task 2) — measured, not assumed

- The composition's `bootstrap_units` are `activated_mod_source`,
  `ue4ss_backend` (pad lineage, `native-coherent-pad-runtime-1/build-1/source`),
  `runtime_owner` (`native-coherent-runtime-1/source/bootstrap`) — all PRE-ack
  in composition-2 (`zcode_supplier_composition_2`, sha 6701258cbf432407…,
  364 units). Its exported `PrepareActivatedUE4SSRuntime` has the old 3-arg
  mangling and ZERO undefined references to it — the module_start caller does
  not exist yet; it is the next module's new work.
- Real drift found: the canonical outputs `activated_mod_source.cpp` does NOT
  compile under the composition argvs (`NativeCppModCandidates` /
  `NativeModSource::Prepare` absent from this closure —
  `evidence/syntax-activated_mod_source.stderr`). So bundle 02 does NOT
  replace it; instead the ack was forward-ported onto the composition's own
  3-arg lineage: `runtime_owner.{hpp,cpp}` verbatim ack-patched +
  composition-adapted `ue4ss_backend.{hpp,cpp}` (same emission semantics,
  no `activation_root` param).
- Verified mechanically: all three units compile exit 0 under the EXACT
  notify-cancel bootstrap argvs against a destination-mirroring staging tree
  (`evidence/DEST-COMPILE.json`, objects in `evidence/dest-stage-objs/`).
  Symbol tables prove the closure-consistent link: new `ue4ss_backend.o`
  references the ack ctor + `RuntimeAckSink::Open` (defined by the new
  `runtime_owner.o`) and the EXISTING 2-arg `MakeActivatedUE4SSModSource`
  (defined by the unchanged `activated_mod_source.o`).
- Supplier facts (MEASURED): `pid` = `getpid()`, `now_ms` =
  `clock_gettime(CLOCK_MONOTONIC)` (identical to the vendored component's own
  `posix_now_ms`); both symbols are already in the frozen import contract and
  `getpid()` is already used inside composition-2's own `lmf-sce-ops`.
  `status_root` = the exact `mfa_config::root` string passed to `mfa_activate`
  (mod root) → `<root>/runtime-status.txt`, matching RUNTIME-ACK.md §1 and
  worker-e2e-final's reader rule (`running` = last state `mods-started`).
  Full spec with reference code: `controller-patch/02-composition-wiring/WIRING-SPEC.md`.
- Import safety: the ack adds ZERO new strong imports. New references are
  defined by the replaced pair itself, in-closure (`mfa_status_*` are `T` in
  the composed object; `libc++.a` `to_string` + string append/assign/insert
  present in composition-2 `runtime.map`), or already in the contract.
  Expected recomposition gate delta: `added == []`; anything else = stop and
  reconcile.

## 4. Packaging wiring point (task 2) — no input-list diff

`controller-patch/PACKAGE-PRX-INPUT-DECISION.md`: `package_prx.py` runs
UNCHANGED. Its 10 `BOOTSTRAP_UNITS` never touch the ack symbols; the ack code
enters inside the composed object; adding a second copy at packaging would
duplicate strong definitions in the `-r` link; and the supplier belongs where
`PrepareActivatedUE4SSRuntime` is called (composition caller unit). Contract
stays 196 strong.

## 5. Controller patch set (DELIVER)

```
controller-patch/
  01-bootstrap-replacement/            canonical outputs tree (4 files + the diff + COPY-MANIFEST.json)
  02-composition-wiring/
    destination-relative/…             4 ack files for the composition-3 bootstrap (spike tree)
    REPLACE-MANIFEST.json              hashes + DO-NOT-REPLACE markers for activated_mod_source
    WIRING-SPEC.md                     supplier code, pid/now_ms, status_root, session rules, integration order
    build_runtime-bootstrap-replace.PROPOSAL.md
                                       bounded --replace-bootstrap-unit/--replace-receipt mechanism (PROPOSAL-ONLY;
                                       script is pinned — controller decision, mirrors the affected-units precedent)
  PACKAGE-PRX-INPUT-DECISION.md        why package_prx.py needs no diff (proof)
```

## 6. Integration order (controller steps after this delivery)

1. Apply bundle 01 to the canonical outputs bootstrap tree; re-run
   `suite/build.sh` (drift gate re-checks against current sources).
2. Apply bundle 02 files to the spike tree per REPLACE-MANIFEST.json.
3. Adopt (or reject/adapt) the bootstrap-replacement mechanism proposal, then
   recompose → composition-3. Gate expectation: `undefined_delta.added == []`.
4. Re-run `package_prx.py --verify-only` then `--link` against the
   composition-3 checkpoint (its inputs remain frozen here; no input diff).
5. Author the module_start supplier caller with the next game-bound module
   (WIRING-SPEC S1–S3), hash-attested like any extra unit.

## Honest limits

Host-only: no console execution, no PRX relink, no in-game evidence. The
composition-2 object itself remains pre-ack by design (my delivery targets the
NEXT recomposition); the build_runtime.py mechanism is a proposal, not an
applied change; `mods-started` remains the honest bootstrap-visible boundary
(engine callback + verified deferred native startup), with the residual
Lua-`main.lua` window documented by worker-runtime-ack and unchanged. The
composition-adapted `ue4ss_backend` pair and the canonical outputs pair are
deliberately two lineage-faithful variants of the same emission semantics;
they reconverge when the composition's port grows the native-entries APIs.
