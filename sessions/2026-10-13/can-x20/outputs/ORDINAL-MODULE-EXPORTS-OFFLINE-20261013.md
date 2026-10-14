# Stage 27: guarded mapped-module ordinal exports

**Status:** isolated Mac host work; not a console-only wait and not proof of universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordinal-module-exports-20261013-27`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-mapped-module-registry-20261013-26`

The PE loader now rejects ordinal import thunks with reserved bits set and extracts the 16-bit ordinal. Mapped-module lookup accepts names or ordinals after process attachment. Ordinals are translated using `ExportDirectory.Base`, then checked against the export address table. Zero entries, forwarder RVAs, non-executable targets, absent entries, and values outside the 16-bit import range refuse. The named and ordinal paths share executable-target validation. This follows the [Microsoft PE format definition](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format). Forwarder traversal is still unsupported.

The focused host suite passed **32/32** checks. It includes a copied synthetic DLL that imports `fixture.dll!#1` and calls the attached provider through its resolved IAT, plus failures for an unattached provider, malformed thunk, holes, forwarders, non-executable targets, bounds, and a non-default export base. The original fixture SHA-256 remained `acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce`. No original DLL or shared candidate was edited.

All 13 existing offline regression runners exited 0, including five unchanged DLL read-only TLS checks. The original synthetic end-to-end fixture passed **11/11**. The seven-file incremental patch passed `git apply --check`, application to a scratch copy of the exact stage-26 base, and byte-for-byte replay. The evidence ZIP passed CRC verification.

- [Incremental patch](ordinal-module-exports-increment-20261013.patch)
- [Base/stage hash manifest](ordinal-module-exports-manifest-20261013.json)
- [Full regression and replay evidence](ordinal-module-exports-evidence-20261013.zip)

Next offline work: bounded forwarder resolution and dependency ordering, dynamic module APIs, thread attach/detach handling for new guest threads, broader reviewed CRT/UE4SS imports, bridge signatures, generic #1/#3 gaps, and authenticated build correspondence. Console segment, memory protection/cache, and gameplay evidence remain unavailable.
