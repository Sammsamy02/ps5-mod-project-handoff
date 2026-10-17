# Universal unchanged-DLL adaptation: review draft

**Status:** Architectural draft, Mac-only. No product code or console behavior is claimed by this document. It refines the existing `C5-RETARGETING-PROPOSAL.md` and the capability #13 assignment. The user clarified that the desired end state is **every DLL in every game**. The design below pursues maximum reuse, but cannot establish that absolute guarantee; it retains precise refusals instead of calling unsupported behavior compatible.

## Outcome and limits

An original Windows x64 gameplay DLL remains byte-identical. The same PS5 runtime discovers its format and dependencies, supplies supported Windows/framework services, finds corresponding PS5 targets, and performs the intended behavior with measurable restoration wherever compatibility can be verified. Townfall is one acceptance case; a different game must use the same runtime code with no game-name branches, hand-entered offsets, or per-mod binary edits. No finite sample demonstrates “every DLL in every game.”

The runtime may need build-specific *data* generated from authentic PC and PS5 images and engine metadata. These facts are keyed by image identity, signed or hash-pinned with provenance, and never confused with a portable function address. A PDB is an optional input when a particular DLL depends on its PC symbols; it does not itself identify the corresponding PS5 function. If evidence is absent, stale, conflicting, or ambiguous, admission fails before mutation. Arbitrary DLLs with fixed PC addresses, incompatible direct instructions, game-specific side effects, or unavailable Windows behavior defeat any honest blanket guarantee until those exact behaviors are supported and verified. The same is true when the required PC and PS5 functions have no established semantic correspondence. This is a technical limit, not a reason to add per-game special cases.

## Approaches considered

1. **Name/pattern substitution:** simple to implement, but identical-looking names and byte patterns do not prove cross-build semantics or calling compatibility. Reject as an admission rule.
2. **Typed engine mediation with verified build profiles (selected):** generic DLL/ABI services, engine-level adapters, and a three-gate target proof. It supports incremental, testable coverage and exact refusal reasons.
3. **General x64 binary translation/emulation:** could theoretically broaden direct instruction compatibility, but is a separate major runtime with performance, threading, exception, and platform risks. Keep outside the first implementation plan.

## Components and contracts

| Component | Input → output | Boundary |
|---|---|---|
| Read-only DLL intake | Original bytes → hash, architecture, PE features, import/dependency graph, entry/export set, TLS metadata, direct GS candidates, debug identity | No DLL execution or mutation. Isolated patches add TLS detail, the image's own CodeView GUID/age, ordinal-only forwarding, and a source-pinned current-loader blocker dossier. A later patch scans executable bytes for GS memory operands; a clean linear scan does not prove absence. These patches are not integrated into the shared candidate. |
| Generic Windows and framework runtime | Admitted imports and guest calls → typed service results | PE mapper, Win64/SysV bridge, Windows CRT/WinAPI, UE4SS ABI and lifecycle are reusable libraries. Unsupported imports fail with their exact names. Static TLS, exceptions, delayed imports, C++ layout and dynamic `LoadLibrary` need their own proven support or explicit refusal. |
| Engine adapter | Typed semantic request → engine operation and native signature | Contains engine-family differences, not a game's name or offset. Existing reflection/service hooks are the starting point; a native-function hook is admitted only when its timing and effects can be preserved. |
| Build-profile generator | Authenticated PC/PS5 image identities and independent evidence → versioned profile facts | Image-specific data only. The current generator emits pinned layout facts; it is **not** yet a general PC-to-PS5 function matcher. Wrong image or ambiguous fact refuses. |
| Target resolver | DLL request + adapter + profile → a unique typed PS5 binding or refusal | Three separate gates: named target, cross-build semantic correspondence, then call/instruction compatibility. No address is usable until all three pass. |
| Hook/patch transaction | Admitted binding + requested operation → installed hook and full restoration receipt | Requires #3 executable-memory permissions, safe instruction relocation/branching where applicable, quiescence, cache handling, state capture, and dirty-state refusal. It does not infer #3 from a successful DLL load. |

### Automatic admission sequence

1. Hash and classify the unchanged DLL; inspect direct and delay dependencies and feature requirements. Produce a machine-readable dependency report.
2. Check each required PE, Windows, CRT and framework feature against a versioned capability registry. An unknown dynamic dependency is checked when requested, before execution of dependent code.
3. Authenticate the intended game/build using available owned inputs. When the mod requests a PC PDB, match the PDB's build identity to the PC executable; obtain it from the licensed game installation if present. No runtime string or hardcoded Townfall filename is allowed.
4. Resolve each requested target through the three gates. Engine reflection may supply semantic names; native targets need independent correspondence evidence. Verify signature, layout, ownership and callback lifetime before a call or hook.
5. Start the DLL only after its startup requirements are admitted. Record entry/CRT, import calls, target bindings, intended event and restoration separately. A refusal makes no target mutation and names the failed gate.

## Verification ladder

**Mac-only now:** deterministic PE/metadata fixtures, malformed-image tests, unchanged hash checks, import/ABI host tests, absent/ambiguous/wrong-build profile tests, synthetic hook rollback, and at least two previously unseen DLL families. A load or resolved import is never scored as gameplay behavior.

**Console later:** verify anonymous executable mapping and permission behavior; #1 raw return contract; #3 patch, cache visibility and complete restoration; then an unchanged gameplay DLL for its intended game. A second supported game must run through the exact same runtime source with a newly generated profile. Each console action follows the existing authorization and kernel-work gates.

## Proposed implementation slices

1. **Admission dossier:** integrate the isolated read-only analyzer and capability-registry patch after review. The staged dossier currently maps real TLS, callback tables, delay imports, CLR directories and permission partitions to known current-loader refusals, while reserving loader preflight and runtime behavior as separate gates. This produces useful results without starting a DLL. The unchanged TrueFOV `winmm.dll` is a regression case for the earlier proxy-hint false negative; ordinary UE4SS exports are a non-proxy control.
2. **Runtime ABI coverage:** implement and test the supported Windows/CRT/UE4SS surfaces and bridge signatures driven by measured imports. Real static TLS is an immediate feasibility gate: all five unchanged upstream DLLs in the current mixed-game census carry it. Empty or absent TLS metadata also cannot establish absence of Windows TEB access: two unchanged binaries have decoded direct `GS` references despite those states. A genuine, generic userspace TEB/static-TLS route must be proved before any broad executable-compatibility claim; otherwise those DLLs remain refused. The staged loader now has a conservative `0x65` byte refusal, which can also refuse non-GS code.
3. **Generated target proof:** versioned profile schema and automatic three-gate resolver; the generator may emit only facts backed by image identity and independent evidence. No game-specific runtime branches.
4. **Transactional hooks:** generic #3 detour/restore machinery with instruction and concurrency validation, then selected engine-adapter operations.
5. **Cross-game acceptance:** unchanged intended-game DLLs, exact hashes, same runtime code, generated profiles, observed behavior and restoration on two supported games.

## Evidence behind this draft

- Current reviewer handoff: `CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md` in the same outputs folder.
- Existing target design: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-resume-20261013-01/C5-RETARGETING-PROPOSAL.md`.
- Existing loader and tests: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-console-wait-20261013-04/loader/`.
- Existing classifier: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap13/TOOL/pe_analyzer.py`.
- Existing second-game profile dry run: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/FZ-GAME2-PROFILE-DRYRUN.md`.
- Later direct GS evidence and staged guard: `DLL-GS-ACCESS-AUDIT-20261013.md` in this outputs folder.
