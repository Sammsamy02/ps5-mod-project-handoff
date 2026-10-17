# Stage 32: automatic static PE dependency closure

**Status:** isolated Mac host work. This is not a console-only wait and does not establish universal DLL compatibility.

Stage: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-dependency-closure-20261013-32`

Exact base: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ordered-dll-batch-20261013-31`

The new read-only PE32+ scanner walks bounded import descriptors and returns their DLL names. `pe_attach_guest_closure` starts from one DLL, asks a generic caller-supplied source for locally available imported DLLs, follows transitive static imports, then passes the collected images to stage 31's dependency-ordered guarded batch. It skips already registered or built-in shim module names. It checks the fetched file basename against the requested import, limits the closure to 256 DLLs and 512 MiB, and refuses malformed import metadata or unsupported delayed imports before guest entry. Unavailable imports remain exact named refusals during resolution. No game names, addresses, or per-mod rewrite rules are embedded in the route.

The focused Mac suite passed **22/22** checks. It demonstrated discovery of one provider and a two-step transitive chain, clean detach, named missing-import refusal, substituted-basename refusal, cycle refusal before guest entry, and malformed import-RVA refusal before guest entry. The scanner read five unchanged upstream DLLs; their SHA-256 values and the original synthetic fixture hash were verified before and after testing. This was structural inspection only: no real gameplay DLL executed.

All **14** offline regression runners exited 0. The original synthetic end-to-end fixture passed **11/11**. The eight-file incremental patch passed `git apply --check`, application to a scratch copy of the exact stage-31 base, and byte-for-byte replay. The evidence ZIP passed CRC verification.

- [Incremental patch](dependency-closure-increment-20261013.patch)
- [Base/stage hash manifest](dependency-closure-manifest-20261013.json)
- [Regression and replay evidence](dependency-closure-evidence-20261013.zip)

The caller still must implement a trusted source that locates dependent DLL files and supplies the guest segment backend. Dynamic `LoadLibrary` and `GetProcAddress`, Windows/MSVC CRT and UE4SS ABI coverage, cyclic PE initialization, guest-thread lifecycle, target matching, and intended gameplay behavior remain unresolved. The production segment backend still refuses guest entry. No PS5 compiler or console run was available in this pass. “Every DLL in every game” is not an achievable guarantee from a static dependency scanner: DLLs may request OS behavior or game symbols with no valid PS5 equivalent, which must be detected and refused.
