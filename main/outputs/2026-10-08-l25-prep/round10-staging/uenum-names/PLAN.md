# UEnum::Names: APPLIED in R11 (2026-10-13). Mac-only.

> Applied, with two corrections found while applying it:
> - Root cause: all five versioned Names getters are PRIVATE in the port's generated wrapper, so the
>   requires-probe fails for every alternative. FNameData is not incomplete.
> - The override therefore names GetNames415's exact return type, instead of calling it.
> - No packaging overlay is needed. The binder compiles in the runtime build
>   (2026-10-07-l19-local-profile-r4/units/NativeLayoutBinding.cpp), so a profile CHANGES row routes the header.
> - layout_binder_gate PASS with 81 rows.
>
> The original plan follows.

## Problem (console-proven, launch 11)

`runtime-status: failed Native accessor width unavailable: UEnum::Names`. The binder asks
`GetBindingAccessorWidth` (packaging lineage header `NativeLayoutActiveWidths.hpp`).

- That header has no override for UEnum::Names, so it falls back to the broad table
  (`ue4ss-native-layout-widths-2/src/NativeLayoutWidths.cpp:2085`). The broad table needs ALL five versioned getter
  types to be complete.
- `GetNames507()` returns `UEnum::FNameData&`, which is incomplete in that compile, so there is no width.
- Until this is fixed, the 9 enum-API read sites are FIELD-GATE-OK and fail closed, as in launches 1-10.

## Fix

Add one override to the active-width header. This is the same pattern as the 10 existing overrides (genuine active
getter, no game layout values).

```cpp
template<class T> constexpr auto EnumNamesWidth() noexcept -> std::optional<uint32_t> {
    if constexpr (requires(T& value) { value.GetNames415(); })
        return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNames415())>();
    return std::nullopt;
}
// in GetBindingAccessorWidth, before the fallback:
    if (class_name == "UEnum" && key == "Names")
        return ActiveWidthDetail::EnumNamesWidth<Unreal::UEnum>();
```

Why GetNames415:
- The port's own UEnum code uses `GetNames415()` for UE 4.15 up to below 5.7 (port `unreal/src/UEnum.cpp:159-163,
  213-215, 252-254`).
- The FIELDS track measured offset 0x40 for TArray<TPair<FName,int64>> from two sources (UE 5.6 headers compiled
  for PS5; the game image's FEnumProperty code).
- The range covers Townfall (5.6) and Stellar Blade (UE4 4.26). UE 5.7+ would need GetNames507 with a complete
  FNameData. Note this for later engines.

## Where it has to land

- The header is pinned from `outputs/2026-10-07-l19-r4-full-build/driver/staging/lineage-overlay-2/NativeLayoutActiveWidths.hpp`
  (sha 619e9dcf…; `lineage_source_pins` in BUILD-INPUTS).
- `l26_packaging_driver.py derive_manifest` only replaces the l24 `lineage_overlays` destinations plus 2 named new
  ones. So it needs:
  1. A profile candidate `NativeLayoutActiveWidths.hpp` (`profile/units/`, with a CHANGES.json row).
  2. A third named destination in the driver's `(('CodeSignatureProfile.hpp',…),('CodeImageCache.hpp',…))` list,
     with destination `NativeLayoutActiveWidths.hpp` at the lineage root.
  3. Re-add `[UEnum]\nNames=0x40\nUEP_TotalSize=0x68` to the compiled declaration (the backup with it:
     `merge-backup/CompiledNativeLayoutDeclarations.hpp.r10-launched`).
  4. Remove the 9 `FIELD-GATE-OK: UEnum::Names …` comments (LuaUObject.cpp:1839, LuaType/LuaUEnum.cpp
     71/100/105/165/238/284/330/382).
  5. Extend `harvest/layout_binder_gate.py` so ACTIVE points at the staged header. It must then PASS with the UEnum
     row present.
- One reviewer, then fold into the next planned launch.
