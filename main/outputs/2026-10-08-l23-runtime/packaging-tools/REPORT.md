# l23 packaging tools — prepared, not executed

`prepare_package.py` and `finalize_review.py` adapt the completed l22-package-r2 controller and checks. The approved target is **93 retained facts, zero new Map/Set offsets**. A blocked official compiler receipt is not a blocker for this explicitly approved build.

Paths resolve from the packaging-tools subdirectory: repository ROOT is the parent of `outputs`, runtime input is `outputs/2026-10-08-l23-build-r2`, and package destination is `outputs/2026-10-08-l23-package`. The failed first build is retained and never selected. These scripts have not prepared or run the l23 package.

## Preparation

Preparation requires `MAC_ONLY_IDENTICAL_L23_RUNTIME_PAIR`, pins both pass runtime objects/composition receipts and source selection, requires matching object hashes/sizes, unchanged runtime import census, and checks all four profile CHANGES records. It verifies the profile exactly equals the preceding 93 facts, rejects any promoted container offset, checks the new digest pair, required Array Inner and conditional/unpinned Map/Set entries.

The l22 controller already injects profile headers. Its single injection loop is replaced once with a destination-merged overlay set: RuntimeBuildProfile.hpp, CompiledNativeLayoutDeclarations.hpp and JoinedLayoutPreflight.hpp use current l23 candidates. Any other inherited overlay remains, and the complete inherited approved pin list/lineage source pins remain in BUILD-INPUTS. No l22 loop is injected a second time. Pass1 and pass2 select their corresponding runtime-l23.o separately.

The explicit clear-only `inputs/NuGet.Config` is copied from l22-package-r2. Both retained dotnet build commands are checked for the explicit RestoreConfigFile argument. Existing package-script local tool-environment handling is preserved. Controller exception handling explicitly sets failure status, including errors raised after a preceding success assignment; no failed import/array check can produce a successful package receipt.

## Finalizer

Finalizer requires twin SELF hashes and sizes, the exact 20 admitted static-gate names and true results in each pass, SDK0x11000000, constructor/destructor array byte/slot preservation, receipt-to-input binding, and exact structural equality of all207 final published import identities with l22-package-r2. Only relocation-dependent value_offset fields and row ordering are ignored. Provider names/IDs/versions, import type and publication data remain exact. The raw runtime211 strong symbols are a separate census, not confused with final207 published imports.

Current compile/retain/link counts are read from composition receipts rather than copying l22's counts. All profile, typescan-r2 and thread CHANGES rows must match current build selection. Actual inputs remain immutable; the finalizer writes a fresh package report, receipt, source-diff review and ZIP only when all checks pass.

The packet explicitly includes local runtime/tool sources, receipts and the artifact. It excludes the official compiler/source tree and official header directories. Retained profile JSON contains17 historical licensed method declaration snippets, so **review JSON copies and corresponding diffs omit source_declaration.text**, preserving omitted-text SHA256, original artifact path/hash and transform metadata in ORIGIN-INDEX. Actual build JSON and profile digest remain unchanged. Code copies remain exact. No stale pre-approval runtime BUILD-REPORT is copied as the new package report.

## Focused checks

Run `python3 outputs/2026-10-08-l23-runtime/packaging-tools/test_tools.py`. Ten focused checks pass: subdirectory paths; one overlay injection/per-pass object selection/exception status; controller anchor drift refusal; all4 current profile records/93-fact invariance; inherited overlay preservation/replacement; new-offset rejection; explicit offline NuGet setup; exact207 structural import matching and provider-drift detection; retained completed l22 package20-gate contract; Epic-tree rejection and17 declaration redactions without input mutation.

These tests use pure helpers and read-only retained l22 artifacts. They do not create the l23 package destination, compile/link/package, run the finalizer, or contact a console.

## Parent execution sequence after runtime-r2 is ready

1. Run `prepare_package.py` once; it refuses an existing package destination.
2. Run the copied package checker tests and retain `checker-tests.log`.
3. Run the generated package controller with `--pass-id 1` and `--pass-id 2`, retaining `pass1-controller.log` and `pass2-controller.log`.
4. Run `finalize_review.py`; inspect FINAL-RECEIPT.json and PACKET-RECEIPT.json. Runtime/package and source pins must still match.

No console or upload command is provided or executed by these tools.
