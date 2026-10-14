# Isolated L13 pipeline review

Read-only review of the generator, generated runner/driver, inherited build helpers, and available first-pass receipt. No pipeline execution, source mutation, or console contact was performed by this reviewer.

## Findings sent to the owner

1. **Resolved before compilation: duplicated absolute source path.** The first generator version replaced a relative `outputs/.../units-l12` substring with an absolute L13 directory inside an already absolute `shadow-copy=...` / `unreal-unrealinitializer=...` argument. That would generate `/what//Users/.../what/...`. The owner changed the replacement to `UNITS.relative_to(ROOT)`. Current generated driver lines 364 and 400 point to the intended candidate source, and no duplicated root remains.
2. **Resolved for successful passes in the current generated runner: per-pass driver evidence overwrite.** `post_build()` archives top-level runner receipts, integration files and the overlay manifest. It does not yet archive `driver/staging` flat records (`odr-gate.json`, `entry-preflight.json`, `receipt-preflight.json`, `chain-symbols.json`, `compose6.stdout`, `compose6.stderr`) or `driver/transport-ledger.composition6-mirror.json`. Pass two uses the same paths and replaces this pass-one evidence. The current generated runner now copies these into the candidate-specific receipt archive before starting pass two.
3. **Resolved in the current generated runner: retry archive collision.** `the_pass()` moves existing composition/candidate/overlay directories aside, but `post_build()` then uses `receipts/<candidate>.mkdir(exist_ok=False)` without moving an older candidate archive aside. A full rerun following a previously successful first pass can fail late after rebuilding. The current generated runner now moves an existing archive aside under a nanosecond-suffixed name before creating the new archive.

4. **Residual failure-path preservation issue sent to the owner.** The archive is created only after successful composition, packaging and post-build gates. A failure before that point skips archive creation; restarting the attempt can overwrite driver staging records. The top-level log captures driver output, but some failure paths print only a tail. Preserve existing driver flat files/mirror before a new attempt or archive in a failure/finally path. This does not change source or binary correctness; it affects complete failure evidence.

## Verified isolation and coverage

- Generated `MY`, composition runs, integration outputs, package output and driver overlay all resolve beneath `outputs/2026-10-07-l13-full-build`.
- Original l12 edition/source/build paths remain read-only inputs. The fixed composer receives an explicit isolated `--output`; the receipt regenerators receive isolated `--output`; the finalizer writes alongside the isolated manifest. No console command occurs in the reviewed pipeline.
- The overlay builder creates symlinks for the historical lineage's existing files, then writes new lifecycle entry/bridge names. The actual historical lifecycle directory contains neither new name, so these copies do not follow a symlink into the frozen baseline. Header staging explicitly rejects existing/symlink destinations.
- Edition and regenerated replacement-receipt SHA pins match current `UnrealInitializer.cpp`, `PoolAdmissionEngine.hpp`, `CopyinSink.hpp`, `CopyinStartup.hpp`, the bridge, `SeedValidation.hpp`, and `shadow_copy.cpp`.
- The three new headers are explicitly passed to `--stage-header` with hashes. The initializer includes the same co-located production headers from the candidate source directory.
- The post-build routine now selects the exact requested `run-l13-<n>` rather than the newest directory by mtime. New bridge APIs are included in the provider/resolution checks.
- AST parsing succeeds for generator, runner, driver and fixed composer. This is syntax validation, not a claim that both full builds have passed.
- Package implementation is separately owned/reviewed by `l13_pipeline_audit`; this review did not duplicate that work.

Reviewed snapshot hashes:

- `prepare_pipeline.py`: `4f70e989dccd020d77d9e0955108c614894099ac2fabcc8a27db7f9ffda6b881`
- `launch13_one_pass.py`: `181270d7217471000edc74e92a0f28e7cdf670d72eeeeef569da32c45cb18c54`
- `build_runtime_composition6_l13.py`: `b0ffc29da6d7cac598821f05ad3fd2bd06bdc4e0eea2e4ba0afed0e0e493a5fe`
- `build_runtime_composition6_fixed_l13.py`: `9eab257e92fe8d37b0e0143ecf18024f0f4642ae07aee13efd4eb93b0839750d`

The owner is still running the full pipeline. This report does not claim final artifacts, double-build determinism, or upload readiness.

## Follow-up snapshot

Successful-pass evidence and archive-collision corrections reviewed at runner SHA `80d8c4b820e4cb54f793eab717871dbf334b3f3ced58d1c0d0d22bdaee6100c0`, generator SHA `be501ca498b9e05981efb6112674e931329f46417459bd080d6e53641dace7a1`. The residual failure-path issue above remains separate.
