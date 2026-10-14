# Runtime address contract remains an observation gap

The concrete source contract is now explicit: the proposed producer emits **final ELF VMA S**; the existing sidecar emitter copies that value unchanged into `symbols[].addr`; the crash consumer compares it with **RIP−X**, where X is the parsed `xotext.start`. The field is `addr`, not an independently defined or validated RVA. The sidecar loader performs no rebase. Local PRX hash pairing proves selected artifact identity, not the runtime coordinate convention or captured build.

Exact source anchors (paths relative to repository; hashes and before/after identities in SOURCE-RECEIPT.json):

- DynamicWriter.cs in retained package-A/pass-1/harness/tools/SharpProspero.Link:862,997–1015,1452–1475,1831–1848 sets textAddr=0, resolves retained symbols with final SectionAddr+Value, and writes executable PT_LOAD at text.Addr. The source-only staged Program.cs preserves final ELF VMA convention explicitly.
- l26_packaging_driver.py:482–494,505–518 emits final dynsym VMAs without subtracting executable segment VMA.
- phasea-tools/symbolize_crash.py:259–269 stores xotext start/end,342–357 loads addr unchanged,378–400 subtracts owner.start from RIP. Its lines47–49 assert an image-base law based on Townfall eboot; those comments do not supply an independent PRX loader/klog definition.
- crash_report.py:22–52 scopes symbol use to the explicitly selected module name and retains capture pairing UNVERIFIED. Module-name equality cannot close the coordinate or deployed-build observation gap.

For a uniformly relocated image, let B be actual load bias and E the linked VMA of the segment represented by xotext. These are conditional alternatives, not newly established runtime facts:

| Meaning of observed X | Expected runtime pointer A for symbol S | Correct consumer-space symbol |
|---|---|---|
| X is that segment's actual start, X=B+E | A=B+S | S−E |
| X is load bias, X=B | A=B+S | S |
| X is a subrange/another segment/rounded extent or lacks exact module association | Unestablished | Refuse until its relation is observed |

The saved current package-binding receipt establishes one executable load at E=0, filesz=memsz19497319, flags1. Thus S−E=S **conditionally**. This removes a numerical subtraction for this exact zero-VMA layout; it does not prove X=B, affine runtime mapping, selected segment or capture association. A nonzero-E image would expose the generic mismatch if X=B+E: for hypothetical E0x4000/S0x4010 the consumer needs0x10, not0x4010. No such different image or runtime failure was observed in this audit.

The package layout receipt binds current bare-original33a7c628…/bare-SDKcf48356f…/packageddc2d570…; all full identities are preserved in SOURCE-RECEIPT.json. Its exact wrapper preservation remains static package evidence. No whole binary was reread here. Genuine final-symbol producer input remains absent, and old capture/build association stays UNVERIFIED.

## Existing helpers are not independent loader evidence

CompatEmitter.cs:392–421 synthesizes `dladdr` for the current module using a RIP-relative reference to `__executable_start`, linked at zero; its reported pathname is hardcoded `/app0/eboot.bin`. It does not resolve its input address to arbitrary module ownership or query the OS loader. Using its output as independent path/base evidence would be circular. This is a limitation of using that helper for proof, not a newly reproduced runtime defect.

CompatEmitter.cs:425–460 also synthesizes unwinder headers instead of reading actual loaded headers. Its synthetic code-header flags5 differ from the actual writer's executable-only flags1. The deliberate synthetic description must not be mistaken for native module-segment observation.

The retained NativeLoaderContext.hpp:4–29 and PlatformModuleSnapshot.hpp:18–34 accept owner-supplied mapping/export facts; these interfaces do not discover or authenticate the native module's load bias. native-module-metadata/module_metadata.c:26–60 copies reported segments, and its header:37–47 explicitly leaves protection uninterpreted and fingerprint opaque. The native provider's read-one wrapper forwards an info call, while enumeration refuses with LIST_ABI_UNVERIFIED. None of these selected sources binds ELF VMAs, actual function addresses and klog xotext for this PRX. No OS loader or klog-producer implementation was established within this narrow source scope.

## Minimal next observation

1. Obtain a genuine normal-build producer receipt tied to its exact bare ELF and packaged SHA, plus authentic source/assembly build record. Preserve the package binder's layout attestation. Do not replace this with fabricated rows or a caller build label.
2. In one identified process/run, retain the actual module handle/mapping owner, exact deployed packaged build association, and independently observed runtime addresses A1/A2 of two known functions whose final VMAs S1/S2 come from that genuine artifact. Require A1−S1=A2−S2=B with checked bounds; this tests the affine relation for those anchors only. Do not use the synthesized dladdr pathname/base as independent evidence.
3. Bind that same module/run to the crash xotext X:end (and genuine mapped executable segment start T). Establish whether X=T and T=B+E, or whether X=B, rather than selecting either by convention. Record ambiguity/refusal if selected segment/base cannot be distinguished. Current E0 alone cannot distinguish the two meanings numerically.
4. Only after this observation select consumer coordinate S−E or S, verify the RIP/selected-function bounds and preserved package identity, then adapt the sidecar explicitly. Multiple/nonzero-VMA segments require their own established mapping; do not globalize the zero-VMA result. Actual capture/build binding remains a separate requirement.

No normalizer or runtime patch was added: the missing input is an exact segment/bias observation, not more arithmetic code. Fourteen small source/receipt hashes were stable before/after; source anchors and saved zero-VMA receipt were checked. No tests rerun, binary reads, compiler/build, console/network, originals or shared docs/manifest changes. This does not increase the scoped utility-test total.
