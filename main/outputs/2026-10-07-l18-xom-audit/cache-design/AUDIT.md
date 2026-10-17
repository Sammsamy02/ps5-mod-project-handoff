# l18 local feasibility and cache design audit

Feasible with a small immutable cache acquired in `module_start`, before the initializer worker starts. This audit changes no runtime source and proves no console success. The full image must be authenticated; the proposed cache is a separate static code-signature proof, never a successful live-memory read or heap admission proof.

The retained decrypted image is **194,241,424 bytes**, SHA-256 **`3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`**, matching `SeedTable::kImageSha256`. The local receipt parses all14 ELF64 little-endian program headers and validates each PT_LOAD's file bounds. All42 frozen l17 files still match `FREEZE.json`. Receipts: [retained image](receipts/RETAINED-IMAGE.json), [frozen l17](receipts/FROZEN-L17.json), [existing public I/O](receipts/PUBLIC-IO.json).

The l16 console result remains a controlled refusal: the fresh32-byte FName read at game+`0x23d590` / `0x63d590` returned EFAULT14 with0 bytes; no native constructor or constructor roundtrip occurred. Its successful VQ code-region result was not logged. Static PF_X evidence does not establish the current mapping's protection.

**Function and signature inventory**

| Pin | Current startup behavior | Linked address | File offset | Bytes |
|---|---|---:|---:|---:|
| `fname_ctor_wchar` | Active exact signature, then native ctor and pool roundtrip | `0x23d590` | `0x241590` |32 |
| `static_construct_object_internal` | Active Phase5 address-class binding; identity/ABI explicitly unverified | `0x490aa0` | `0x494aa0` |25 in staged `shadow_arm` |
| FName staged pattern | Inactive shadow scanner; covered by the longer active32-byte pin | `0x23d590` | `0x241590` |16 |

Both named functions are in PT_LOAD0: file offset`0x4000`, linked vaddr0, filesz=memsz`0x7479b1c`, ELF flags1(PF_X without PF_R/PF_W). Their exact retained bytes match their profiles; staged FName16 and SCO25 each have exactly one executable-file hit. Cache the active FName32 **and** the known staged SCO25 profile pin before worker start (57 bytes total). Retaining SCO's pin does not activate a new SCO ABI check or the shadow scanner.

The other staged signatures are code cross-references to **data seeds**, not unnamed function profiles: GUA primary16B has1 hit at linked`0x2b25f0`; corroborating16B has2 at`0x2b212a` and`0x2b2399`; all RIP targets equal`0x93ba110`. GMalloc bootstrap-store16B has1 hit at`0xb660`, RIP target`0x930add8`. They are inventory evidence, not active runtime scans. GUObjectArray/GMalloc validation and all FName pool/heap reads remain live data checks. Optional slots currently have no pinned signature and must not acquire invented profiles.

Concrete source locations (frozen originals):

- [FName exact32-byte pin](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/FNameAbiProfile.hpp:5), [active signature gate](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/NativeFNameChecks.hpp:12), [FName call/bind](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/UnrealInitializer.cpp:1046), [SCO binding](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/UnrealInitializer.cpp:1084).
- [All staged patterns](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/shadow_arm.hpp:68), [inactive shadow fallback](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/UnrealInitializer.cpp:864), [generated seed labels/hash](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/SeedTable.h:11).
- [Existing streaming SHA primitives](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/shadow_module_source.hpp:74), [whole-image identity comparison](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/shadow_module_source.hpp:289), [ELF parser/reference mapping](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/shadow_copy.cpp:112).

**Identity, base and file inputs actually available**

The generated game profile's disk source is `/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin`, with the same full SHA and exact byte length. This is a **configured retained decrypted-image path**, not the kernel's observed game executable path. The old shadow-arm patch's `/data/homebrew/townfall-ps5-ue56-v1/decrypted/eboot.bin` is a documented placeholder and must not be used as evidence. Neither path is currently opened by frozen l17 initialization. No additional runtime JSON/configuration open is needed: generate these constants from the existing profile for the candidate.

Startup has the supplier's nonzero `facts.observed_base`, identity source, VM scan result, optional real module-info segments, and a successful conversion's `resolved_image_base/end`. The selected supplier assigns VmScan's actual chosen base into `observed_base`; on this target the wrapper's span-gated B/C/D/E byte correspondence validates the candidate`0x400000`. It is not a newly discovered OS module handle or a live whole-image SHA. The module-info route remains unavailable unless its existing checks actually succeed; firmware handle-domain equality is explicitly disabled. Missing executable-path observation stays missing; configured`/app0` is a directory input only.

