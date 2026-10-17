# GATES — capability #10 (Team G)

Rule followed throughout: **never weaken a gate.** GPT's V2 patch replaced the
fixed G7 / P7 / P8 expectations with numbers the build computes from its own
inputs (self-referential — it would accept whatever import set the inputs
imply). That is **rejected**. Every expectation below is a fixed, reviewed
number or an explicit admitted list, pinned in a file the build does not
generate.

## The reviewed source of truth

`cap10/ADMITTED-IMPORTS.json` (schema `cap10-admitted-imports/1`), pinned by
sha in `cap10/INTEGRATION-INPUTS.json`. It carries: the R15e 207-identity
baseline (copied, pinned), the 143 admitted new rows (each with captured
provider evidence and two named sources), the declared raw-undefined delta, the
fixed `init_array`/`fini_array` counts, and the list of SceNet names removed by
policy. `cap10_integration.admitted()` loads it and **refuses** (raises) if any
row lacks `class == "captured-provider-export"` or either named source, if a row
does not match its captured provider export when re-parsed (identity, kind,
encoded NID — the label is never trusted), if a provider file's sha has
drifted, if a name is duplicated, or if the count is zero/mismatched. It is
never computed from the build being gated.

## Gate-by-gate

| Gate | Where | R15e | #10 expectation (FIXED) | How enforced |
|---|---|---|---|---|
| **G7 raw import delta** | `build_variants.py` link step | raw == BASE − Lua delta | raw == BASE − Lua delta **+ declared cap10 delta** (from ADMITTED-IMPORTS.json: +146 strong, +5 weak, −0), total **361** (R15e 210 + 151) | restored `record('G7-import-delta-declared…-cap10')` compares `llvm-nm -u` to the declared set exactly; raises on any diff. GPT's input-closure prediction kept as an **additional** check `G7b-cap10-predicted-raw-closure` (not a replacement). The cap10 delta is measured on top of R15e; it assumes #7 adds 0 raw imports (INTAKE §4a), which #7's own G7 enforces. |
| **P7 import identities** | `l26_packaging_driver.py` | `==207` | `==350` **and** the identity set == R15e 207 ∪ the 143 admitted rows (name, module name/version, library name/version, type) | `p7` compares the packaged `EXACT-IMPORT-IDENTITIES` identity-tuple set to `CAP10_ADMITTED['identity_set']`; the computed contract must also agree. |
| **P11 pair import sets** | packaging pair receipt | `==207` | `==350` on both variants and equal to each other | checks `len == CAP10_ADMITTED['final_count']` on A and B. |
| **P8 constructor arrays** | packaging | `240 / 2` | `240 / 2` (fixed; the computed sum must also agree) | `p8` compares `init/fini` to `CAP10_ADMITTED['init']/['fini']` AND to the input-sum. |
| **G6 link census** | `build_variants.py` | 396 | **401** = 391 + 8 NEW_UNITS + 2 artifact inputs. The 8 NEW_UNITS = 5 already registered (pakmount, lua-lfb, buildstamp, tcselftest, lua-pathshim) + 3 cap10 units (owner, boundary, **offline-net**). The 2 artifacts = static corlib object + Mono archive. GPT's ordered-input check kept. | `cap10.link_census(…, 391+len(NEW_UNITS)+len(CAP10['artifacts']))`; exact count + fixed-input order. |
| **include-graph** | `compile_unit` | per-unit BASE deps | adds exactly one exemption: `<csignal>` for **`cap10-mono-owner` only** (exact unit+path+sha, `238f1f21…72d1`), recorded in `G-CAP10-include-exemption`. Every declared exemption must be USED (a stale one fails). The 24 Mono headers pass under the existing `l26-runtime/` rule; their shas stay pinned. | new `cap10.include_exempt()` + `G-CAP10-include-exemption` gate. No directory/wildcard waiver. |
| **G2 CHANGES pins** | `build_variants.py` | files[] sha pins | +1 row: `UE4SSProgram.cpp` candidate sha `b6a68e3f…` (the lazy-start edit); `cap10_row_sync` record added and **enforced**: `cap10.pin_chain()` (called by `admitted()`) refuses unless that record holds the exact shas of `INTEGRATION-INPUTS.json` and `ADMITTED-IMPORTS.json` | standard G2; the row's sha is refreshed by `tools/refresh_pins.py`. |
| **field_gate** | engine-field reads | 49 fields / 1 accepted (full tree) | the lazy-start edit reads **no** engine field; the cap10/native code reads none. On Team G's minimal scratch tree: 38 proven, 0 unproven, 0 accepted. | the integrator re-runs on the full live tree; expectation = unchanged from R15e (no new field). |
| **fs_throw** | throwing std::filesystem | 14 keys in UE4SSProgram.cpp (baseline debt) | **0 added.** Key-set of the edited UE4SSProgram.cpp is byte-identical to live (verified). New files `Cap10CSharpMods.hpp` / `Cap10OfflineNet.cpp` use only `error_code` overloads → 0 sites. | `fs_throw_gate.py --baseline fs_throw_baseline-1e49188b.txt`; cap10 code passes with 0 sites. |
| **no_detour** | detours of game code | PASS | PASS over candidate **+ typescan-r4/include + cap10/native** (data-only hooks; no exec memory; the dl/mprotect hooks fail closed). | `no_detour_gate.py --root …candidate --root …/include --root …/cap10/native` → 0 sites. Integrator must add the two extra `--root`s to the pipeline (INTAKE §5). |
| **verify_build markers** | `harvest/markers-declared.json` | 66 markers | add the cap10 `diagnostic_event` tags (utf-8, from `MonoOwner.cpp`): `CAP10_STAGE, CAP10_IMAGE, CAP10_RESOLVE, CAP10_RUN, CAP10_THROW, CAP10_RECOVER, CAP10_DONE, CAP10_COOKIE1, CAP10_COOKIE2, CAP10_R1_CALLS, CAP10_R2_CALLS, CAP10_T_CALLS`, plus the two `[CS]` lines (utf-16le) from `UE4SSProgram.cpp`: `[CS] C# probe requested…`, `[CS] C# runtime not started…`. | the integrator runs `harvest/add_build_markers.py` before the build; the exact literals are in `APPLY.md`. |

### What is a staged diff, never applied live
The P7/P8/P11/G7/include-graph changes are in the **patch** copies of
`build_variants.py`, `l26_packaging_driver.py` and `cap10_integration.py` under
this folder's `PATCH/`. Nothing here is applied to the live candidate tree, the
real packaging driver, `CHANGES.json`, or any frozen build. The integrator
applies the patch and runs the one real build.

### Gates print their count and fail on zero
All Team G gates/tests print `items_checked=` and exit non-zero on zero:
`target_compile items_checked=4`, `include_graph_check items_checked=2184`,
`test_team_g_gates items_checked=152`, `test_team_g_drivers items_checked=4`,
`test_csharp_planner items_checked=16`, `test_offline_net items_checked=10`.
`admitted()` and `include_exempt()` raise when their lists are empty.

## Grow-only rule
`cap10/ADMITTED-IMPORTS.json` grows **only** by adding a row that carries
captured-provider evidence (its own `provider` file + two sources). No row may
be admitted on an SDK stub. If a later build legitimately adds an import, the
reviewer adds its evidenced row and the fixed count moves by exactly that many —
the build never sets its own expectation.
