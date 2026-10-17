# REPORT — worker-post-promotion-runtime

Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what` (session CWD
unrelated; all work absolute). Date 2026-10-04. No console contact, no network, no
nested subagents; existing files elsewhere read-only. ~40 tool calls, under the 80 bound.

## 0. Deliverables

| file | content |
|---|---|
| `RUNTIME-STAGING-PLAN.md` | §1 full module-discovery trace (module_start -> supplier/LMF -> NativeProgramContext -> mfa_activate -> MakeActivatedUE4SSModSource -> UE4SSProgram ctor/setup_paths/init), §2 exact console layout, §3 ploader.ini entry, §4 every file (local path, destination, bytes, sha256 recomputed on disk) + required loadlist rows, §5 launch sequence, §6 expected evidence per stage, §7 critical layout-pair prerequisite |
| `FAILURE-TRIAGE.md` | T1 load failure, T2 early module death, T3 settings parse, T4 layout pair (missing/edited/axis-refusal), T5 PRX-side environment (UE4SS.log writability, input/pad, ack-wiring state), T6 mfa activation error table (MFA_E_* codes), T7 mod-level, T8 honesty gates |
| `CLAIM.json` | written first |

## 1. Task 1 — module discovery trace (measured, with anchors)

- `module_start` (`outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c:48`):
  entry_guard, diagnostic channel, `probe_lifecycle_start` (constructors + worker).
- Supplier facts: `sceKernelGetModuleInfo` is composition-3's ONLY added SDK import
  (worker-composition-3 REPORT §0); `module_file_path` is caller-BOUND into
  `TryMakeNativeLoaderFactsFromSupplier` (lmf_to_native_loader_facts.cpp:305) and
  `MakeNativeProgramContext` fills/validates `NativeProgramContext`
  (NativeProgramContext.hpp:85-101: absolute, normalized, existing regular files;
  module_source mandatory).
- Activation: `mfa_activate` runs ONCE during module_start (only moment /data is
  readable; vendored modforge_activation.h:4-6); preloads roles lua_mod/config/script,
  size-checks every file against the loadlist, all-or-nothing.
- `MakeActivatedUE4SSModSource` (activated_mod_source.cpp:20-84): title match, engine
  `unreal`, framework `ue4ss-ps5`, forwards exactly `ue4ss/*` files with role
  lua_mod/config/script as immutable logical keys.
- `UE4SSProgram::setup_paths` (UE4SSProgram.cpp:749-785, __PROSPERO__ branch):
  `m_working_directory = module_file_path.parent_path()`; with a mod source it returns
  EARLY — settings/config come exclusively from logical `ue4ss/` keys; the filesystem
  settings path is diagnostic-only; `UE4SS.log` opens at
  `<working directory>/UE4SS.log` (:255-257) => the PRX directory must be writable.
- Layout gate: `load_unreal_offsets_from_file` (:894-908) reads BOTH keys
  `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini` from the mod source inside
  setup_unreal (before any Lua) and throws
  "Native activation requires both member and vtable layout files" if either is absent;
  publish later refuses staged-vs-resolved axis disagreement.
- PRX directory needs: the PRX itself, writability (UE4SS.log), and its ploader.ini
  entry — nothing else; all `ue4ss/` bytes come from the activation root.

## 2. Task 2 — console layout (in RUNTIME-STAGING-PLAN §2)

PRX at `/data/plugins/modbridge_supplier_composition.prx` => working dir
`/data/plugins`; activation root `/data/modbridge/roots/PPSA33286` (installer-managed,
`--roots /data/modbridge/roots`); ack status at `<mfa_config::root>/runtime-status.txt`;
module diagnostics at `/data/townfall_supplier_composition.log` (package_prx.py:666).

## 3. Tasks 3-5 — plan + triage

Written as §0 describes. Expected evidence distinguishes per-stage artifacts and gives
exact log lines (`working directory: /data/plugins`, `mods directories: [0] ue4ss/Mods`,
`[TownfallItemSpawner] ...` Lua prints) and the ack state order
`activated -> engine-ready -> mods-started` / `failed` / `stopped`.

## 4. Key findings beyond the ask (measured, load-bearing)

1. **The itemspawner bundle loadlist is NOT stageable as-is**: its settings row is the
   OLD 221-B ini (sha `f6d06cad…`) which hard-fails `validate_native_services`
   (worker-ue4ss-settings). The plan requires regenerating the bundle with the
   corrected 779-B ini (sha `9f3fa127…`, host-verified) and adds the three missing
   rows: layout pair (588/186 B) and `ue4ss/PS5Pad.ini` (2960 B) — all role `config`,
   i.e. exactly mfa's default preload roles and exactly what
   `MakeActivatedUE4SSModSource` forwards. Sizes are mfa's commit truth (MFA_E_SIZE
   otherwise).
2. **Ack-wiring honesty**: composition-3's linked object defines
   `PrepareActivatedUE4SSRuntime` but NOT `MakeRuntimeAckConfig`/
   `StartActivatedRuntimeWithAck` (0 grep hits in
   worker-query-s2-arm/probe/target-probe-composition-3/nm-composed-defined.stdout);
   the module_start-side ack supplier is a composition-4 extra unit by its own header.
   Consequence recorded in the plan and triage: with composition-3 as linked, expect
   the module diagnostic + facts evidence; runtime-status.txt/UE4SS.log/mod prints
   require the composition-4 recomposition (worker-integration-checklist STEP 6).
   Healthy module log + absent ack file is a wiring state, not a load failure.
3. **PS5Pad.ini consumption point confirmed**: read from logical key `ue4ss/PS5Pad.ini`
   via `ConfigureRetainedPS5PadInput` (NativePadInput.cpp:13) during backend
   construction — it must ride the activation, not the PRX dir.
4. **PRX candidate identity verified on disk**: sha256
   `b8bfae77c8d147b54864a6fc5ac1f24355ba3fb13c79b0cd92e11bc7e36aab35`, 30756791 B,
   build-receipt gaps `[]` — matches the task's stated composition-3 candidate.
5. **emit-final-pair was in flight**: pair hashes frozen here
   (`f291d552…` / `9c20f7c9…`, parsepair OK, negative controls passed, code-anchor
   confirmation pending-and-non-blocking per its receipt); the plan mandates re-verify
   at execution time and notes the pair's own "stageable once the pending code-anchor
   byte confirmation lands" condition.

## 5. Limits (honest)

- Static analysis + host-verified inputs only; nothing was staged, sent, or executed on
  a console; no ploader.ini was read or modified (no network). The console IP/ports and
  installer verbs are quoted from prior workers' runbooks, not re-verified.
- No claim that the composed runtime runs, that mods load, or that any quality bar is
  met; prototype status unchanged. The exact in-game entry that passes
  `module_file_path`/activates inside the launched PRX is controller-owned wiring
  (INTEGRATION-MAP SEAM S1 + STEP 6), documented as prerequisite, not assumed done.
- mfa_config::root example strings differ between worker docs (`/data/modbridge/PPSA33286`
  in runtime_ack_supplier.hpp comment vs installed `/data/modbridge/roots/PPSA33286`);
  the plan pins the rule (status file follows the EXACT root string passed) instead of
  guessing.