At startup require `observed_base != 0`, `observed_base == SeedTable::kBaseObserved`, successful conversion, `resolved_image_base == observed_base`, and authenticated ELF extent matching`0xbc602c8`. At initializer consume also require the retained MainExe `module_view_base()` to be nonzero and equal to the cached observed base and pin. Fresh whole-file identity and these base prerequisites apply to **both live and disk code-signature paths**, as the parent's latest directive requires. The existing seed path accepts a zero module view as unavailable; that exception must not authorize either new code-signature route. This evidence associates the pinned image, verified data anchors, translated PT_LOAD geometry and loaded MainExe view; it cannot prove that unobservable live instruction bytes have never been modified.

Source locations: [profile disk path/hash/size](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json:18), [loaded-image capture](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/runtime_start_bridge_r14_diag_spanpub.cpp:790), [same-capture supplier base](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/live_module_facts.geometryfix.cpp:299), [selected conversion geometry](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp:179), [initializer MainExe view](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l17-local-lifecycle/units/UnrealInitializer.cpp:710).

**Proposed startup preparation**

Place preparation after `bridge_capture_loaded_image(supplier->facts())` and successful geometry conversion, before `StartActivatedRuntimeWithAck` launches its worker. Acquire all entries atomically or refuse startup; do not publish a partial cache. The bridge already imports public `sceKernelOpen`, `sceKernelPread`, `sceKernelClose`. Public saved declarations are lines339/349/81; existing l17 native nm evidence confirms all three imports. No raw syscall, memory-permission change, whole shadow allocation, scanner activation or late image open is required.

1. Clear unpublished/stale cache while startup is serialized and no owner is running. Start one steady-clock60,000ms deadline. Open the configured image once, read-only and O_NOFOLLOW; retain a local sentinel owner for that fd.
2. Read64KiB-or-smaller chunks through the same open fd and hash **exactly those returned bytes**. Parse the bounded ELF header/program-header bytes from that same hashed stream. The retained header table fits in the initial chunk. If a general parser needs more header bytes, its fixed cap and deadline must remain explicit.
3. Validate ELF64/LE/machine/header-table layout; use checked arithmetic or subtraction bounds for `phoff`, entry count/size, `p_vaddr+memsz`, `p_offset+filesz`, slide additions and all requested ranges. Require filesz<=memsz and exactly one executable PT_LOAD fully containing each prefix in its **filesz**, not merely memsz. Reject ambiguous/overlapping mappings, gaps, BSS/zero-fill prefixes, overflow and truncation. Use `p_offset + (linked_vaddr-p_vaddr)`; never a universal+`0x4000` rule. LOAD8's actual delta is negative`0x324000`.
4. Copy only requested window bytes from the chunks being hashed into temporary fixed-size entries, including windows spanning chunks. Hold them as untrusted until the exact194,241,424-byte stream and final EOF/one-byte excess check succeed, the whole SHA matches, and every expected prefix matches. Do not authenticate one pass and reread signature bytes from a later pass. Short reads advance by actual byte count; no-progress/error refuses within the same deadline.
5. Consume the fd sentinel before one close attempt on every exit. A close error is logged and never retried after numeric FD reuse. Publish only immutable metadata and57 bytes after complete success; carry no image descriptor into initialization. The transient additional fd while the64-sink bank exists is a real capacity risk, not a headroom claim.

Suggested immutable record: cache generation, observed/resolved base and source, authenticated hash/size/extent, checked PT_LOAD metadata, fixed entries keyed by named profile ID with linked address/file offset/full expected length/bytes, and the shared start/deadline timestamps. No lookup accepts arbitrary memory addresses as disk-readable data.

**Initializer consume policy and lifetime**

The getter checks l17's synchronized initializer-thread ownership **before** reading cache, generation, bank, nonce, capture, fault ledger or telemetry state. Cache publication precedes worker construction; entries remain immutable while that worker can run. `bridge_copyin_initializer_complete()` still retires unused bank descriptors immediately after FName signature, native ctor, roundtrip and binding, before Phase5. It preserves cache and existing fault/capture evidence. Failed-start RAII clears unpublished/published candidate cache; supported stop joins the owner before clearing any cache or fallback release. Restart prepares a fresh generation. Owner leave revokes access without permitting foreign diagnostic reads.

For the signature gate, preserve the existing `known_fault(address,size)` terminal refusal before selecting evidence. Query the exact code address once and log its actual result/bounds/protection/flags. Disk proof is eligible only if VQ is **Available**, fully contains the entire prefix, is committed(flag0x10), has EXEC(bit4), lacks READ(bit1), and matches the authenticated executable PT_LOAD translated by the validated base. A conservative cache policy also refuses WRITE(bit2), so mutable executable memory is not presented as read-only XOM. If real module-info segments exist, log them distinctly and refuse contradictory segment evidence; otherwise label module-info unavailable and the segment source authenticated-PT_LOAD plus validated base, never fabricate a successful kernel segment result.

