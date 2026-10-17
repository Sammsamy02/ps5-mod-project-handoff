# REPORT — worker-required-slots-feasibility

Worker: worker-required-slots-feasibility, 2026-10-04. PROPOSALS ONLY.
Deliverables in this directory: `CLAIM.json` (first), `SLOTS-FEASIBILITY.md`,
`offline_anchor_scan.py` + `anchor-measurement.json` (the local measurement),
this `REPORT.md` (last). Workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what` abbreviated `W:`.

## Question

Given worker-tick-witness-design's B4 (the "PS scan timed out" throw, staged
`UnrealInitializer.cpp:630`, is driven by REQUIRED slots {0,2,3,4,6},
`MappedRequiredSlots=0x5d`) and the exec-only game text [0x400000,0x787c000)
live (igf6-proven): can the REQUIRED slots ever resolve, or is ps_scan
structurally dead here?

## Answer

**Structurally dead — decisively.** Five stacked gates, any one fatal:

1. **G0:** `mapped.text_base` is never supplied (stays 0) —
   `W:.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp:226-227`.
2. **G1:** the consumed scan-source ctor eagerly requires
   `permits_code(text_base)` (`ps-scan-provider-1/include/OwnedMappedImageCaller.hpp:57,33`)
   but the bridge registers readable spans `PLH::R` only
   (`worker-module-start-wiring/runtime_start_bridge_r11.cpp:483`) and no
   readable span is executable on this target → in the consumed composition
   `MakeNativeProgramContext` throws at bridge Stage 2 → `MRS_ERR_CONTEXT`
   (`runtime_start_bridge_r11.cpp:658-661`) before any scan runs.
3. **G2:** the Rust view gate demands an EXECUTE-flagged segment containing
   text_base (`ue4ss-native-mapped-candidates-1/src/mapped_image.rs:48-52`).
4. **G3 (measured):** the required-slot anchors are absent from the pinned
   image or unreadable — see table below; all xref/CFG steps read code
   (`xrefs.rs:12-23`).
5. **G4:** code-result slots 2/4 require `permits_code(result)` at admission
   (`OwnedMappedResolverCollector.cpp:51`) — impossible even for a correct
   constant.

A "working ScanGame" on this target therefore cannot mean "ps_scan succeeds";
it means "ScanGame completes without ps_scan". The staged initializer already
contains all the shapes needed for that; the proposed diff set is small
(SLOTS-FEASIBILITY.md §4b/§5).

## Per-slot verdict (task 4 table)

| slot | scans (resolver) | anchor span (measured) | consumer (staged UnrealInitializer.cpp) | resolvable without code reads |
|---|---|---|---|---|
| 0 GUObjectArray | 9 code idioms + rip4 (`guobject_array.rs:8-100`); object_cfg CFG fallback | CODE; **0/9 patterns exist image-wide**; diagnostics split readable/unreadable | `:895` → `SetupGUObjectArrayAddress` | constant-only YES (data slot); ps_scan NO |
| 2 fname_ctor_wchar | 5 UTF-16 module strings → code xrefs → code pattern (`fname.rs:13-96`) | DATA head (3/5 strings unreadable) + CODE body | `:887` → `FName::ConstructorInternal` | NO — code-bytes-required; constant must bypass G4 |
| 3 gmalloc | 14 code idioms + rip4; UTF-8 strings route (`gmalloc.rs:21-44,152-213`) | CODE; **0/14 patterns exist**; both UTF-8 anchors absent; "DeleteFile %s" readable but xref step reads code | `:891` → `GMalloc` | constant-only YES (data slot); ps_scan NO |
| 4 static_construct_object_internal | 4 code patterns + UTF-16 string → code xrefs/disasm (`static_construct_object.rs:11-95`) | CODE; **0/4 patterns exist**; string anchor readable, stranded | `:814` → `SetupStaticConstructObjectInternalAddress` | NO — code-bytes-required; bypass needed |
| 6 engine_version | code patterns + disassembled stores + UTF-16 strings/branches (`engine_version.rs:53-284`) | 1 raw code match (unreadable); 0 plausible strings (readable hits are character tables); 0 branch names | `:720-721` → `Version::Major/Minor` → layout publish `:863-876` | **YES — settings** `[EngineVersionOverride]` (`UE4SSProgram.cpp:857-879`), profile pins 5.6 |
| 10 tick (optional) | — | — | `:816-819`, fallback `:1352-1364` | YES — existing vtable fallback |

## Key findings

1. **Geometry correction (measured):** the retained eboot's LOAD0 is `--X`
   (116.48 MiB — the entire code segment, live [0x400000,0x7879b1c), the
   exec-only span) and LOAD1 is `R--` (10.24 MiB rodata, live [0x787c000,…),
   the readable window). The trusted live records match these flags exactly.
   TICK-WITNESS-DESIGN §2's table has the two flags swapped; its conclusions
   are unaffected.
2. **The shipped patterns are absent, not merely unreadable:** 27 of the
   required-slot instruction patterns have ZERO occurrences in the pinned
   185.2 MiB image. Even a fully readable-text target running THIS image would
   miss slots 0/3/4 through the pattern arm. This kills resolution route (a)
   "re-derive patterns offline" for the shipped resolver set.
3. **Readable data anchors exist but are stranded:** "DeleteFile %s" (live
   0x7d202d6), "NewObject with empty name…" (0x7d99084), GUO-ALLOC (0x7e16c9a)
   all sit in the readable window, but every consumer of those anchors proceeds
   to scan/decode code bytes.
4. **Slot 6 has a real zero-scan route:** `[EngineVersionOverride]` settings set
   `Version::Major/Minor` directly and install a no-op `version_finder`
   override, removing bit 6 from the required mask — the only required slot
   with an existing non-code route.
5. **Recommended path (full diffs in SLOTS-FEASIBILITY.md §4b/§5):** settings
   override for slot 6 + a staged-initializer seed table for slots {0,2,3,4}
   (constants derived offline against the sha-pinned image, in-game base
   anchor 0x400000, data-side guards for the two data slots, honest
   identity-unverified labeling) + gated DoScan arms + the existing tick vtable
   fallback. Blast radius: one staged initializer unit + one ini file; failure
   mode without constants is unchanged (loud early throw).

## Method and bounds

- Sources: consumed composition copies (provenance per
  `worker-composition-4-fix/build_runtime_composition4_fixed.py:27,75,94`),
  staged initializer `worker-query-s2-arm/staging/UnrealInitializer.cpp`,
  Rust resolver crate `ue4ss-native-mapped-candidates-1`,
  C++ collector/caller `ue4ss-native-mapped-candidate-selection-1` and
  `ue4ss-native-ps-scan-provider-1`, bridge r11/r12, settings sources.
- Measurement: one local read-only mmap scan of `decrypted-eboot.bin`
  (sha256 `3b6cfad4…`, 194,241,424 B) applying the exact Rust pattern strings
  and string anchors per required slot; script `offline_anchor_scan.py`, raw
  output `anchor-measurement.json` (per-pattern match vaddrs, span
  classification, live readability).
- No console contact, no text-read probes, no nested subagents; no frozen or
  wiring file modified; all diffs are proposals.

## Non-claims

- No seed constant is derived or claimed correct; values in the diffs are
  placeholders for the proposed bounded derivation task.
- No claim which exact failure shape the current binary exhibits at runtime
  (G1 predicts Stage-2 `MRS_ERR_CONTEXT`; the B4 timeout shape presumes
  G0–G2 pass); the verdict is identical under either.
- No claim about /data throughput, scan latency, or memory costs; none of it
  is spent on the recommended path.
- Critic-style quality scores are out of scope; this is feasibility evidence
  with file:line citations throughout.
