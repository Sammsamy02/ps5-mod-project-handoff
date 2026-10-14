# l22 profile, preflight and SCO admission overlay

Local implementation is ready for the parent’s integration and target build. Historical sources, profiles and audit evidence remain unchanged. No console operation, native engine invocation, PRX build or upload was performed here.

- `CHANGES.json` maps four originals to candidates and adds `units/JoinedLayoutPreflight.hpp`. No UE4SSProgram change is needed. `initializer.diff` contains the complete initializer change.
- The profile and compiled declarations now agree on all17 FMalloc slots, including QuantizeSize0x68, Malloc0x30, Realloc0x40 and Free0x50. All69 prior member pins and7 prior virtual pins remain. New profile digest: `1ca87defdcbc7ace7753ef904a1e8cb7cc211a6388628889a4206f93074f45b8`.
- Provenance retains the pinned Epic commit, exact declaration/image candidate records, executable target checks, six independent semantic anchors, conditional confidence distinctions, full image SHA and rejected exec-disabled offsets. Promotion comes from the l22 user directive; nested l21 candidate records retain their original unpromoted status as historical evidence. This is a Townfall image-bound profile, not a universal UE5.6 ABI. The actual game macro override and live slot contents remain unmeasured.
- The exact63-key check runs once on the already loaded staged map snapshot inside the existing early profile gate, before NativeNameThread preparation and seeded engine calls. It logs49 startup-required keys,12 conditional map keys and2 conditional/deferred runtime FText discoveries. It collects every required missing/mismatching key before refusing. It never publishes maps, inserts offsets or reads game objects.
- Missing conditional keys do not block. In particular, absence of FArrayProperty::Inner or FProperty::GetMinAlignment now defers through the older pin loop as well. Their present values still must agree with the original profile. Other existing image/identity/settings/profile checks remain. Lazy accessor/virtual throws are unchanged.
- Slot4 now requires the existing authenticated cached named entry and SCO prefix under the shared deadline before binding. The log reports `statically_verified_empty_callback; native success not observed`. This consumes the existing cache and bridge functions, preserving call-only/XOM behavior. The parent must integrate the separate ABI overlay’s alignment and five assertions with this initializer.

Verification: `tests/RESULTS.json` records10 actual-initializer preflight scenarios under host ASan/UBSan, including all63 logging, three independent missing required keys collected together, old maps missing four startup allocator slots, each conditional spawn pin absent, conditional present-value mismatch refusal, FText deferred, all map keys present, image mismatch refusal and no map mutation. The actual original initializer fails the63-key reporting control. The original profile fails the17-allocator contract. The unchanged digest static_assert rejects an old/new header pairing. The generated JSON, typed profile, compiled INI entries and digest agree.

`tests/SCO-RESULTS.json` records8 ASan/UBSan controls using the actual new slot4 block, existing admission/cache methods and bridge functions, and read-only whole-file authentication of the retained decrypted image. Authenticated cached binding passes; wrong prefix, non-executable target, wrong base, expired deadline, faulted range, denied owner and cleared cache all refuse. No native SCO function is called by these tests.

Reproduce from this worktree:

```sh
python3 outputs/2026-10-08-l22-runtime/profile/generate_overlay.py
python3 outputs/2026-10-08-l22-runtime/profile/tests/run_tests.py
python3 outputs/2026-10-08-l22-runtime/profile/tests/test_sco_cache.py
```

The generator writes only its own overlay directory. Tests compile extracted actual code with host boundary fixtures; they do not establish a successful target compile, runtime initialization, menu display or native SCO call. Parent integration, target-dependent rebuild and review remain required.
