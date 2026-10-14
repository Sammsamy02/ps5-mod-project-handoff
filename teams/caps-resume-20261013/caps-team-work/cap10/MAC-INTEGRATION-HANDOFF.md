# #10 Mac preparation and Claude integration

The minimal Probe now has a genuine source-built target Mono archive, matching static target corlib, the production owner/boundary compiled for the target, and a successful standalone target ELF link. The updated production owner also passes Run → exact managed exception → recovery on the real Mac runtime using statically registered corlib. None of these results is a PS5 execution result. Claude owns the combined mod-loader build, packaging and console test.

## Use this source and these artifacts

- `native/MonoOwner.cpp`, `InvocationBoundary.cpp`, their headers, `Cap10InvocationObservation.hpp`, and the exact embedded `ProbeImage.hpp`.
- `native/current-owner.patch`: three changes to UE4SSProgram (includes, the observation/bridge implementation, and the existing engine-initialized callback). The input remains pinned to SHA256 `44499a0aaeb617c80f9cee3ad62222cdffe9f45b4b71cc663fb25cce616150f1`. Preserve later Claude changes when composing it; do not run the old bridge-only patch as well.
- `../cap10-port-closure/target-work/mono/mini/.libs/libmonosgen-2.0.a`: the real target runtime, SHA256 `38d0342104fe5c7741225c6d4e6937d1e60869ed3ab2c409a743994320e6cca9`. The SDK Mono `.so` is an import stub and cannot replace it.
- `../cap10-cross-aot/artifacts/mscorlib.full-interp.ps5.o`: genuine target static AOT, SHA256 `a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de`. An equivalent archive is also supplied; use only one form.
- `../cap10-cross-aot/managed-input/mscorlib.dll`: required matching IL, SHA256 `86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4`. The AOT object does not eliminate the IL file.
- Genuine configured headers from `../cap10-port-closure/target-work/` and its `mono/eglib/`; final `config.h` SHA256 `9f0324038b43cb4e01a20bb00b3e8c8e2a8083d9a5895aa8cbe5c6d032c34ee6`. Use the actual generated configuration and complete internal thread API, not the historical small host declaration shim.
- Target-generated `../cap10-port-closure/target-work/data/config`. The link-only fixture copies it to `owner-link/runtime/etc/mono/config`; its receipt records the exact hash.

The strict owned-library resolver patch is already compiled into the target archive. Do not patch the binary or substitute the historical unpatched host runtime.

In the portable packet, the port manifest maps the runtime archive to `cap10-port-closure/handoff/lib/libmonosgen-2.0.a`, configuration to `handoff/runtime-config/mono/config`, and the 24 actual Mono header/config dependencies to `handoff/runtime-headers/`. Use that header root and its `mono/eglib/` directory as include paths when consuming the packet; the public SDK/libc++ remain external. These copies have the same verified hashes as the local build files above.

## Merge once, then build once

1. Compose #7's full reviewed stage and the #10 callback patch into Claude's current tree. #7 changes the CodeImageCache layout; all consumers need the full rebuild specified by its handoff. The two capabilities touch different C++ source files but share build/packaging metadata.
2. Register **two new C++ units** (owner and invocation boundary) as real NEW_UNITS. Add the actual owner headers and genuine Mono configuration/include dependencies to the driver's pinned dependency metadata. The UE4SSProgram change remains a replacement of the existing unit. Do not add the link-only observer/provider fixtures to the game build: the production observation/provider implementation is already in the callback patch.
3. Compile the target owner, boundary and callback unit with `MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1` only as part of Claude's planned integration/test build. This switch selects code; it does not assert that target GC/TLS/EH has already passed. Never define `MODBRIDGE_CAP10_MONO_HOST_VALIDATION` in that build. The owner automatically registers static corlib under `__PROSPERO__`; its generated symbol's value is passed before runtime initialization.
4. Set `MODBRIDGE_CAP10_CORLIB_DIR` to the console runtime's **lib prefix** and `MODBRIDGE_CAP10_CONFIG_DIR` to its **etc prefix**. Package matching IL at `lib/mono/4.5/mscorlib.dll` and configuration at `etc/mono/config`. Do not embed the Mac link fixture's paths. Use the final configured native mod/runtime root, not an invented console path.
5. Add the single static corlib object/archive and genuine Mono archive as pinned link inputs. The proved standalone order is owner/boundary/provider objects, corlib object, Mono archive, pthread and the public SDK C++/C/kernel libraries. Keep the module linked for the entire runtime lifetime. Preserve the complete archive and required registration/rooting functions; do not replace missing functions with placeholders.
6. Extend the existing link census to count the two new translation units and the **two additional artifact inputs**, exactly once each. After #7's reviewed five NEW_UNITS, the ordinary unchanged baseline would be 391 + 7 + 2 = 400 inputs; recompute if Claude added other units. Record the real new imports and final package library identities instead of assuming the previous no-new-imports result. The standalone link's public dependencies are listed in the port report.
7. Run the existing source/include/field/no-detour/build/package checks for the composed build. The one full game build and package remain Claude's work; the standalone ELF is a test artifact and must never be deployed as the mod loader.

## Evidence and remaining target proof

`../cap10-static-corlib-host/evidence/PRODUCTION-STATIC-OWNER-CHECK.json` binds the actual current owner to the static host pass. Its harness does not perform registration; the production owner does. `../cap10-mono-build/evidence/LOADER-PATCHED.json` proves seven genuine runtime loader tests, with the unpatched failures retained. The minimal host flow requests only the two owned native exports; `../cap10-native-deps/` records that it does not load System.Native. Broader managed file/process/library APIs have not been promised.

`../cap10-cross-aot/evidence/FINAL-TARGET-ABI-AUDIT.json` checks all 278 metadata and 91 mini layout constants against the final target headers. The target AOT completed 35,820/35,828 methods; the skipped-method report is retained. `../cap10-port-closure/evidence/TARGET-OWNER-LINK.json` records the actual target compilation and complete standalone link with `--no-undefined`/`-z defs`, library order and artifact identities. It uses a clearly labeled link-only observer and was not run.

Claude's console acceptance is the minimal Townfall Probe: real callback admission; interpreter startup; exact Probe image; both native exports; two different nonzero invocation cookies; Run, deliberate exact exception and recovery all pass; cleanup completes. Verify actual SGen stack rooting, pthread TLS, signal/context and AOT transitions there. Process/module diagnostics currently select upstream file-based fallbacks and remain unsupported/unproved on the target; an empty result is not proof they work. Full Mono.Native/System.Native and arbitrary managed mods remain outside this tiny Probe proof. Unity is the later full proof.

No PS5 contact, PRX build/deployment, main-tree write, frozen-build edit or kernel change was performed here. #10 must not delay other ready capabilities or Stellar Blade.
