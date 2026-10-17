# REPORT.md — worker-settings-v3-package, 2026-10-04 (written last)

Owner of the settings package going forward (v2 delivered by
worker-engineversion-ini and completed). Host-only work on this Mac; no
console contact, no engine writes, no nested subagents.

## Delivered

- `CLAIM.json` (written first) — key identification, Lua-path survival,
  both-outcomes safety, oracle claims, explicit non-claims, rollback.
- `package-v3/` — the shipped pair:
  - `ue4ss/UE4SS-settings.ini` 1448 B
    `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038`
  - `loadlist.txt` 447 B
    `4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d`
  - 5 payload files byte-identical to package-v2:
    MemberVariableLayout.ini `f291d552…`, VTableLayout.ini `9c20f7c9…`,
    PS5Pad.ini `1d919fbf…`, main.lua `f586043a…`, enabled.txt `e3b0c442…`
    (empty), UEHelpers.lua `5a8f6609…`.
- `SETTINGS-V3-DECISION.md` — the decision note (keys, values, both-outcomes
  analysis, honest scope).
- `host-test/` — oracle sources, binaries, logs (hashes below).

## Answer to the A-PI question (see decision note for full chain)

- Keys: `[Hooks] HookProcessInternal=0` **and** `[Hooks]
  HookProcessLocalScriptFunction=0` (SettingsManager.cpp:279-280;
  UE4SSProgram.cpp:1075-1076; gate `SINI:1497` is an OR and the 164-byte
  sub-derivation at `SINI:1516` never re-checks its own flag, so BOTH keys are
  required; `ScanOverrides` is programmatic-only and NOT ini-reachable).
- Lua path: unaffected. ProcessEvent resolves from the vtable in a separate
  PostInitialize step (`SINI:1448-1456`) under its own key
  `HookUObjectProcessEvent` (default true); ExecuteInGameThread drains via
  EngineTick with the ProcessEvent cross-fallback; packaged Lua uses zero
  RegisterHook; the loss (`can_create_custom_events=false`) is warning-only.
- Decision: **one package with the keys** — safe under BOTH writability
  outcomes (prevents a probable unconditional init fault at `SINI:1506` when
  text is unreadable; behavior-neutral + removes unused detour-write risk when
  readable). `worker-exec-writability/` is empty — no verdict needed for this
  call. No v3-safe-now/v3-conditional split shipped because the code does not
  support a hazard that would gate inclusion.

## Oracle receipts (same oracles as v2, extended)

### O1 — retained-IniParser parse + deserialize replay + gates: PASS

Source `host-test/settings_parse_test_v3.cpp`
(`94d8f1fc3ab43ed45c61c01f374aeee8ecac4108251d6b8b3b110978f2c4ef3c`) =
worker-engineversion-ini's `settings_parse_test_ev.cpp` (baseline sha
`3466fde0…` verified before patching) PLUS four assertions:
`Hooks.HookProcessInternal==0`, `Hooks.HookProcessLocalScriptFunction==0`
(default-true get_bool, guarding the silent-default trap),
`HookUObjectProcessEvent`/`HookEngineTick` still default true (the Lua path
must not be disabled). Binary `settings_parse_test_v3`
(`a8c051b6fa7614d15cd80117dbb6d89e43997a13c3adc2d76eea872e3e7233c8`), built
from the same vendored parser sources as v2
(`ue4ss-native-settings-port-1/patched/IniParser` + `ParserBase`; 0 errors;
only upstream deprecated-codecvt warnings in `Helpers/String.hpp`).
Differences from v2's link, recorded honestly: POSIX File backend
(`RC_FILE_USE_POSIX_BACKEND` + `RC_FILE_BUILD_STATIC` + `*_BUILD_STATIC`
defines + `FMT_HEADER_ONLY`), so the never-executed link stub is
`posixfile_stub.cpp` (`ab9bf5f2…`, `PosixFile::read_all`) instead of v2's
`linuxfile_stub.cpp` (`LinuxFile::read_all`) — same role, never executed; the
oracle parses from the StringType& overload.

