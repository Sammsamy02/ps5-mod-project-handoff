# RECEIPT — worker-pad-hardware-probe (2026-10-04)

FROZEN DELIVERY (local build + host verification; NO console run — the
console launch is the user-coordinated test in STAGE-PLAN.md).

## Artifact

- **Build ID `padp-79947e016dfe`**
- **PRX `modbridge_pad_probe.prx`** (top level) — **20427 B**,
  sha256 `deccca38e58ac76ea38e1a80b780eb3235fd4dfab7f81a32f776621cac75803a`
- Inner ELF (bare-sdk, ftpsrv decryption-oracle target): **69920 B**,
  sha256 `96fbdfbcfc6c95f5c18f9275e9c85ced769c3ea3d3af3c2d7666680b7eaba4fa`
- Combined object `combined-pad.o`:
  `32d82043de7d76bb36b663c5ea4bc04b0cde485fa8e39193f2a22ec4a9f9dd19`
- Contract `import-contract.json`:
  `e9fe6035e3ac6f467cfa8ea1aac5a2899298ed674ce40c851752d84cde039d0c`
- Log path (module writes): `/data/pad-probe-events.jsonl`
- Gates at freeze: HOST-CHECKS-PASSED; LOCAL_PACKAGE_CHECKS_PASSED;
  frozen lineage hash verified; forbidden-symbol gate passed;
  determinism run1 == run2 byte-identical (fself, inner ELF, combined
  object, contract).

## Import identity proof (14 strong rows, 0 weak)

ALL 14 rows are byte-identical rows of the frozen 191-import
aligned-entry contract (`.superpowers/native-dependency-spike/
native-full-runtime-aligned-entry-1/import-contract.json`), re-verified
post-build by sorted-JSON string equality + the packaged-dynsym gate
(14 NID-encoded strong dynsym entries, zero weak, no forbidden friendly
names anywhere). The 7 pad rows carry Townfall-executable declaration
evidence (encoded NIDs, e.g. scePadReadState `YndgXqQVV7c#A#B`) and were
cross-checked field-by-field against
`.superpowers/native-dependency-spike/ue4ss-public-import-declarations-1/`
in the build script (mismatch = LOUD build failure).

| # | Identity | Module | Provenance |
|---|----------|--------|------------|
| 1 | `__error` | libc | verbatim frozen contract (v2/v3 console-proven set) |
| 2 | `sceKernelOpen` | libkernel | verbatim frozen contract |
| 3 | `sceKernelWrite` | libkernel | verbatim frozen contract |
| 4 | `sceKernelClose` | libkernel | verbatim frozen contract |
| 5 | `sysctl` | libkernel | verbatim frozen contract |
| 6 | `getpid` | libkernel | verbatim frozen contract |
| 7 | `clock_gettime` | libkernel | verbatim frozen contract |
| 8 | `scePadInit` | libScePad | verbatim frozen contract (Townfall-declared) |
| 9 | `scePadOpen` | libScePad | verbatim frozen contract (Townfall-declared) |
| 10 | `scePadReadState` | libScePad | verbatim frozen contract (Townfall-declared) |
| 11 | `scePadClose` | libScePad | verbatim frozen contract (Townfall-declared) |
| 12 | `sceUserServiceInitialize` | libSceUserService | verbatim frozen contract (Townfall-declared) |
| 13 | `sceUserServiceGetLoginUserIdList` | libSceUserService | verbatim frozen contract (Townfall-declared) |
| 14 | `sceUserServiceTerminate` | libSceUserService | verbatim frozen contract (Townfall-declared) |

The aligned-entry runtime built from this same frozen contract LOADED on
console; THIS module is a NEW plugin — its own load (the pad rows inside a
ploader plugin) is precisely what the staged launch tests.

## Host-test receipt (build_and_test_host.sh, ASan+UBSan)

`pad_probe_host: ALL STRUCTURAL CHECKS PASSED` +
`HOST JSONL CHECKS PASSED (a=7 b=2 c=2 d=5 e=8 f=2 records)` over six
scripted-backend scenarios:

- A golden path: 2 OPTIONS presses fire exactly 2 key_fired; same-timestamp
  duplicate suppressed; release/repress re-fires; teardown releases pad +
  owned user-service lease; probe_end end_reason=grace_elapsed.
- B user_initialize rc=5 -> probe_failed stage=activate
  detail=UserInitialize rc=5, no pad opened.
- C empty login list -> probe_failed detail=NoUser; owned lease terminated.
- D healthy fast-window run + direct constructor gate: zero buttons mask ->
  InvalidBindings phase.
- E unplug/replug: Disconnected phase recorded; no phantom edge on replug
  (baseline reset); exactly 1 key_fired on the post-replug press.
- F missing backend -> probe_failed stage=factory.

Every non-truncated JSONL line parses; marker `modbridge.padprobe`;
seq strictly increasing; the record vocabulary matches the prepared module
(probe_start / activate / register / key_fired / phase_change /
probe_failed / probe_end / teardown).

## Honesty boundary

- Nothing native was executed here. `in_game_results: false`.
- The retained C++ Handler.cpp/PS5PadSource.cpp are NOT linked (libc++ is
  unproven in the ploader PRX lineage); `pad_source.c` is a line-faithful C
  port of their exact backend sequence, validity gates, edge/timestamp
  semantics and phase names, host-tested as above.
- The probe records F7 key delivery through THIS module's registered
  binding; it does not exercise the composed runtime's Lua dispatch path.
