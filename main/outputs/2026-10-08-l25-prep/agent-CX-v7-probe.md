# agent-CX — zzdiag v7 probe (alternate-path resolution discrimination)

2026-10-08. Mac-local; no console access. Deliverables in
`outputs/2026-10-08-l25-prep/zzdiag-v7/`: `main.lua` (2487 B, sha256
`6c7624c5114f2cf83ddaec18fbcafa4285bd828e4fb55ef5643a973d044ae4ac`,
`luac -p` clean, ASCII-only), `LOADLIST-ROW.txt` (size-exact admission
sidecar), `loadlist-zzdiag-v7.txt` (545 B staged reference; identical to
v3's staging except the zzdiag main.lua row 2314 -> 2487).

## Problem

FindXOf returns empty for EVERY class in-game; names resolve and the walk
runs (user-perceptible lag). Need to discriminate: object-array reads
broken vs class/name compare broken vs the FindXOf exclusion filter
broken.

## Source-verified facts (read before writing, per assignment)

Port Lua surface — `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/LuaMod.cpp`
(identical registrations in l23/l26/r5-thread copies):

- `StaticFindObject` overloads (L1640-41): `#1 StaticFindObject(string name)`
  and `#2 (UClass*, UObject* outer, string, bool exact)`. v7 uses #1 only —
  no wrapper args needed.
- **Wrappers are never nil.** Not-found goes through
  `LuaType::auto_construct_object` which is documented nullptr-safe
  (L1651-52) and constructs an empty UObject wrapper
  (`LuaUObject.cpp:342-348`). Therefore `o ~= nil` is ALWAYS true for
  StaticFindObject/FindFirstOf; **the only resolution test is
  `pcall o:IsValid()`** (IsValid's generic binding is a nullptr-safe remote
  pointer check, `LuaUObject.hpp:185-193`; UEHelpers itself calls IsValid on
  invalid wrappers at boot, so it is proven safe on-console).
- `FindFirstOf(string)` (L1731) — same always-wrapper contract.
- `FindAllOf(string)` (L1766) — **returns real `nil` when nothing found**
  (L1815), a table otherwise. Different contract; used as the A1 rung.
- `IsKeyBindRegistered(key)` (L1834, true mods) — a trivially-working,
  UObject/FName-free sanity API. `GetModDir` is NOT registered in this port.
- `ModRef` global is set per-mod (L7476-77) — UEHelpers' shared-variable
  cache will function. `require("UEHelpers")` is proven on-console: the
  deployed TownfallItemSpawner main.lua:14 requires it
  (agent-AI-spawn-procedure.md).

Native resolution paths — port `unreal/src/UObjectGlobals.cpp`
(coherent-runtime source reconstruction):

- `StaticFindObject(nullptr,nullptr,"/full/path")` = global object cache
  (L233-240) -> `FUObjectHashTables::StaticFindObjectByPath` (hash /
  name-parts walk, L249-267) -> long-name fallback scan. **No
  `IsValidObjectForFindXOf` anywhere on this path** — it can return CDOs,
  UClasses, anything the path names.
- `FindFirstOf(name)` (L434-479) = `FName(name)` -> (hash-tables mode)
  `FindRootClassesByName` + class-instance searcher, or (no hash tables)
  full `ForEachUObject` walk with `Class->GetNamePrivate().Equals(name)` +
  super-struct range compare. **Every hit passes
  `IsValidObjectForFindXOf`** (L220-224: rejects CDO/archetype flags and
  anything IsA UClass/UFunction/UPackage/UEnum/UScriptStruct).
  `FindAllOf` (L506-590) shares the same compare + exclusion.
- Consequence: the perceptible lag + empty result is consistent with the
  no-hash-tables full-walk branch running to completion (reads OK enough to
  walk) while nothing matches — pointing at the FName `Equals` compare or
  the exclusion filter — OR hash mode returning empty instantly (no lag).
  Which branch ran is itself a discriminator v7 cannot see directly, but
  S-vs-F results bound it (below).

UEHelpers (console copy, 9962 B, v3 API):

- `GetKismetStringLibrary()` = `CacheDefaultObject("/Script/Engine.Default__KismetStringLibrary", ...)`
  -> `StaticFindObject(full CDO path)` + `ModRef:Get/SetSharedVariable` —
  the GetSubsystem-style pattern; also the CDO-path form of S.
- `GetEngine()` = `FindFirstOf("Engine")` — FindFirstOf via an independent
  caller (cross-checks F through UEHelpers' own code).
- `GetUEHelpersVersion()` — pure-Lua module sanity.

## Token table (press Key.F7 once, in-game)

| Token | Probe | Emitted |
|---|---|---|
| Z0 | script load banner | `Z0 v7` |
| K1/K2 | keybind fired / queued | `K1 f7` ... `K2 q` |
| Z1 | `IsKeyBindRegistered(Key.F7)` | `Z1 true` (native bridge, no UObject/FName) |
| S1r | `StaticFindObject("/Script/Engine.PlayerController")`:IsValid | `S1r y`/`S1r n` (`S1e` call error) |
| S2r | same, `"/Script/Engine.GameEngine"` | `S2r y/n` |
| S3r | same, `"/Script/Engine.KismetStringLibrary"` | `S3r y/n` |
| U1 | `require("UEHelpers")` + version | `U1 y v3` / `U1 n` |
| U2r | `UEHelpers.GetKismetStringLibrary()`:IsValid (CDO path + shared var) | `U2r y/n` |
| U3r | `UEHelpers.GetEngine()`:IsValid (FindFirstOf("Engine")) | `U3r y/n` |
| F1r | `FindFirstOf("PlayerController")`:IsValid | `F1r y/n` |
| F2r | `FindFirstOf("GameEngine")`:IsValid | `F2r y/n` |
| F3r | `FindFirstOf("KismetStringLibrary")`:IsValid | `F3r y/n` (**expected n even when healthy**: no live instances; its CDO is excluded) |
| A1r | `FindAllOf("PlayerController")` ~= nil (real nil contract) | `A1r y/n` |

Error tail prints as two leading spaces + first 24 chars, location
stripped (v2/v3 chunk style). No GetClass, no FindFunction, no
class-children traversal anywhere; every native call is inside pcall;
U1's version fetch is itself pcall-wrapped so no rung can abort the ladder.

## Interpretation matrix

1. `Z1 true` + `K1` + `Z0` seen, everything else silent past K1: ExecuteInGameThread drain issue, not resolution — rerun once, then treat as l22-pump regression.
2. **S y (any of S1-S3) + F1r n + F2r n**: object array, FName path lookup, and name plumbing are FINE; FindFirstOf's class-scan path is specifically broken — audit `FindFirstOf` scan branch: FName `Equals` compare, `FindRootClassesByName`, and especially `IsValidObjectForFindXOf` (over-excluding). This is the "huge narrowing" case.
3. **S n + F n (all six negative)**: common layer broken — global object cache / `FUObjectHashTables::StaticFindObjectByPath` / FName(path-parts) admission. Deeper than the scan: suspect hash-table init or the GATE-1 FName pool admission oracle. Object-array reads or permission fault.
4. `S2r y` but `S1r n`/`S3r n`: partial path resolution — /Script/Engine package outer chain resolves but per-class hash entries missing (partial hash-table init).
5. `U2r y` with `U3r n` and `F n`: confirms row 2 via an independent caller; also proves ModRef shared-var bridge works.
6. `U1 n`: require/shared-scripts path broken — discount U2/U3, rely on S/F.
7. `F3r y`: exclusion filter is NOT excluding (CDO returned) — compare over-matching or `IsValidObjectForFindXOf` bypassed; combined with F1/F2 n it means the filter misbehaves in both directions.
8. `A1r y` + `F1r n`: scan DOES find instances and Lua table construction works — the F1 empty-wrapper result is a wrapper-construction/`auto_construct_object` bug, not a scan failure. `A1r n` + `F1r n` consistent with rows 2-3.
9. `S*e`/`F*e`/`r?` variants: pcall caught a native throw — read the 24-char tail; overload mismatch would indicate the port's Lua surface diverges from the source read here.
10. All y (S,F1,F2,U,A1 y; F3 n): resolution is globally healthy at probe time — the spawner failure is timing/instance-specific (ItemDatabaseSubsystem not constructed at probe moment), not resolution machinery.

## Caveats

- luac here is 5.5.1 (UE4SS embeds 5.4); the file uses only constructs
  common to both — also verified `loadfile` clean under homebrew lua.
- Tokens must stay short: the wide-char log writer spaces every char and
  collides long lines (v2 evidence); longest v7 payload is < 30 chars.
- One keypress per attempt; v2's crash came from repeated GetClass/FindFunction probes, which v7 does not contain.
- Sidecar assumes last-known console state (zzdiag v2 1107 B or v3 2314 B);
  root must pull the live loadlist and change only the zzdiag main.lua row
  to 2487 (size-exact law) before admission.
