# GameEngine copied-vtable fallback — design only

No hook is implemented, selected, installed, or included in the l23 candidate by this document. This is a possible later carrier for the existing game-thread queue. The current work remains the actual R3/Lua queue no-op self-test.

## Established scope and evidence

The authenticated Townfall image SHA256 is `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`. The retained named-class proof and independent Tick audit bind the GameEngine primary address point to linked VA `0x8477708`, and Tick to byte displacement `0x310` (entry 98), relocated target `0x2fac8c0`. The Engine base's same slot reaches an assertion naming `UEngine::Tick`. The GameEngine constructor installs its primary vptr on the same unadjusted receiver that was passed to Engine construction. The observed Tick entry preserves the SysV receiver in RDI, bool in ESI and float in XMM0; the intended typed entry has the logical signature `void(UEngine*, float, bool)` and requires independent compiler ABI verification before use.

`tools/measure_vmt_design_inputs.py` freshly checked the saved image and all dynamic relocations at the two immediately preceding words and Tick slot. At this address point the raw `[-2]` and `[-1]` words are both zero and have no dynamic relocations. Tick's raw file word is zero and has a relative relocation to `0x2fac8c0`; copying raw image function words would therefore be wrong. These are saved-image facts, not current live values. No complete table extent, active engine instance, current vptr, or safe mutation opportunity has been established. Exact source hashes and byte windows are in `evidence/vmt-design-inputs.json`.

## Table representation and minimum ABI conditions

Under the selected Itanium-style ABI, the object's vptr points at an address point inside the table allocation. The word at address-point `[-2]` is the signed offset-to-top and `[-1]` is the typeinfo pointer. Preserve their exact admitted runtime bit patterns, including a null typeinfo value when this build uses one. Do not fabricate a typeinfo object or reinterpret null as a missing required live object. The copied object's vptr must address the replacement's first virtual slot, not its allocation beginning: for an independently proved two-word prefix, that is replacement allocation plus 16 bytes.

Those two words are a minimum prefix, not a universal complete layout. Before installation prove whether this exact primary table has earlier vcall/vbase entries, secondary-table relationships, construction tables or other ABI metadata that any valid operation can address relative to this address point. Copy all required negative metadata and the complete valid positive slot range. Refuse an unknown range. A run of executable relocations, the next constructor vptr, a null word, or the highest currently used slot does not by itself prove the boundary. A 99-entry copy through Tick alone is insufficient: other virtual calls and destructor slots must remain valid after the object vptr changes. Do not copy a secondary address point while assuming the primary receiver convention.

A complete source/symbol/table-group argument must establish the prefix length, positive slot count and object subobject placement. Arithmetic is checked for underflow, overflow, alignment, and readable byte ranges. The replacement must remain pointer-aligned, readable throughout its lifetime and initially writable while populated; publishing a fully prepared immutable copy is preferable. Allocating executable memory is unnecessary because the only new code pointer names an already loaded PRX entry with the admitted target calling convention.

## Admission before any data write

1. Reuse the actual image hash, load-base, build identity, selected profile and executable-segment gates. Convert linked addresses with the admitted base exactly once. Bind all captured values to an engine/session generation. A saved image is not a substitute for validating current data mappings and current object identity.
2. Obtain the active ticking engine through an admitted lifetime-aware route. A reflection class default object is not automatically the active engine; existing source's `GameEngine CDO/Instance` lookup is insufficient for this purpose. Validate readable object storage, object identity/class and lifecycle, the precise primary subobject, and the writable pointer-sized vptr slot. Do not infer lifetime from a readable address or cached object name.
3. Read the current vptr through the established guarded reader. Validate the entire copied table range, metadata, and every retained callable slot according to the proved table schema. A live foreign/subclass/customized vptr, unexpected offset-to-top, target outside an admitted executable module, or existing third-party swap requires a separately proved profile/chain; it is not silently overwritten.
4. Validate the live Tick slot and preserve that exact original callable pointer. For this profile it must match the admitted GameEngine Tick target after relocation; otherwise refuse. A readable table can point into execute-only text. Function identity may use the authenticated image/XOM admission route only under its existing mapping and identity conditions. No code-byte scan, permission change, or instruction write is required by this design.
5. Prepare the complete copy, replace only entry 98, establish owner/generation/retirement state, and verify the copy before publication. Never write the shared original vtable. Installation changes only the selected live object's data vptr after all gates pass.

XOM compatibility is conditional: the relevant object and table data must be safely readable, the vptr storage must be writable under authorized conditions, the complete bounds and ABI must be proved, and the original function pointer must remain callable. An execute-only code mapping alone neither enables this backend nor establishes its safety.

## Publication and thread safety

Installation must run at a verified engine synchronization point with a documented guarantee that no unsynchronized virtual dispatch or object destruction races the vptr mutation. Calling an aligned 64-bit store atomic at the hardware level does not resolve C++ data races with ordinary engine vptr loads. A compare/exchange against the admitted original pointer prevents clobbering a competing writer but is not, by itself, that synchronization guarantee. If no supported mutation/quiescence mechanism is available, leave the fallback uninstalled. Do not invent a game-thread identity from the installer thread to overcome this prerequisite.

