# Claude handoff — #7 step 2 and #10, Mac-ready V2

This supersedes the earlier `GPT-7-10-MAC-FINAL-*` packet for current integration inputs. The isolated Mac implementation, dependency preparation, verification and review are complete to the extent possible with the available local public SDK and no console access. **Use the combined V2 stage below.** Target acceptance remains yours.

Owned worktree: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`, branch `codex/caps-resume-20261013`.
Main is read-only to GPT: `/WORKSPACE/.codex/worktrees/translation-core/what`.

## Delivered and verified

| Item | Mac result | Evidence in `caps-team-work/` |
|---|---|---|
| #7 runtime pak mounting | Complete reviewed implementation; current metadata preserved; 90 original C++ assertions; focused tool checks; six genuine target TUs compile, covering all five kMaxPins=16 consumers | `cap07-mac-ready/MAC-READINESS.md`, `FROZEN-READY.json`; original regressions in `cap07-step2/` |
| #10 runtime foundation | Genuine target Mono static archive and target static corlib; real production owner; target compile and final strict ordinary shared link pass | `cap10-port-closure/`, `cap10-cross-aot/`, `mac-readiness-max/shared-final/` |
| #10 managed behavior | Actual assembly runs, calls owned native exports, throws the expected managed exception, then recovers; genuine fixed runtime and static corlib | `cap10-signal-ready/evidence/*-v2.json` |
| #10 signal cleanup | Fixed upstream saved-action use-after-free and SIG_IGN dispatch; restored owned signal state and entry-thread mask | `cap10-signal-ready/README.md`, `evidence/mono-signal-lifecycle.patch` |
| Broader native C# support | Real 42-member libmono-native.a, 125 SystemNative exports, strict whole-archive link; real getline/fgetln ownership and error paths pass sanitizer tests | `cap10-native-ready/README.md`, `handoff/libmono-native.a` |
| Combined integration | Complete 55-file text patch + four binary copies; real source/header/assets pins; 400-input composition; typed provider, constructor and twin checks; 19 tests pass; exact apply/reverse | `cap10-integration-ready/README.md`, `derived/target-ready/INTEGRATION.json` |
| Final review | Root accepts the component work and separate integration review found no concrete code blocker; all 470 selected component files independently rehashed | `mac-readiness-max/REVIEW.md`, `MANIFEST-VERIFICATION.json` |

The broader native PAL is ready as a separate dependency artifact. It is not silently injected into the current Probe integration and is not a claim that arbitrary framework, Unity or Windows DLL mods run. #13 is the separate ZCode assignment.

## Authoritative integration inputs

Stage:
`caps-team-work/cap10-integration-ready/derived/target-ready/`

- `INTEGRATION.json` SHA256 `d6a07f82e10cf173a4d5110ede0e2a4cb8bde14166c9358ae62346bd0b8e8584`.
- `integration.patch` SHA256 `59e25e4b3ba0c813d79147851ac2a174e92fb47eb6f75237ed660891bd5765ac`.
- The stage manifest lists all text outputs and the four exact `binary_copy_inputs`. Copy those binaries from this same stage; the text patch cannot encode them.
- The full patch already includes #7 and #10. Do not stack the older #7/#10 patches on top.
- Preserve Claude's later work. The nine original-source pins all match at GPT's final read-only audit (`mac-readiness-max/CURRENT-MAIN-PINS.json`); check again at integration. Reconcile any later drift deliberately.

The accepted signal pair must stay together:

| Input | SHA256 |
|---|---|
| `cap10/native/MonoOwner.cpp` in the combined stage | `03edcc27dced6d2a1fa03162bbe3fbd74345e9d2207653688422a78d086c0bad` |
| `cap10/native/MonoOwner.hpp` | `8121c4ceb2c8bb6d3172c22abd7e969ecc0643ddd24cdcba1109ccf49bc169ed` |
| `cap10/lib/libmonosgen-2.0.a` | `24eebde5354ccf8ea3061d3a2bc47fd8eec48182934a91b304cd99b366d76a4f` |
| `cap10/lib/mscorlib.full-interp.ps5.o` | `a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de` |
| `cap10/managed/Probe.dll` | `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d` |

Keep the fixed v2 runtime, not the historical archive ending `38d03421…`. Never substitute the SDK's Mono import stub. Headers are genuine pinned dependencies. All large current binary inputs occur once in the V2 delivery packet; earlier manifests remain provenance and may reference historical local artifacts excluded from this packet.

## Claude's remaining work

1. When your #9/#11/#12 work is ready, independently review the final combination, check the original-source pins, apply the complete V2 patch and copy its four binary inputs. Use the stage's current helper/source receipt, not the old pin-only handoff.
2. Rebuild every CodeImageCache consumer together. The real composition is 374 BASE units + 7 NEW_UNITS + 17 fixed inputs + 2 binary link artifacts = 400. Keep the owned custom libc and all fixed inputs in order. Do not reuse kMaxPins=8 objects.
3. Complete the actual combined import/provider contract. The helpers derive raw imports from selected ELF/archive bytes and check actual nm. They derive final imports after actual packaging bootstrap definitions; they preserve FUNC/OBJECT kinds and sum constructor/finalizer inputs. Keep the existing exact final identity and 20 static package checks and require both independent twins. Final import/constructor counts must come from that real build.
4. If still unresolved after that build, supply genuine matching provider evidence for `sceNetErrnoLoc`, `sceNetPoolCreate`, `sceNetPoolDestroy`, `sceNetResolverCreate`, `sceNetResolverDestroy`, `sceNetResolverStartAton`, `sceNetResolverStartNtoa`. Current local captures prove libc/kernel identities but contain no real SceNet export identity records. SDK stubs prove declarations, not the final module/library versions. `PROVIDER-READINESS.json` is a diagnostic, not an invented final contract. The newly needed `sigismember` identity has independently verified captured libkernel evidence.
5. Stage the matching runtime assets from `runtime-assets/`: `lib/mono/4.5/mscorlib.dll` and `etc/mono/config` under the pinned active root `/data/modbridge/roots/PPSA33286`. The code sets Mono prefixes to root/lib and root/etc; Mono appends mono/config itself. Probe is embedded; the host-only `.dll.config` is not a production asset.
6. With exclusive console access, test #7's real mount and visible behavior using its launch card. Then test #10 at its assigned end-of-queue position: ready callback, interpreter startup, both native exports with distinct nonzero cookies, Run → exact managed exception → recovery → cleanup. Verify target stack rooting, GC/TLS, AOT transitions and signal behavior. Full Unity/framework proof follows later. If only #10 remains, do not hold Stellar Blade.

## Concrete runtime limits to carry forward

The final shared ELF has 290 strong SDK-backed imports and six unprovided weak hooks: `__dladdr`, `__dlclose`, `__dlerror`, `__dlopen`, `__dlsym`, `kernel_mprotect`. Link success does not establish those hooks' runtime behavior. Do not assume an executable-memory path can call a missing hook.

A controlled actual-owner Mac trace observed 33 mmap and seven mprotect calls, with no drops/failures. Its only executable request was Mono's 4 KiB Darwin-only WRITE|EXEC permission probe, confirmed against source and compiled callsite. The target source/object excludes that Darwin branch. No executable mprotect or additional executable mapping request was observed in this workload. This supports the interpreter/static-corlib design; it does not prove target permissions or every C# workload. See `cap10-exec-ready/`.

Signal handling requires exclusive ownership of Mono-affected process actions during the lifecycle and cleanup on the initialization thread for its mask. A distinct replacement SIGPIPE action is retained and reports failure rather than being overwritten. POSIX provides no atomic compare-and-restore; identical concurrent replacements are not detectable. Fatal/partial Mono initialization is not claimed recoverable. The host dispatch tests invoke the genuine runtime dispatch directly; they do not prove OS fault delivery.

#1/#3 remain ZCode's work and go last under your existing integration/restart-test rules. #13 remains its documented downloaded-DLL compatibility project. GPT made no console calls, main/candidate/frozen edits, full game PRX build, package, deployment or external messages.

## Packet and evidence

`GPT-7-10-MAC-READY-V2-PACKET-2026-10-13.zip` contains the complete authoritative stage, current target binaries, wider PAL archive, component sources/tests, licenses, build recipes and selected evidence. Its companion manifest describes exactly what is included; companion archive receipt records SHA256, CRC and every member hash verification. Large historical link-only ELFs, duplicate archives, host runtime/build trees and SDK installations are retained locally and excluded. Historical source/receipts are preserved with their original paths and hashes, not relabeled as final inputs.
