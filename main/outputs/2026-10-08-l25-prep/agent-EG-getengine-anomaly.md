# Agent EG — U3r GetEngine invalid-wrapper anomaly analysis (v7, 2026-10-08 night)

Read-only analysis; only this report written. Sources: /tmp/v7-run.log,
staged UEHelpers.lua (9,962 B, v3 API), port source (l25-runtime tree that
produced tonight's binary + coherent-runtime source reconstruction + l26
candidate), agent-CZ-session-analysis.md, agent-CX-v7-probe.md,
agent-DA-verified-query-fix.md.

## 1. What GetEngine actually calls

Staged copy (9,962 B; identical bytes in every console packet, e.g.
`outputs/2026-10-08-menu-complete-package/console-test-20261008T140903Z-a0626b/UEHelpers.lua`):

- `UEHelpers.lua:35` — `local EngineCache = CreateInvalidObject()` (cache
  seeded INVALID at module load; module provably loaded: `U1 y v3`).
- `UEHelpers.lua:38-43` — GetEngine: `if EngineCache:IsValid() then return
  EngineCache end; EngineCache = FindFirstOf("Engine"); return EngineCache`.

So it is **one port API, one arg**: global Lua `FindFirstOf("Engine")`,
registered at `LuaMod.cpp:1731` (l25-runtime typescan-r4 candidate —
registration identical in l23/l26 copies per CX). That binding calls
`Unreal::UObjectGlobals::FindFirstOf` (coherent-runtime source
`port/unreal/src/UObjectGlobals.cpp:434-479`): `FName(ClassName)` →
hash mode (`FindRootClassesByName` + `FindObjectSearcher`) or full
`ForEachUObject` walk with `Class->GetNamePrivate().Equals(ClassName)`
direct compare + `TSuperStructRange` super-chain compare, every candidate
gated by `IsValidObjectForFindXOf`. Not found → `nullptr`. The Lua binding
then pushes `LuaType::auto_construct_object(lua, object)` — documented
nullptr-safe (`LuaMod.cpp:1726-1727` region), and its body
(`ue4ss/src/LuaType/LuaUObject.cpp:438-444`) constructs an **empty
non-nil wrapper** (`UObject::construct(lua, nullptr)`) for nullptr. The
`IsValid` binding (`LuaUObject.hpp:185-193`) is `get_remote_cpp_object()
!= nullptr` → false. **No caching bug, no different overload, no
bUseCache/2-arg path is involved.**

Notably, v3's GetEngine queries **only "Engine"** — no GameEngine/
UnrealEngine fallback (that is why CX's F2 rung exists as cross-check).
UEngine is abstract: no live object's *direct* class name is "Engine";
the only arm that can ever match is the **super-struct chain compare**
(a GameEngine-family instance's super chain contains the class named
"Engine").

## 2. Why invalid-but-non-nil while "FindFirstOf yields nil"

**The premise is half-wrong, and that dissolves the anomaly.** In this
port FindFirstOf *never* returns nil: not-found = nullptr = non-nil empty
wrapper (contract above). The v7 F-tokens did not yield nil — they printed
`F1r n / F2r n / F3r n`, i.e. the **same invalid-wrapper observable as
`U3r n`**. Had any FindFirstOf returned literal nil, the probe's second
pcall would have errored with "attempt to index a nil value" and printed
`F*r?` — **zero `r?` tokens exist anywhere in v7-run.log**. The only
real-nil API on this surface is FindAllOf (`A1r n` = literal nil), a
different contract. So U3's "resolved-but-invalid wrapper" is byte-for-
byte the F-token not-found signal — GetEngine is one Lua line away from
FindFirstOf and rides the identical machinery, exactly as CZ's UEHelpers
verdict predicted.

Cache is ruled out as a contributor: the guard is `IsValid()`, and an
invalid result stored at :41 never passes the :39 guard — every call
re-queries. All three F7 cycles re-ran a fresh `FindFirstOf("Engine")`
and got the same nullptr→wrapper.

Root cause of the empty scan (not new to this report): the converged N1
diagnosis — `FNAME_Find` for a non-init name returns a WRONG-but-valid
id, so `FName("Engine")` never `Equals` any stored class name; the walk
runs to completion (the perceptible ~100-400 ms hitch at ~72k objects)
and returns nullptr. DA's harness case DA2_N1_CONSOLE_SYMPTOM reproduced
exactly this shape pre-fix (nullptr / 0 objects, no crash). Corroborating
discriminator in v7-run.log: **zero `[FFO]` markers** — if the binary's
CP add-fallback instrumentation was compiled in, zero add-fallback means
the probed names *resolved* via FNAME_Find (to wrong indices, hence no
match), the N1 signature rather than a name-admission miss.

## 3. Console fit — timing and menu state

- U3r ran three times, all F7 cycles: **22:24:34 (log :834), 22:24:37
  (:861), 22:24:38 (:888)** — 41-45 s after UE4SS start (22:23:53), zz
  load banner `Z0 v7` at 22:23:57 (:803).
- State was **MENU**: the spawner's own `main.lua:66` error at 22:24:38
  ("Item database not ready. Load a save first.") and CZ's menu-churn
  markers prove no save was loaded.
- **GEngine exists at menu** (created at engine init, long before the
  menu renders — the menu is drawn through it), and the engine instance
  is a plain object: not CDO/archetype, not UClass/UFunction/UPackage/
  UEnum/UScriptStruct, so `IsValidObjectForFindXOf` would not exclude it.
  A healthy `FindFirstOf("Engine")` (super-chain arm) should return a
  VALID wrapper at menu.
- Verdict on fit: `U3r n` at menu is a **genuinely anomalous result**
  (unlike F1/PlayerController, where menu legitimately has no live
  instance) — but it is **fully explained by the same N1 query-mismatch
  bug in wrapper form**: scan ran, matched nothing because the
  constructed FName comparison index is wrong, returned nullptr, and the
  port's always-wrapper Lua surface showed it as invalid-not-nil. Not a
  distinct failure mechanism, and not a stale wrapper.
- Sibling discrimination in the same ladder: the *other* resolution
  family errored rather than returned — `S1e/S2e/S3e` and `U2e` all
  threw `GetPackageNameFromLongNa...`, which is exactly the first 24
  chars of the throw at `UObjectGlobals.cpp:274`
  (`"GetPackageNameFromLongName: Name wasn't long."`) on the
  StaticFindObject long-name path. So hash/path resolution is
  broken-by-error while the class-scan path is broken-by-empty. U2e also
  proves UEHelpers' `ModRef:GetSharedVariable` bridge works (no error
  before the throw) — consistent with CX's U2 design.