If VQ has READ, retain the existing fresh live-copyin and owned-capture comparison, even if cached disk bytes match. EFAULT, partial copy, prefix mismatch, existing overlapping faults, heap/data failures and uncommitted/non-executable/invalid mappings never select disk proof. An unavailable VQ may retain the old live-copyin behavior; it cannot select disk proof. Current single-region prefix containment is sufficient and conservative; do not silently widen it with a permissive multi-region test.

Return a distinct source label such as `authenticated-disk-XOM-signature`. Do not route disk bytes through `ops.capture()`, the generic pool reader or live-readability service. Logs must distinguish static image-prefix agreement from fresh live-prefix agreement. Native FName ABI, out8/align8/non-case-preserving profile, FNAME_Find, behavioral ByteProperty call, roundtrip and id checks remain unchanged. SCO's cached25-byte pin is retained; this audit does not authorize activating staged scans or inventing function ABI validation.

**Timing policy requested by parent**

The existing **5,000ms sink-acquisition budget stays separate and unchanged**: it is checked before each acquisition iteration, which can then complete open, required unlink and diagnostics; cleanup is not preempted. It is not a hard bound on all of `module_start`.

Cache acquisition, hashing and initializer admission/signature/roundtrip use **one60,000ms absolute steady-clock deadline starting at cache acquisition**. Export that original start/deadline into the initializer; do not create a fresh `now+60000` at Phase3. Intervening context construction/worker scheduling and settings I/O consume the same elapsed budget. Check before and after open/read/hash chunks, validation/publication and signature work; expiration refuses and closes resources. Avoid retry sleeps during image reads except a deliberately bounded error policy under that same deadline. No300s startup relaxation is proposed. Public OS calls, scheduling and mandatory close/release cannot be preempted, so an in-flight call or cleanup may finish after expiry. A blocked OS call has no hard wall-clock bound.

The local Python hash pass took roughly0.12s on this Mac; this is only a host measurement using Python's optimized hashlib. It predicts neither console I/O nor the portable C++ SHA implementation. The earlier shadow document's sub-second console estimate is not an observed budget receipt. Startup read access to the real profile path and console whole-file timing remain untested.

**Focused test cases for implementation review**

| Case | Required outcome |
|---|---|
| Real retained image and profile | All entries match, exact full SHA/length/ELF mapping recorded;57 bytes published; file closed before worker starts |
| Wrong SHA/prefix; SELF instead of ELF; empty/truncated/extra image | Startup refuses; no partial cache, late open or leaked/second-close fd |
| PH table/count/fileoffset/vaddr/slide overflow; filesz>memsz; overlapping/gap/BSS prefix | Checked refusal; no zero-fill substitute |
| Short reads, no-progress, normalized SCE error at each read position, close error with FD reuse | Correct byte accounting/error log; one close obligation consumed; fault ledger unchanged |
| Whole prefix in committed non-readable executable mapping | Disk source selected before any copyin; logs VQ/segment/base/hash; native behavior checks still run |
| Readable executable mapping with live match/mismatch/EFAULT/partial read | Existing fresh live path; all failures remain terminal, cache cannot rescue them |
| VQ unavailable/unmapped/degenerate/partial range/uncommitted/noEXEC/WRITE | No disk proof; preserve old live policy only where it was already allowed |
| Absent/mismatched actual startup/resolved/module-view base or stale cache generation | Refusal; constants cannot impersonate observations |
| Preexisting overlapping fault, foreign initializer thread | Refuse before query/cache read or unsynchronized diagnostic mutation; no fault erasure |
| Deadline expires during preparation; delayed worker consumes remainder | Startup/initializer share the original start/deadline; no fresh60s allowance; mandatory cleanup still finishes |
| Startup failure, initializer exception/refusal, success retirement, stop/restart | l17 once-only retirement preserved; no image fd retained; clear cache only when no running owner; no stale generation |
| Normal + ASan/UBSan + TSan actual bridge/initializer/owner harnesses | Existing l17 suites stay green; new cache/getter thread and lifecycle checks use real extracted code; native full TUs/import/relocation checks before full pipeline |

Current risks requiring later evidence are console file access/performance, temporary extra FD capacity, VQ protection semantics and optional module-info availability. This disk route deliberately cannot establish a fresh live instruction-byte match. Source inspection and local retained-image checks establish a viable implementation seam, not the menu, engine binding, loader permissions or console success.

Exact audit command, from `/WORKSPACE/.codex/worktrees/translation-core/what`:

```sh
python3 outputs/2026-10-07-l18-xom-audit/cache-design/inspect_inputs.py > outputs/2026-10-07-l18-xom-audit/cache-design/receipts/RETAINED-IMAGE.json
```

The audit script is a read-only offline inspector, not runtime cache implementation. Receipt inputs include hashes of the selected l16 supplier/conversion sources and build command manifest, frozen l17 sources, public API declaration and prior console report. No network/console contact, source/config/mod/shared-checkpoint change, executable packaging or deployment occurred.
