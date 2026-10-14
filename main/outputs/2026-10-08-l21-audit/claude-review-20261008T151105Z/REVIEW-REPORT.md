# l21 — remaining facts and build decision

**Mac-only audit. No new runtime/PRX build, upload, launch or console contact.** The requested all-measured startup/menu condition is not met. The current runtime, game profile and original mod are unchanged. This packet supplies the missing-facts tables rather than another candidate for a console test.

The prior console result remains: PID184 passed pool admission, the FName ByteProperty round trip and the declared build/layout gate, then stopped before Lua began on `Virtual FMalloc::QuantizeSize is unavailable, possibly unsupported in engine version`. That is a missing map entry, not evidence that the allocator function was called and failed.

## What is still missing

| Priority / path | Verified finding | Still missing before a justified startup/menu build |
|---|---|---|
| 1 — allocator/profile | Exact official 5.6.1 declarations under the documented Shipping default assign all 17 linked FMalloc methods. Every candidate target lies in executable image text; six independent semantic anchors agree. | Reviewed promotion into the authenticated profile and complete requirement gate. Actual compiler overrides were not captured: preserve both conditional layouts and image identity; do not call this an unconditional ABI for other games. |
| 2 — complete startup requirements | The frozen ELF contains 254 virtual and 435 member failure literals. Joining profile facts and additional readers produces 711 distinct keys; 76 are in the current profile, 635 remain unclassified/unmeasured. | Close the startup/menu reachability and measurement set, including inline FString/TArray allocation. Every retained requirement needs a measured fact or an evidenced, scope-specific unreachability reason. No such exemptions have been certified by this audit. **635 is not a count of menu requirements.** |
| 3 — StaticConstructObject | Saved image instructions match the authenticated executable. RDI parameter pointer / RAX object result and 13 field placements agree with the exact official declaration. | The selected callback placeholder gives the enclosing type alignment8, while the official inline-callback model requires alignment16. Actual selected caller alignment and exact target configuration are not measured. Runtime still labels slot4 identity/ABI unverified; static evidence must be incorporated into admission before a launch. |
| 4 — reflection / value conversion | Generic dispatch and top-level frame checks exist. The unchanged mod/helper uses 33 distinct reflected method names, with 14 value-shape and 9 direct-property paths enumerated. | Complete measured metadata-reader requirements; nested bounds, whole out-struct conversion, bool packing and array element handling. Generic runtime reflection is the intended mechanism; this does **not** require 33 hardcoded per-mod function recipes. |
| 5 — text and allocation ownership | FText uses Kismet conversion plus text-property copy/destruction. Kismet FName-to-string returns an engine FString; copying/growing/releasing it reaches allocator virtuals. | Active text/value-ops facts, native parameter-value cleanup on success/error, and bounds/ownership of the raw name-conversion frame. The mod ignores SetText errors, so a menu-creation marker would not establish working labels. Source-visible risks are not claims of observed leaks. |
| 6 — input / game-thread / lifetime | Six key registrations, the queued executor and LoadMap registration implementations are present. Four startup hook lookups are enabled; six exact branches are disabled by the saved settings. | Join all enabled dependencies to measured facts; preserve refusal and cleanup semantics. Queue execution, pad delivery, menu rendering and spawning remain future console checks. Their lack of prior execution alone is **not** being used as a circular precondition against building. |
| Spawn only | LoadAsset, LineTraceSingle/out FHitResult, actor array parameters, transform/deferred spawn and pickup writes are separately enumerated. | Their metadata, conversions and ownership must be validated before claiming spawn support. They are not all prerequisites for merely displaying the menu. |
| Separate upstream gap | UTF-8/ANSI value-pusher bodies exist, but their two dispatch registrations are absent, including in the canonical selected upstream revision. | Restore/verify these when addressing that API coverage. It is not established that ItemSpawner uses either property type, so this is not assigned as this launch's cause. |

## Allocator: two-source comparison

The user has no local engine checkout. Existing authorized GitHub access nevertheless supplied six narrow source files at Epic commit `6978b63c8951e57d97048d8424a0bebd637dde1d`, including the exact FExec/FMalloc declarations and the build-rule default. No engine checkout was created.

`bUseExecCommandsInShipping=true` is the documented default. With that branch, FExec contributes two destructor slots plus four Exec-family entries, and Malloc begins at byte0x30. The disabled alternative has an additional Exec overload and shifts allocator methods by8 bytes. Both branches are in the evidence; only the independently documented default matches the six saved-image anchors.

