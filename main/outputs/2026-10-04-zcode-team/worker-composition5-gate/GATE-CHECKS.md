# GATE-CHECKS — composition-5 pre-staging acceptance gate

Worker: worker-composition5-gate · 2026-10-04 · offline only (no console, no
network). Run the checker against a composition-5 candidate build dir BEFORE
anything is staged to console. The gate is fail-closed: any FAIL blocks staging.

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-04-zcode-team/worker-composition5-gate/gate_check_composition5.py \
  --build-dir outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/prx-candidate-composition5-1 \
  --json outputs/2026-10-04-zcode-team/worker-composition5-gate/gate-receipt-composition5.json
# optional: --compare-dir <second candidate dir>  for the determinism double-build check
```

Exit code 0 = all checks PASS (staging may proceed, console conditions in the
GO/NO-GO section still apply). Exit 1 = at least one FAIL (do not stage).

A build dir means the packaging output directory shaped like
`worker-composition-4-fix/prx-candidate-composition4-1/`:
`build/` (per-unit `.o` files + `combined-supplier.o`),
`modbridge_supplier_composition.prx` (or under `bare-original/`),
`build-receipt.json`, `verification-evidence/static-checks.json`.

## The checks

| id | check | how it is verified (scriptable) | PASS condition |
|---|---|---|---|
| W1 | Chain symbols DEFINED in the combined object | `nm` on `build/combined-supplier.o` (or the largest `build/*.o`): require exact/prefix matches for `mfa_activate`, `_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE`, `_ZN9modbridge6native12RuntimeOwner5startEv`, `_ZN2RC8Platform15NativeModSourceC`; report `StartActivatedRuntimeWithAck`/`StopRuntimeWithAck` presence | all four required names resolve to DEFINED (T/W) symbols |
| W2 | `module_start` actually CALLS the chain (not just presence) | locate the entry translation unit (`build/*.o` defining `module_start`; in the combined object use symbol value+size to bound `module_start`); parse its ELF64 `.rela.text.*` relocations (pure-Python parse, no tool dependency) and require call-range relocations (`R_X86_64_PLT32`, `R_X86_64_BRANCH4`, `R_X86_64_PC32` w/ PLT semantics) against `mfa_activate` AND (`StartActivatedRuntimeWithAck` OR {`PrepareActivatedUE4SSRuntime` AND `RuntimeOwner5startEv`}); emit `objdump -dr` disassembly of `module_start` into the receipt as evidence | the required reference set is non-empty |
| W3 | Entry exports intact | `module_start` AND `module_stop` both DEFINED with GLOBAL+DEFAULT binding in the entry object | both present, exported |
| I1 | Import census unchanged vs composition-4 | parse the Packager stdout JSON embedded in `build-receipt.json` (`"imports": N`); compare against `--expected-imports` (default 196 = frozen 191 + 5 extended); require static check `imports_equal_extended_contract` pass=true; a delta is acceptable ONLY with `IMPORT-DELTA-JUSTIFIED.md` in the build dir naming and justifying every added import | count equal, or justification file exists AND delta names listed there |
| D1 | Determinism double-build byte-identical | either (a) `--compare-dir` given: sha256 of both candidate PRXs equal, or (b) a `determinism.json` in the build dir with `build1_sha256 == build2_sha256`, or (c) two `bare-original`-style PRXs recorded in receipts with equal digests | two independent package runs, byte-identical sha256 |
| F1 | Fail-open diagnostic markers present | strings of the entry object contain the entry-guard/state markers `module_start_entered` and `module_start_return` (state discipline of `module_entry.c:59,72`) plus a diagnostic-channel open marker (`diagnostic_open_channel` reference); wiring-specific markers (e.g. an activation/mfa failure event) are reported as info | state markers + channel marker present |
| S1 | Size sanity vs composition-4 | sha256+bytes of the candidate PRX vs `--baseline-bytes` (composition-4 bare PRX: 30,771,688 bytes, sha256 `9b3758171db6c91661ac2d579c34b00c850bc687c28a95799103f5f7cdbf961d`); tolerance ±5% and hard cap `--max-size-bytes` (default 64 MiB) | within tolerance and cap |
| P1 | Packager (sharpprospero) static checks all green | `verification-evidence/static-checks.json`: `status == STATIC_CHECKS_PASSED`, every named check `pass == true` (the accepted set: elf_magic_bare, fself_magic_wrapped, wrapped_embedded_elf_header_equal, fself_payload_digest_matches_bare, wrapped_segments_equal, elf_module_type_flags, elf64_x86_64_le, pt_sce_module_unique, sdk_stamp_0x11000000, explicit_dt_init_dt_fini, loader_owns_no_arrays, init_fini_slots, init_fini_slots_present, section_.eh_frame_present, section_.eh_frame_hdr_present, section_.gcc_except_table_present, imports_equal_extended_contract, linker_provided_dynamic_not_imported, imports_resolve_to_sdk_or_linker, entry_alignment_evidence); plus Packager receipt fields `explicit_init=module_start`, `explicit_fini=module_stop`, `loader_array_ownership=false`, `runtime_or_hooks_executed=false` | every check true and receipt fields correct |

Negative control (already executed, see `gate-receipt-composition4.json`): the
checker run against `worker-composition-4-fix/prx-candidate-composition4-1`
must FAIL W2 (comp4's `module_entry.o` references only `probe_lifecycle_start`)
while passing W1 (the composed object defines the chain), F1, S1, P1 — proving
W2 detects exactly the comp4 defect and not something incidental.

## Minimal runtime matrix

| candidate state | W1 | W2 | W3 | I1 | D1 | F1 | S1 | P1 | verdict |
|---|---|---|---|---|---|---|---|---|---|
| composition-4 (unwired, negative control) | PASS | **FAIL** | PASS | PASS | FAIL (never double-built) | PASS | PASS | PASS | NO-GO — expected |
| composition-5 correctly wired | PASS | PASS | PASS | PASS | PASS | PASS | ~PASS | PASS | GO (offline) |

## GO / NO-GO criteria for staging composition-5 to console

GO (all must hold):

1. `gate_check_composition5.py` exit 0 on the composition-5 candidate dir —
   every check above PASS, receipt JSON saved next to this file.
2. W2 evidence reviewed by a human: the emitted `module_start` disassembly
   shows the chain calls; the wiring order matches WIRING-SPEC section 3
   (activate → [bindings] → prepare/mod-source → start; owner retained for
   module_stop).
3. Determinism: two independent packaging runs byte-identical (D1), and the
   entry unit digest matches the attested sha256 from
   `worker-module-start-wiring/REPORT.md` (driver Gate 1 recorded it in
   `worker-composition-5-rebuild/staging/entry-preflight.json`).
4. Packaging script report `STATIC_CHECKS_PASSED` with `gaps: []` and the
   lineage verification attesting the STAGED entry digest (P1 + rebuild
   procedure section 4).
5. The PRX sha256 + bytes recorded in the staging notes; nothing staged from a
   candidate whose receipt references a composition run other than the attested
   run (composition-5 run dir or re-verified run-3).

NO-GO (any one blocks staging):

1. Any gate FAIL, including import-census drift without a written justification
   file — a new import means the entry escaped the frozen contract.
2. W2 passing only via `PrepareActivatedUE4SSRuntime` WITHOUT `start()` —
   prepare without start reproduces the comp4 "loads, does nothing" state one
   level deeper; the gate treats {C without F} as FAIL.
3. `module_stop` wired to call `RuntimeOwner::stop()` from the init thread path
   or the owner not retained (spec section 4) — visible in review of W2
   evidence, not statically; reviewer must confirm.
4. Any mismatch between the attested entry sha256 and the entry compiled into
   the candidate (attestation drift) — re-attest, never bypass.
5. Size delta beyond ±5% without a written explanation (a wiring change should
   move the PRX by kilobytes, not megabytes).
6. Missing determinism evidence (D1) — a single run is not stageable.

## Minimal launch-readiness conditions (on-console, after GO)

These are the conditions under which a staged composition-5 can be called
"launched" — none of them are provable offline, so the offline gate deliberately
does NOT claim them:

1. Loader accepts the PRX (module_start invoked exactly once; no `-20`/`-21`
   re-entry or state fault on the diagnostic channel).
2. Diagnostic channel opens and shows, in order: entry state markers, the
   wiring/activation markers, then the RUNTIME-ACK states (`activated` →
   `engine_ready` → `mods_started`, points 1-3 of `runtime_owner.hpp:52-54`) in
   `<status_root>/runtime-status.txt`.
3. The game process remains playable with the PRX loaded (fail-open held: a
   startup failure must degrade to "mod absent", never crash or hang the game).
4. UE4SS observable side effect exists (log file or Lua mod entry point reached)
   — the first actual evidence that the composed runtime is alive, which no
   static gate can substitute.
