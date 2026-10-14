# Focused Mac handoff review closure — 2026-10-13

Verdict: PASS for the reviewed filesystem/build-integration source additions and the repaired Mac-only handoff tooling at the stage identity below. No remaining source-stage blocker was found in this scope. This is not evidence of a target build, full driver plan, complete packaging execution, engine ABI correctness, or console behavior.

The prior filesystem and staged build additions were independently inspected in this review; the dry-run/test-runner changes made here were self-tested. No separate review of those review-tool changes is claimed.

The reviewed authoritative stage is `derived/review-final/INTEGRATION.json` with 8 original inputs and 15 staged files. Its SHA-256 is `7ee0696709b11e32ded211fb072d3e04cdda25e3728ff663b7584cdd0796ffdc`; its complete `integration.patch` SHA-256 is `9239ed7f9251eb9d71a436a92365aef9e21f15abe0c3dfad34fdc108f8f0897d`. Older stages and receipts remain historical.

## Findings and fixes

- `dry_run_build_selection.py` previously executed at import, hardcoded the historical `build-closure` stage, and always attempted to create the occupied `BUILD-SELECTION-CLOSURE.json`. The regression reproduced this even for `--help`; `evidence/REVIEW-tooling-red.log` preserves the failure. The tool now has an import-safe entry point, explicit `--stage`, required fresh `--out`, optional separate `--audit-out`, and defaults to `review-final`. Stages must be direct children of owned `derived/`; receipts must be fresh files under owned `evidence/`.
- The dry run verifies SHA-256 and byte size for every selected staged file before extracting definitions. Its receipt identifies the selected directory, manifest hash, patch hash and all 15 verified file pins. It records a read-only audit of the eight live originals and reports any mismatch as requiring a deliberate rebind.
- `tests/run.sh` now includes `test_build_integration.py`. The focused tests execute actual definitions extracted from hash-verified staged drivers. Driver imports and main functions are never executed.
- The first live-original audit found `typescan-r4/CHANGES.json` had drifted from the old stage while the other seven originals matched. The parent task preserved that evidence, reviewed the live metadata delta, and generated `review-final` without overwriting the old stage. The fresh final audit matches all eight originals.
- The parent task also repaired `build_integration.py`: the old patch omitted seven additions and mishandled an original JSON file without a final newline. Tests verify the repaired `integration_patch` helper with ordinary files, missing final newlines, and an added file. The complete real 15-file patch applies to a temporary copy of the eight verified originals and reverses to their exact bytes, removing all seven additions.

## Source review scope

The filesystem changes use explicit error-code status and size queries and error-code directory iterator construction/increment. The producer checks errors both during iteration and after iterator termination, preserves the bounded all-or-nothing discovery behavior, and rejects symlink roots/entries and incomplete sidecars. The native entry retains its exception boundary. No additional defect was found in these new changes; historical core/ABI review evidence was not relabeled as fresh testing.

The build integration registers the new source with an exact SHA pin, copies the initializer template arguments, preserves template metadata, refreshes the two C++ candidate rows, pins five runtime headers and the required packaging profile, and refuses incremental selection before it reads build artifacts. The packaging helper admits only the declared exact profile header. The test for duplicate refusal executes the actual four-statement integration block from `derive_manifest`; it does not run the complete function or package controller.

## Fresh validation

`python3 tests/test_build_integration.py -v`: 18 tests passed. Evidence: `evidence/REVIEW-BUILD-INTEGRATION-TESTS.log`. Coverage includes explicit stage identity, all-file hash refusal, exclusive owned receipts, read-only original drift reporting, real staged NEW_UNIT source-hash refusal and initializer-template selection, incremental refusal, packaging overlay positive/missing admission/missing file/wrong hash/duplicate paths, and both patch roundtrips. `bash -n tests/run.sh` also passed. The complete host runner and unchanged JA core suites were deliberately not rerun for this focused review.

The actual metadata dry run on explicit `review-final` passed: 15 staged files verified; five new units with one `pakmount`; required profile overlay pinned; incremental refused; all 374 BASE dependency metadata files checked (50,920,691 bytes); no missing metadata. The five retained CodeImageCache consumers remain initializer, LuaMod, program, NativeThreadNameAdapter and ThreadNameRuntime. Evidence: `evidence/REVIEW-BUILD-SELECTION.json` and `evidence/REVIEW-ORIGINAL-AUDIT.json`. BASE receipt SHA-256 remains `b8adeb9680049881ac5a290826d94823b65ab7167283692e729e0ea4559057d6`.

To repeat against a later owned stage, use fresh receipt names:

```sh
python3 tools/dry_run_build_selection.py \
  --stage derived/review-final \
  --out evidence/REVIEW-BUILD-SELECTION-next.json \
  --audit-out evidence/REVIEW-ORIGINAL-AUDIT-next.json
```

All test fixture writes were under owned temporary test directories. No main, candidate, build, frozen, mod, or device path was written; no object bytes, SDK contents, console APIs, network APIs, compiler or target execution were used by this review.

## Remaining integration/runtime evidence

Use one combined full build after integration. The next build must prove actual source/include/VFS selection and rebuild every CodeImageCache layout consumer, then satisfy the existing filesystem/no-detour gates, exact import/constructor census, package manifest and build verification gates. Retained BASE dependency metadata describes the prior build only. External mod-root reachability, real engine ABI/delegate/allocator lifetime and the observable runtime result still require the approved launch. Live input hashes must still match when the stage is integrated.
