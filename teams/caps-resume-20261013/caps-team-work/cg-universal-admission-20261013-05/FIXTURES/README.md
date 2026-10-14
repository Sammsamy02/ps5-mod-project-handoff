# cap13 FIXTURES — clearly labeled test fixtures

**These are hand-crafted test files, NOT real mods and NOT real game content.**
No downloaded Townfall DLL mod exists in any staging tree on this machine
(evidence: `../REPORT/mod-selection.md`), so capability #13 preparation uses
labeled fixtures per the handoff instruction ("do not call that a real-mod
pass").

| File | What it is | sha256 |
|---|---|---|
| `cap13-ue4ss-native-mod-FIXTURE.dll` | x64 PE32+ DLL shaped like a minimal UE4SS native mod: exports `start_mod`/`stop_mod`, imports `UE4SS.dll!is_ue4ss_initialized` (a name verified against the real staged UE4SS.dll export table) and `KERNEL32.dll!DisableThreadLibraryCalls`. DllMain calls both imports. Never executed — Windows-only bytes, built on macOS by `../TOOL/make_pe_fixtures.py` with Python `struct`. | `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce` |
| `cap13-native-x86-FIXTURE.dll` | 32-bit PE32 variant; exercises the `unsupported-arch` classification (cannot load into the 64-bit PS5 host process). | `90c46b82b1e4d3eda200526760a391522157b59b7f771cdc187ae4f3017ac543` |
| `NOT-A-PE-FIXTURE.dll` | Plain ASCII text with a `.dll` extension; exercises the `unknown` classification and precise-diagnostic path. | `637c449474b77e008a8fd9e2628ea01df4c127478fdfdf843dda165e4960bd65` |

Each DLL also embeds the string `CAP13 FIXTURE: hand-crafted minimal PE ... ;
not a real mod; not for distribution.` in its `.rdata` section, so the files
identify themselves even if renamed.

Rebuild (deterministic, bit-identical output):

    python3 ../TOOL/make_pe_fixtures.py

Regression suite: `python3 ../TOOL/run_tests.py` (31 checks, must be 31/31).
