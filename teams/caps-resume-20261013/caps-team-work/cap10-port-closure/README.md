# Public Mono target port closure — Mac preparation, 2026-10-13

The pinned Mono runtime now genuinely cross-compiles with the public SDK, and the exact production owner/boundary links with that runtime and the real target `full,interp,static` corlib object. This completes the source/build/link work exercised here. **There has been no console execution, PRX/shared-image link, packaging, deployment, production admission, or game integration.**

The final link uses the real Mono implementation and all its selected GC/thread/context/suspend sources. No Mono or platform API is replaced with a success stub. The observation/provider code in `owner-link/` is an explicitly unexecuted link-only fixture, derived from the prior host test. Parent authorization permits the admission macro in this compile probe only; no main/candidate/admission source is changed.

## Concrete results

| Check | Evidence |
|---|---|
| Actual target compiler/TLS diagnosis | `SOURCE-PROBES.json`: public compiler defines Prospero + FreeBSD9; actual pinned pthread TLS and context sources compile; original compiler-TLS mismatch reproduced |
| Real configure and native build | `TARGET-PROCESS-CAPS-*`: successful Autogen/configure and EGLIB, architecture, utilities, CIL, zlib, SGen, metadata, interpreter/runtime builds, each `make -j1` |
| Exact source changes | `SOURCE-INTEGRITY.json`: all 56,725 Mono and 18,730 CoreFX archive files checked; exactly seven intended Mono files changed, no CoreFX changes |
| Staging/policy verification | `CONFIGURATION-VERIFICATION.json`: 29 exact patch/inverse, tuple and generated-shell checks; strict loader also verified forward/inverse and hash-bound separately |
| Final configuration and TLS object | `FINAL-TARGET-CONFIGURATION.json`: actual final config selects AMD64, JIT disabled, AOT retained, cooperative suspend, pthread TLS and capability-selected process fallback |
| Exact owner link | `TARGET-OWNER-LINK.json`: current owner + boundary compile with `-Wall -Wextra -Werror`, genuine target internal headers, actual runtime archive and target corlib; `--no-undefined -z defs` link passes |
| Relocation/callsite/import audit | `TARGET-LINK-AUDIT.json`: actual registration/rooting call relocations retained; all Mono symbols locally resolved; 289 public-library dynamic imports; PIC and allocated relocation census |

Evidence paths in the table are under `evidence/`. Full command output is under `logs/` and `owner-link/`. Every failed investigation stage is retained; the final `TARGET-PROCESS-CAPS-*` receipts supersede earlier failed stages.

Final native artifacts, built from genuine source:

- `target-work/mono/mini/.libs/libmonosgen-2.0.a`: **30,553,848 bytes**, SHA256 `38d0342104fe5c7741225c6d4e6937d1e60869ed3ab2c409a743994320e6cca9`, 322 ELF members.
- `target-work/mono/mini/mono-sgen`: **13,631,896 bytes**, SHA256 `2ab3b448606509d53ad6ca00dd8cc5d6c1e87dbdd71ffae762daec3b108332c3`.
- `owner-link/mono-owner-link-only.elf`: **45,278,784 bytes**, SHA256 `69b3cf71d0d5d294bc3901d79a2ef76c07d7fcd69dde49b1596240a0c9b6f030`.

The public linker wrapper supplies `-pie` and its linker script. The resulting normal-link ELF is `ET_DYN` with `PIE`, **entry point zero**. It is a link-only image, not a runnable deployment executable or an approved shared module. No `-shared` build was performed.

## Defensible source changes

All patches start from Mono `0cbf0e290c31adb476f9de0fa44b1d8829affa40`; stage tools bind whole-file hashes and exact inverse transforms.

