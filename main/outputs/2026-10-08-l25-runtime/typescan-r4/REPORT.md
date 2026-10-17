# L25 provenance note (2026-10-08)

This report body is the l23 typescan-r2 text carried through l24 unchanged
(known l24 staleness, disclosed in the l24 REVIEW-PACKET section 6). It is
retained only as the baseline reference for the guard's original contracts.
What actually changed for l25 lives in `CHANGES.json` (scope + recomputed
hashes): the scan loop is wired to the Checked guard reads with per-object
[PropertyPointer] containment, `ScanCollectionScope` RAII closes scan mode
on any unwind, the bootstrap check compares pre-computed known-name indices
first and bounds the FName index by the verified pool before any ToString,
and the throwing wrappers embed reason/object/field/class detail. Nothing
here has been host-tested, target-compiled, packaged or run on console in
the l25 edition yet.

---

# L23 type scan r2: focused integration fixes

Sources and CHANGES.json are frozen for parent full rebuild. Six selected target translation units compile (exit 0); five host suites pass in plain and fatal ASan/UBSan modes (10 runs). The accepted `typescan` directory remains intact. No console contact, uploads, commits or new layout offsets.

Compared with accepted typescan, runtime changes are limited to:

1. TypeChecker's native field iteration now calls `NativePropertyGuard::ReadNext`. It reads the published `FField::Next` offset through `find`, validates the current field before reading the slot, and validates every non-null next field before returning it to the loop. Null terminates the chain. Missing Next remains the precise collectable missing-layout exception; unreadable/invalid/current/self/foreign targets remain hard pointer refusals. The non-PS5 branch retains its original getter. No raw native next getter runs before validation.
2. The selected CustomProperty.cpp had two assignments through Array::GetInner that the earlier source audit missed. Both now call a TU-local owned-object writer. It accepts only the fresh unique_ptr already allocated in the respective constructor; reads the existing published Inner offset; checks pointer-width bounds against `sizeof` that actual allocation type; and copies the pointer into runtime-owned memory. It exposes no raw game-memory setter or new offset. The Lua overload holds its dummy inner in unique_ptr until successful publication and then preserves the original lifetime. No references into game memory are restored.

No profile, initializer, thread, core gate or prior pointer-validation behavior was otherwise changed. All CHANGES original keys still point to l22 selected source/header paths, not the prior l23 candidate, so parent replacement selection remains direct. Twelve replacements and four helper headers/inls are fully hashed.

Evidence:

- `results/RAW-NEXT-RED.log`: accepted scan extraction reaches the deliberately forbidden raw native Next getter before a guarded read. R2 scan extraction succeeds.
- `results/CUSTOM-ASSIGNMENT-RED.log`: actual original CustomArrayProperty constructor bodies fail to compile against the by-value getter at both assignments. R2 constructor bodies compile and exercise both overloads, correct pointer publication, unchanged offsets/sizes, missing-key no-insertion and bounded refusal, with sanitizer coverage.
- `results/HOST-TESTS.json`: 10 successful runs including actual production guard, actual scan/core gate extracts, actual backend and both actual custom constructor bodies. Guard controls include unreadable initial/current field and unreadable next target before any raw getter/dereference, null termination, self refusal and missing Next collection.
- `results/TARGET-COMPILE.json`: Map, Set, Array, TypeChecker, initializer and CustomProperty selected target commands all exit 0; inherited Unreal-header warnings are retained.
- `../review/SELECTED-GETTER-WRITES.json`: scan across all 374 selected l22 source units finds precisely the two CustomProperty assignments addressed here.

Parent owns full closure rebuild twice, import equality, packaging and final packet. Host fixtures do not establish target engine execution or menu success. The VQ-then-read mechanism retains the stable-reflection-lifetime limitation documented in the original type-scan report.
