# l22 review packet — Mac-only, awaiting upload approval

The requested four-change l22 runtime builds and packages successfully. Two independently rebuilt affected-unit sets, complete runtime links and complete PRX packaging passes are byte-identical. No console contact occurred. This report does not claim that the menu, allocator calls, widget construction or spawning work on the PS5.

## Artifact and receipts

| Item | Verified result |
|---|---|
| PRX | `pass-1/modbridge_supplier_composition.prx`, identical to pass 2 |
| Bytes | **31,261,655** |
| SHA-256 | **eb32990807c942730c7c404482b0b57a74bd2f9ed218e10f35f0e0f899f80dec** |
| SELF magic | `4f153d1d` |
| SDK | `0x11000000` |
| Static package checks | **20/20 per pass** |
| Exact imports | **207**, no changes to names, NIDs, module/library identities or versions versus 885f43c6 |
| Constructor/destructor arrays | **238 / 2**, all modern and legacy entries preserved |
| Runtime object | 119,561,392 bytes; SHA `00820f3c18bce1d210bd57fc22114986a2d472e192403403b41f24acad4853a7` |
| Runtime compilation | **167 freshly compiled units per pass**, 207 unchanged units retained under pinned receipts; 374 selected units, 391 link inputs |
| Header invalidation | All 166 consumers of the changed SCO header rebuilt, plus the additional affected unit |

These are two identical incremental runtime builds with the complete affected dependency closure rebuilt, followed by two independent bootstrap/package builds. They are not two from-scratch rebuilds of every third-party library. `FINAL-RECEIPT.json` joins the build, package, import, source and unchanged-mod checks. Per-pass receipts and the full controllers are included in the ZIP.

## Exactly what changed

### 1. Authenticated FMalloc profile

All 17 exec-enabled byte offsets were added. Existing 69 member pins and seven virtual pins are unchanged. The profile remains bound to Townfall image SHA `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`; its new canonical digest is `1ca87defdcbc7ace7753ef904a1e8cb7cc211a6388628889a4206f93074f45b8`. Runtime and outer-bootstrap declarations carry this same digest.

| FMalloc method | Byte offset | FMalloc method | Byte offset |
|---|---:|---|---:|
| Malloc | 0x30 | TryMalloc | 0x38 |
| Realloc | 0x40 | TryRealloc | 0x48 |
| Free | 0x50 | QuantizeSize | 0x68 |
| GetAllocationSize | 0x70 | Trim | 0x78 |
| SetupTLSCachesOnCurrentThread | 0x80 | ClearAndDisableTLSCachesOnCurrentThread | 0x98 |
| InitializeStatsMetadata | 0xa0 | UpdateStats | 0xa8 |
| GetAllocatorStats | 0xb0 | DumpAllocatorStats | 0xb8 |
| IsInternallyThreadSafe | 0xc0 | ValidateHeap | 0xc8 |
| GetDescriptiveName | 0xd0 | | |

Two independent evidence families are retained: the official UE 5.6.1 declaration/TargetRules default and exact-header compiler probes; and the authenticated image's GMalloc dispatches, executable vtable targets and semantic anchors. The image provides ten dispatches at +0x30/+0x40/+0x50 and six semantic anchors. The remaining method names rely on the corroborated declaration order plus executable image entries; they are not seventeen independently identified function bodies. This confidence distinction, the actual-game macro override uncertainty and the rejected exec-disabled branch remain in the provenance. Promotion follows the user's l22 rule, without inventing missing values or claiming native calls were tested.

### 2. StaticConstructObject ABI and admission

The existing empty 0x30-byte callback placeholder has `alignas(16)`. Five assertions enforce callback 0x40, SerialNumber 0x70, SubobjectOverrides 0x78, size 0x80 and alignment 16 under the selected Prospero Shipping configuration. No nonempty callback implementation was added.

Slot 4 is now labeled **statically verified, empty callback only** after the existing authenticated cached signature check succeeds. Evidence remains the 114+86-instruction image comparison, RDI parameter-pointer/RAX object-pointer ABI and thirteen official-header field matches. This adds no live game-code read or new engine-call ABI. It does not claim a successful PS5 SCO call.

### 3. Exact 63-key startup preflight

The preflight evaluates already staged layout declarations before seeded engine calls, logs one complete block and inserts no defaults. It contains **49 startup-required keys, 12 conditional map keys and two deferred FText runtime discoveries**. All 49 required declarations are currently present and match their pins.