1. Recognize the honest `x86_64-sie-ps5` tuple in generated `config.sub`, and select an explicit build branch. Require the actual compiler's `__PROSPERO__` for that host; reject disguising it as desktop FreeBSD. The compiler itself supplies `__FreeBSD__=9`. Build characteristics reuse existing BSD/POSIX code without invented OS macros.
2. Select the existing pthread TLS implementation. Reject explicit incompatible compiler TLS, whose public compiler generates emulated TLS while Mono's AMD64 offset path assumes native TLS.
3. Derive the target math link flag from the genuine existing `sqrtf` link probe. The SDK supplies math through its default C libraries and has no separate `libm`; other target branches retain their existing math flags.
4. Probe the declaration of `pthread_getname_np`, not only its symbol. The public import symbol has no supported declaration here, so diagnostic thread-name retrieval uses Mono's existing return-zero path. Thread creation/stack/TLS/suspend code remains enabled.
5. Probe `kinfo_getfile`; when absent, use the existing actual descriptor-close loop. Require actual `dl_iterate_phdr` availability for the Prospero BSD process backend; otherwise select the existing file-based default backend. **Process/module diagnostics remain unsupported/unproven on the console.** Their `/proc` dependencies are unverified, and existing caller behavior can return success with a NULL placeholder. This is not a claim of clean failure or functional `Process.Modules`.
6. Compile in the root-reviewed strict owned-library loader patch. Reserved `modbridge-owned:` names bypass ambient OS lookup, unresolved reserved names fail, and fallback-handler head removal assigns the updated list. Its actual runtime behavioral tests are in the separate frozen host build.

Mono.Native's existing Unix source set is selected honestly by configure, but its full PAL is not part of this minimal build. `cap10-native-deps` demonstrates the actual host owner flow does not request System.Native/MonoPosixHelper; that workload finding does not cover general C# or Unity. The generated configuration contains wider dllmaps, which are stored without eager loading. The link-only configuration fixture is not a finished production package.

## Inputs and reproduction

Read-only upstream source and local Autotools are shared with `../cap10-mono-build`. Exact public inputs:

- Mono archive SHA256 `d35e720999e3b3274654386b8d9afdb6ff56e2db9c95ebfe40be3c2e7f5257c9`.
- CoreFX commit `c4eeab9fc2faa0195a812e552cd73ee298d39386`, archive SHA256 `44d92d54a210bc718fd55187630acbaf187adeeef9101dfafef4588b5710fd16`.
- Public SDK v0.43 and LLVM21.1.8 paths are bound in `LOCAL-INPUTS.json`; the SDK provenance is in `../cap10-port-research/LOCAL-INVENTORY.json`.
- Compiler extraction lacks `llvm-ar`. `archive_bin` points to the existing complete matching public LLVM21.1.8 extraction. The build script checks its exact hash; it does not use the missing SDK archiver target. Explicit SDK `LD` and `AS` prevent host linker discovery.
- Autoconf2.69, Automake1.16.5, Libtool2.4.7 are the local tools built by the host-build component. No global installation is required.
- Corlib object and matching IL come from `../cap10-cross-aot`; `AOT-ARTIFACT.json` binds SHA256 `a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de`. The final config differs from its layout snapshot only by negative diagnostic feature probes; `AOT-CONFIG-DELTA.patch` records the exact difference.

For a fresh owned copy, first set the paths in `LOCAL-INPUTS.json`, then follow this sequence from this component directory. Do not run copy commands over an existing `target-work` experiment; preserve its evidence or use a new directory.

