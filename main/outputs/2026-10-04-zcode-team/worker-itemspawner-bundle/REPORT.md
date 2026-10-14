# REPORT — worker-itemspawner-bundle

2026-10-04. All work under `outputs/2026-10-04-zcode-team/worker-itemspawner-bundle/`
only; nothing outside written except nothing (CLAIM first, REPORT last). No
network, no console, no nested subagents, and the mod's Lua was NEVER executed —
only its bytes were read, hash-pinned, copied, and memory-loaded by the host test.

## Built: the actual activation bundle (`bundle/`, install-ready modulo layout pair)

| Key | Role | Size | sha256 | Source |
|---|---|---|---|---|
| `ue4ss/UE4SS-settings.ini` | config | 221 | `f6d06cad…37f` | generated, FIRST-RUN-CONFIG minimal options; byte-identical to worker-activation-bundle's validated build |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | lua_mod | 21415 | `f586043a…658` | intake member-00000000.bin, UNCHANGED |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | lua_mod | 0 | `e3b0c442…855` | intake member-00000001.bin, UNCHANGED |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | lua_mod | 9962 | `5a8f6609…15a` | retained distro asset, UNCHANGED |
| `ue4ss/MemberVariableLayout.ini` | config | — | — | **awaiting staged pair, no bytes** (see below) |
| `ue4ss/VTableLayout.ini` | config | — | — | **awaiting staged pair, no bytes** (see below) |

Plus `bundle/loadlist.txt` (written last): header, `title PPSA33286`,
`engine unreal`, `framework ue4ss-ps5`, `mod TownfallItemSpawner 1.0.0`, the
four `file` rows above, zero `order` rows. All four file hashes re-verified
against their pins at build time and post-build; settings bytes independently
match the prior worker's validated 221 B artifact (cross-worker byte identity).

## Host-tested through the vendored activation path: 32/32 PASS, 0 FAIL

`test_activation.c` + `run_host_test.sh` compile worker-7's vendored
`modforge_activation.c` + `modforge_loadlist.h` (unchanged) and load THIS bundle
through `mfa_activate` exactly as `PrepareActivatedUE4SSRuntime` would. Log:
`host-test/host-test.log`. Covered: exact loadlist record set; activation OK
with title/engine/framework/mod facts; all four files preloaded (31602 blob
bytes = 31598 payload + 4 NUL terminators — the one check I initially wrote
wrong was MY expectation, fixed against modforge_activation.c:383);
byte-identity of every file against pinned sources; NativeModSource enumeration
oracle (one Lua mod, `shared/` never a mod, enabled.txt → startup list
`[TownfallItemSpawner]`, no mods.txt); both layout keys honestly ABSENT while
the activation is still valid (Lua-only, root not required); negative roots:
wrong title → `MFA_E_TITLE`, truncated file → `MFA_E_SIZE`, missing loadlist →
`MFA_E_NO_LOADLIST`.

## Layout keys: awaiting staged pair, honestly empty

`AWAITING-STAGED-PAIR.md` + `bundle.json.layout_keys_awaiting_staged_pair`:
no synthetic bytes, no placeholder bytes in the actual bundle. Documents the
runtime hard gate (UE4SSProgram.cpp:899-904 throws before any Lua), the
validation identity the emitted pair must carry, and the exact completion steps
(copy two files, append two measured-size loadlist rows, update manifest,
invert T5, re-run host test) sourced from worker-staged-pair-emission's
EMISSION-RUNBOOK. Never ship the synthetic-preparation bytes.

## Documents

- `INSTALL-PACKAGE.md`: final console tree `/data/modbridge/PPSA33286/`
  (FORMATS.md:219), permissions/invariants (loadlist last = commit truth,
  MFA_E_SIZE all-or-nothing), the retained installer path (worker-7 `modinst`
  register-mod/install/records/recover), how the runtime discovers the bundle
  (module_start single read → mfa_activate → MakeActivatedUE4SSModSource →
  UE4SSProgram), post-install verification via `runtime-status.txt`.
- `FIRST-RUN-CHECKLIST.md`: pre-flight gates; phase-1 byte-exact evidence
  `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the
  menu.` exactly once in the active retained Lua log (path-agnostic; current
  lineage `/data/townfall_runtime_aligned_entry.log`) with the full absence
  list and load-failure discrimination; phase-2 `[TownfallItemSpawner] Scan
  failed: Item database not ready. Load a save first.` with the
  `FindFirstOf`-nil discriminator tail, expected silence, PlayerController
  sub-case, and the honest ExecuteInGameThread/key-delivery ambiguity warning.

## Honest limits

- The two layout keys block any actual run until worker-staged-pair-emission
  emits the real pair; nothing here claims a mod run, engine execution, or any
  console-side result. Phase-1/2 evidence is expected-evidence documentation
  only, none of it observed yet.
- `host-test/root-*` are mutated scratch copies kept only as reproducible
  negative fixtures; `host-test/test_activation` is the compiled harness.
- Only loader-path facts were newly exercised; every UE4SS-side consumer
  citation (SettingsManager, NativeModSource, UE4SSProgram, LuaMod line
  numbers) is carried over from BUNDLE-SPEC/FIRST-RUN-CONFIG traces, not
  re-traced by this worker.
