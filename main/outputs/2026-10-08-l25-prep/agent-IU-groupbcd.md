# Agent IU — GROUP B/C/D round-13 items 3-5 + item 6 hygiene

2026-10-12. ANALYSIS + staging only: zero builds, zero console contact, zero
live-tree edits (self-test deploy artifacts edited directly per assignment;
port patches staged unapplied in `round5-staging/group-b/`). All Lua changes
passed `luac -p`; all staged patches verified with `patch --dry-run` + apply +
byte-compare on pristine temp copies.

Upstream reference = vendored
`.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/`
(the parity table's designated upstream; LuaMod.cpp 7,331 lines), cross-checked
against `native-coherent-runtime-1` and the l21-audit source-context revision.
Port = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` union the
linked coherent-runtime units.

## GROUP C verdict: ALL FIVE are TEST ERRORS — zero real divergences (item 3)

| Call | Test expected | Upstream actually returns | Port | Disposition |
|---|---|---|---|---|
| 8 PropertyTypes.ObjectProperty | number | TABLE `{Name, Size, FFieldClassPointer, StaticPointer}` (`register_all_property_types` → `add_property_type_table`, upstream LuaMod.cpp:871/:729) | table (matches) | TEST FIXED: expect table w/ numeric `.Size` |
| 20 `__OriginalReturnValue` | table | **nil at rest** — upstream `prepare_mod` explicitly `lua.set_nil(); lua_setglobal("__OriginalReturnValue")` (upstream :5879-5881; every vendored revision has exactly this one site — the value is only exposed as the post-hook callback's trailing argument) | nil (matches; port :8005 same) | TEST FIXED: expect nil |
| 28 FName.Equals | boolean | **undefined** — the lambda `return name_a...Equals(...)` returns a raw C++ bool as the Lua return-COUNT (1/0) pushing nothing, so Lua hands back a stale stack slot (observed: the pcall'd test closure itself → "function") | byte-identical to upstream | TEST FIXED: assert error-free call only; comparison parity stays on `==` (call 29). INHERITED upstream defect, documented — not a port divergence, not fixed this round per instructions |
| 58/59 GetCurrentThreadId / GetMainModThreadId | number | `LuaType::ThreadId` **userdata** ("ThreadIdUserdata", ToString/type members, __eq) — upstream :3705-:3725; port :4355-:4371 identical | userdata (matches) | TEST FIXED: expect userdata + working `:ToString()` |

Log cross-check (`evidence/acceptance-pass2-20261012.decoded.log`): calls 60
and 61 (GetAsyncThreadId/GetGameThreadId) failed identically with
`type=userdata` — same test error, same fix applied to all four getters.
Deliberately NOT asserted: ThreadId `__eq` (shares the return-bool-as-count
upstream quirk, result would be luck, not parity).

Net effect on the next run: -7 false FAILs (8, 20, 28, 58, 59, 60, 61).

## GROUP D verdict: partial-file deploy desync — loader and part11 both innocent (item 4)

- `part11.lua` is syntactically valid (`luac -p` OK) and returns a proper
  6-entry table locally.
- The log line `part11 LOAD-FAIL table: 880304a60` shows `require("part11")`
  **succeeded and returned a table** — the failure was main.lua's strict
  `#t == cnt` check: all four console readback copies
  (`evidence/rowcheck/rowcheck-*/readback/...part11.lua`) are an older
  5-entry generation missing `UEHelpers.GetWorld`, while the console ran the
  NEW main.lua (`boot v1 M=168`, `LP(11, 6)`). Deployed set was internally
  coherent at M=167; someone pushed new main.lua without part11.lua.
- The native module searcher is exonerated: parts 2-10+12 loaded through the
  identical path; nothing about "11" is special.
- Fix applied (self-test artifact): main.lua `LP()` DESYNC branch — a loaded
  table with a count mismatch now RUNS its real entries, pads the gap with
  named `part-entry-missing-desync` stubs, and logs
  `part11 DESYNC loaded 5 expected 6 (redeploy all ueselftest files)`.
  Host-harness verified against the actual deployed 5-entry readback
  (total=6, real=5, stub=1). Operational fix: next deploy must push the whole
  ueselftest dir atomically.

## GROUP B: actual state + three staged patches (item 5)

Key finding: **none of the three were "missing"** — all are bound and
upstream-identical; the failures have three different real causes.

1. **ForEachUObject (call 133, iterate-err)** — REAL BUG, inherited verbatim
   from upstream: the loop calls `lua.call_function(3, 1)` and never discards
   the result. `get_userdata` never pops here (global function — callback
   stays at stack index 1), so ONE stale Lua stack slot leaks per visited
   object; on this game's large GUObjectArray the stack reaches LUAI_MAXSTACK
   and the whole call errors (164 ms run 1 / 23 ms run 2 — matches growth-
   then-overflow). Upstream's own UEnum.ForEachName contains the correct
   discard pattern (with a comment explaining exactly this corruption).
   `round5-staging/group-b/01-luamod-foreachuobject-fix.patch`: discard per
   iteration, `lua_checkstack` guard, bounded per-object construct-failure
   skip (`[FOU]` markers, first 3 + summary), callback `true` → Break
   (ForEachName convention). The walker itself is proven (FindAllOf passes on
   the same ForEachUObject_Chunked path).
2. **IterateGameDirectories (call 134, ret=nil)** — correct upstream code,
   impossible heuristic on PS5: configured game dir is the flat app mount
   (`/app0`, executable `/app0/eboot.bin` — console-verified), so
   `exe_dir.parent.parent/"Content"` collapses to `/Content`. Staged
   `02-luamod-iterategamedirectories-ps5root.patch`: `__PROSPERO__`-gated
   fallback — if `<GameDir>/Content` exists (same runtime exists() evidence
   standard as upstream), iterate from the game dir and rawset a `Game`
   self-reference so upstream-shaped mod code (`dirs.Game.Content...`) works.
   Classic layout untouched; still nil+warning if neither layout is found.
3. **UEnum.ForEachName (call 100, n=0)** — fully implemented at both layers;
   the console ran it clean but the Names data read came back EMPTY for
   `/Script/Engine.ENetRole` (GetNameByValue(0) also nil). Remaining suspects:
   `MemberOffsets["Names"]` (5.06 default 0x40) vs this title's layout, or
   version dispatch. Per the offset law NO blind offset edit: staged
   `03-luauenum-foreachname-diagnostics.patch` adds bounded first-3 `[UEN]`
   logs of NumEnums() vs iterated count (discrimination table in APPLY-NOTES).
   Real fix follows that evidence next round.

Staging dir `round5-staging/group-b/`: the three patches + APPLY-NOTES.md
(base sha256s, apply preconditions, acceptance criteria, deploy note).
Nothing applied — the l26-A frozen tree is untouched.

## Item 6 hygiene (deployed side)

main.lua now enforces ONE completed run per session (`S.done`; extra F12
presses log `already-done-once-this-session` and exit). That removes the
measured GROUP-A "not enough memory" mechanism: 3 re-runs x re-registering
the no-unregister callback families. RegisterHook→UnregisterHook and
RegisterCustomEvent→UnregisterCustomEvent unregistration already existed in
part8 (calls 112/114) and stays. Boot line bumped to `v1.1 ... one-run=true`
for log identification.

## Files touched

- `ueselftest/part2.lua` (calls 8, 20), `ueselftest/part3.lua` (call 28),
  `ueselftest/part4.lua` (calls 58-61) — expectations corrected to upstream.
- `ueselftest/main.lua` — DESYNC-tolerant LP + one-run guard + version note.
- `round5-staging/group-b/{01,02,03}*.patch` + `APPLY-NOTES.md` — staged only.
- No source tree, frozen artifact, or console state modified.
