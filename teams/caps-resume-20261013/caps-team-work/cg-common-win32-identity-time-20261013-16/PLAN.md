# Console-wait Mac work (2026-10-13)

Direct user goal: make #1, #3, and unchanged gameplay DLL support as ready as possible before console access. Do not touch Claude's #2/#4–#12 work, the shared candidate, kernel source, mod binaries, or console. Own only this stage.

1. [x] Pin existing sources and identify exact offline gaps for a real unchanged mod and PC/PS5 target matching.
2. [x] Harden the PE loader with red-green host tests: PE header/section/import/export bounds, relocation policy, and precise empty-versus-real TLS handling.
3. [x] Rerun fixture execution and unchanged upstream DLL refusal with the revised loader and fixed bridge; preserve input hashes.
4. [x] Check #1 return contract and #3 hook/patch route for remaining implementable Mac work. Existing C1/C3/C5 analyses identify the unproven return path and generic detour/target-correspondence gaps; no console-dependent result was invented.
5. [x] Independent read-only review and fresh verification; produce a truthful console-wait matrix and guarded integration artifact without editing the candidate.

Known external input gap: local `inputs/pc-townfall` has only README; no matching PC executable/PDB/build manifest. An unchanged author-built Townfall TrueFOV gameplay DLL is now pinned in `third_party/` and has real static TLS; it is not a UE4SS C++ mod. Two unchanged third-party UE4SS gameplay DLLs in the prior stage are from other games and also have real static TLS.
