# TICK-WITNESS-DESIGN — can the serialized-ELF witness resolve UGameEngine::Tick without reading exec pages?

Worker: worker-tick-witness-design, 2026-10-04. PROPOSALS ONLY — no frozen or
wiring file was modified; every diff below is a proposal for the controller.
Static source reading plus one local measurement (the PT_LOAD table of the
retained `decrypted-eboot.bin`); no console contact, no text-read probes, no
nested subagents. Every claim cites `file:line`. Workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what` is abbreviated `W:`.
The composed runtime's consumed sources are the tick-file-owner-1 copies, proven
by `W:outputs/2026-10-04-zcode-team/worker-composition-4-fix/
build_runtime_composition4_fixed.py:27,75,94` (`W/src/OwnedMappedScanSource.cpp`,
`W/src/NativeLoaderContext.cpp`, `W/src/OwnedSerializedElfSource.cpp` are the
staged unit sources; the compile line injects `-I W/include -I
.../ue4ss-native-tick-witness-1/include`), and by run-3's `.d` resolving
`ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp` for every
affected unit.

---

## 0. Verdict up front

**The witness route is NOT viable on this target — no artifact, trimmed or
otherwise, can make it resolve the tick. There are four independent blockers,
any one of which is fatal, and they compound:**

- **B1 (file refused outright, Stage 2):** the C++ parser refuses W+X PT_LOADs
  at construction. `decrypted-eboot.bin` has two W+X LOADs (measured below), so
  `OwnedSerializedElfSource.cpp:28` (`(flags&3)==3` → `refuse()`) throws
  "Serialized ELF does not match the retained mapped owner" (:9) inside
  `OwnedMappedScanSource` construction — which is EAGER
  (`OwnedMappedScanSource.cpp:11`), i.e. inside
  `MakeNativeProgramContext` (`NativeLoaderContext.cpp:19`) at bridge Stage 2
  (`runtime_start_bridge_r11.cpp:623-624`). Staging the verbatim file converts
  today's Stage-3 scan timeout into a guaranteed `MRS_ERR_CONTEXT` startup
  failure. The same W+X refusal exists on the Rust side
  (`elf_tick.rs:169-170`, `flags&3==3 → Error::Incomplete` → status 5).
- **B2 (matches() demands readable live text):**
  `OwnedSerializedElfSource::matches` (`:78-91`) walks EVERY non-writable
  PT_LOAD with file content and requires its FULL `p_filesz` span to be (a)
  covered by live image-view segments that are readable, non-writable, and —
  for X loads — executable (`:85`), and (b) byte-compared against live memory
  modulo R_X86_64_RELATIVE slots (`:88` → `source_matches` `:68-77` →
  `image.permits_data` `:69`). The eboot's RO LOAD spans live vaddrs
  `[0x400000,0x7879b1c)` at base 0x400000 — the region the trusted records
  call exec-only (r8:113-119; igf6 record). The first live view segment starts
  at 0x787c000 > cursor 0x400000 → `:86` returns false → ctor `:60` refuses.
- **B3 (the resolver needs the exec bytes in the file):** the Rust resolver
  scans `lea` xrefs only over flags&1 spans (`elf_tick.rs:110`) and proves CFG
  over real code bytes (`:118-131`, `:189-230`); dropping the text load from
  the artifact yields NoCandidate, keeping it re-triggers B2. A synthetic
  X-flagged load over readable data fails `:85` because live view segments
  carry no `PS_MAPPED_EXECUTE` (the bridge registers `PLH::R` only,
  runtime_start_bridge_r8.cpp:411-417; `segments()` maps exactly the lease
  bits, ps-scan-provider-1 `OwnedMappedImageCaller.hpp:25-27`).
- **B4 (strategic: tick is not what times out):** slot 10 (`gameengine_tick`)
  is OPTIONAL — `MappedRequiredSlots = 0x5d`
  (`OwnedMappedResolverCollector.hpp:25`) covers bits {0,2,3,4,6} only. The
  "PS scan timed out" throw (staged `UnrealInitializer.cpp:630`, loop `:610`,
  `SecondsToScanBeforeGivingUp` default 120 s,
  `UnrealInitializer.hpp:125`) is driven by the four REQUIRED code-CFG slots
  (GUObjectArray, FName ctor, GMalloc, StaticConstructObjectInternal), which
  miss identically. A hypothetically perfect tick witness would not prevent
  the throw; `PostInitialize` (where the tick is consumed, staged
  `:1312-1378`) is never reached.

Therefore the answer to task 2 is **NO, decisively**: the tick cannot resolve
through the witness without reading exec pages, and the witness's own
validation (`matches` → `permits_data`) is precisely a read of those pages.
Trimming does not help (section 4): the mandatory byte set is ~147 MiB of the
185.2 MiB file, and even a perfect trim still hits B1–B4. The recommended
fallback is the settings-level live-vtable route plus honest recognition that
the runtime on this target is blocked by the four REQUIRED slots, not by the
tick (section 6).

---

## 1. The scan chain, exactly as consumed (task 1)

1. `runtime_start_bridge_r11.cpp:623-624` calls the **1-arg**
   `MakeNativeProgramContext(std::move(converted.facts))` after
   `bridge_register_readable_ranges()` (:545; r8 body :398-421 registers only
   `kVmProtRead` mappings, clamped to `[0x400000, 0x400000+0xbc602c8)`).
2. `NativeLoaderContext.cpp:4-6` (consumed copy): the 1-arg overload forwards
   `serialized_elf = {}` to the 2-arg overload `:7-21`. `facts.scan_policy`
   defaults to `CandidateSelectionPolicy::OriginalOnly`
   (`NativeLoaderContext.hpp:13`); the LMF adapter never changes it
   (`lmf_to_native_loader_facts.hpp:68-69`).
3. `NativeLoaderContext.cpp:19`: constructs
   `OwnedMappedScanSource(snapshot, facts.mapped, facts.scan_policy,
   serialized_elf)` and stores it as `context.scanner_source`.
4. `OwnedMappedScanSource.cpp:7-12` (consumed copy): with **non-empty**
   `serialized`, `serialized_ = OwnedSerializedElfSource::Create(checked,
   serialized)` runs **eagerly in the constructor** (:11) — a mismatched
   artifact throws at context construction, not at scan time. With empty
   bytes, `serialized_` stays null and the witness is dormant.
5. `PsScanBinding.cpp:3-6` (consumed copy is the 6-line shim in
   ue4ss-native-mapped-candidate-selection-1): `ps_scan` →
   `scan_owned_source(config.NativeScanSource, config.ModuleSource, ctx,
   results)`.
6. `OwnedMappedScanSource.cpp:13-29`: `collect_with_evidence` builds a fresh
   `OwnedMappedImageCaller` (:15; provider variant:
   ranges from `ReadableRanges().at(MainExe)` :14-20, ReadLease admits R
   pages only, `segments()` maps lease R/W/X bits to wire flags :21-31) and
   calls `collect_owned_mapped_candidates(image, config, previous, policy_)`.
7. `OwnedMappedResolverCollector.cpp:43-46` runs core resolver ids 1..7 over
   `provider_slots[]{6,0,2,1,3,4,10}` (:39); slot 10 (tick) = id 7 =
   `ps_resolve_mapped_core_v1(&image.view(), 7, &value)`. Rust dispatch
   `mapped_api.rs:50` → `resolve_ugameengine_tick`
   (`game_loop.rs:4-10`): UTF-16 `"causeevent=\0"`/`"CAUSEEVENT \0"` scans
   over the SUPPLIED view bytes, `xrefs` → `get_root_function` →
   `ensure_one` (exactly one distinct root or error). Error mapping
   (`mapped_api.rs:57-61`, NoMatch line :60): "found multiple" → 4
   (Ambiguous); metadata malformed → 5; "expected at least one value" → 3
   (NoMatch). Admission:
   `value.reserved==0` and `image.permits_code(value.address)`
   (`OwnedMappedResolverCollector.cpp:48-54`; slot 10 is in the code-permit
   branch :51); otherwise RejectedAddress. A miss alone does NOT fail the
   scan: slot 10 ∉ `MappedRequiredSlots` 0x5d (hpp:25), `succeeded()`
   (hpp:23) ignores it.
8. The staged `UnrealInitializer.cpp` ScanGame loop: `config.gameengine_tick`
   set at :594, `ps_scan` at :616, retry `while(true)` :610, throw
   "PS scan timed out" at :630 after `SecondsToScanBeforeGivingUp` (120 s,
   `UnrealInitializer.hpp:125`). This is the loop D2 §4 predicted would
   consume the runtime after the D2 fixes — but its trigger is
   `required_missing_mask ≠ 0` over slots {0,2,3,4,6}, never slot 10.

### 1.1 The witness arm (dormant since r3)

`OwnedMappedScanSource.cpp:18` fires the witness only when ALL hold:
`serialized_` non-null AND `policy_ == SelectUniqueAfterNoMatch` AND
`config.gameengine_tick` AND `!out.original.image_error` AND slot-10 status
`NoMatch` AND `endpoint_status==3`. An Ambiguous scan (4) never reaches it.
When it fires: `OwnedSerializedElfSource::tick` (`:92-111`):

- `:93` re-runs `matches(image)` — any false → status 2;
- `:95-99` two-call `ps_ugameengine_tick_file_witness_v1` (capacity query
  then fill; status 6 = capacity, required for the first call);
- `:97` header sanity `version==1, flags==1, linked_tick≠0, 0<witness_count≤
  8192, reserved==0`;
- `:101-107` every witness revalidated against the LIVE image under the
  caller's lease: kind 1 (SOURCE_BYTES) via `source_matches`
  (`:102` → `:68-77`: `image.permits_data(runtime,size)` `:69`, then memcmp
  of live bytes vs file bytes at `file_offset(linked)`, R_X86_64_RELATIVE
  slots compared as `translate(addend)`); kind 2 (RELATIVE64, 8 bytes) must
  match a parsed R_X86_64_RELATIVE row (`:104-105`) and the live 8 bytes must
  equal `translate(expected_linked)` (`:106`);
- `:109` `translate(header.linked_tick, 1, true)` (code translate requires a
  load with `(flags&1) && !(flags&2)`, `:63`) + `image.permits_code(runtime)`.

File-side resolution is `ue4ss-native-tick-witness-1/crate/src/elf_tick.rs`
`resolve_with_witness` (:166-177) → `resolve_image` (:178-230): strings
`"causeevent="`, `"CAUSEEVENT "`, `"UEngine::Tick"`, `"Pure virtual not
implemented (%s)"`, `"GameEngine"`, `"/Script/Engine"` (`:179-181`);
`lea`-rip xrefs over X spans (:108-114); relocation-backed vftable roots
(:115-117); pure-virtual-stub CFG (:189-193); constructor CFG with
`("/Script/Engine","GameEngine")` call-argument constants (:195-205);
vftable shape ≥12 entries with zeroed 16-byte headers (:123-131); slot-index
correlation (:217-227); >1 candidate = Ambiguous (:228).

