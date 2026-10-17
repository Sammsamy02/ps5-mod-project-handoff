# GROUP-B staged patches — agent IU, Claude round-13 item 5 (2026-10-12)

ANALYSIS + STAGING ONLY. No builds, no console contact, no source-tree edits:
the three .patch files below are staged here and were never applied to the
live trees. Patches were verified to apply cleanly (`patch --dry-run` + real
apply onto pristine copies in a temp dir; results byte-compared against the
intended modified copies — all three `RESULT-EQUALS` receipts captured in the
session log).

## Base identities (apply-time preconditions; verify before applying)

| Patch | Base file | sha256 (base, 2026-10-12) |
|---|---|---|
| 01, 02 | `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` | `bfc392c53f346c57dac9644148aafd0a62b9cdf4790a668d002653c9b7bd16a2` (9,509 lines) |
| 03 | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUEnum.cpp` | `4b4949d9799c04aff8ac0f97666faae71bbc0d925f81e1ec9467d1071a7b8ac3` |

Note: the l26 frozen snapshot (24025929) pins LuaMod.cpp at `61f6cb98…`;
the current on-disk candidate has moved past the freeze (post-freeze harvest
work). Apply against the CURRENT file only after re-hashing; if the tree has
moved again, re-stage — do not force-apply.

## 01-luamod-foreachuobject-fix.patch

`ForEachUObject` is BOUND and its C++ walker (`UObjectGlobals::ForEachUObject`,
chunked path for 5.6) is proven by FindFirstOf/FindAllOf — the "not
implemented" triage in CLAUDE-SELFTEST-RESULTS was wrong. The real defect
(inherited from upstream, present byte-identical in both trees): the loop
calls `lua.call_function(3, 1)` and never discards the requested result.
`get_userdata` (which pops the self userdata and shifts the callback to index
1) never runs here — this is a global function — so one stale stack slot leaks
per visited object. On this game's large GUObjectArray the Lua stack climbs to
LUAI_MAXSTACK and the call fails with the observed `iterate-err` (164 ms run
1 / 23 ms run 2). Upstream's own `UEnum.ForEachName` uses the discard pattern
this patch copies. Beyond parity, all bounded: `lua_checkstack` guard, per-
object `auto_construct_object` failures skip (first 3 + summary logged,
`[FOU]` markers) instead of aborting the walk, and a Lua callback returning
`true` breaks the walk (ForEachName convention; upstream ForEachUObject
ignores the return).

Acceptance (next console run): self-test call 133 returns `n=<large>` instead
of FAIL iterate-err; no `[FOU]` storm (bounded 3 + one summary).

## 02-luamod-iterategamedirectories-ps5root.patch

`IterateGameDirectories` is BOUND and upstream-identical; it returns nil
because the classic `<RootGamePath>/<Game>/Binaries/<Platform>` derivation is
unreachable on PS5: configured game directory is the flat app mount
(console-verified: `configured game directory: /app0`, executable
`/app0/eboot.bin`), so `parent_path().parent_path() / "Content"` collapses to
`/Content`. Patch adds a `__PROSPERO__`-only fallback gated at runtime by the
same `exists()` evidence standard upstream uses: if `<GameDir>/Content`
(i.e. `/app0/Content`) exists, iterate from the game directory and rawset a
`Game` self-reference on the root table so mods written against the upstream
result shape (`dirs.Game.Content...`) keep working. Classic-layout behavior
unchanged; the unknown-structure warning text now mentions both paths checked.

Acceptance: self-test call 134 returns `ret=table` when `/app0/Content`
exists; still `ret=nil` + warning otherwise (legitimate unknown layout —
do NOT invent a root without evidence).

## 03-luauenum-foreachname-diagnostics.patch

`UEnum.ForEachName` is fully implemented and upstream-identical at BOTH
layers (Lua binding in LuaUEnum.cpp; versioned generator in
`port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp`, 4.15–5.6 path via
`GetNames415()` at `MemberOffsets["Names"]`, 5.06 default offset 0x40).
Console 2026-10-11 ran it clean but iterated 0 names and
`GetNameByValue(0)` yielded nil for `/Script/Engine.ENetRole` — the Names
data read comes back EMPTY for this game. Remaining suspects: (a) the
`Names` member offset for this title's UEnum layout, (b) version dispatch.
Per the offset law there is NO blind offset edit in this round; the patch adds
bounded first-3 `[UEN]` diagnostics logging `NumEnums()` (the reader's view of
the count) versus the count the generator actually yields, plus the enum full
name. Discrimination table for the next console run:

- `expected N>0, iterated 0` → generator/version-path or TArray decode issue;
- `expected 0, iterated 0` → Names offset wrong for this title (data-side);
- `expected -1` → NumEnums itself threw (deeper layout problem).

Only after that evidence: derive the real offset from a console read (FFO2-
style single-object probe), then fix `5_06_MemberVariableLayout_DefaultSetter_UEnum.cpp`.

## Self-test hygiene (item 6, deployed side — already edited, not staged)

`outputs/2026-10-08-l25-prep/ueselftest/main.lua` now: (1) tolerates a part
count mismatch (DESYNC branch runs the loaded entries and pads with named
`part-entry-missing-desync` stubs — prevents a single stale deployed file
from zeroing a whole part), and (2) enforces ONE completed run per session
(`S.done`), which removes the measured GROUP-A registry-exhaustion mechanism
(3 re-runs x no-unregister callback families). RegisterHook→UnregisterHook
and RegisterCustomEvent→UnregisterCustomEvent unregistration was already
present (part8 calls 112/114). All edited Lua passed `luac -p`.

## Deploy note (GROUP D)

The console currently runs NEW main.lua against OLD part11.lua (5 entries,
missing `UEHelpers.GetWorld`). The loader is exonerated — `require` returned
a valid table; the strict `#t == cnt` check was what failed. Next deploy must
push the ENTIRE `ueselftest/` directory atomically (all parts + main.lua from
the same generation; local generation sums to M=168).
