# Executable source prep tools — Mac-only

`surface_inventory.py` inventories selected real mod source without compiling or modifying it. It removes ordinary comments/quoted literals while preserving source line numbers, records hashes and emits ImGui calls, hook references (including function-address registration), getter candidates and array-write candidates. `ACTUAL-SURFACE.json` has exact per-reference file/line locations.

This is a lexical survey, not a full C++ parser. Raw strings/preprocessor reachability are unresolved. Accessor names are candidates for the current field gate, not proven typed engine fields; array writes are candidates for human review, not automatically hooks or executable patches. No ABI, thread or runtime support is inferred from symbol presence.

```sh
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap09/surface_inventory.py .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/cppmods
python3 -B outputs/2026-10-12-caps-prep-chatgpt/cap09/test_prep_tools.py
```

The combined isolated prep test suite covers comment/string exclusion, actual hook address references, table-write visibility, empty scans, unknown/unsupported API coverage, mutated C# imports/signatures and absent/invented host APIs. Test-first failure receipts and fresh successful output are retained in this folder and cap10. Only this isolated utility suite was run; no repository-wide tests/builds were authorized.
