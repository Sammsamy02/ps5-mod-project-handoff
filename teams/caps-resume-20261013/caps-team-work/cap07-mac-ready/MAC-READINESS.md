# #7 Mac readiness — superseding source stage, 2026-10-13

**Mac-only PASS:** real `RuntimePakMount.cpp` plus all five retained
`CodeImageCache.hpp` consumers compile as x86-64 target ELF objects with the
same exact 16-pin header. No new runtime code change was required.

Use `derived/target-ready/INTEGRATION.json` and `integration.patch` for the
complete 15-file source/build stage. The frozen `cap07-step2` packet remains
unchanged (205 files verified). Main's eight original inputs still match this
fresh stage. Root remains the integration reviewer.

- Manifest SHA-256: `c4c14d6e85fff921f4f9b4741959e4d062e27ae7a010b5949f0f1c47d903d483`
- Complete patch SHA-256: `f91fbea596269b8b10329a555288e76c1adf576ec73518ae96fd5b6a788ddeb5`
- Expanded cache-header SHA-256: `3fa64af86ea25543d1197b683b8ddc86a910e3b83500679395aa12bdcc8b00c1`

Only `typescan-r4/CHANGES.json` differs from the frozen reviewed stage:
Claude's newer three header pins/two notes are preserved, and two independently
audited stale dependency pins are corrected to the actual current bytes.
The complete delta is `evidence/FINAL-vs-REVIEWED.diff`; original/live drift
and explicit rebind decisions are retained beside it. All runtime, profile,
producer, adapter and driver bytes match the previously reviewed #7 stage.

The stale dependencies are genuine current `NativePropertyReadBackend.inl`
and `LuaFNameBridge.hpp`, copied into immutable owned snapshots and consumed
through the target VFS. Root admitted their current contracts after inspection;
see `evidence/ROOT-DEPENDENCY-REVIEW.md`. The reader checks range, real VQ
readability and budget, **not** the startup fault ledger. Restoring the released
startup-owner checks would cause later calls to refuse. The exact old Lua header
pin bytes could not be recovered; historical continuity is not claimed.

| Real target unit | Exit | Actual dependency files | Warnings |
|---|---:|---:|---:|
| RuntimePakMount.cpp | 0 | 693 | 0 |
| UnrealInitializer.cpp, including native adapter | 0 | 1469 | 6 |
| LuaMod.cpp, including pak-mount entry call | 0 | 1320 | 13 |
| NativeThreadNameAdapter.cpp | 0 | 824 | 0 |
| ThreadNameRuntime.cpp | 0 | 1123 | 2 |
| UE4SSProgram.cpp with #10's actual observer composition | 0 | 1354 | 6 |

Clang 21.1.8 targets `x86_64-sie-ps5` through the genuine public payload-SDK
wrapper, with retained BASE argv and current declared include/alias selection.
Compiler executable SHA-256 is
`8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a`.
The compile receipts contain the exact argv, source/object/dependency hashes,
actual dependency pins, metadata and selected header routes. All 139 declared
source/header checks matched after the explicit corrections. Six ELF headers
and the pak-mount caller/definition symbol spellings were verified. Source and
dependency pins were rechecked after compilation. Warnings come from existing
headers and binding sites; none point to added #7 code.

`evidence/TARGET-VERIFICATION.json` is the verification receipt. The three
successful compile receipts are `target/attempt2/COMPILE.json`,
`target/cache-consumers/COMPILE.json`, and
`target/program-cap10-composed-fixed/COMPILE.json`. The program is peer-owned
source SHA `4fab1e61549bdb8ff6bcc10cdfa35c47b350086e80ec937d9b8c0b4996ba2eba`.
No duplicate program compile is needed for the same #7/#10 composition.

The 18 focused integration checks passed; six additional rebind/current-route
checks passed, including byte-exact application and reversal of the complete
final 15-file patch. One own-tool relative extra-include path defect caused the
first program compile to fail; normalization to absolute paths passed its
targeted regression and the program-only retry. Failed attempts remain in
evidence. Unchanged core runtime/host suites were not rerun.

The next combined build still must prove its complete source/include selection,
rebuild the full current header-consumer graph, link/import/constructor census,
packaging/manifest and verification gates. Incremental compilation remains
prohibited for this layout change. No full game/PRX build, build-driver main,
mod edit or console contact occurred. Actual engine ABI/delegate/allocator
lifetime, external pakmods reachability and a visible mounted/precedence result
remain Claude's approved console acceptance. Commands are in `COMMANDS.md`.
