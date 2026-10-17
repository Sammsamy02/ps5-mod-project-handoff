# Mac strict Mono loader closure — 2026-10-13

The strict loader patch now compiles into a genuine pinned Mono runtime and passes actual runtime tests on this Mac. The production `MonoOwner.cpp` and `InvocationBoundary.cpp` also run successfully against that patched runtime in `MONO_AOT_MODE_INTERP`: two managed runs, one exact managed exception, recovery and cleanup, with clean diagnostics. This closes the earlier **Mac-only source/host strict-loader gap**. It does not establish a usable target runtime or game-process behavior.

Everything authored by this worker is under `caps-team-work/cap10-mono-build`. No console access, target PRX build, deployment, mod edit, proprietary SDK use, or changes to the shared candidate/frozen build occurred. Other workers' target-port work is outside this report.

## Direct runtime regression evidence

`tests/strict-loader.c` calls the real internal `mono_dl_open`/`mono_dl_symbol`/`mono_dl_close` APIs and real public fallback APIs. It links the complete native runtime archive produced by Mono's own build. It uses genuine configured upstream headers and EGLib. There are no recreated internal headers, loader copies, loader mocks, or runtime function stubs.

`tests/ambient-library.c` is a real x86_64 Mach-O dylib. Its constructor appends a marker file if the OS loader runs it. Its exported sentinel returns 99; the registered test provider returns 7. Each case runs in a separate process.

| Case | Unpatched runtime | Strict runtime |
|---|---|---|
| Direct existing `modbridge-owned:` dylib | Loaded ambient library; marker created | Refused; no marker |
| Prefixed/suffixed existing reserved dylib | Loaded ambient library; marker created | Refused; no marker |
| Explicit reserved `.la` naming ordinary dylib | Loaded ambient library; marker created | Refused; no marker |
| Implicit reserved `.la` lookup | Loaded ambient library; marker created | Refused; no marker |
| Ordinary non-owned dylib control | Loaded; sentinel 99 | Loaded; sentinel 99 |
| Provider registered over existing reserved dylib | Ambient loader won; test failed | Provider won; unknown symbol rejected; close callback observed; no ambient marker |
| Remove fallback head, look up surviving provider, close, remove last provider, look up again; 32 cycles | Real SIGSEGV, exit -11; no core | All 32 cycles passed |

`evidence/LOADER-BASELINE.json` records the expected failures; its PASS label means the regression reproduced, not that the old loader was correct. `evidence/LOADER-PATCHED.json` records all seven green cases. Original raw case output is under `logs/BASELINE-*.log` and `logs/PATCHED-*.log`.

## Actual production owner

`tools/run_owner_check.py` compiles the current production owner and boundary directly, without editing them, with `-Wall -Wextra -Werror`. The host observer remains explicitly host-only. The small `tests/MonoGcRegionApi.hpp` forwards to **the complete upstream `mono-threads-api.h` and real configure-generated `config.h`**; it defines no replacement ABI or runtime behavior.

The executable links the actual patched `libmonosgen-2.0.1.dylib`. Its dependency path is verified with `otool -L`, and its own install-name reference is relocated to that dylib. Its exact embedded Probe identity remains `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d`. Corlib is the hash-bound official MDK file and its full/interp AOT image was produced by the newly built compiler. `MONO_PATH`, `MONO_ENV_OPTIONS` and `MONO_AOT_CACHE` are removed for owner execution.

`evidence/OWNER-CHECK.json` reports `REAL_MONO_OWNER_STRICT_HOST_PASS`, stage 7, two managed runs, one exception, all Run/throw/recovery flags true, exit 0, and clean diagnostics. `logs/OWNER-CHECK.log` includes both native-import observation counts and final result. This remains a host observer, not an engine-readiness observer.

## Genuine source and build identity

- Mono: commit `0cbf0e290c31adb476f9de0fa44b1d8829affa40`, full repository archive from GitHub codeload, SHA256 `d35e720999e3b3274654386b8d9afdb6ff56e2db9c95ebfe40be3c2e7f5257c9`.
- CoreFX: exact Mono submodule commit `c4eeab9fc2faa0195a812e552cd73ee298d39386`, archive SHA256 `44d92d54a210bc718fd55187630acbaf187adeeef9101dfafef4588b5710fd16`. The upstream submodule API response is retained.
- Local build tools: public GNU autoconf 2.69, automake 1.16.5, libtool 2.4.7, built into `build-tools`; no global install.
- Native core built with at most `make -j2`: EGLib, architecture helpers, utilities, CIL, zlib glue, SGen, metadata, mini/interpreter/AOT compiler. Managed-library builds, LLVM, Boehm, BTLS, support and Mono.Native builds were omitted. The MDK supplies the existing matching corlib/config inputs. This is a real native runtime build, not a build of every distribution component.
- Patched loader SHA256: `0a6c6d8674f95627aa1d8f17141c7fe58154bd952cd03817fe06725cc06a8a8c`.
- Patched dylib SHA256: `c564b99bf331c552323c57c4d70ce9a38d5f7e7c304fa0f343bfa1dbc3428f7c`.
- Patched compiler SHA256: `c301fbc2012d02f0f00c1625f6ffb366f46e8fa5cabfa24fd073138365d786fe`.

The immutable `sources/mono-*` and `sources/corefx-*` trees were compared byte-for-byte to all 56,725 and 18,730 regular files respectively in their downloaded archives. `evidence/SOURCE-INTEGRITY.json` records zero mismatches. The working Mono tree differs from upstream files only in `mono/utils/mono-dl.c` and the host compatibility change below; CoreFX has zero changes. Build-generated files are additional outputs, not substituted upstream source.

## Host compatibility and exploratory limitation

Apple's current C headers expose `bool` as `_Bool`. Pinned Mono expands that token into `ICALL_SIG_TYPE__Bool`, but only has `ICALL_SIG_TYPE_bool`. The initial genuine metadata build failed on exactly this symbol. The separate `evidence/HOST-COMPAT.patch` adds a Darwin/C-macro-guarded enum alias with the same existing value. It changes no loader policy. Its original failure log, preprocessing evidence, exact source/derived hashes, and successful build log are retained.

An exploratory new-compiler `--aot=full,interp,threads=2` run generated output but then failed during native runtime shutdown, exit -6. Its log/receipt are preserved as `OWNER-CORLIB-AOT.THREADS2-FAIL.*`; the generated artifact is isolated in `experiments/aot-threaded-crash` and is not used by the successful test. No runtime patch was added for that failure. The supported reproduction uses the default single-thread `--aot=full,interp`, which completed exit 0 cleanly and produced AOT SHA256 `ac1bf99e6be972e885bc0b06216dc0fcb5dd677920091d2de5cadc00dd3879e7`.

## Remaining scope

This work proves real host loader rejection and provider lifetime, plus the actual production owner on a patched host runtime. Target build/dependency closure, target runtime and AOT execution, signal/stack/GC/thread behavior in the game, current-source integration, and Townfall Run → throw → recovery still need their own evidence. The host output must not be labeled a target artifact or console readiness.
