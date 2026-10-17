# Guarded mapped-module registry: offline continuation

## Result

Isolated stage 26 lets the reviewed shim discover named exports from an attached guarded PE module. It derives the registry key from the supplied DLL path's lowercase basename, reserves the name before DllMain, and withholds export addresses until process attachment succeeds. Duplicate mapped names, collisions with manual modules or reviewed built-in DLLs, and wrong-owner removal refuse without replacing a live entry. Failed attachment and clean detach remove the reservation. The loader still validates each published export as file-backed executable code; ordinal and forwarded exports remain unresolved.

Source stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-mapped-module-registry-20261013-26`. Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-shim-policy-20261013-25`. The shared candidate, Claude's #2/#4–12 work, kernel code, and original DLLs were not edited.

## Evidence

The focused x64 Mac host test passed **19/19**. An in-memory copy of the pinned synthetic PE fixture was changed to import `fixture.dll!start_mod` and call it through its IAT. It refused while the provider was unattached, then resolved and called the provider after guarded process attachment. Other checks covered provisional export hiding, basename normalization, duplicate and reserved-name refusal, wrong-handle removal, backend-refusal cleanup, and clean detach. The source fixture's SHA-256 remained unchanged.

Fresh regressions passed: the stage 25 reviewed-shim policy checks, the stage 24 five-DLL guarded mapping checks, strict loader preflight, static TLS, TEB lifecycle, context bridge, import registry, lazy and typed imports, synchronization/S-list, thread policy, five unchanged DLL read-only TLS preparation, and the original **11/11** synthetic end-to-end fixture. The seven-file patch passed `git apply --check`, exact-stage-25 scratch application, byte-for-byte replay, and ZIP CRC verification.

## Reviewer handoff

- [Incremental patch](mapped-module-registry-increment-20261013.patch)
- [Exact-base hash manifest](mapped-module-registry-manifest-20261013.json)
- [Full evidence ZIP](mapped-module-registry-evidence-20261013.zip)

Apply the stage 15–23 rollup to its stage 14 base, then stage 24, stage 25, and stage 26 increments in order. Alternatively, apply the individual increments in order; do not mix the routes.

## Remaining work

The production segment backend still refuses entry. This stage does not implement ordinal or forwarded exports, automatic dependency and thread lifecycle ordering, dynamic `GetProcAddress`/`LoadLibrary`, broad Windows/MSVC CRT/UE4SS APIs, mixed floating-point ABI, guest pointer validation, PS5 executable-memory/cache behavior, or PC-to-PS5 target correspondence. #1 and #3 retain their separate gaps. No unchanged gameplay DLL has run or produced its intended effect on PS5. This is **not** a console-only wait or proof of “every DLL in every game.”
