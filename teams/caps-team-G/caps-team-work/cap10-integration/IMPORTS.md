# IMPORTS — capability #10 vs R15e's 207 identities (Team G)

All counts are **Mac-only**, measured by a partial link (`ld.lld -r`) of the
R15e `53a64553` link command with the #10 units/artifacts wired exactly as the
build wires them (`tools/link_probe.py`; R15e runtime object `84dfccb3…`).
They are a plan-mode prediction; the integrator's real build recomputes them and
the gates below enforce the fixed list.

## Count

| | R15e | with #10 (Team G) |
|---|---|---|
| Final PRX import identities (P7) | 207 | **350** |
| New vs R15e | — | **143** |
| Evidenced by captured console provider | — | **143 / 143** |
| SDK-stub-only (not admitted) | — | **0** (7 SceNet removed, see below) |
| Removed by offline policy | — | 7 SceNet |
| Weak unprovided hooks (unchanged from R15e policy) | 6 | 6 |

The full admitted list, one row per new import with its evidence, is
`cap10/ADMITTED-IMPORTS.json` (in the patch). Summary of the 143:

| Provider module / library | Kind | Count |
|---|---|---|
| libkernel / libkernel | FUNC | 84 |
| libkernel / libkernel | OBJECT | 1 (`environ`) |
| libSceLibcInternal / libSceLibcInternal | FUNC | 56 |
| libSceLibcInternal / libSceLibcInternal | OBJECT | 2 (`__isthreaded`, `sys_signame`) |

## Provider identity evidence (two sources per row)

Every admitted row has `evidence.class == "captured-provider-export"` and names two sources:

- **Source 1** — GPT's `cap10_integration.provider_identity()` (its own ELF
  parser) resolving the NID against the **console-captured** provider ELF:
  - `libkernel.sprx` sha256 `f89a698bf7a0082ec0e361cebf77e40c938196cd13d46016531f71d6ca760912`
  - `libSceLibcInternal.sprx` sha256 `a2d70cc64c3abc3927662cbe0c3765b74b2385b4b2ad80eb7b942a891f9259fb`
  - both are read-only FTP retrievals from `/system/common/lib/…` (receipts beside them in `.superpowers/native-observations/user-return-20261003T140907Z-3bf558aa/`). Each row records the exported NID (`encoded_name`), symbol index, module/library name+version, and FUNC/OBJECT kind.
- **Source 2** — the intake analyst's independent NID recomputation + export-table
  search (`INTAKE.md` §4b): 143/143 confirmed present in the named captured
  module with matching kind.

An SDK stub `.nm` alone is **not** final evidence and is marked as such wherever
it appears; no admitted row relies on one.

`sigismember` (new with the v2 signal owner's `same_action`) is one of the 143:
captured libkernel export, module/library `libkernel` 257 / `libkernel` 1, FUNC
(NID at index 1415; GPT's parser and the intake both resolve it).

## Removed: the 7 SceNet imports (offline rule)

`sceNetErrnoLoc, sceNetPoolCreate, sceNetPoolDestroy, sceNetResolverCreate,
sceNetResolverDestroy, sceNetResolverStartAton, sceNetResolverStartNtoa`.

- They are pulled in only through the **owned** libc `netdb.o` (its
  `resolve_aton`/`resolve_ntoa`, reached by `getaddrinfo`/`gethostbyname*` that
  Mono's `networking-posix.o` / `w32socket.o` reference). They had **only
  SDK-stub evidence**; no genuine `libSceNet.sprx` capture exists, and GPT's own
  packager refuses them.
- **Fix (owned, fail-closed):** `cap10/native/Cap10OfflineNet.cpp` defines all 7
  (source asks for hidden visibility; on the PS5 target they end up default in
  the object — harmless, the packager exports only `module_start`/`module_stop`;
  APPLY.md §5b checks it) returning `-1` / a readable `ENETDOWN` errno slot,
  each logging its first refusal once via `diagnostic_event("CAP10_NET_REFUSED", id)`.
  The object links before the archives, so it satisfies `netdb.o`'s references and
  **no SceNet import is emitted**. Proven: the partial link's `llvm-nm -u` has
  **0 `sceNet*`** rows (`build/link1/undefined.stdout`); the host test
  `tests/host/test_offline_net.cpp` checks the return values, errno slot and
  once-only logging (10/10). ABI confirmed from two sources: the only caller
  (owned `netdb.o` disassembly) and the SDK stub `libSceNet.nm`.
- The #10 probe needs no name resolution (it calls two in-process native exports
  and throws once), so removing SceNet does not affect it.
- Libkernel sockets (`socket/connect/bind/listen/accept/send/recv/select/poll/
  kqueue/kevent`) remain imports: they are referenced directly by Mono's attach
  agent, `mono-rand` and `w32socket`, reached only by SIGQUIT, an env var, or
  managed socket APIs — none exercised by the Probe. They are **admitted**
  (captured libkernel evidence); being imported is harmless because the console
  exports them and the Probe never calls them.

## The 6 weak unprovided hooks — proven fail-closed

`__dladdr, __dlclose, __dlerror, __dlopen, __dlsym, kernel_mprotect`.

- Not new at package level: R15e's frozen packager already allows exactly these
  six, and `kernel_mprotect` is already a weak raw import in R15e.
- They become weak **raw** rows of the runtime object because Mono pulls the
  owned libc's `dlfcn.o`/`mman.o`. At runtime they stay **null**.
- **Proof no reachable executable-memory path** (two angles, both verified on this Mac):
  1. The owned libc wrappers check the hook pointer and fail closed when it is
     null — disassembly of `libc.a` `dlfcn.o`: `dlopen/dlsym/dladdr/dlerror`
     each `cmpq $0, __dl*@GOT; je ret0` → return `0`/`-1`; `dlclose` → `-1`.
     `mman.o`: `mmap`/`mprotect` take the `kernel_mprotect` branch only for
     `PROT_EXEC`, and with the hook null set errno and return failure
     (INTAKE §3b: ENOSYS). So a missing hook cannot execute anything; it returns
     an error.
  2. #10 resolves the Probe's two native imports through Mono's own
     `mono_dl_fallback` (variant-C-style in-image resolver in `MonoOwner.cpp`),
     **not** `dlopen`; the interpreter build (`DISABLE_JIT`, `MONO_AOT_MODE_INTERP`)
     requests no JIT/exec mapping. GPT's controlled host trace observed 33 mmap /
     7 mprotect with the only exec request being a Darwin-only probe that the
     target source branch excludes.
- No executable-memory path exists; the no_detour gate passes (data-only).

## Library choice (review finding 9)
R15e imports its 44 POSIX functions through library `libScePosix`. 54 of the new
libkernel rows are exported by both `libkernel` and `libScePosix`; the admitted
list (GPT's pinned identity family) imports them through `libkernel`. In the
captured provider both exports point to the same address (54/54), so behaviour
is the same.

## Files
- `cap10/ADMITTED-IMPORTS.json` — the reviewed list (the gate's source of truth).
- `cap10/baseline/R15E-EXACT-IMPORT-IDENTITIES.json` — R15e's 207 (copied from package-A, PRX `53a64553`).
- `build/link1/RAW-DELTA.json`, `build/link1/NEW-IMPORTS.json` — the measurement.