### 1.2 What the 2-arg `MakeNativeProgramContext` does with the bytes

`NativeLoaderContext.cpp:7-21`: validates `PS_MAPPED_IMAGE_ELF64` (:10-11),
builds the `ModuleSnapshot` (:12), `ValidateNativeProgramContext` (:18), then
constructs `OwnedMappedScanSource(...)` with the `const std::vector<uint8_t>&`
(:19). **It takes a const-reference and the scan source copies the vector**:
`OwnedSerializedElfSource` stores `const std::vector<uint8_t> bytes_` by
value (`OwnedSerializedElfSource.hpp:14`) and keeps it for the object's
lifetime — the scan source is retained by `GlobalConfig.NativeScanSource`
(`scan_owned_source` keeps `retained`, `OwnedMappedScanSource.cpp:38`), i.e.
process-lifetime. So the integration cost is: one full file read + one full
copy + permanent residency of the artifact, plus the ctor's `relocations_`
vector (every nonzero-type RELA row, `:53-55`; cap 2,000,000 rows `:51`,
24 B each ≈ up to 48 MB transient + sort).

---

## 2. Measured target artifact: `decrypted-eboot.bin` PT_LOAD table

Local measurement (read-only parse of
`W:.superpowers/native-observations/townfall-static-executable-2026-10-02/
decrypted-eboot.bin`, 194,241,424 bytes = 185.2 MiB; ELF64 LSB, e_type
0xfe10, 14 phdrs, phentsize 56, phoff 64). All PT_LOADs use p_align 0x4000
and satisfy `p_offset ≡ p_vaddr (mod 0x4000)`:

| i | type | file off | vaddr (linked) | filesz | flags |
|---|------|----------|----------------|--------|-------|
| 0 | LOAD | 0x0004000 | 0x0000000 | 0x7479b1c (116.48 MiB) | R-- |
| 1 | LOAD | 0x7480000 | 0x747c000 | 0x0a3e918 (10.24 MiB) | --X |
| 3 | LOAD | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 (18.49 MiB) | RWX |
| 7 | LOAD | 0x9140000 | 0x913c000 | 0x01cc5f2 (1.80 MiB) | RWX |
| 8 | LOAD | 0x930f420 | 0x9633420 | 0x262cea8 (38.18 MiB) | --- |
| — | DYNAMIC | 0xb93b278 | 0xbc5f278 | 0x1050 (4160 B) | RWX |

(Non-LOAD entries omitted; LOAD8 flags=0 with filesz==memsz; the build-id NOTE
lives at linked 0xbc5f250, the anchored window, inside LOAD8's vaddr span.)

Geometry at live base 0x400000: LOAD0 covers live `[0x400000,0x7879b1c)` —
the span the trusted records call exec-only (kve_protection=4; r8:113-119,
igf6 record). LOAD1 covers live `[0x787c000,0x82bf918)` — where all verified
readable anchor windows sit (r8:133-140, all ≥ 0x7c26782). The D2 §1.5
statement "first readable bytes start at 0x787c000" is consistent with either
reading of which static segment is unreadable; **the design below fails under
BOTH readings** (B2 fails on LOAD0 under the first reading; under the inverted
reading LOAD0 is still the unreadable one because it is the segment below
0x787c000 — the conclusion is reading-independent).

---

## 3. Decisive answer (task 2): NO — and here is the exact failure cascade

Staging the verbatim 185.2 MiB file:

1. `OwnedMappedScanSource` ctor :11 → `Create` → `OwnedSerializedElfSource`
   ctor. Header checks pass (:20-22: e_type 0xfe10 is accepted, EI bytes
   02 01 01 == 0x010102, machine 62, ehsize 64, phentsize 56, 14 ≤ 4096
   phdrs).
2. **Death at `:28`**: LOAD3 and LOAD7 have `flags==7` → `(flags&3)==3` →
   `refuse()` → `std::invalid_argument("Serialized ELF does not match the
   retained mapped owner")` propagates out of `MakeNativeProgramContext`
   (`NativeLoaderContext.cpp:19`) → bridge Stage-2 catch
   (`runtime_start_bridge_r11.cpp` maps it to `MRS_ERR_CONTEXT`). Startup
   fails; the scan loop is never reached.
3. Had the WX loads been re-modeled away, the ctor reaches `matches()`
   (:78-91): sorted loads walk LOAD0 first; `translate(0,1,false)` = live
   0x400000; the live view's first segment is at 0x787c000 >
   cursor → `:86` false → `:60` refuse. Same throw, same Stage-2 failure.
4. Had LOAD0 also been dropped, `matches()` then fails `executable` at `:89`
   only if LOAD1 was dropped too — but the Rust side needs LOAD0's rodata for
   all six required strings (`elf_tick.rs:179-181`, strings live in R-- LOAD0
   measured by `strings()` over flags&5 spans :104-107) — NoCandidate →
   status 3 → silent no-selection. And with the file intact the Rust witness
   refuses independently at `elf_tick.rs:170` (W+X spans → status 5).
5. Even if ALL file-side gates passed, every kind-1 witness — and
   `resolve_with_witness` records each R-X/R-- span's FULL bytes as one
   (`:171`) — is revalidated by `source_matches` → `permits_data` over the
   live image (`:102`, `:69`), i.e. the route by construction reads the
   116.48 MiB RO span that this target does not serve. **The witness is a
   read-amplifier, not a read-avoider.**

Where do the witness literals live? Kind-1 SOURCE_BYTES witnesses are whole
RO/RX load spans plus table headers and pointer slots (`elf_tick.rs:171`,
`:98`, `:126`); kind-2 RELATIVE64 witnesses are vftable/constructor pointer
slots. The string literals and pointer tables that survive into witnesses sit
in LOAD0 (rodata) and LOAD3/LOAD8 vaddr ranges — all below or above the
readable window, and `source_matches` validates each against live bytes via
`permits_data`. On this target at least one mandatory span is unservable, so
validation cannot complete. This also answers "readable data segments only?":
the witness machinery is DESIGNED to touch readable data only
(`permits_data`, `translate(...,false)`), but its mandatory span set includes
the unreadable RO span on this target — that is exactly why it cannot run.

---

## 4. Trimmed-artifact spec with byte math (and why trimming is moot here)

worker-tick-address-validation's REPORT.md:57-69 claim is **CONFIRMED and
sharpened**: the consumers read the full `p_filesz` of PT_LOADs, not just
witness bytes.

What the two parsers read from the artifact:

- C++ ctor: ELF header (:20-22), all phdrs (:24-31), `p_offset+p_filesz`
  within bytes (:28), PT_DYNAMIC ≤ 64 KiB at its own p_offset (:26), the full
  DT_REL/DT_RELA tables mapped vaddr→file via ALL loads (`file_offset`
  :65-67 has no flag filter), ≤ 2,000,000 rows (:51).
- C++ `matches()`: full `p_filesz` bytes of every `(flags&5) && !(flags&2)`
  load, byte-compared live (:82-88).
- Rust: `bytes[p_offset..p_offset+p_filesz]` for every LOAD/DYNAMIC
  (`elf_tick.rs:39-46`); strings over flags&5 spans (:106); xrefs over
  flags&1 spans (:110); CFG over code bytes (:118-131); relocations read at
  vaddr→span (:64-69).

Honest minimum for a HYPOTHETICAL fully-readable target (not achievable here):

| component | bytes | mandatory because |
|---|---|---|
| LOAD0 R-- | 116.48 MiB | strings + matches() byte-compare + RELA dir likely inside |
| LOAD1 --X | 10.24 MiB | xrefs + CFG + `executable` requirement (:89) |
| LOAD3 RWX | 18.49 MiB | xrefs scan it (X); W+X must be RE-FLAGGED (both parsers refuse WX) or dropped, losing its xrefs/CFG |
| LOAD7 RWX | 1.80 MiB | same WX handling; droppable if no xref/CFG/slot needed |
| DYNAMIC | 4,160 B | DT_REL/DT_RELA/DT_RELASZ/DT_RELAENT/DT_RELAENT tags |
| RELA rows | ≤ 48 MB at cap; actual size must be MEASURED from the DYNAMIC table before any claim | relocation-backed roots |
| phdrs + ehdr | 64 + 14×56 B | parse |
| **total** | **≈ 147.0 MiB (79.4%)**, plus measured RELA | |

Savings = LOAD8 (38.18 MiB) + everything beyond the last LOAD (~23 KB) ≈
**20% of the file** — NOT "low single-digit percent" as the r4 note
estimated, but also NOT decisive for any decision: LOAD8 is droppable only if
no consumed address (RELA row target, witness slot, CFG read) maps into
`[0x9633420,0xbc602c8)` — the DYNAMIC vaddr 0xbc5f278 already lives in that
span, so at minimum the DYNAMIC+NOTE region and any RELA rows inside LOAD8
must stay (they can be re-homed into a small synthetic load since offsets may
be compacted freely once `p_align` is lowered to 1; congruence is only
checked when `align>1`, `OwnedSerializedElfSource.cpp:28`, and the Rust side
checks no alignment at all).

On THIS target the spec is academic: any artifact faithful enough for the
resolver (B3) contains the LOAD0/LOAD1 spans, and `matches()` (B2) then
requires both readable live. **A trimmed artifact cannot exist that both
resolves and matches.** Additionally, cost at module_start (16 GB console,
game resident): 185 MB read + 185 MB copy + permanent residency + up to
~48 MB transient relocation vector + sort; /data throughput unmeasured —
none of it is spent if the route is not wired.

---

## 5. Conditional integration design (IF a future target had readable code — DO NOT APPLY TODAY)

Marked do-not-apply because of section 3; recorded because the task asks for
exact diffs if viable, and because the same shape would be correct on a
readable-text target.

1. **Staging role**: the artifact rides the existing mod payload under the mod
   root (the bridge already resolves `module_file_path`/`mod_root` while
   /data is reachable in module_start, `runtime_start_bridge_r11.cpp:618-620`
   comment "both paths must exist NOW, while /data is still reachable"). No
   loadlist role change; it is an inert data file to every other consumer.
2. **Bridge r12 diff sketch** (against `runtime_start_bridge_r11.cpp:623-624`):

```cpp
        try
        {
+           // r12 (PROPOSAL, gated by kStageTickWitnessElf && readable-text
+           // target): load the staged serialized ELF once, before Stage 2.
+           std::vector<uint8_t> serialized_elf;
+           if (kStageTickWitnessElf && !tick_witness_path.empty())
+           {
+               // Bounded read; the artifact is copied again inside
+               // OwnedSerializedElfSource (hpp:14), so peak = 2x size.
+               serialized_elf = bridge_read_file_bounded(tick_witness_path,
+                   kMaxSerializedElfBytes /* e.g. 200ull<<20 */);
+           }
+           converted.facts.scan_policy =
+               RC::Unreal::Platform::CandidateSelectionPolicy::SelectUniqueAfterNoMatch;
            RC::Platform::NativeProgramContext context =
-               RC::Platform::MakeNativeProgramContext(std::move(converted.facts));
+               RC::Platform::MakeNativeProgramContext(std::move(converted.facts),
+                   serialized_elf);
```

   Pass by const-reference (`NativeLoaderContext.hpp:24`); expect one internal
   copy (`OwnedSerializedElfSource.hpp:14`) — do not free the bridge vector
   early and do not mmap: the consumer copies and retains anyway, and mmap
   would only avoid one of the two copies while adding page-cache pressure.
3. **Policy choice**: `SelectUniqueAfterNoMatch` is REQUIRED for the witness
   (`OwnedMappedScanSource.cpp:18`) but has side effects: it also enables the
   slot-0/2/3 candidate endpoints (`OwnedMappedCandidateSelection.cpp:19-50`)
   after original NoMatch/status-3 — each with its own permits gates — and
   changes what `scan_owned_source` reports. On this target all of those
   endpoints scan the same unservable spans, so the flip buys nothing today.
4. **Hazard to design around**: eager validation means a stale artifact
   (image updated, artifact not) is a STARTUP failure (`MRS_ERR_CONTEXT`),
   not a degraded scan. Any real integration must version-pin the artifact to
   the eboot build-id and refuse to stage on mismatch, or the failure mode is
   worse than the disease.

---

## 6. Recommended fallback (what to do instead)

1. **Do not stage the witness and do not flip `scan_policy`** on this target
   (section 3; section 5.3).
2. **Tick route B needs no code and no readable text**: the staged
   initializer already prefers scan and falls back to the live vtable
   (staged `UnrealInitializer.cpp:1352-1364`; `VTable` preference exists at
   `:1365-1378`; vtable lookup `UnrealVirtualBaseVC.hpp:118-131` reads the
   live object's vtable pointer and the entry — data reads in serviced
   memory — then `ASM::resolve_function_address_from_potential_jmp`). The
   method is settings-driven:
   `config.EngineTickResolveMethod = settings_manager.Hooks.
   EngineTickResolveMethod` (`UE4SSProgram.cpp:1232`; default Scan,
   `SettingsManager.hpp:126`), so `EngineTickResolveMethod=VTable` in the
   UE4SS settings is the zero-diff lever, and the Scan default already
   falls back when the scan missed. Predicted fallback result: the
   vtable-derived address, with tick-address-validation's file-side
   prediction 0x400000 + 0x33ac8c0 = 0x37AC8C0 available as a cross-check
   constant in logs only (REPORT.md:33-37) — never as a patch source.
3. **Honest framing**: this unblocks ONLY the tick arm, and only if the
   runtime ever reaches `PostInitialize`. Today it cannot: the four REQUIRED
   code slots miss → `required_missing_mask ≠ 0` → "PS scan timed out"
   (staged `:630`) before `PostInitialize` runs. Resolving THAT is a
   separate, larger deviation (per-slot file-backed candidate routes inherit
   the same `matches()` blocker; making the four slots resolvable requires
   readable code or per-symbol non-scan routes). Until then, launch-7's
   expected honest shape stays: Stage-3 scan timeout, loud, D
   `runtime_start_ok` + S-only `failed` (D2 §5), with the tick witness
   correctly recorded as investigated-and-dead rather than dormant-pending.
4. If the controller still wants a tick-only experiment on launch-7, the
   minimal honest probe is settings-level only (`EngineTickResolveMethod=
   VTable`) plus a ScanOverrides experiment for the four required slots —
   the latter changes which failures surface downstream (GUObjectArray null
   etc.) and must be labeled diagnostics-only, not a runtime fix. That is a
   controller decision, out of this worker's scope.

---

## 7. Explicit non-claims

- No console contact, no target execution, no text-read probes; the eboot
  measurement is a local file parse of an already-retained artifact.
- The "exec-only/unreadable" premise rests on trusted records (r8:113-119,
  igf6, D2 §1.5); the design never depends on re-proving it and fails under
  either geometry reading.
- No claim about /data read throughput, module_start latency with a 185 MB
  artifact, or console free memory at that moment — all unmeasured.
- No claim that the vtable-derived Tick address is semantically the engine's
  tick beyond being the live object's own dispatch target (tick-validation
  REPORT.md:29-32 still applies).
- All diffs are proposals; no frozen file, ABI, OwnedMemory unit, or Rust
  crate was modified.