```
$ settings_parse_test_v3 package-v3/ue4ss/UE4SS-settings.ini \
      worker-ue4ss-settings/host-test/probe-inline-comment.ini \
      worker-ue4ss-settings/host-test/probe-emptysection.ini
package-v3/ue4ss/UE4SS-settings.ini -> PASS (all gates + expected values OK)
probe-inline-comment.ini -> FAIL (... + EXPECT-FAIL[Hooks.HookProcessInternal==0];
                                  EXPECT-FAIL[Hooks.HookProcessLocalScriptFunction==0])
probe-emptysection.ini   -> FAIL (PARSE-THROW: Syntax error(3 : 1) ...)
exit=0   (argv[1] passes; probes prove the oracle still discriminates)
```

Lockstep discrimination for the NEW checks: the v2 bytes (no `[Hooks]`) run
against the v3 oracle -> `FAIL (EXPECT-FAIL[Hooks.HookProcessInternal==0];
EXPECT-FAIL[Hooks.HookProcessLocalScriptFunction==0])`, exit=1 — the checks
detect absence, and the two keys are proven to be what flips the verdict.

### O2 — mfa activation over the package: 46 PASS / 0 FAIL

Source `host-test/test_activation_v3.c`
(`4f206d0b4b8dbe5a916ce2a51b088c7116fb1bc0bd07f6a46da840c004c0f9c1`) =
worker-engineversion-ini's `test_activation_ev.c` (copied byte-exact) with
ONLY the settings-size expectation 834 -> 1448 and the payload total
35945 -> 36559 (= 1448+588+186+2960+21415+0+9962, +7 NUL terminators = 36566).
Binary `test_activation_v3`
(`af9439623e8388f47c67bead2aa9269aaebd3520cf9a470899133f6463e09b83`), linked
against the same vendored `modforge_activation.c` + `modforge_loadlist.h`
(`worker-f-r2-fix/composition/vendored`). Full log:
`host-test/v3-activation-output.txt`.

```
PASS T1 7 file rows exact role/path/size incl. settings row size==1448
PASS T2 mfa_activate(package-v3) OK; 7 files preloaded;
       preloaded bytes == 36566 (36559 payload + 7 NUL terminators)
PASS T3 ue4ss/UE4SS-settings.ini present, size==1448, NUL-terminated at [size]
PASS T3/T4/T5 layout pair 588/186; startup list [TownfallItemSpawner]
PASS T6 wrong title -> MFA_E_TITLE (rc=-4); truncated -> MFA_E_SIZE (rc=-8);
       missing loadlist -> MFA_E_NO_LOADLIST (rc=-2)
test_activation_final: 0 failure(s)  — 46 PASS / 0 FAIL
```

### O3 — loadlist size/sha discipline: PASS

- Every loadlist row size re-checked against the actual staged file (7/7,
  including settings 1448).
- Pair lockstep negatives (reusing worker-engineversion-ini's unmodified
  `mismatch_probe` binary): v3 loadlist + v2 settings bytes ->
  `rc=-8 "file is shorter than the load list says"`; v2 loadlist + v3 settings
  bytes -> `rc=-8 "file is longer than the load list says"`. The pair moves
  together or the mfa refuses, both directions.
- v3 settings bytes verified a strict prefix-extension of v2 (`cmp -n 834`:
  no differing byte before v2 EOF), no CR, no NUL, ASCII+LF only, full-line
  comments only (the retained parser wedges on comment-only sections and the
  v2 header documents the inline-comment trap; neither pattern is used).

## Handoff notes

- v3 supersedes v2 as the staged pair; v2 remains the rollback (bytes + oracle
  receipts intact in worker-engineversion-ini).
- If the exec-writability verdict later lands "readable AND writable", the
  keys can stay: they only disable unused PI/PLSF detours. If a future mod
  needs `RegisterHook` on BP script functions or custom BP events, that is the
  one feature that requires re-enabling the derivation (and then the
  offline-constants remedy from FLAG-3 becomes the right fix instead).
- EngineTickResolveMethod (default Scan) remains a separate open scan
  dependency owned by the tick workers; untouched here on purpose.
