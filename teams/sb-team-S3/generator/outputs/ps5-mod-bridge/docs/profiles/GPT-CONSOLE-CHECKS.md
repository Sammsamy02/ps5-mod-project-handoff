# Console checks for GPT: what the console must confirm in the Townfall profile

2026-10-07. Written on the Mac from the generated profile (`tools/profilegen`, Townfall PPSA33286 02.003.000, executable sha256
`3b6cfad4...622fc`). **Nothing here has been run.** Every check is read-only: data reads and observation of what the runtime
already logs. No patches, no writes, no new code paths, no calls into game code that the runtime does not already make.

Conventions. `B` is the load base the runtime observes each launch (0x400000 so far, `lmf_extent_base_low`). Addresses are written
`B + label` (label = linked file vaddr). Code pages (LOAD0, `--X`) are execute-only on the console and refuse reads (igf4), so code
addresses are confirmed through data that points at them (vtables in RELRO, globals in BSS) or through the runtime's own
behaviour, never by reading code bytes. Read with the same mechanism the igf6/igf7 probes used, without the writeback step. "At
the menu" means after the game reaches its main menu, so the engine has initialised and objects exist. Each check says what the
reads should show for each candidate and what to change afterwards.

Order: values the runtime uses today come first (seed slots, then the support constants that gate them, then layout values it
binds), then values it does not use yet.

## 1. GMalloc (seed slot 3): 0x9350950 or 0x930add8

Candidates: **A** = `B + 0x9350950` (the generator: the variable `FMemory::Malloc` and `FMemory::Free` both load, 417 code
references) and **S** = `B + 0x930add8` (the current seed: the slot the lazy-create function stores into; also in the profile as
`symbols.gmalloc_base_slot`). The global instance is `I` = `B + 0x93089b8`.

