# l15 native FName adapter — local build report

**Mac-only complete; upload HOLD.** No console contact, upload, payload, game launch or configuration changes were made. The l14 crash is the diagnosis; l15 has no console result yet.

Artifact: [modbridge_supplier_composition.prx](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/prx-candidate-l15-2/modbridge_supplier_composition.prx)

- Size: **30,924,039 bytes**.
- SHA-256: `1cb1b27a2771a9178b6e5725665d398b81b56e8e84685903a67aa407b5585acb`.
- SELF magic: `4f153d1d`; native dynamic-module type retained; SDK `0x11000000`.
- Both freshly compiled/packaged candidates have identical PRX bytes, composed objects and initializer objects.
- Final offline verdict: **GO**, 11/11 checks; package verification 20/20.
- Strong imports: 200. No additions, removals or import identity changes versus l14. Fresh evidence file paths differ by build directory.

## Change summary

Only four production staging files differ from the frozen l14 staging: UnrealInitializer.cpp plus three new headers. Pad, pool admission/image coverage, copyin sink/bridge, existing game files and public retained FName headers remain unchanged. See [exact source delta](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/receipts/SOURCE-DELTA.json) and [initializer diff](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/reports/UnrealInitializer.cpp.diff).

1. Retained Function/FName API binds to a local thunk. The game callee uses `void(out, const char16_t*, mode)`: RDI output, RSI UTF16 input, EDX mode. Output is zeroed, eight bytes and aligned eight. Native RAX is ignored; the owned output is consumed. Public FName remains size8/alignment4/trivial, preserving retained ABI.
2. The exact saved routine at linked 0x23d590/runtime 0x63d590 scans UTF16 words to NUL, derives length internally and writes one qword through output. It is the wide/no-length form. The saved function incidentally leaves packed bits in RAX; that does not remove its output-pointer argument. See [saved disassembly audit](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/audit/ctor-signature/REPORT.md) and profile [PPSA33286 ABI facts](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/profiles/PPSA33286-fname-abi.json).
3. Before the first call, a fresh successful existing kernel-copyin probe must supply the 32 owned live prologue bytes. Cached capture, disk-only proof, unavailable/partial capture, mismatch, faults, unmapped/non-executable coverage and deadline expiration all refuse. No direct CPU text read, mprotect or kernel feature was added.
4. The Find-only ByteProperty probe resolves its returned id through the safely admitted pool, checks bounds/number/header/text and compares exact UTF16 input plus the independently admitted expected id. Failure logs/refuses before binding or further engine calls. Ordinary retained Add1 construction forwards through the same native ABI after bootstrap; Replace2 refuses. Add insertion side effects are not console-proven here.
5. Prefix capture and ToString reuse the admission budget. Cancellation and expiry guard first call and final binding; the new binding receipt includes elapsed time from the shared start. Pool-only acceptance is logged separately. The 60s deadline cannot preempt an OS/native call already entered.
6. Checklist row O covers admission passed followed by an engine ABI mismatch. [updated smoke checklist](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/reports/SMOKE-CHECKLIST.md) retains historical rows as classification, with current approval/executable/one-launch rules overriding their old recipes.

## String and engine ABI audit

The pinned Prospero compiler actually uses **two-byte wchar_t, TCHAR and CharType**, not the Mac's four-byte wchar_t. Static target assertions/macros establish that distinction. The new FName native boundary nevertheless uses explicit char16_t buffers and u literals, including conversion of retained string inputs. No active four-byte character-pointer crossing was confirmed. Globally switching CharType/FORCE_U16 would change retained C++ symbol identities; it was not done.

The complete inventory covers the selected 369 units, 4,667 source/header dependencies, 284 declared boundary probes including virtuals, direct Function families, native pointers and a trampoline. It records expected registers, hidden-result/this placement, literal sites and evidence level for each. [ABI audit](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/audit/engine-calls/ENGINE-CALL-ABI-AUDIT.md) · [full register table](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/audit/engine-calls/ENGINE-CALL-SITES.md) · [machine-readable inventory](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/audit/engine-calls/ENGINE-CALL-ABI-INVENTORY.json).

