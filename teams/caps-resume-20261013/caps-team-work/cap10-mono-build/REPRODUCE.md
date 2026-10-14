# Reproduce the Mac-only strict loader checks

Run from this `cap10-mono-build` directory in the isolated worktree. The existing artifacts may be checked with:

```sh
python3 tools/run_loader_checks.py PATCHED
python3 tools/run_owner_check.py
python3 tools/check_source_integrity.py
```

These compile only their small host test programs, execute the genuine built runtime, and write receipts in this experiment. `run_owner_check.py` requires the successful hash-bound corlib AOT already present. Do not replace it with the failed exploratory artifact. `run_loader_checks.py BASELINE` selects the preserved `baseline-runtime/libmonosgen-2.0.a` and immutable original loader source; it intentionally reproduces failures, including one isolated SIGSEGV with core files disabled. Baseline execution is unnecessary for routine positive rechecks.

## Full native-runtime recreation in a fresh experiment

The exact executed command arrays and environment settings are in `evidence/BUILD-TOOLS.json`, `AUTOGEN.json`, `CONFIGURE.json`, `BASELINE-COMPAT-*.json`, and `PATCHED-*.json`. Shell command arguments below resolve to the same recipe. `task_root` must refer to a **fresh experiment**, because existing evidence/build outputs should be preserved.

Download these public source archives and compare their SHA256 values against `evidence/DOWNLOADS.json` and `evidence/COREFX.json`:

- `https://codeload.github.com/mono/mono/tar.gz/0cbf0e290c31adb476f9de0fa44b1d8829affa40`
- `https://codeload.github.com/mono/corefx/tar.gz/c4eeab9fc2faa0195a812e552cd73ee298d39386`
- `https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz`
- `https://ftp.gnu.org/gnu/automake/automake-1.16.5.tar.gz`
- `https://ftp.gnu.org/gnu/libtool/libtool-2.4.7.tar.gz`

Unpack each archive under `sources/`. Keep the Mono/CoreFX source extracts immutable. Separately unpack Mono to `runtime-work/` and CoreFX to `runtime-work/external/corefx/`, stripping one archive directory component. These work copies need normal write permissions. Preserve the original archives. The full source archives do not bundle all external submodules; only CoreFX is needed by this disabled-component native-core recipe.

1. Run `python3 tools/bootstrap_build_tools.py`. It configures, builds at `-j2`, and installs each GNU tool under this experiment's `build-tools/`.
2. In `runtime-work/`, run `./autogen.sh --disable-boehm` with `PATH` beginning with the absolute local `build-tools/bin`, followed by `/usr/bin:/bin:/usr/sbin:/sbin`; set `NOCONFIGURE=1` and `ACLOCAL_FLAGS='-I /opt/homebrew/share/aclocal'`. The latter was present in the successful command; this build did not require `pkg.m4` or a global pkg-config installation.
3. From the experiment root run `python3 tools/build_runtime.py configure`. The full configure arguments, compiler selection and flags are in that script and its receipt. Configure generates the genuine native headers/Makefiles.
4. In `runtime-work/`, apply `patch -p1 < ../evidence/HOST-COMPAT.patch`. This is the isolated modern-Apple-header compatibility fix, required before building metadata.
5. Run `python3 tools/build_runtime.py BASELINE-COMPAT` from the experiment root. It builds each real native-core subdirectory sequentially, each at `-j2`, and stops on any failure.
6. Copy `runtime-work/mono/mini/.libs/libmonosgen-2.0.a`, `.libs/libmonosgen-2.0.1.dylib`, and `runtime-work/mono/mini/mono-sgen` into `baseline-runtime/` before changing the loader. Run `python3 tools/run_loader_checks.py BASELINE` if reproducing the regression.
7. In `runtime-work/`, apply `patch -p1 < ../../cap10/native/mono-owned-loader.patch`. The exact unchanged strict patch has SHA256 `c1c2c5e5f3613e340fcbaa1147f781d46fb45f4f865ca492b2914dcf3d3b8267`. A fresh original `mono-dl.c` must hash to `b7ccf30b3828f0fb64cf07afda22f574f00b6f665b17faeb9f9b7fb3df8f9e8e`, and the derived file to `0a6c6d8674f95627aa1d8f17141c7fe58154bd952cd03817fe06725cc06a8a8c`. The actual initial application used the existing pinned `stage_mono_loader.py` transform and verified its exact inverse; see `STRICT-PATCH.json`.
8. Run `python3 tools/build_runtime.py PATCHED`, then `python3 tools/run_loader_checks.py PATCHED`.
9. Ensure the unchanged official MDK extraction exists at the sibling `mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0` path. Run `python3 tools/build_corlib_aot.py` once. It checks and copies the genuine corlib, then invokes the new compiler with `--aot=full,interp`. It refuses to overwrite an existing AOT artifact.
10. Run `python3 tools/run_owner_check.py` and `python3 tools/check_source_integrity.py`.

Every receipt binds the source/configuration/runtime/test inputs with hashes and preserves compiler/linker command arrays. The host dylib and corlib AOT are ordinary Darwin outputs. No install step places Mono globally, and no step builds a target PRX or contacts a console.

## Small handoff payload

Include `STATUS.md`, `REPRODUCE.md`, `tools/`, `tests/`, `evidence/*.json`, `evidence/HOST-COMPAT.patch`, and the test/owner/configure/build logs as needed. Include the sibling component's unchanged `native/mono-owned-loader.patch`. Exclude large `downloads/`, `sources/`, `runtime-work/`, `build-tools/`, `baseline-runtime/`, `owner-runtime/`, `owner-objects/`, `bin/`, `fixtures/`, `experiments/`, and `logs/ICALL-PREPROCESS.i` from review ZIPs; their identities and reproduction inputs are recorded.