Reads, at the menu: qword `[A]`, qword `[S]`, qword `[I]` (the instance's vtable pointer), and, if `[A]` is not `I`, qword `[[A]]`
(the vtable pointer of whatever `A` points at).

| Result | Meaning | Action |
|---|---|---|
| `[A] == [S] == I` | both name the same allocator; no wrapper | either seed works; switch to A anyway (it is what the engine's own FMemory uses) and keep S as the V1 cross-check |
| `[A] != 0`, `[A] != [S]`, `[S] == I` | the engine routes allocation through a wrapper at `[A]` (thread-safe, poison or tracking proxy) around the raw instance | seed **A**: UE4SS allocations must go through the same object as the engine's; seeding S bypasses the wrapper |
| `[A] == 0` at the menu | A is not GMalloc in this build | keep S; report the finding (the generator will then need a new method) |
| `[S] == 0` at the menu | S is not filled the way the seed derivation assumed | seed A |

Before the runtime binds slot 3, also note the time order: if the runtime arms slot 3 earlier than the menu, read `[A]` at that
moment too (a null A at arm time is a timing fact, not a wrong address).

## 2. GMalloc V2 vtable constant: 0x7ec26e0 or 0x7ec2e28

`kGMallocVtableLabel` in the current `SeedTable.h` is `0x7ec26e0`; the instance constructor at `0xafd40` stores `0x7ec2e28`
(`lea rax,[rip+0x7e130aa]` at `0xafd77`, found by the SeedTable emitter work; not re-verified). If the instance's vtable pointer is
not the constant, the runtime's V2 validator refuses a correct GMalloc.

Read, at the menu: qword `[I]`.

| Result | Action |
|---|---|
| `B + 0x7ec26e0` | constant is right; nothing to change |
| `B + 0x7ec2e28` | change `kGMallocVtableLabel` to `0x7ec2e28` (V2 would otherwise refuse) |
| anything else | the instance is not what the derivation assumed; hold slot 3 on A per check 1 |

## 3. FUObjectArray tail: five member offsets

The staged `MemberVariableLayout.ini` takes them from the UE4SS 5.06 Windows template; only the first 96 bytes of the live array
were ever read (igf6), so they were never measured. The generator reads them from this image's code:

| Member | Generator | Staged (template) |
|---|---|---|
| UObjectCreateListeners (TArray) | 0x58 | 0x68 |
| UObjectDeleteListeners (TArray) | 0x68 | 0x78 |
| PrimarySerialNumber (int32) | 0x90 | 0xB0 |
| bShouldRecycleObjectIndices (bool) | 0x94 | 0xB4 |
| total size | 0x98 | 0xB8 |

Read `0xC0` bytes at `B + 0x93ba110` at the menu, then again about ten seconds later while the game is doing something (moving,
opening a menu). A TArray here is `{data qword, num int32, max int32}`.

| Check | Generator layout holds if | Template layout holds if |
|---|---|---|
| listener arrays | at +0x58 and +0x68: `0 <= num <= max`, small (a few), `data == 0` exactly when `max == 0` | the same at +0x68 and +0x78 |
| serial number | int32 at +0x90 is positive and grows or stays between the two reads | int32 at +0xB0 does |
| recycle flag | byte at +0x94 is 0 or 1 and bytes +0x95..+0x97 are 0 | the same at +0xB4 |
| end of object | bytes from +0x98 belong to another object (do not satisfy the next member's shape) | +0xB8 |

Action: write the five values that satisfy the invariants into `MemberVariableLayout.ini` (`[FUObjectArray]`). If the runtime ever
registers create or delete listeners through these arrays, do not do so until this check passes: a wrong offset there would write
into the wrong member.

## 4. ProcessEvent: vtable offset 0x268, function 0x859830

Read, at the menu, a live UObject through the object array (`ObjObjects` is the TUObjectArray at `+0x10`, whose first member
`Objects` is the chunk table): `table = [B + 0x93ba110 + 0x10]`, `chunk0 = [table]`, `obj = [chunk0]` (the first `FUObjectItem`'s
`Object`; skip to the next item, `+0x18` in UE 5.x, if it is null). Then its vtable pointer `v = [obj]`, then qword `[v + 0x268]`.

| Result | Action |
|---|---|
| `B + 0x859830` | VTableLayout `ProcessEvent=0x268` and the function address are confirmed |
| another code address in LOAD0 | wrong slot; re-run the generator's vtable census against this object's class |

## 5. Name pool (seed slot 1): 0x9765ec0 and its layout

Read, at the menu: qword `[B + 0x9365ec0 + 0x10]` (`Blocks[0]`), then 6 bytes at that pointer. Expected: a non-null pointer to a
readable span; the first u16 header has `header >> 6 == 4` and bit 0 clear, followed by the ASCII bytes `None`. Also read the u32
pair at `+0x8` / `+0xC` (CurrentBlock, CurrentByteCursor): CurrentBlock is a small index (0 is valid), cursor below 0x20000.
Action: none if it holds (this is the runtime's own pool predicate); otherwise report the bytes.

## 6. FName constructor (slot 2) and StaticConstructObjectInternal (slot 4)

Code addresses: no read-only byte check exists (execute-only pages). They are confirmed by the runtime's existing seeded use:
when the Item Spawner loads, the runtime's log lines for the FName behavioural probe and for its first StaticConstructObject call
are the evidence. Record those lines; nothing else to do. ABI the generator found for the ctor: rdi = out FName*, rsi =
const char16_t*, edx = EFindName (callers pass 1; the body never reads it).

## 7. Load base

The runtime already logs it each launch (`lmf_extent_base_low`); confirm it is still 0x400000 on the run that tests these values.
If it differs, every address above is `B + label` with the observed B.

## 8. Retired heap envelope (no console check)

The current `SeedTable.h` still carries `kHeapLo`/`kHeapHi`, which the oracle marks retired. Confirm in the runtime source that
nothing reads them; if nothing does, the generated `SeedTable.h` writes an unbounded range (or omits them with
`--seed-heap-span omit`).

## 9. UObject member layout (bound for Lua today)

Generated (high): ObjectFlags `+0x8` (u32), InternalIndex `+0xC` (i32), ClassPrivate `+0x10`, NamePrivate `+0x18` (FName = u32
ComparisonIndex + u32 Number), OuterPrivate `+0x20`, size `0x28`; FUObjectItem.Object `+0x0` (medium).

Reads, at the menu: the object `obj` from check 4 and a second one, item `k` of chunk 0 (`[chunk0 + k*0x18]`, e.g. k = 5).

| Check | Holds if |
|---|---|
| InternalIndex | i32 `[obj + 0xC]` equals the item number it was read from (0 and k) |
| ClassPrivate | `cls = [obj + 0x10]` is a non-null data pointer; following `+0x10` from `cls` reaches, within three hops, an object whose `+0x10` points to itself (the `Class` class) |
| NamePrivate | u32 `[obj + 0x18]` decodes through the name pool to readable text: block `id >> 16` from `Blocks[]` (check 5), entry at byte `(id & 0xFFFF) * 2`, u16 header with length `header >> 6` |
| OuterPrivate | `[obj + 0x20]` is null or another object pointer (a package's outer is null) |

Action: if any row fails, stop binding the Lua object API and report the bytes; these offsets are what every `FindFirstOf` / property
read uses.

## 10. UStruct, UField, FField, FProperty and UFunction members

Generated: UField.Next `+0x28` (medium), UStruct.SuperStruct `+0x40` (medium), Children `+0x48` (medium), ChildProperties `+0x50`
(high), PropertiesSize `+0x58` (high), FField.ClassPrivate `+0x8`, Owner `+0x10` (medium), Next `+0x18` (high), NamePrivate `+0x20`
(medium), FProperty.ArrayDim `+0x2C` (high), ElementSize `+0x30` (high), PropertyFlags `+0x38` (medium), Offset_Internal `+0x44`
(medium), UFunction.FunctionFlags `+0xB0`, NumParms `+0xB4`, ParmsSize `+0xB6`, ReturnValueOffset `+0xB8`, Func `+0xD8` (medium).

Reads, at the menu, starting from `cls` (check 9):

| Check | Holds if |
|---|---|
| SuperStruct | following `[cls + 0x40]` reaches a class whose `+0x40` is null (UObject) in a few hops, and every name on the way decodes |
| PropertiesSize | i32 `[cls + 0x58]` is positive and not smaller than the parent's |
| ChildProperties / FField | walking `f = [cls + 0x50]`, `f = [f + 0x18]`: each `[f + 0x20]` decodes to a name, `[f + 0x10]` points back to `cls` (Owner), `[f + 0x8]` is a field class pointer |
| FProperty | for each property: ArrayDim `[f + 0x2C]` is 1 or small, ElementSize `[f + 0x30]` is positive, `Offset_Internal + ElementSize * ArrayDim` `<= PropertiesSize` |
| Children / UFunction | walking `u = [cls + 0x48]`, `u = [u + 0x28]` over a class with functions: for each function `Func = [u + 0xD8]` lies in LOAD0 `[B, B + 0x787c000)`, `NumParms [u + 0xB4]` is small, `ReturnValueOffset [u + 0xB8]` is 0xFFFF or below `ParmsSize [u + 0xB6]` |

Action: offsets whose rows hold can be promoted from medium; any that fail are removed from `MemberVariableLayout.ini` (the
generator already leaves low and unresolved ones out).

## 11. Engine tick: UEngine vtable slot 0x310, UGameEngine::Tick 0x2fac8c0

Reads, at the menu: find the engine object by walking the object array and decoding class names (`[obj + 0x10]` then its
NamePrivate) until the class name is `GameEngine`; then `v = [engine]` and qword `[v + 0x310]`.

| Result | Action |
|---|---|
| `B + 0x2fac8c0` | the slot and the function agree with the generated profile and the runtime's existing vtable fallback |
| another LOAD0 address | report it; the runtime's vtable fallback already binds from the live object, so nothing changes until reviewed |

## 12. GNatives: 0x93b8900

Read 256 qwords at `B + 0x93b8900` at the menu. Holds if the large majority are non-null and lie in LOAD0 `[B, B + 0x787c000)`, and
the same handful of addresses repeats (the default "undefined" native fills unused slots). Action: none if it holds (the runtime does
not use GNatives today); otherwise mark it unresolved.

## 13. FName::ToString: 0x23f150 (out-parameter form) and 0x23f0c0 (hidden return)

Code addresses: no read-only byte check (execute-only). They are confirmed when the runtime first converts an FName to text for a mod
(for example the Item Spawner printing an object name): record that log line. Nothing to do otherwise.

## Not checkable on the console with reads alone

Unresolved in the profile, each with its reason in `runtime-profile.json` `unresolved`: `UClass.Interfaces` (known 0x1d8 from static
analysis), `FProperty.CopyValuesInternal` vtable slot, `UEngine.LoadMap` slot, `AGameModeBase.InitGameState` slot, 23 internal UClass
members, four UStruct reference/state members, `UFunction.RepOffset`, five FUObjectItem build variants, and `version_map.pc_build`
(needs PC game files; see the review package).