The key offsets are Malloc0x30, Realloc0x40, Free0x50, QuantizeSize0x68 and GetAllocationSize0x70. The [full 17-method comparison](allocator/REPORT-SOURCE-CORROBORATION.md) supplies exact source lines, both branches, image targets, confidence and limitations. Eleven identities rely on source order plus executable entries; six also have distinct semantic/body anchors. This is stronger than selecting a Windows table and is still not an unconditional, console-proven allocator call.

Scope for reuse: **UE5.6.1, matching FExec macro/layout branch, Itanium-family ABI, compatible engine build facts**. Image identity and per-game corroboration remain required. No Townfall address belongs in generic code.

## Full lookup inventory and collect-all status

The [targeted startup/menu join](mod-api/MENU-STARTUP-MISSING-JOIN.md) narrows the immediately actionable numerical omissions to four allocator keys: QuantizeSize, Malloc, Realloc and Free. Its 71 dependency rows deduplicate to 63 keys: 47 currently profiled, 14 absent and two FText runtime discoveries. Ten of the 14 absent keys are conditional (six disabled initializer branches and four optional-property dependencies); they have not been established as menu blockers or certified unreachable elsewhere. This bounded join is not the complete dynamic dependency closure. It separates missing offsets from constructor alignment, value ownership and delivery evidence.

[FULL-INVENTORY.tsv](inventory/FULL-INVENTORY.tsv) is the full 711-key review table, with JSON alongside it. [OTHER-MAP-READERS.json](inventory/OTHER-MAP-READERS.json) records direct map and macro readers beyond wrapper strings. Operator-named virtuals and parent-class member fallback literals are included. Generated table provisioning is distinguished from runtime lookups; retained sources are distinguished from actual selected translation units.

All 76 current profile values exactly agree with the compiled declarations. That validates selection consistency, not every fact's original measurement independently. New allocator candidates are **not** counted as already in the profile.

The offline audit reports all unresolved requirements together and catches the actual QuantizeSize omission. Eleven focused extractor/collect-all controls passed. **The deployed runtime startup gate has not been changed.** An offline inventory test is not a replacement for the requested runtime integration. The remaining reachability/measurement work prevents claiming that the next launch cannot encounter another missing fact.

## Mod/API review and version support

Read [mod-api/REPORT.md](mod-api/REPORT.md), [the official constructor comparison](mod-api/OFFICIAL-SCO-COMPARISON.md), and the accompanying API, reflected-method, value-shape, property and call-order TSV tables. The two narrow “unbound but failing safely” entries describe missing-key handling only; they do not certify the corresponding whole API. No mod API is newly console-proven by these audits.

The banner is compile metadata. The selected public revision `e3ba1016562d6c0868c410d0a71e88bfcdbf691b` is 1,152 commits ahead of stable v3.0.1; all 374 selected translation-unit source hashes match their receipt. [The upstream catalog](upstream/REPORT.md) distinguishes concrete 5.4/5.5/5.6 code changes already present from partial support and the missing string registrations. Exact introduction commits for the private engine implementation are unavailable; current source evidence is not presented as private historical proof.

The original Nexus main.lua remains SHA256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`. It was not edited to fit this runtime.

## Saved console context: kstuff

The saved ShadowMount line says `pause deadline reached for PPSA33286 pid=184 but kstuff was already disabled`. It establishes the observed state at that deadline, not who changed it or why. Available logs do not establish the cause. The PRX had loaded and executed; the recorded terminal runtime error is the absent allocator map entry. No new status check was made and no kernel change is proposed here.

## Verification and delivery boundaries

- Parent reran the saved-image allocator measurement: seven outputs matched the worker's measurements byte-for-byte. Repeatability is not counted as an independent semantic source.
- Parent reran the exact-header conditional layout probes: both passed; 17 source-assigned targets executable, six semantic anchors agreeing. These are host Clang/LLVM-IR probes with explicit dependency stubs, not a Prospero runtime build.
- The inventory includes byte-parser versus system-strings agreement, profile-versus-compiled-declaration agreement, and 11 focused controls.
- Saved SCO/ObjectInitializer instruction validation covers 114 + 86 matching instructions. It supplies static ABI evidence, not a native constructor result.
- No double runtime build, packaging gate or upload receipt is claimed for l21, because no l21 runtime candidate was built.

The packet includes source-context copies, manifests and saved evidence for cloud review, excluding the game executable, PRX binaries and credentials. Earlier pre-header reports are retained as history and explicitly superseded by the source-corroboration and official-constructor reports. Persistent progress points here. The next work is to close the measured requirement set and source-level ABI/ownership gaps locally; another launch is not needed to establish these current omissions.