State records the exact object identity/generation, original and replacement address points, complete copied range, original Tick pointer, callback registration and retirement state. Publish only fully constructed state with correct memory ordering for our own readers. A globally shared mutable singleton is insufficient where more than one engine lifetime or instance can exist. Retain a module owner while any caller can reach the replacement entry.

## Tick forwarding and queue behavior

Every entered replacement Tick forwards to the captured original Tick exactly once, with the same receiver, delta seconds and idle flag and the exact target ABI. It must call the saved pointer directly; calling the object's virtual Tick again would recurse. No guessed receiver adjustment, function-pointer/member-pointer conversion, or invented original target is allowed. Preserve platform nonvolatile registers, stack alignment, unwind rules and exception behavior through the target compiler.

Before enabling delivery, verify that the genuine Tick callback is running on the independently admitted engine game thread; the callback's name alone is not thread proof. Refuse queue work on a different thread. The wrapper uses the existing queue/dispatcher and lifetime checks. It neither clones pending Lua work into a second queue nor drains from initialization. If both R3 and Tick carriers are present, one shared drain guard and once-only work state prevent duplicate or reentrant execution. The chosen integration point is an explicitly reviewed pre-Tick drain, followed by original Tick; changing to post-Tick requires a reviewed semantic decision rather than accidental ordering drift.

Queue work is bounded and uses the existing error boundary. A self-test or callback error must not omit the normal original call or run it twice; catch only errors the queue contract says it owns. Preserve normal original-engine exceptions/termination behavior. Do not suppress allocation failures, memory faults, or foreign exceptions to manufacture a successful tick. Avoid per-tick allocator/logging work in the carrier, and limit diagnostics to state transitions. Paused, idle, blocked, shutdown or loading states may stop or alter tick delivery; the 30-second monotonic self-test deadline reports non-delivery and does not authorize a blind backend switch.

## Revert, destruction and module lifetime

Retirement first disables new queue delivery and invalidates its generation without dereferencing released Lua owners. At the supported synchronized lifecycle point, restore the original vptr only if the exact same live object still owns our replacement address point. If another writer has changed it, report ownership loss and do not overwrite that writer. If destruction has begun or object lifetime cannot be proved, do not touch the old object address.

Restoring the pointer is not sufficient to free the copy or unload the PRX. A caller may already have loaded the replacement vptr or function pointer before entering the wrapper. An active-callback count does not cover that interval. Require a platform/engine-supported quiescence or grace-period proof covering all possible readers, plus completion of all entered callbacks, before releasing the copied table, callback state, or executable module. A shutdown protocol must retire the swap before the object and underlying original code disappear. If that proof fails, refuse installation up front or retain the replacement resources/module through the bounded engine/process lifetime under an explicitly designed ownership policy; do not free them optimistically.

Revert preserves normal virtual destruction: all destructor entries in the copied table were preserved, but that fact alone does not supply an object-deletion notification or solve the teardown race. Such a lifecycle hook is a prerequisite, not an implementation detail to defer until unload.

## Relationship to the current R3 carrier

| Property | Existing R3 carrier/queue | Proposed copied GameEngine vtable |
|---|---|---|
| Delivery event | An admitted reflected carrier function invocation; harness observations are carrier invocations, not engine ticks. | A genuine admitted engine Tick on the chosen active instance. |
| Data mutation | Existing selected UFunction Func-pointer marshal path, with its own identity/ABI/lifecycle requirements. | Per-instance primary vptr swap to a full copied table; shared engine table retained. |
| Coverage | Depends on the carrier being invoked in the relevant game state. | Depends on actual virtual Tick dispatch through this object; direct/devirtualized calls bypass the copied table. |
| Ownership | Reuses current registered marshal/queue ownership. | Requires new proved table extent, safe publication, object lifecycle, retirement and module retention. |
| Current status | l23 no-op must exercise this actual dispatcher and report enqueue thread, drain thread/time and timeout honestly. | Design only; no delivery or cross-game support established. |

Neither mechanism is guaranteed for every game or engine build. A timeout in one does not prove the other is available. Preserve R3 as the current experiment; evaluate this fallback only after its missing prerequisites have concrete evidence and a separately reviewed implementation.

## Required future verification

Host controls should model metadata/address-point preservation, complete-table copying, integer bounds, exactly-once original forwarding, wrong-object/identity/slot rejection, ownership loss on revert, nested dispatch, queued-owner shutdown and a pre-entry reader racing retirement. Target compilation must verify the actual function type/calling convention and generated table addressing. A controlled native test then needs fresh read-only admission receipts, one authorized installation, genuine callback thread/arguments, one queue item drained once, continuing original engine behavior, synchronized revert, and teardown evidence. None of those future native results is claimed here.