Examples: ProcessEvent expects UObject RDI/UFunction RSI/payload RDX. The 4.26+ StaticConstructObject wrapper expects parameter-struct RDI and UObject result RAX. FString value-return virtuals use hidden output RDI and receiver RSI; FName value-return virtuals currently return packed RAX. StaticFindObject is local traversal in this build. FText routes through reflected frames/ProcessEvent. Those are declared compiler expectations; unlike this saved constructor, their actual game ABI/layout agreement remains unvalidated. Placeholder structured-archive declarations are flagged.

A separate active NameTypes GetPlainNameString buffer-copy bug was discovered (8*length rather than 2*length). No caller/address-use was found in the selected graph and Lua does not expose it. It is documented for a separate fix; no unrelated NameTypes rebuild is included in l15.

## Host and target-compiler receipts

- Four fresh host runs passed: adapter/checks, plain and ASan+UBSan. Adapter covers zero/alignment, Find0/Add1 forwarding, invalid mode, UTF16/UTF32/non-BMP/invalid/oversized inputs and missing constructor. Deliberate wrong-ABI negative executes only in an isolated plain-build child; sanitizer build omits that UB experiment while retaining all sanitizer checks.
- Thirteen signature cases and seven pool roundtrip cases passed, including stale-cache/unavailable refusal, faults/deadline/partial/extent, narrow and UTF16 entries, wrong id/number/text/length. [host receipts](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/receipts/HOST-TESTS.json).
- Target assembly of the real production invoke helper confirms aligned zeroed output in RDI, UTF16 buffer RSI, Find0 EDX, indirect call and output reloaded instead of trusting RAX: 8/8 assertions. [target-call receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/receipts/TARGET-CALL-ABI.json).
- Saved-binary audit: 23/23 local assertions. Independent reviewer compiled the final initializer with the pinned Prospero compiler and reran plain/sanitized checks: [review](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-fname-adapter/audit/review/REVIEW.md). No remaining serious source-review blocker was identified.

## Full pipeline receipts and inherited diagnostics

[final receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/FINAL-RECEIPT.json) · [gate receipt](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/gate-receipt-l11-prx-candidate-l15-2.json) · [package checks](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/prx-candidate-l15-2/verification-evidence/static-checks.json) · [finalized change manifest](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/integration/change-manifest.finalized.json) · [full runner log](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l15-full-build/logs/full-pipeline-reviewed.txt).

The first candidate's provisional D1 failure was solely the not-yet-built twin; final candidate2 passes D1 and all other unchanged checks. Original ODR advisories remain in receipts. A legacy raw runtime-object delta logs a gap against the older composition2 baseline, then the inherited stage-aware rule validates the exact bootstrap definitions and post-link resolution. All final lineage checks are true and build gaps empty; the original LINEAGE_GAPS status string is stale after that explicit reconciliation. The copied runner's completion banner still says L14; all actual final paths, sources, receipts and artifact hashes are l15. These labels were not used to bypass a check.

Earlier local attempts and errors were preserved: initial runner pin/freshen-list/manifest staging omissions, then the alignment assertion caught by target compilation. Final reviewed source is the one used by both successful builds. l14 PRX rehashed unchanged: `769284e3a713f741b51ca2c5da64e654db0fb93e6b725511488600bc35ac8444`.

## Remaining console uncertainty and next step

The next authorized test must first prove that fresh code-prefix copyin succeeds on this native game's executable mapping. If it does, the corrected ctor and safe pool roundtrip can be observed; if not, this build refuses rather than calling an unverified target. A prefix match does not prove the full live function or remove races. Other engine-call ABI/layouts, full startup, input/menu and spawn remain unproved. Pad code is unchanged; ALREADY_OPENED/borrow/event behavior remains a possible later blocker.

**Await explicit approval before any l15 upload.** A future PRX-only step needs fresh decrypt-off backup/readback, exact full SHA and SELF magic, a new klog file and the user's single launch; then read-only harvest before any further launch. No config, loadlist, autoload, executable or kernel changes are included.
