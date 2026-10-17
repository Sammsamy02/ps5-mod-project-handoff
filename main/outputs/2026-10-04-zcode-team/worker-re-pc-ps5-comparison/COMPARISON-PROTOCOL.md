# COMPARISON-PROTOCOL.md — PC acquisition + PC-vs-PS5 comparison protocol for the Item Spawner operation set
(worker-re-pc-ps5-comparison, 2026-10-04. Case B deliverables 2+3. Nothing here has been
executed against a PC image because none exists locally (INVENTORY.md §4); every PC-side
value is TO-COLLECT, never assumed.)

## 1. Purpose and honesty bounds

Goal: when a PC Townfall executable becomes available, fill the SAME slot table the PS5
side already fills (SEED-TABLE model), producing a correspondence table per operation:
`operation | PC evidence | PS5 evidence | difference | adaptation required`.

Bounds: no capability claim; no claim the two builds share addresses, layout, or even
allocator flavor; the PS5 column below restates retained derivations with their recorded
provenance classes (validated / recorded / armed / derived-static) and was not
re-verified by this worker.

## 2. Acquisition protocol (what to obtain, how to record it)

1. **Files** — the PC shipping executable (e.g. `Townfall.exe` / `<Game>-Win64-Shipping.exe`)
   PLUS its companions for version cross-checking: the main `.pak` / `.utoc` / `.ucas`
   set and, if present, the store's build manifest. Executable alone is acceptable but
   the manifest lets us pin the title update.
2. **Version match law** — the PS5 pinned image is sha `3b6cfad4…3622fc`, UE **5.6**
   (validated-static serialized version; revision/patch unknown per profile caveat).
   Prefer a PC build of the SAME title update as the PS5 capture. If unobtainable,
   proceed anyway but the version delta is FIRST-CLASS output: every non-matching
   anchor is reported as "version-divergent" rather than "platform-divergent" until
   proven otherwise. Never merge the two failure classes.
3. **Placement + receipts** — a NEW capture directory under
   `.superpowers/native-observations/` (e.g. `townfall-pc-executable-<date>/`) containing
   the files, a `capture.json` in the same shape as the PS5 one (`remote_path`/origin,
   expected/actual bytes, sha256, complete flag), and nothing else. Read-only intake;
   never convert in place; keep originals byte-exact.
4. **Provenance notes** — store of origin, acquisition date, and any build id visible in
   the executable's own resources; recorded in `capture.json`, not assumed.

## 3. Tooling (what exists on this machine)

| tool | status | role |
|---|---|---|
| lief 1.0.0 (python3) | READY | structural diff: ELF program headers/segments/DT_RELA vs PE headers/sections/imports(IAT)/exports/relocs; entry point; auth/hash quirks |
| capstone 5.0.7 (python3) | READY | targeted disasm at pinned addresses on BOTH sides (x86-64 either way) |
| Ghidra | `re-tools/ghidra.zip` (may still be downloading) | function-level RE; free fallback for BinDiff (below) |
| BinDiff | **ABSENT (commercial) — do not wait for it** | — |
| Ghidra Version Tracking / FunctionID | free, ships with Ghidra | THE fallback: create a Version Track session PS5→PC, run function matching (hash-based + sim), manually confirm correspondences at the pinned/anchor addresses; export with BinExport for sharing |

Working pattern proven by the retained derivations: lief/bytes + capstone + manual
xref census found every PS5 constant without Ghidra; the same style works on PE and
must remain the floor (Ghidra accelerates but is not load-bearing).

## 4. Pass 1 — structural diff (lief, mechanical)

Expected-by-construction differences to record first (they explain everything else):
- **Format**: ELF64 (e_type 65040, PS5) vs PE32+; section table absent on PS5 side
  (offset beyond EOF, no symbols) vs PE section table (probably stripped-ish shipping
  symbols too — record what exists).
