# Narrow Prospero pthread TLS configuration fix

This stage fixes one concrete Mono 6.12 source gap. It is **Mac-only**, unmerged, and not a complete target port or target admission.

The public SDK v0.43 wrapper uses `-femulated-tls`; the actual assembly probe calls `__emutls_get_address`. Pinned Mono defaults to compiler TLS, assumes it works when cross-compiling, and its AMD64 offset code references native `GOTTPOFF`/`TPOFF` offsets. Those assumptions do not describe this emulated TLS implementation.

`prospero-pthread-tls.patch` changes only the pinned `configure.ac`. It detects the real compiler's `__PROSPERO__` macro, selects Mono's existing pthread TLS implementation by default, accepts an explicit `--with-tls=pthread`, and reports a configure error for an explicit incompatible TLS choice. Other targets retain their existing selection. It adds no runtime calls or target admission macro.

Fresh compiler evidence also corrected the earlier backend diagnosis: genuine `x86_64-sie-ps5` compilation defines both `__PROSPERO__=1` and `__FreeBSD__=9`, with `_POSIX_VERSION=200112L`. Existing Mono FreeBSD stack and POSIX backend code is already selected. No fake OS macro was added; no replacement stack/GC/context/suspend implementation is staged.

Inputs are pinned to Mono `0cbf0e290c31adb476f9de0fa44b1d8829affa40`. `stage_tls_policy.py` verifies the original complete configure file hash and exact anchor, writes only here, and produces an exact inverse and patch receipt. Regenerate with:

```sh
python3 stage_tls_policy.py
python3 verify_stage.py
python3 run_feature_probes.py --sdk /path/to/public/ps5-payload-sdk --llvm-bin /path/to/pinned/LLVM/bin
```

Evidence: two expected policy failures on pristine source, four passing policy tests after staging, exact actual patch application/inverse, drift/reapplication refusal, and five genuine SDK compiler/syntax/assembly checks. See `VERIFICATION.json`, `FEATURE-PROBE-RECEIPT.json`, `logs/`, and `probe/`. The configure policy test executes the exact staged shell fragment; the target detector probe compiles the exact added Autoconf C payload. Full Autoconf regeneration and the full runtime were not built.

Claude should treat this as one component of the genuine target source port. Host canonicalization/build dependencies, actual target SGen/stack/TLS behavior, signals/context coexistence with the engine, target AOT transitions, System.Native/configuration and the owned loader still require completion and target proof. The root agent's separate actual-owner host test has now passed `MONO_AOT_MODE_INTERP` with generated full/interpreter corlib AOT data; that does not resolve these target requirements. #10 must not hold ready builds or Stellar Blade.

All work here is isolated source preparation and bounded compiler probes. No console contact, full target/runtime/PRX build, main-tree/cache mutation, mod edit or kernel work occurred. Source licensing remains the pinned public Mono license; the SDK/compiler are read-only independently licensed public dependencies.
