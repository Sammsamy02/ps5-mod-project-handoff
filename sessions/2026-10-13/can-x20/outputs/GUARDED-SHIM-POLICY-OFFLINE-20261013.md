# Guarded PE and reviewed shim policy: offline continuation

## Result

Isolated stage 25 connects the guarded PE module to `WinApiShim` through a dedicated runtime preparation route. It resolves only shim-published imports and registers the mapped HMODULE's thread-notification policy before DllMain. A failed segment entry removes the provisional policy so attachment can be retried; a clean process detach removes it as well. The generic structural mapping test route remains separate and still uses a placeholder resolver only in tests.

Source stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-shim-policy-20261013-25`. Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guarded-pe-activation-20261013-24`. The shared candidate, Claude's #2/#4–12 work, kernel code, and original DLLs were not edited.

## Evidence

The focused x64 Mac host test passed **16/16** with the original synthetic PE fixture and an in-memory variant carrying relocated static TLS. Its DllMain called the reviewed `DisableThreadLibraryCalls` import through the real shim: ordinary thread notifications were suppressed, while the static-TLS variant kept them. Backend refusal, retry, detach, and reattach exercised policy cleanup. The source fixture's pinned SHA-256 stayed unchanged.

Using the same reviewed shim, unchanged TrueFOV refused with a missing-import list, and ZeroCompanyMandoWardrobe refused with a full long decorated import name in that list. Neither gameplay DLL executed. The test confirms the real route does not substitute the structural test resolver's placeholder addresses for unsupported imports.

Fresh stage 24 and stage 25 focused tests, all existing loader/TLS/bridge/import regressions, five-DLL read-only TLS preparation, and the 11/11 original synthetic end-to-end fixture passed. The eight-file incremental patch passed `git apply --check`, exact-base scratch application, byte-for-byte replay, and ZIP CRC verification.

## Reviewer handoff

- [Incremental patch](guarded-shim-policy-increment-20261013.patch)
- [Exact-base hash manifest](guarded-shim-policy-manifest-20261013.json)
- [Full evidence ZIP](guarded-shim-policy-evidence-20261013.zip)

Apply the stage 15–23 rollup to its stage 14 base, the stage 24 increment to the stage 23 result, then this stage 25 increment to the stage 24 result. Alternatively, apply all individual increments in order. The two routes must not be mixed.

## Remaining work

The production segment backend still refuses entry. Broad Windows/MSVC CRT and UE4SS imports, PS5 executable-memory/cache behavior, automatic new-thread lifecycle, dynamic and delayed dependencies, mixed floating-point ABI, guest pointer validation, PC-to-PS5 target correspondence, and intended gameplay proof remain. #1 and #3 retain their separate gaps. This is **not** a console-only wait and does not establish “every DLL in every game.”
