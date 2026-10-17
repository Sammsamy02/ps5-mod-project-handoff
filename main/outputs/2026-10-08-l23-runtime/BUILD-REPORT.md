# l23 review packet — local changes complete, package blocked on source evidence

**No l23 PRX was produced, uploaded, or run. No console contact occurred.** The latest console evidence remains l22/PID196: runtime initialization reached TypeChecker and stopped cleanly at missing FMapProperty::KeyProp before Lua started.

## Track A

| Requested part | Result | Evidence |
|---|---|---|
| Two-source Map/Set measurements | **Blocked**. Saved image measures KeyProp=0x70, ValueProp=0x78, ElementProp=0x70, calibrating Array Inner=0x78. Official full-header PS5 compiler probe has no valid output. New offsets are not promoted. | profile/REPORT.md; profile/evidence/image-measurements.json; ../2026-10-08-l23-layout-generator/TRACK-A-SOURCE-RECEIPT.json |
| Inner required | Implemented locally; preserved all 93 existing facts. Joined preflight has 66 keys. New Map/Set entries remain absent and unpinned until independent agreement. | profile/CHANGES.json; profile/tests/RESULTS.json |
| Optional nested discovery | Independent catches for Array/Set/Map-key/Map-value missing keys. No Ansi/Utf8 pushers added. | typescan/REPORT.md |
| Whole scan | Collects unique exact missing-key exceptions per field/object. Unrelated errors still propagate. Core gate accepts either object representation; UMG is not a generic requirement. | typescan/results/; actual original-field-scan failure control |
| Pointer validate-on-use | All const/nonconst Map/Set/Array accessors use proved readable bytes and authenticated known property class checks. Pointer values returned rather than unsafe slot references. Complete dependent rebuild required before packaging. | typescan/CHANGES.json; typescan/include/ |
| Game-thread self-test | Protected call through actual registered ExecuteInGameThread. Queue/ran/native thread/duration/once-only 30s timeout/cancel markers; existing queue lifetime. No alternate pump. | thread/REPORT.md; profile/REVIEW.md |
| VMT fallback | Written only; no hook built. Covers Itanium prefix, complete table extent, original forwarding, writable-data admission, lifetime and concurrent revert. | profile/VMT-FALLBACK.md |

### Focused verification (Mac-only)

- Profile: six plain/sanitized scenarios, digest mismatch refusal, 13 strict-join controls and 30 image replay assertions pass. No unproved offset accepted.
- Type scan: four suites in plain and fatal ASan/UBSan configurations (eight successful runs); actual scan/getter/guard/backend/core-gate extracts. Five selected PS5 target units compile.
- Queue: 31 assertions in normal and sanitizer configurations (62 total); actual callback/wrapper and retained insertion/drain bodies with host Lua. Full API registration is source-reviewed and target-compiled. Lua C dependency objects are not sanitizer-instrumented.
- State: normal, ASan/UBSan and TSan pass, including 100 concurrent timeout/drain/cancel trials per configuration. Both queue/Program translation units target-compile.
- Independent queue/lifecycle review found no actionable findings. Parent review corrected the initial overly restrictive Object/Ptr and CanvasPanel core gates; corrected scenarios pass.
- These are individual-unit and host receipts, not a complete linked runtime or proof of native execution. No twin-build determinism, final package checks or final import delta is claimed for l23.

## Track B

All 711 keys are inventoried: 688 compiler-blocked, 15 narrowly source-verified absent, eight unresolved native owner mappings. Of those rows, 180 also need UHT artifacts. **All 93 calibration observations are blocked: zero matched and zero mismatched. This is not a calibration pass.** The generated accepted-new-values list is empty. Ten generator/calibration controls pass, including intentional wrong/missing/duplicate observations and shim rejection.

Original official headers stop at PS5/PS5PlatformCompilerPreSetup.h. The authorized pinned source checkout lacks the PS5 platform extension. A separately labelled generic experiment also encountered missing platform mutex definitions and mismatched intrinsic resource headers; it yielded no accepted offsets. Layout-bearing types were not guessed, expected values were not adjusted, and incomplete compiler output was not promoted.

The complete minimum-observed dependency list, local compiler commands and source hashes are in the generator report. It separates the missing engine platform definitions from locally fixable LLVM resource headers and secondary UHT/macro-provenance requirements. Epic source and diagnostics quoting that source remain local and are excluded from this review ZIP. Nothing was committed or pushed.

## Next step / approval boundary

Complete a qualifying official-header PS5 measurement for the four container fields, then run the strict source/image join. Matching platform definitions are currently unavailable; a different evidence route must be described and reviewed rather than silently substituted for the requested method. Track B's broader 93-key calibration may continue separately and does not itself block a valid Track A measurement.

Once Track A evidence is accepted, rebuild the full dependent closure twice (including every user of changed pointer getter declarations), package twice, verify exact import delta and all gates, and present final PRX receipts for upload review. **No console launch is requested now.** Existing l22 files and rollback copies remain unchanged.
