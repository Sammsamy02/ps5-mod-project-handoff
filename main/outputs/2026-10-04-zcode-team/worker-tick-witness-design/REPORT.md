# REPORT.md — worker-tick-witness-design

Date: 2026-10-04. Scope: decisive viability answer for the serialized-ELF tick
witness (GAP-1) on the current target, the full scan-chain reading, the
trimmed-artifact byte math, and the fallback design. Deliverables in this
directory, in write order: `CLAIM.json` (first), `TICK-WITNESS-DESIGN.md`, this
report (last). PROPOSALS ONLY: no frozen file, wiring file, ABI, OwnedMemory
unit, or Rust crate was modified. No console contact, no nested subagents, no
text-read probes. All citations are `file:line` in TICK-WITNESS-DESIGN.md; the
consumed-source identities are proven up front (composition4-fixed build
script `:27,75,94` pins the tick-file-owner-1 copies; run-3 `.d` files pin the
ps-scan-provider-1 `OwnedMappedImageCaller.hpp`).

## What was done

1. Read the full chain: bridge r11 Stage-2 call site
   (`runtime_start_bridge_r11.cpp:545,623-624`) → 1-arg/2-arg
   `MakeNativeProgramContext` (`NativeLoaderContext.cpp:4-21`,
   `NativeLoaderContext.hpp:13,24`) → `OwnedMappedScanSource`
   (consumed copy `OwnedMappedScanSource.cpp:7-29`: eager `Create` :11,
   witness gate :18) → `PsScanBinding.cpp:3-6` →
   `OwnedMappedResolverCollector.cpp:39-63` (slot 10 = core resolver id 7,
   `MappedRequiredSlots=0x5d` hpp:25 excludes tick) →
   `OwnedMappedCandidateSelection.cpp:16-53` (policy semantics and side
   effects) → Rust `mapped_api.rs:50,57-61` + `game_loop.rs:4-10` →
   witness C ABI (`patternsleuth_tick_file_witness.h:28-48`) →
   `OwnedSerializedElfSource.cpp` (whole file) → `elf_tick.rs` (whole file) →
   staged `UnrealInitializer.cpp` (ScanGame loop :576-630, tick selection
   :1312-1378) → config surfaces (`UnrealInitializer.hpp:125,146`,
   `SettingsManager.hpp:125-126`, `UE4SSProgram.cpp:1230,1232`).
2. Measured the retained `decrypted-eboot.bin` (185.2 MiB) PT_LOAD table
   locally (section 2 of the design): five LOADs — R-- 116.48 MiB @vaddr 0,
   --X 10.24 MiB @0x747c000, RWX 18.49 MiB @0x7ebc000, RWX 1.80 MiB
   @0x913c000, --- 38.18 MiB @0x9633420 — plus DYNAMIC 4160 B. This is the
   measurement tick-address-validation explicitly did NOT do ("no phdr dump
   was consulted", its REPORT.md:93-94).
3. Answered task 2 decisively: NO (four independent blockers B1-B4, section
   0/3 of the design), including the exact failure cascade for the verbatim
   file and the reading-independence of the conclusion (both geometry
   readings fail).
4. Confirmed and corrected the r4 trim estimate: the honest minimum artifact
   is ≈147.0 MiB (79.4%) plus unmeasured RELA rows — the achievable trim is
   ~20% (drop LOAD8 + tail), not "low single-digit percent", and it is moot
   because no artifact can both resolve (needs text bytes) and match (needs
   those bytes readable live).
5. Designed the conditional integration (bridge r12 sketch: stage under mod
   root, bounded read, `SelectUniqueAfterNoMatch`, 2-arg call) with the
   eager-validation hazard called out, and marked it DO-NOT-APPLY here.
6. Delivered the fallback: settings-level `EngineTickResolveMethod=VTable`
   (zero-diff lever, `UE4SSProgram.cpp:1232`) using the already-staged
   vtable-fallback path (`UnrealInitializer.cpp:1352-1364`), plus the honest
   strategic finding that the scan timeout is caused by REQUIRED slots
   {0,2,3,4,6}, never by the optional tick slot — so a perfect tick witness
   could not have prevented the "PS scan timed out" throw
   (`UnrealInitializer.cpp:630`) anyway.

## Key findings (loudest first)

1. **The witness route cannot work on this target in any artifact form.**
   B1: both parsers refuse W+X PT_LOADs and this eboot has two
   (`OwnedSerializedElfSource.cpp:28`, `elf_tick.rs:169-170`) — and the C++
   check fires EAGERLY at context construction (`OwnedMappedScanSource.cpp:11`,
   `NativeLoaderContext.cpp:19`), turning staging into a guaranteed
   `MRS_ERR_CONTEXT` startup failure. B2: `matches()` (:78-91) byte-validates
   every RO/RX load's full span against LIVE readable memory; the 116.48 MiB
   RO load covers live `[0x400000,0x7879b1c)` — unservable. B3: the resolver
   needs the text bytes in the file (`elf_tick.rs:110,118-131,179-181`).
   B4: tick is optional (`MappedRequiredSlots=0x5d`); it is not the timeout
   cause, so the route could never have fixed the Stage-3 failure it was
   hypothesized for.
2. **Eager validation is an integration trap**: any mismatched/stale artifact
   fails startup, not the scan. Any future wiring must version-pin the
   artifact to the eboot build-id.
3. **Memory/latency cost if ever wired**: full-file read + internal copy +
   process-lifetime residency of the artifact (`OwnedSerializedElfSource.hpp:14`)
   plus up to ~48 MB transient relocation vector (2M rows × 24 B) + sort.
   Unmeasured on console; recorded as a design cost, not a claim.
4. **Trim correction**: mandatory ≈147 MiB; droppable ≈38 MiB (LOAD8) + ~23 KB
   tail, subject to a measured RELA-in-LOAD8 check (DYNAMIC vaddr 0xbc5f278
   already sits in LOAD8's span).
5. **Fallback**: vtable route is config-only, reads no code bytes, and is
   already the coded fallback; the real blocker for reaching it is the four
   required code slots — a separate deviation, honestly out of scope here.

## Honest limits

- The "exec-only text" premise is the trusted record set (r8:113-119, igf6,
  D2 §1.5); not re-probed per user directive. The design fails under either
  geometry reading, so the answer does not depend on it.
- /data read throughput, module_start latency, and free memory with a staged
  artifact are unmeasured.
- The RELA row count of the real DYNAMIC table was not parsed; the "≤48 MB"
  figure is the ABI cap, not a measurement. Any future trim claim must parse
  DT_REL/DT_RELASZ first.
- r8 bridge line citations (:398-421, :411-417) and
  `UnrealVirtualBaseVC.hpp:118-131` are taken from the D2/tick-validation
  reports' verified citations, not re-read in this pass.
- The predicted tick value 0x37AC8C0 remains tick-address-validation's
  derivation (their REPORT.md:33-37); nothing here verifies it live.

## Deliverables

- `CLAIM.json` (written first; TWD-1..TWD-6 with confidences)
- `TICK-WITNESS-DESIGN.md` (verdict, chain, decisive answer, byte math,
  conditional r12 sketch, fallback, non-claims)
- `REPORT.md` (this file, written last)