- **Relocation model**: PS5 has NO absolute pointers in data — 1,661,599 R_X86_64_RELATIVE
  entries at DT_RELA (file 0x932F430); vtables/pointer arrays only materialize after RELA
  reconstruction. PC PE uses base relocations (.reloc) + IAT; absolute pointers ARE in
  data. Consequence: the PS5 vtable-chaining method (GMalloc derivation step 2) has NO PC
  analogue — on PC, scan .data/.rdata directly for the vtable pointer.
- **Load environment**: PS5 in-game maps text EXECUTE-ONLY (0x400000–0x787c000 prot 0x4)
  and shifts by live base 0x400000; PC host reads its own text freely. Consequence: PC
  validators can be code-reading where PS5 validators had to be data-side only.
- **Allocator**: PS5 uses FMallocBinned3 (console default, 4 UTF-16 literals + constants).
  PC Win64 default allocator flavor is a TO-COLLECT fact (Binned3 and Binned2 both
  plausible for 5.6) — determine it empirically, do not assume.

## 5. Pass 2 — the prioritized comparison list (PS5 evidence already in hand)

Priority order = the Item Spawner's actual call order (extracted from
`TownfallItemSpawner/Scripts/main.lua`, 746 lines): lookup (FindFirstOf/
StaticFindObject) needs (1)+(2); construction (StaticConstructObject + `class(...)`)
needs (3); reflected calls (`K2_GetActorLocation`, `AddToViewport`, `SetVisibility`,
Kismet/GameplayStatics defaults) need (4); (5) GMalloc underpins initializer seeding.
`ExecuteInGameThread` (line 695 — the mod's measured F7 blocker) needs the L2 tick
backend, whose PS5 status is tracked separately in ADAPTER-FRAMEWORK.md §2.

| # | operation | PC evidence (TO COLLECT — anchors that work on PE) | PS5 evidence (IN HAND — provenance) |
|---|---|---|---|
| 1a | GUObjectArray singleton | census method (identical to PS5): count `lea rip→X` refs; the PS5 census measured **1565** lea refs (gmalloc stage-1 reproduced it) — a PC count in the same order of magnitude corroborates the right global. Corroborate via FUObjectArray init: `ObjConstants::GUObjectArrayInitialPageSize`-style constants, first-chunk alloc, and the GObjects label referenced from `FObjectInitializer`/`StaticAllocateObject` regions | GUA linked `0x93ba110` → runtime `0x97ba110`; **validated in-game** (window gate + GUA anchor); layout A, 71,959 objects, 2 chunks (worker-checkpoint-reconcile CURRENT-STATE:7). StaticFindObject/FindObjectSlow path NOT yet derived on PS5 — note as PS5-side TO-DO the PC comparison should inform |
| 1b | FName resolution / FNamePool layout | anchors: UE5 `FNamePool::Init` interns the literal `"None"`, `"ByteProperty"`, `"IntProperty"` immediately after pool init (find these three wide literals, xref the init function); layout confirmation: store to `[pool+0x10]` (Blocks[0]), blocks area `pool+0x10040`, entry ids at `pool+0x14040`, hash probe stride `shl 6` (64-byte buckets), block size in the allocator call | FNamePool linked `0x9365ec0` → runtime `0x9765ec0`; interning core `0x242470` LEA-refs the singleton; init `0x23a5d0` (`mov [rbx+0x10], rax`, interns None/ByteProperty/IntProperty); find `0x23c0b0` reads `[pool+0x10]`/`[pool+0x18]`; add `0x23c340` probes `pool+0x10040` with 64-byte buckets (**derived-static**, FNAME-VERDICT §4.3). Block size / field offsets beyond +0x18: PS5-side NOT fully enumerated — the PC layout diff fills exactly this gap |
| 2 | FName ctor (the mod's `FName(name)` path) | same shape census: wchar-in ctor with length walk + wide→narrow split + call into interning core; confirm via consensus windows (PS5 method: 3/4 cluster windows picked `0x23d590` over `0x23d8a0`) | ctor linked `0x23d590` → runtime `0x63d590`; bytes frozen `55 48 89 e5 53 50 48 89 f0 48 89 fb`; SysV sret ABI; chain →0x23d600→0x23e0d0→0x242470 (**derived-static**; fc1 exact-byte B1/B2 probe spec'd, pending). ABI NOTE for the diff: PC is Win64 x64 ABI — FName returns by hidden sret pointer in RCX, not RDI; that difference is mechanical but lands in every generated binding |
| 3 | Object creation: StaticConstructObjectInternal / SpawnActor chain / SetForSpawn | census (PS5 method): the FNAME_FIND-of-class-string sites cluster around construction — PS5 census had 1,918 xrefs to the class-name-string site with call-target consensus {0x490aa0, 0x493580, 0x493530}; on PC run the same census, then follow the winner into `FObjectInitializer` + `StaticAllocateObject` (which also corroborates 1a), and locate `SetForSpawn` + `UWorld::SpawnActor`→`SpawnActor` for the actor path | SCO linked `0x490aa0` → runtime `0x890aa0` (**recorded in-game**; pinned-label uniqueness owned by worker-seed-table-derivation); neighbors `0x493580`/`0x493530` from the same census (derivation.json sco_call_targets_per_xref). SpawnActor/SetForSpawn PS5-side: NOT yet derived — PC comparison fills this |
| 4 | script→engine call: ProcessEvent / CallFunction by name | anchors: UFunction lookup = GUA-walk + FName compare + `UFunction::Func` field deref; ProcessEvent is the vtable slot invoked from blueprint thunks — on PC find via `"Accessed None"`/script-call wrappers or Ghidra FID from the UE4SS Win64 release (we retain the actual UE4SS.dll — its cached offsets/patterns are legitimate PC-side priors, verify per build) | PS5-side: ProcessEvent address NOT derived; the R3 tier + vtable backend + PLH layer (LAUNCH11-MATRIX §0) are the *delivery* machinery, all unexecuted on target. This is the widest PS5 gap; PC evidence gives the target semantic before any PS5 probe spends budget |
| 5 | GMalloc / allocator | determine PC flavor FIRST: search UTF-16 literals `FMallocBinned3` / `FMallocBinned2` / `FMallocAnsi`; the retained literal list (PS5, 4 hits at linked `0x765b45a` free-msg, `0x77d1b8c` realloc-msg, `0x78788da` small-block-msg, `0x78f7c14` alignment-msg) corresponds to Binned3's validation messages — same texts expected on PC if Binned3; flavor constants `movabs 0xc000000000 / 0x1000000000 / 0xbfc0000000 / 0x7fc0000000` + pool params `0x12c00000 / 0x10000` are cross-platform Binned3 fingerprints; then GMalloc global = the unique bootstrap store (`mov [rip+X], rbx` after in-place ctor), consumers = pure vtable-dispatch thunks (`mov rdi,[rip+GM]; mov rax,[rdi]; call [rax+0x30|0x40|0x50]`) | GMalloc linked `0x930add8` → runtime `0x970add8` (**armed**; launch-10 line `slot 3 gmalloc bound at 0x930add8`); FMallocBinned3 instance global `0x93089b8`, vtable `0x7ec26e0` (RELA-reconstructed — the step with NO PC analogue, see §4), exactly 1 store / 11 consumer thunks image-wide (**derived-static-strong**, worker-gmalloc-derivation) |

Cross-cutting metric for the correspondence table: for each slot record (a) anchor
identity (same literal/constant?), (b) neighborhood shape (same instruction pattern?),
(c) census counts (1565 GUA lea refs / 1918 SCO xrefs / 11 GMalloc thunks — PC counts
go next to them), (d) derived address, (e) derivation-step parity. Differences get
classified version-divergent vs platform-divergent vs method-divergent (§2.2).

## 6. Output format when the PC image lands

`CORRESPONDENCE.md` with one row per slot per the §1 schema, plus `PC-PROFILE.json`
in the `townfall-ps5-ue56.profile.json` schema (same axes) so L2/L3 tooling consumes
both identically (ADAPTER-FRAMEWORK.md §4).

## 7. Non-claims

No PC file exists today; nothing in this protocol has run; no anchor is promised to
exist on PC; UE4SS.dll pattern priors are priors, not evidence, until verified against
the actual PC game build.