Missing startup-required keys stop after the complete table is printed. Missing conditional entries continue with the existing lazy exceptions. Known present values conflicting with authenticated pins still refuse. FText size/value-ops discovery remains a runtime step and is explicitly deferred. The full table is `PREFLIGHT-TABLE.md`. The generic 635-key inventory remains parked.

### 4. Runtime error and progress visibility

Runtime logging now precedes the existing UFunction, FText, SCO and property-set error paths, so a mod's `pcall` cannot hide the diagnostic. Original error objects, exception types, returns and cleanup remain unchanged. Logging is best-effort and cannot replace the original result if the logger itself fails.

Markers cover Lua main.lua completion, each RegisterKeyBind return with key/modifiers, callback entry, ExecuteInGameThread queue/drain, non-null widget/object construction and AddToViewport return. Existing pad borrow/first-state markers remain; new markers distinguish an L3+R3 edge, its allow/subscription/modifier decisions, and actual F7 enqueue. They preserve the original input conditions. AddToViewport is labeled from an already resolved lookup and adds no FName conversion or engine call for diagnostics. A void function returning is not proof of a visible menu.

There are **no UTF-8/ANSI pusher registrations, mod edits, configuration edits, game-file changes or unrelated runtime fixes**. Existing hooks and platform mechanisms are unchanged.

## Focused verification

| Check | Result and scope |
|---|---|
| Actual initializer preflight | 10 scenarios, ASan/UBSan: all 63 reported, multiple missing required entries collected, missing conditional entries continue, mismatches refuse, FText deferred, no map mutation |
| Actual SCO cache/admission block | 8 ASan/UBSan controls: identity-gated cached match accepted; prefix/base/execute coverage/deadline/fault/owner/cache failures refuse; no native call |
| SCO structure compiler contract | Original actual header fails alignment 8 versus 16; candidate passes all five assertions with the PS5 target compiler; generated IR shows align 16 |
| Diagnostics | 44 behavior assertions and 9 marker/order checks pass using actual extracted bodies and a real host Lua 5.4.7 VM; original logging/progress baselines fail the intended controls |
| Actual pad provider | 22 controls pass in plain and ASan/UBSan runs; original provider fails the missing-marker control; input semantics retained |
| Build command rewriting | All 374 selected compile recipes preserve their exact source/output arguments |
| Packaging checker regression suite | 11 tests pass, including import identity corruption and malformed/extended section tables |
| Final artifact | Two PRXs identical; 20 static checks per pass; exact import provider identities unchanged; 238 init and 2 fini entries retained |

Independent reviews checked diagnostics behavior and the profile/ABI integration. A separate final review verified all 374 compile argument rewrites, 167/207 fresh/retained unit counts, all 166 SCO-header consumers, matching runtime hashes and bootstrap profile staging. No introduced actionable defect remained. Review details are in `FINAL-DIFF-REVIEW.md`.

## Preserved build-controller failures

Three local build-controller issues were found and corrected before the final successful runs: an already-pinned signature header newly appearing in the selected dependency graph; compile recipes that placed the source before `-c`; and an omitted offline NuGet configuration in the first new package directory. Failed directories and logs remain preserved. They were not relabeled as successful and did not involve console execution. The controllers now record the exact admitted header hash, rewrite the exact source argument, and copy/pin the offline NuGet configuration. Shipping candidate source was not changed to work around these build failures.

## Original mod and next test

The retained original TownfallItemSpawner `main.lua` is unchanged, 21,415 bytes, SHA `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` (Nexus Townfall mod 95, file 218, v1.0). This is a local verification against the saved release/harvest artifact; the live console was not queried this turn.

**Next step: review this packet before upload.** After approval, use a fresh decrypt-off backup of the actual live PRX, replace only this PRX, verify full decrypt-off SHA/magic, start a NEW klog capture, then one user launch and L3+R3 test. No current console state is assumed. Harvest the preflight block and ordered runtime/input/widget markers before any relaunch.

Residual risk: newly reachable native allocator/widget/engine paths remain untested on PS5 and may fail or crash. Missing conditional keys are diagnostic gaps, not compatibility claims. The saved clean missing-key stop does not prove every other failure path safe. Menu rendering and spawning remain **unproved** until the approved console test.