```sh
MONO_SOURCE=../cap10-mono-build/sources/mono-0cbf0e290c31adb476f9de0fa44b1d8829affa40
COREFX_SOURCE=../cap10-mono-build/sources/corefx-c4eeab9fc2faa0195a812e552cd73ee298d39386
cp -cR "$MONO_SOURCE" target-work
chmod -R u+w target-work
cp -cR "$COREFX_SOURCE"/. target-work/external/corefx
chmod -R u+w target-work/external/corefx
python3 tools/stage_target_configuration.py --source "$MONO_SOURCE" --autogen-source ../cap10-mono-build/runtime-work
python3 - <<'PY'
import json, pathlib, shutil, subprocess
root = pathlib.Path.cwd()
for item in json.loads((root/'evidence/CONFIGURATION-STAGE.json').read_text())['bindings']:
    path = pathlib.Path(item['path'])
    if path.name != 'config.sub':
        shutil.copyfile(root/'derived'/path, root/'target-work'/path)
subprocess.run(['/usr/bin/patch','--batch','-p1','-i',str(root/'patches/mono-owned-loader.patch')],cwd=root/'target-work',check=True)
PY
python3 tools/build_target.py autogen --label FRESH
cp derived/config.sub target-work/config.sub
python3 tools/build_target.py configure --label FRESH
python3 tools/build_target.py build --label FRESH
python3 tools/verify_configuration.py
python3 tools/verify_target_configuration.py
python3 tools/check_source_integrity.py
python3 tools/link_target_owner.py
python3 tools/audit_target_link.py
```

`build_target.py` holds the exact configure recipe, genuine compiler/linker bindings and one-job limit. It supplies no cached feature answers. Autogen can replace `config.sub`, which is why its staged version is copied **after** Autogen. The aggregate static archive is an explicit upstream convenience-library target because upstream's normal `shared=no` build does not request that aggregate automatically. For only the wider optional PAL, `--part native` exists but is not required by this proof.

Owner link order is: exact owner/boundary plus test fixture objects, then the target static corlib object, then the genuine aggregate Mono archive, then `-lpthread`, followed by the public wrapper's C++/C/kernel default libraries. The owner itself calls `mono_aot_register_module` before initialization. No forced registration in the fixture or custom runtime API is used. Source, header, archive, corlib, configuration, output and command identities are recorded together in `TARGET-OWNER-LINK.json`. These hashes identify this build; debug paths and generated timestamps mean byte identity is not promised across directories or rebuilds.

The exact owner/boundary preprocessor dependencies are frozen as 24 real Mono headers/configs in `handoff/runtime-headers/` (225,881 bytes). `OWNER-HEADER-DEPENDENCIES.json` binds their paths/hashes and the actual compiler `-M` commands. Preserve those paths; include that root plus its `mono/eglib` directory. Public SDK and libc++ headers remain external toolchain inputs.

## Limits for Claude's console/integration pass

Native compile/link work is complete for this scope. It does not prove target GC roots/stack bounds, pthread TLS, exception handling, signal-handler coexistence, transition trampolines, cleanup, actual P/Invoke, loader behavior in the game, or Townfall continuation. Those require execution through the real owner and current integration.

The final image has **289** undefined dynamic symbols supplied at link by SDK import libraries: `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`. There are no undefined Mono/SGen/icall symbols and no dependency on the SDK's fake Mono import library. Public import libraries are not implementations or evidence those functions were executed. The actual integrator's older fixed import census must be reconciled; this component never edits it.

The C compiler uses PIC level2 by default. In allocated sections, the runtime archive contains 2,717 `R_X86_64_64` data-pointer relocations; corlib contains 1,543. Neither contains allocated absolute32/32S relocations. Final PIE has no `TEXTREL` tag. PC-relative ranges, data relocations, symbol visibility, loader relocation handling, final shared/PRX link, entry/init lifecycle and actual imports remain the integrator's checks. These facts do not by themselves prove shared-image compatibility.

Full Mono.Native/PAL support and wider managed library functionality are separate from the tiny Probe; Unity remains a later proof. The current ready-build/candidate/mod work is untouched. Nothing here holds ready builds or Stellar Blade.

All writes were confined to this owned component. Licenses are retained under `licenses/`; generated GNU `config.sub` retains its own copyright/license. Full source copies, test ELF, and large relocation listings stay local. `HANDOFF-MANIFEST.json` selects the genuine runtime archive, target configuration, 24 dependency-complete headers, patches, scripts and receipts for review. They are build inputs/evidence, not production admission.
