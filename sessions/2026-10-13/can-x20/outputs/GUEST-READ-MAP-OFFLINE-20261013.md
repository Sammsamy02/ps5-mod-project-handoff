# Stage 36: bounded reads from attached PE images

**Status:** isolated Mac host work; not a console-only wait or proof of gameplay.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-read-map-20261013-36`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-guest-crt-fallback-20261013-35`

Stage 34 made `GetModuleHandleA` and `GetProcAddress` guest-callable only when the caller supplied a validating reader. This stage adds an owned, bounded read map and connects validated mapped PE image ranges to that reader automatically during process attachment. The range is revoked after clean detach. The map rejects overlapping or wrapping registrations and reads that cross a range boundary. It copies only from registered ranges whose owner keeps their backing memory readable and alive.

The focused batch suite passed **40/40** checks. A copied synthetic DLL imported `KERNEL32.dll!GetModuleHandleA` and `GetProcAddress`; its DllMain passed module and symbol literals from its mapped image to the sealed Win64 wrappers and returned success only when the export lookup succeeded. The test checked that its image range was readable while attached and unavailable after detach, and that a mapped registry entry without a read range could still be removed. The original synthetic fixture and five upstream DLLs remained hash-pinned and unchanged. No real gameplay DLL or CRT was executed.

All **15** offline regression runners exited 0. The original synthetic end-to-end fixture passed **11/11**. The five-file patch passed `git apply --check`, exact-stage-35 scratch application, byte-for-byte replay, and ZIP CRC verification.

- [Incremental patch](guest-read-map-increment-20261013.patch)
- [Base/stage hash manifest](guest-read-map-manifest-20261013.json)
- [Regression and replay evidence](guest-read-map-evidence-20261013.zip)

The map currently registers PE image pages. It does not discover guest heap or stack allocations, verify arbitrary OS memory permissions, or provide the PS5 segment backend. Callers must quiesce guest users before removing a range. MSVC CRT/SEH behavior, UE4SS C++ ABI, broad imports, target correspondence, and PS5 execution remain open. Unsupported cases still refuse by name. No console, shared candidate, kernel file, or original mod binary was changed.