## 4. Verdict + DA coverage

**Verdict: not a new anomaly.** U3r n ≡ F1r/F2r/F3r n — the same FindXOf
class-scan failure observed through UEHelpers' independent caller, with
the port's universal not-found contract (nullptr → non-nil empty wrapper
via auto_construct_object) making it *look* different from a nil. The
IsValid-gated cache contributes nothing.

**Covered by DA's verified-query fix: YES, transitively and completely.**
GetEngine → Lua `FindFirstOf("Engine")` → one of the five string overloads
DA rewired (l26 candidate `UObjectGlobals.cpp:730-771` →
`FindFirstOfVerified` :661-669: MakeFindXOfClassName + ToString/iequal
verify → `FindFirstOfStringWalk` on mismatch, which mirrors the walk
**including the super-struct arm** and the same IsValidObjectForFindXOf
placement, :595-623). No separate fix-list line for GetEngine. Post-fix
console expectation: `U3r y` at menu, with one `[FFO] query-fallback
[n:X]` for the Engine query if N1 is live in that pool state (that is the
fix working); if neither marker fires and U3r is still n, N1 is falsified
for that name → escalate per DA's own signal table (A2 wrapper layer /
CR §4 re-read).

**Fix-list ADDITION needed (adjacent path, surfaced by the same U-rung):**
the StaticFindObject family — `U2e` (GetKismetStringLibrary →
CacheDefaultObject → StaticFindObject CDO path) and `S1e-S3e` — throws
`GetPackageNameFromLongName: Name wasn't long.` (UObjectGlobals.cpp:269-
277, throw at :274) even though every probed path contains a '.'. DA's
fix explicitly does not touch StaticFindObject/FindObjects, and every
UEHelpers CacheDefaultObject lookup (UEHelpers.lua:210-245 — GameplayStatics,
KismetSystem/Math/String/TextLibrary, GameMapsSettings) rides it, so
U2-style errors will persist after DA ships. Recommended line: "Diagnose
StaticFindObjectByPath long-name handling: GetPackageNameFromLongName
throw fires despite '.'-containing input — suspect string form at the
Lua→C++ boundary (ensure_str/get_string) or IsShortPackageName
misclassification in UObjectGlobals.cpp:269-284; DA fix does not cover
this path." (If the boundary-string hypothesis is right it would also be
an alternative explanation for N1-shaped misses — distinguishable by the
[FFO] add-fallback marker firing vs staying silent.)

## Evidence appendix (v7-run.log, wide-char cleaned by grep -a)

- :803 22:23:57 `Z0 v7` (load)
- :817/:847/:874 22:24:34/36/38 `K1 f7` — three cycles
- per cycle: `Z1 true`, `S1e/S2e/S3e` + `GetPackageNameFromLongNa`,
  `U1 y v3`, `U2e` + `GetPackageNameFromLongNa`, **`U3r n`**,
  `F1r n`, `F2r n`, `F3r n`, `A1r n`
- zero `r?` tokens, zero `[FFO]/[FN]/[UF]` markers in the whole log
- 22:24:38 `[TownfallItemSpawner] Scan failed ... Load a save first.`
  → menu state at probe time

— agent EG (ZCode), 2026-10-08. Read-only over logs/trees/port source;
wrote only this report.
