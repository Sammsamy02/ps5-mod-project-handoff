# LFB Bridge compiled but never executes — need guidance

## What we built (PRX c1324408, verified on console)
1. LuaFNameBridge.hpp: hand-written UTF-8→UTF-16 converter (no std::wstring,
   no Helpers, no wide libc) → direct slot-2 ctor call via
   FName::ConstructorInternal. [LUA-FN] first-20 log lines.
2. ALL Lua→FName entry points routed: LuaMod.cpp :1720/:1782/:1831/:1886
   (StaticFindObject×2, FindFirstOf, FindAllOf) + 14 more sites across
   LuaUObject, LuaFName, RegisterHook paths, __index/__newindex.
3. Build verified: Bridge symbols in luamod.o (nm: utf8_to_utf16,
   utf16_to_utf8, ascii_fold_unit all present). [LUA-FN] marker found in
   the PRX binary (UTF-8 encoding). [LFB] composed marker in PRX (UTF-16LE).

## What the console shows
- ZERO [LUA-FN] lines — to_fname() never logs
- ZERO [FN] seam refused / [FN] ctor unready — neither early-return fired
- SAME three one-char FFO indices (0x1DA05='I', 0x1DA13='P', 0x1D9FD='C')
- The 19 C++-literal self-tests still PASS (full names, correct indices)
- Mod runs, keybinds fire, scan fails "Item database not ready"

## The contradiction
The Bridge is on the only FindFirstOf registration path (:1806, no
duplicates found in any TU). Its constructor + to_fname() are compiled in
(symbols in luamod.o). The code follows:
```
Bridge name{lua.get_lua_state()};
FName class_name = name.to_fname(FNAME_Find);
if (class_name.IsNone()) class_name = name.to_fname(FNAME_Add);
UObject* object = FindFirstOf(class_name);
```
If to_fname() executed, log_use() MUST fire (it's unconditional after the
ctor call, unless the early returns trigger — and their markers would fire).
Neither happens. The FFO markers (from the string-overload path in
UObjectGlobals.cpp) still fire with the same one-char indices.

## Questions for Claude
1. The FFO markers come from MakeFindXOfClassName in UObjectGlobals.cpp —
   but the Bridge calls FindFirstOf(FName) (the FName overload) which should
   NOT go through the string path. Why are FFO markers still firing?
2. Could lua.get_lua_state() return a different Lua state or stack layout
   than the Bridge expects? The binding lambda receives
   `const LuaMadeSimple::Lua& lua` — is get_lua_state() the correct way
   to get the raw lua_State* from this wrapper?
3. Is there a COMPILED-OUT code path? The Bridge was in an #ifdef
   __PROSPERO__ block — verify it's active for the luamod TU's compile flags.
4. Could the FFO markers be coming from a DIFFERENT caller (not the Lua
   binding but internal C++ code that still uses the old string path)?
   The zzdiag auto-probe also calls FindFirstOf from Lua — but that goes
   through the same Bridge.

## Evidence files
- /tmp/lfb-result.log — the full session log
- luamod.o nm output — Bridge symbols present
- LuaMod.cpp :1806-1855 — the routed binding code
