# Phase 4 review package: per-game runtime profile generator

Sections: the follow-up first (what changed on 2026-10-07), then the original package below it.

2026-10-07, branch `claude/townfall-item-spawner-hyj9eg`. Mac only: nothing here contacted the console, and nothing under
`launch11-combined/units` or `outputs/2026-10-07-l1*` was touched. Nothing is marked console-verified; only GPT's console test may.

## Follow-up (2026-10-07): what changed

Mac-only and host-tested, like everything here: no console contact, nothing written to any profile's `verification`, nothing
marked console-verified, and no edits in `launch11-combined/units` or `outputs/2026-10-07-l1*` (read-only use only).

| # | Asked | Done | Where |
|---|---|---|---|
| 1 | Generate the missing values | UObject / UField / UStruct / UClass / FField / FFieldClass / FProperty (+ subtypes) / UFunction members, vtable slots, engine tick (function 0x2fac8c0 and `UEngine` vtable slot 0x310), `FName::ToString` (0x23f150 out-parameter form, 0x23f0c0 hidden-return form), GNatives (0x93b8900). Each value has evidence and confidence; two agreeing channels give high, one gives medium, disagreeing channels publish nothing. No template value is emitted. All 72 values another static analysis of this image measured (`2026-10-07-l19-*`, `2026-10-08-native-frame-audit`, read-only, also not console-verified) are reproduced exactly. Oracle: **150 of 169 keys match** (was 146), 9 differ (the documented ones), 10 not produced (was 14) | `tools/profilegen/detectors/*`, README "Layout detectors" (99-row validation table) |
| 2 | Console checklist for GPT | 13 read-only checks in risk order, each with the reads, the expected result per candidate and the action | `docs/profiles/GPT-CONSOLE-CHECKS.md` |
| 3 | Runtime hand-off: JSON and SeedTable.h | `--emit-seedtable PATH` writes `SeedTable.h` in the runtime's v2 format (compiles as C++20); diffed against the runtime's current file (sha256 `d183e0c0...`, identical in `launch11-combined/units`, `l19-r4` and the 2026-10-08 menu package; not touched) | `tools/profilegen/emit_seedtable.py`, `docs/profiles/SEEDTABLE-DIFF.md` |
| 4 | Robustness | 63 tests: encrypted SELF refused before anything is read (exit 5); missing / malformed `param.json` (exit 2, nothing written); stripped and unusual ELFs; truncation at every point; header fuzzing; engine 5.1 / 5.3 / 5.4 / 4.27 read as themselves; one failing-assumption image per listed assumption (each value comes out unresolved or capped, never wrong). Fixes from these tests: bounded reads in `elf.py`, input checks before any write, and four over-confident claims lowered (`process_event` destructor slots, `FNamePool::Init` naming, `FUObjectArray` neighbours, engine-branch double count) | `tests/profilegen_robustness_tests.py`, README "Assumptions" |
| 4b | Another decrypted Unreal executable | **None on this Mac.** Every PS5 executable here is Townfall: the original (`3b6cfad4...`) and three copies ZCode patched (`worker-early-load/eboot-earlyload-A1/A2.bin`, `eboot-backup/eboot-original.bin`). The generator gives identical values for all three (they differ only in about 5.4 KB of SCE dynlib data), which shows it tolerates a modified image but is not a second game | - |
| 5 | PC version map | **No PC Townfall files on this Mac** (searched: Steam library, Downloads/Desktop/Documents, the ZCode and Codex trees; ZCode's `worker-pc-acquisition-pack` reached the same conclusion on 2026-10-05). The PS5 executable has no game build number in it (no `153829`, no `1.x.xxxxxx`, no `++UE5+Release` string); the in-menu version most likely lives in the packaged config. `version_map.pc_build` stays unresolved. Needed, below | - |

### SeedTable.h differences (from `SEEDTABLE-DIFF.md`)

1. `kGMalloc` label: current `0x930add8`, emitted `0x9350950` by default. `--seed-gmalloc base_slot` reproduces the current value
   (it is the profile's `symbols.gmalloc_base_slot`); the header always says which was chosen. Settled by console check 1.
2. `kHeapLo` / `kHeapHi`: current `0x8018c000` / `0x100118000` (retired per the oracle); the profile has no heap span, so the default
   writes an unbounded range with a comment; `--seed-heap-span LO:HI` or `omit` instead. Console checklist item 8.
3. `kGMallocVtableLabel` (`0x7ec26e0`): present in the current file only; the profile does not produce it, so it is not invented.
   The instance constructor stores `0x7ec2e28`, not `0x7ec26e0` (console check 2).
4. Derivation strings differ in content and keys for all four slots (ours carry `method`, `confidence`, `source`); the FName ctor
   prologue grows from 16 to 32 bytes with the same first 16.
5. Format: producer line, one extra provenance comment, more comment lines, and escaping (the old generator escaped only `"`; ours
   escapes `\` first, so the compiled string equals the JSON).
Equal: field order, guard, includes, namespace, the `SlotSeed` struct, `kBaseObserved`, `kFNamePoolLabel`, `kGMallocInstanceLabel`,
`kRequiredSetComplete`, `kImageSha256`, and the labels of slots 0, 2 and 4.

### What the PC version map needs

The main PC executable (`SilentHillTownfall/Binaries/Win64/<Project>-Win64-Shipping.exe`) of the patch that matches console
02.003.000, its store build identity (Steam `appmanifest_<appid>.acf` `buildid`, or Epic `Manifests/*.item` `BuildVersion`), and
ideally the same patch's `Config/DefaultGame.ini` `ProjectVersion` from the PC paks; and on the console side the same
`ProjectVersion` from the dump's packaged config (read on the Mac from files already copied, never from the console). Matching rule:
engine `5.6.1-0+++UE5` and the game's `ProjectVersion` must be equal; GNU build ids differ across platforms and are not evidence.
Drop the PC files where `outputs/2026-10-04-zcode-team/worker-pc-acquisition-pack/ACQUIRE.md` says; `verify_intake.py` checks them.

### Still unresolved (each carries its reason in `runtime-profile.json` `unresolved`)

`UClass.Interfaces` (static analysis says 0x1d8; no anchor identifies the member here), `FProperty.CopyValuesInternal` vtable slot,
`UEngine.LoadMap` slot, `AGameModeBase.InitGameState` slot, 23 internal UClass members, `UStruct` {ScriptObjectReferences,
ScriptAndPropertyObjectReferences, UnresolvedScriptProperties, StructStateFlags}, `UFunction.RepOffset`, five `FUObjectItem` build
variants, `version_map.pc_build`. Low (JSON only, not in the ini): `UStruct.PostConstructLink`, `FFieldClass` {ClassFlags,
DefaultObject, UniqueNameIndexCounter}. Coverage of the port's binding surface: 13 reflection classes measured, 15 more size-only,
11 untouched; for the Item Spawner's set, 51 of 52 reflection members are derived (all but `UClass.Interfaces`).

### Open questions for GPT (one list)

1. Hand-off: read `runtime-profile.json` at startup, or keep compiling `SeedTable.h` (both are produced now)?
2. GMalloc: seed `0x9350950` (what `FMemory::Malloc/Free` use) or `0x930add8` (current)? Console check 1 decides.
3. `kGMallocVtableLabel`: `0x7ec26e0` or `0x7ec2e28`? Console check 2.
4. Five `FUObjectArray` tail offsets: measured `0x58/0x68/0x90/0x94/0x98` or template `0x68/0x78/0xb0/0xb4/0xb8`? Console check 3.
   Does the runtime write through the listener arrays anywhere?
5. `kHeapLo/kHeapHi`: still read by the runtime, or retired as the oracle says?
6. Load base: fixed `0x400000`, or discovered per launch (the profile marks it a platform convention, medium)?
7. Which profile fields are mandatory for the runtime, and which can stay unresolved?
8. Where do console receipts live, and who writes `verification` (the generator and the app never do)?
9. The revision string `5.6.1-0+++UE5`: one `+` or three (it is a staging tag, not in the image)?
10. `UClass.Interfaces` (0x1d8 from static analysis) and the three unresolved vtable slots: does the runtime need them for the Item
    Spawner (it is not in its required set except `Interfaces`)?

### Test results

All Mac-only / host-tested, at the follow-up commit:

| Suite | Result | Skips and why |
|---|---|---|
| Generator: `tests.profilegen_tests`, `profilegen_robustness_tests`, `profilegen_seedtable_tests` | 330 run, 330 pass (205 + 63 + 62) | 0 here. They skip only when capstone, a C++ compiler, the Townfall image, the runtime's current `SeedTable.h` or the real `.self` files are absent: without the image, 16 Townfall tests skip |
| `make test-python` (all Python, includes the above) | 887 run, 885 pass | 2, both file-system limits of this Mac, unrelated to profiles: `titles_root_tests` "overlong names" (APFS refuses a 256-byte folder name) and "two appmeta folders differing only in case" (APFS is case-insensitive) |
| `make test-core` (C++) | 433 pass | none |
| `make test-ui` (all Playwright suites) | all pass: browse 150, generic selection 21, selection restore 22, selection basket 12, retained downloads 14, service records 10, paging 23, title paging, real-host integration | none |

## The five items

| # | Item | Status | Where |
|---|---|---|---|
| 1 | Profile format, based on UE4SS's per-game configs plus PS5 additions | **Proposal** for GPT to confirm (every field marked proposed; questions at the end) | `docs/profiles/PROFILE-CONTRACT-PROPOSAL.md` |
| 2 | Generator: decrypted executable + `param.json` -> profile with evidence and confidence per value | Built; engine-generic detectors, no per-game values in code (a test enforces it) | `tools/profilegen/` (+ `README.md`) |
| 3 | Validate on Townfall | 146 of 169 known values match; 9 differ (explained below); 14 not produced (none needed by a seed slot) | `tools/profilegen/README.md` "Townfall validation", `docs/profiles/TOWNFALL-ORACLE.md` |
| 4 | App: per-game profile status + Generate | Built in the desktop companion: none / generated / console-verified / stale / generating / failed | `docs/profiles/APP-INTEGRATION.md` |
| 5 | Tests + this package | See Tests | this file |

## Run it

```sh
cd outputs/ps5-mod-bridge
python3 -m tools.profilegen --eboot <decrypted eboot.bin> --param <sce_sys/param.json> --out profiles/<TITLE_ID>/<CONTENT_VERSION>
```
About 20 s for Townfall (194 MB). Writes `runtime-profile.json` (`modbridge.runtime-profile/1`, status `generated`), `ue4ss/`
(`VTableLayout.ini`, `MemberVariableLayout.ini`, `UE4SS-settings.ini` `[EngineVersionOverride]` fragment) and `evidence/`.
In the app, a game in "Your games" that is a dump has **Generate profile**; the host finds `<dump>/decrypted/eboot.bin` or
`<dump>/eboot.bin` (ELF only; an encrypted SELF is refused) and `<dump>/sce_sys/param.json`.

Input used for validation (already on the Mac, not downloaded from the console): the decrypted Townfall executable,
194,241,424 bytes, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (the hash ZCode's profile is pinned to).

## Townfall: the values asked for

| Value | Generated | Known | Match | Confidence |
|---|---|---|---|---|
| Name pool (FNamePool) | 0x9765ec0 (+0x8 / +0xc / +0x10 layout) | 0x9765ec0 | yes | high |
| ProcessEvent | vtable +0x268 (slot 77), function 0x859830 | 0x268, 0x859830 | yes | high |
| Shipping / Stats | Shipping (margin 60), Stats off (margin 12) | same | yes | high |
| Engine / build id | 5.6.1, `5.6.1-0+++UE5`, 04bb4330...69d5 | same | yes | high (patch/revision medium) |
| GUObjectArray | 0x97ba110 | 0x97ba110 | yes | high |
| FName ctor (wchar) | 0x63d590; rdi = out FName*, rsi = const char16_t*, edx = EFindName (loaded 1 at all 9 call sites, never read) | 0x63d590 | yes | high (ABI medium) |
| StaticConstructObjectInternal | 0x890aa0 | 0x890aa0 | yes | high |
| FMemory::Malloc / Free | 0x4b7a20 / 0x4b7a90 | same | yes | high |
| StaticFindObject | 0x88a680 | not pinned before | found | medium |
| Load base | 0x400000 | 0x400000 (in-game observed) | yes | medium (platform convention; `--load-base` sets it high) |

## Findings GPT should check before relying on them

1. **GMalloc.** `FMemory::Malloc` and `FMemory::Free` both load the allocator pointer from runtime **0x9750950** (vaddr 0x9350950;
   checked independently by disassembly: `lea r14, [rip+...] -> 0x9350950; mov rdi, [r14]` in both). ZCode's seed slot uses
   **0x970add8**, which the generator finds as the raw allocator slot the lazy-create function fetches and then stores into GMalloc
   (`symbols.gmalloc_base_slot`, medium). Both are in the profile; which one the runtime should seed is a console question.
2. **Five FUObjectArray offsets.** Generated: UObjectCreateListeners 0x58, UObjectDeleteListeners 0x68, PrimarySerialNumber 0x90,
   bShouldRecycleObjectIndices 0x94, total size 0x98. Staged (ZCode, from the UE4SS 5.06 Windows template): 0x68 / 0x78 / 0xb0 / 0xb4 / 0xb8.
   The oracle records that only the first 96 bytes of the live array were ever read, so the staged tail was never measured. In this image
   the allocation path iterates `[this+0x58]`, the free-index path tests `[this+0x94]`, and another BSS object starts at +0x98. If the
   runtime uses these members (listeners, recycling), confirm on the console.
3. **ZCode's hand-built profile** has errors the generator does not repeat (FName anchor addresses recorded as file offsets; runtime values
   labelled as file addresses; `free_vtable_slot: 8` vs +0x50 = slot 10). Listed in `TOWNFALL-ORACLE.md`.
4. **Upstream vs port VTableLayout format**: upstream `VTableLayout.ini` is an ordered name list; the PS5 port's parser requires
   `[NativeLayout]` + `Name=0xOFFSET`. The generator emits the port's form (byte-identical to the staged `[UObject] ProcessEvent=0x268` file).

## Open questions for GPT

Moved into the single list in the follow-up section above.

## App rules (no false "verified")

- `console-verified` only when the profile's `verification` is a pass whose `profile_digest` equals the file's own and whose executable
  sha256 equals the executable the host can read now; any other verification shows `stale`. The host never writes or edits `verification`.
- **Regenerate**: refused while a verification still holds; for a stale verification the profile is regenerated (`--force`) and comes back
  `generated` with no verification, as the proposal's section 5 says for a changed digest.
- Generated profiles live in `profiles/` (git-ignored). The native PS5 host does not generate or show profiles yet.

## Tests

- `tests/profilegen_tests.py`: 104 (94 on synthetic ELF fixtures; 10 on the real Townfall image, skipped when it is absent).
- `tests/runtime_profile_host_tests.py`: 57 host tests (every stale/verified combination, forged files, executable location, failures,
  timeouts, gating, regenerate rules) plus one end-to-end run of the real generator through the host on the real image.
- UI: 13 Playwright tests in `tests/browse_ui_tests.cjs` and a real-host flow in `tests/host_ui_integration.cjs`.
- Full suites at the final commit: see the commit message.

## Not done / limits

- Validated on one game only (the only decrypted Unreal executable on the Mac). Assumptions that may not hold elsewhere are listed in
  `tools/profilegen/README.md` ("Assumptions that may not hold on another title"); a failing assumption leaves a value unresolved, never guessed.
- `version_map.pc_build` is unresolved (no confirmed PC patch for 02.003.000; see `docs/ZCODE-NEXT.md` 2b).
- (Superseded by the follow-up: member layouts, `engine_tick`, `FName::ToString` and `GNatives` are now generated; see the follow-up section.)
- Generation runs on the Mac (desktop companion); the console app shows nothing about profiles yet.
