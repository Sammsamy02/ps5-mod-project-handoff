# Mac-side readiness: capabilities #1, #3, and unchanged DLLs

**Status, 2026-10-13:** Closer, but **not fully ready pending only console access**. The #1 and #3 protocols have local verification. The unchanged DLL path still has substantial offline loader and ABI work, and no intended-game DLL has demonstrated behavior. All new work is in the isolated stage `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03`. The attached handoff supplied context; the user's current request set the scope. I did not contact a console, edit the shared candidate, alter any mod binary, or touch Claude's #2/#4–#12 work.

| Area | Verified now | Remaining |
|---|---|---|
| **#1 memory reads** | Earlier console log proves only existing register-read sub-ops. The corrected v2 probe passed six Mac control-flow cases and 13 classifier cases; target C syntax check passed. | The syscall producer's carry-flag return contract is unresolved. The new v2 probe has no console run. Arbitrary kernel-address reading is not implemented or proven. |
| **#3 code patching** | Staged CodePatch passed 22 host protocol cases and four liveness cases; target C++ syntax check passed. It captures original bytes/protections and tests readback, restore, and dirty-state refusal locally. | PS5 `mprotect`/query behavior, executable writes, cache behavior, and restoration remain unverified. A general code-detour/hook builder is still absent. No revised target ELF was built or run. |
| **DLL loading** | A synthetic PE fixture completed load, entry, import, lifecycle, and unchanged-file checks (11/11). Win64 bridge integer call5/6 and guest-to-host arities 5–8 passed local value tests. | Both unchanged real gameplay DLLs are correctly **refused before execution** because they require static TLS. Their MSVC C++/CRT, UE4SS, and game behavior remain unsupported or unverified. The fixture is not evidence that either real DLL runs. |
| **Target resolution** | Prior C5 design separates name lookup, PC-to-PS5 function correspondence, and instruction compatibility. | No intended-game function mapping or compatible hook/patch instruction path has been demonstrated. |
| **Working gameplay behavior** | None claimed. | Requires an unchanged mod for its intended game/version, a compatible runtime, and console observations of the actual effect and restoration. |

## New Mac-side work

- Fixed the bounded evidence runner's real e2e `CHECK: ... PASS | reason=...` grammar. It now rejects malformed/embedded protocol markers, a hidden `FAIL` verdict, contradictory summaries, timeouts, nonzero exits, missing required passing checks, and input mutation. The adversarial cases failed before each fix; 19/19 runner tests pass now.
- Fixed two Win64 bridge defects in the isolated stage: call5 took its fifth argument from the wrong register, and call6 wrote its sixth argument over its own return address. Guest-to-host thunks now place integer arguments 5–8 in the correct SysV registers/stack slots. Direct local call tests and the fresh synthetic fixture pass. The target assembly syntax check produced an object, not a deployable payload.
- Bridge coverage remains limited to the tested integer signatures; mixed integer/float calls, aggregate returns, and deliberate nonvolatile-register clobber tests remain open.
- Acquired two unchanged upstream gameplay DLL examples: [ZeroCompanyMandoWardrobe](https://github.com/Sternab/ZeroCompanyMandoWardrobe/releases/tag/v0.4.2-build24874058) (SHA-256 `821ecad6c9f7e61faa2a07b31c216475768b72c2066369d3b0b88f715b33a40c`) and [TFWWorkbench](https://github.com/smotti/TFWWorkbench/releases/tag/0.2.1) (SHA-256 `24b05dc267dc6e9faa6cbcb4e85a8491733e0a86b86fbac92f5aa1a0950aa6ca`). Both have real static TLS; the loader's expected refusal passed 5/5 checks for each, with bytes unchanged. These mods are for other games and are not a substitute for the intended-game mod.
- PE import census: Zero Company requests 33 UE4SS symbols, all present **by name** in the pinned framework DLL; TFW requests 81, of which 80 are present by name. Symbol-name matches do not prove C++ ABI compatibility. Both also import MSVC/Windows CRT families. The small staged `construct_object` host-table addition addresses one API gap only.
- Prepared hash-pinned patches for the bridge and `construct_object` overlay. Each patch applied cleanly to a scratch copy of its pinned base. The `construct_object` preparation script refuses a divergent base; the bridge manifest records the hashes that must be checked before applying. Neither patch was applied to the shared candidate.

## Fresh local checks

- #1: probe flows **6/6**, classifier **13/13**; target syntax exit 0.
- #3: host protocol **22/22**, liveness **4/4**; target syntax exit 0.
- DLL runner **19/19**; direct bridge tests for call5/6 and guest-to-host arities 5–8 pass; synthetic fixture **11/11** plus bridge **3/3** checks; two framework-tool refusal cases **5/5** each; two real gameplay DLL expected-refusal cases **5/5** each. A refusal PASS means the safety boundary worked, not that gameplay loaded.
- `construct_object`: host checks **26/26**, host-core checks **122/122**; target syntax exit 0 with nine existing header warnings and no errors.
- Independent read-only review found two runner fail-closed issues; both were reproduced with failing tests, fixed, and re-reviewed. The reviewer accepted the result for **guarded Mac-only integration**, not deployment.

## Integration artifacts

- [Bridge patch](win64-bridge-integration-20261013.patch) and [its source-hash manifest](win64-bridge-integration-manifest-20261013.json).
- [`construct_object` patch](construct-object-integration-20261013.patch) and [its source-hash manifest](construct-object-integration-manifest-20261013.json).

The stage also holds the exact test manifests, output logs, PE census, sources, and unchanged DLL inputs. Patches must be checked against their pinned bases before any integration; they are not a console build.

## Remaining gates

**Still possible offline:** implement and verify an honest static-TLS strategy or retain refusal; close required MSVC CRT and UE4SS ABI gaps; test a real unchanged DLL for the intended game and version; complete generic target correspondence and code-detour machinery; review and integrate the staged sources; then build a target candidate.

**Needs console later:** verify the #1 v2 return convention and read scope; measure #3 executable patch, hook, and full restoration behavior on PS5; prove that an unchanged DLL loads, resolves the correct target, and produces its intended in-game effect. The project handoff requires separate user authorization for console actions and Phase B kernel work.
