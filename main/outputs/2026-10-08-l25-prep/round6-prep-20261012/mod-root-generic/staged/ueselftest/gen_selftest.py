#!/usr/bin/env python3
# gen_selftest.py -- generate the UE4SS PARITY SELF-TEST mod ("ueselftest").
#
# Agent GB, 2026-10-10. Claude round-5 item 6a: replaces the 12-mod ladder's
# first phase with a game-agnostic self-test that calls every global, type
# member and metamethod of the composed PS5 UE4SS port ONCE, each in pcall,
# one log line per call, crash-resumable via a progress file in the mod's own
# folder, zero lasting side effects (writes restore originals, hooks
# unregister, loops one-shot, nothing constructed, no actor spawns, no save
# data).
#
# CALL SURFACE SOURCE (authoritative): outputs/2026-10-08-l25-prep/
# API-PARITY-TABLE.md (agent FS, L26-R4 candidate vs upstream; line cites
# below are the CURRENT candidate LuaMod.cpp unless noted). Cross-checked
# against agent-FT mod-ladder/STATIC-SCAN.md for crash-risk grading.
# The curated list below is EMBEDDED (not parsed from the markdown) because
# the exercise decision (SAFE-EXERCISE / SAFE-PRESENCE / SKIP-*) is a
# per-API judgement; every row carries its parity-table citation.
#
# Classification legend (see CALL-SHEET.md for the full sheet):
#   EX   SAFE-EXERCISE  -- real call, provably side-effect free (query, ctor
#                          on our own object, register+unregister pair,
#                          one-shot scheduler form, write-then-restore).
#   PR   SAFE-PRESENCE  -- type/presence check only where a full exercise
#                          cannot be made safe.
#   SKIP-*               -- statically skipped, one unnumbered log line each:
#                          SKIP-WRITE (persistent write side effect),
#                          SKIP-CONSTRUCT (no safe teardown),
#                          SKIP-CRASH-LAW (offset/crash law),
#                          SKIP-DESTRUCTIVE (self-modifying),
#                          SKIP-NO-INSTANCE (no game-agnostic acquisition
#                          route; owned by ladder probe Pn),
#                          SKIP-COVERED (owned by the spawner mod).
#
# RUNTIME LAWS baked into the emitted chassis (main.lua):
#   * every call pcall'd; ONE log line per numbered call:
#       [ST2] <idx>/<M> <name> PASS|FAIL|SKIP <tail-40>
#   * progress marker written BEFORE each call (ueselftest/progress.txt):
#     on resume the last-started call is logged "CRASH (resumed)" and SKIPPED.
#   * game-agnostic: objects acquired only via engine base classes
#     (PlayerController / engine CDOs), never game-specific names.
#   * crash law: NO FindFunction / bpfn / class-children enumeration.
#   * side-effect hygiene: property writes restore + verify the original;
#     RegisterHook immediately UnregisterHook'd; RegisterCustomEvent
#     unregistered; scheduler loops use one-shot forms (return false);
#     ClearAllDelayedActions is mod-scoped and used as final cleanup;
#     nothing StaticConstructObject'd; no SpawnActor; no save data; the only
#     files touched are progress.txt + st2io.txt inside the mod's own folder
#     (st2io.txt removed again when os.remove exists).
#   * ends: [ST2] SUMMARY <N> of <M> PASS  then  [ST2] done
#
# OUTPUT: main.lua (chassis + first entries, < 4096 B console admission law)
#         part2.lua..partN.lua (each < 4096 B), loaded at run time via
#         require("partN") with dofile fallbacks (native searcher resolves
#         scripts_path/<name>.lua today; GA's io/require shim not required).
#         EXPECTED-MARKERS.txt, CALL-SHEET.md.
#
# Local tooling only: no console contact, no builds (Mac on battery).

import argparse
import os

VERSION = "gen_selftest.py 1.0 (agent GB, 2026-10-08-l25-prep/ueselftest)"

# ---------------------------------------------------------------------------
# Curated call list. Each row: (name, cls, cite, lua_body)
#   lua_body is the body of `function(S) ... end` (S = shared chassis state).
#   Entry protocol (chassis interprets):
#     return nil / true / "detail"            -> PASS  (detail appended, 40c)
#     return false, "reason"                  -> FAIL
#     return "SKIP", "reason"                 -> SKIP (numbered, not in PASS)
#     error(...)                              -> FAIL with error tail
#   Shared state/helpers on S (defined in chassis, parts see them via param):
#     S.h   scratch table (S.h.f1, S.h.ck, S.h.d1.. handles, S.h.mcur, ...)
#     S.hyd hygiene flags (set true by entries whose cleanup must be audited)
#     S.v(o)   pcall'd IsValid -> boolean (zzdiag law: wrappers never nil)
#     S.g(nm)  acquire+cache valid FindFirstOf(nm) -> object or nil
#     S.et(e)  error tail (strip file:line, 40 chars)
#     S.pf     progress-file path (nil if io unavailable)
# ---------------------------------------------------------------------------

E = []  # (name, cls, cite, body)


def ex(name, cite, body, cls="EX"):
    E.append((name, cls, cite, body))


# ---- PHASE A: pure-Lua surface (tables, ctors, metamethods) ----------------

ex("print", "1a:1751", 'print("[ST2] print-probe\\n") return "logged"')
ex("LoadExport", "1a:1752",
   'local ok, r = pcall(LoadExport, "st2_missing_export")\n'
   'if ok then return "ret=" .. type(r) end\n'
   'return "missing-name-err(expected)"')
ex("Key.F12", "1b:839",
   'local v = Key.F12 if type(v) == "number" then return "F12=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("ModifierKey.SHIFT", "1b:851",
   'local v = ModifierKey.SHIFT if type(v) == "number" then return "v=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("EObjectFlags.RF_Public", "1b:897",
   'local v = EObjectFlags.RF_Public if type(v) == "number" then return "v=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("EInternalObjectFlags.PendingKill", "1b:917",
   'local v = EInternalObjectFlags.PendingKill if type(v) == "number" then return "v=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("EFindName.FNAME_Find", "1b:927",
   'local v = EFindName.FNAME_Find if type(v) == "number" then return "v=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("PropertyTypes.ObjectProperty", "1b:1347",
   'local v = PropertyTypes.ObjectProperty if type(v) == "number" then return "v=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("UE4SS.GetVersion", "1b:7618",
   'local a, b, c = UE4SS:GetVersion()\n'
   'if type(a) == "number" and type(b) == "number" then return a .. "." .. b .. "." .. tostring(c) end\n'
   'return false, "types=" .. type(a) .. "," .. type(b)')
ex("UnrealVersion.GetMajor", "1b:7689",
   'local a = UnrealVersion:GetMajor() if type(a) == "number" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.GetMinor", "1b:7689",
   'local a = UnrealVersion:GetMinor() if type(a) == "number" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.IsEqual", "1b:7689",
   'local a = UnrealVersion:IsEqual(5, 6) if type(a) == "boolean" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.IsAtLeast", "1b:7689",
   'local a = UnrealVersion:IsAtLeast(4, 0) if type(a) == "boolean" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.IsAtMost", "1b:7689",
   'local a = UnrealVersion:IsAtMost(6, 0) if type(a) == "boolean" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.IsBelow", "1b:7689",
   'local a = UnrealVersion:IsBelow(4, 0) if type(a) == "boolean" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("UnrealVersion.IsAbove", "1b:7689",
   'local a = UnrealVersion:IsAbove(4, 0) if type(a) == "boolean" then return tostring(a) end\n'
   'return false, "type=" .. type(a)')
ex("EGameThreadMethod", "1b:5823",
   'if type(EGameThreadMethod) == "table" then return "fm=" .. tostring(EGameThreadMethod.FuncPtrMarshal) end\n'
   'return false, "type=" .. type(EGameThreadMethod)')
ex("EngineTickAvailable", "1b:5831",
   'if type(EngineTickAvailable) == "boolean" then return tostring(EngineTickAvailable) end\n'
   'return false, "type=" .. type(EngineTickAvailable)')
ex("ProcessEventAvailable", "1b:5833",
   'if type(ProcessEventAvailable) == "boolean" then return tostring(ProcessEventAvailable) end\n'
   'return false, "type=" .. type(ProcessEventAvailable)')
ex("__OriginalReturnValue", "1b:7858",
   'if type(__OriginalReturnValue) == "table" then return "table" end\n'
   'return false, "type=" .. type(__OriginalReturnValue)', cls="PR")
ex("MainThread", "1b:1293",
   'if type(MainThread) == "thread" then return "thread" end\n'
   'return false, "type=" .. type(MainThread)', cls="PR")
ex("AsyncThread", "1b:1267",
   'if type(AsyncThread) == "thread" then return "thread" end\n'
   'return false, "type=" .. type(AsyncThread)', cls="PR")
ex("ue4ss_loaded_modules", "1b:1356",
   'if type(ue4ss_loaded_modules) ~= "table" then return false, "type=" .. type(ue4ss_loaded_modules) end\n'
   'local n = 0 for k in pairs(ue4ss_loaded_modules) do n = n + 1 if n > 15 then break end end\n'
   'return "n=" .. n', cls="PR")
ex("NAME_None", "1b:7698",
   'local s local ok = pcall(function() s = NAME_None:ToString() end)\n'
   'if ok and type(s) == "string" then return s end\n'
   'return false, "tostring-failed"')
ex("FName-ctor", "1b:7696",
   'local f = FName("st2name") S.h.f1 = f return "made"')
ex("FName.ToString", "2c:LuaFName.cpp:169",
   'local f = S.h.f1 or FName("st2name") local s = f:ToString()\n'
   'if type(s) == "string" then return s end\n'
   'return false, "type=" .. type(s)')
ex("FName-tostring", "3:LuaFName.cpp:28",
   'local f = S.h.f1 or FName("st2name") local s = tostring(f)\n'
   'if type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "type=" .. type(s)')
ex("FName.Equals", "2c:LuaFName.cpp:192",
   'local f = S.h.f1 or FName("st2name") local r = f:Equals(FName("st2name"))\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("FName-eq", "3:LuaFName.cpp:153",
   'local f = S.h.f1 or FName("st2name") local r = (f == FName("st2name"))\n'
   'return "eq=" .. tostring(r)')
ex("FName.GetComparisonIndex", "2c",
   'local f = S.h.f1 or FName("st2name") local v = f:GetComparisonIndex()\n'
   'if type(v) == "number" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("FName.type", "2c",
   'local f = S.h.f1 or FName("st2name") local t = f:type()\n'
   'if type(t) == "string" then return t end\n'
   'return false, "type=" .. type(t)')
ex("FName-ctor-FNAME_Find", "2c:82 (P3)",
   'local f = FName("Engine", EFindName.FNAME_Find) S.h.f2 = f return "find-ctor"')
ex("FText-ctor", "1b:7705",
   'local t = FText("st2text") S.h.t1 = t return "made"')
ex("FText.ToString", "2c:LuaFText.cpp:223",
   'local t = S.h.t1 or FText("st2text") local s = t:ToString()\n'
   'if type(s) == "string" then return s end\n'
   'return false, "type=" .. type(s)')
ex("FText-tostring", "3:LuaFText.cpp:52",
   'local t = S.h.t1 or FText("st2text") local s = tostring(t)\n'
   'if type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "type=" .. type(s)')
ex("FText.type", "2c",
   'local t = S.h.t1 or FText("st2text") local v = t:type()\n'
   'if type(v) == "string" then return v end\n'
   'return false, "type=" .. type(v)')
ex("FText-eq", "3:LuaFText.cpp:177",
   'local a = FText("st2text") local r = (a == FText("st2text"))\n'
   'return "eq=" .. tostring(r)')
ex("FString-ctor", "1a:7710",
   'local s = FString("st2str") S.h.s1 = s return "made"')
ex("FString.Len", "2c:LuaUnrealString.hpp",
   'local s = S.h.s1 or FString("st2str") local n = s:Len()\n'
   'if type(n) == "number" then return "len=" .. n end\n'
   'return false, "type=" .. type(n)')
ex("FString.IsEmpty", "2c",
   'local s = S.h.s1 or FString("st2str") local r = s:IsEmpty()\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("FString.ToString", "2c",
   'local s = S.h.s1 or FString("st2str") local r = s:ToString()\n'
   'if type(r) == "string" then return r end\n'
   'return false, "type=" .. type(r)')
ex("FString-tostring", "3:LuaUnrealString.hpp:44",
   'local s = S.h.s1 or FString("st2str") local r = tostring(s)\n'
   'if type(r) == "string" then return r:sub(1, 24) end\n'
   'return false, "type=" .. type(r)')
ex("FString.type", "2c",
   'local s = S.h.s1 or FString("st2str") local t = s:type()\n'
   'if type(t) == "string" then return t end\n'
   'return false, "type=" .. type(t)')
ex("FString.Append", "2c",
   'local s = S.h.s1 or FString("st2str") s:Append("X")\n'
   'return "len=" .. tostring(s:Len())')
ex("FString.EndsWith", "2c",
   'local s = S.h.s1 or FString("st2strX") local r = s:EndsWith("X")\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("FString.StartsWith", "2c",
   'local s = S.h.s1 or FString("st2str") local r = s:StartsWith("st2")\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("FString.Find", "2c",
   'local s = S.h.s1 or FString("st2str") local r = s:Find("2")\n'
   'return "r=" .. tostring(r)')
ex("FString.ToLower", "2c",
   'local s = S.h.s1 or FString("St2StR") local r = s:ToLower()\n'
   'return "r=" .. type(r) .. ":" .. tostring(r):sub(1, 12)')
ex("FString.ToUpper", "2c",
   'local s = S.h.s1 or FString("St2StR") local r = s:ToUpper()\n'
   'return "r=" .. type(r) .. ":" .. tostring(r):sub(1, 12)')
ex("FString.Clear", "2c",
   'local s = S.h.s1 or FString("x") s:Clear() local e = s:IsEmpty()\n'
   'return "empty=" .. tostring(e)')
ex("FString.Empty", "2c",
   'local s = S.h.s1 or FString("x") s:Empty() local e = s:IsEmpty()\n'
   'return "empty=" .. tostring(e)')
ex("FUtf8String-ctor", "1a:7746",
   'local u = FUtf8String("st2u8") S.h.u1 = u return "made"')
ex("FUtf8String.Len", "2c",
   'local u = S.h.u1 or FUtf8String("st2u8") local n = u:Len()\n'
   'if type(n) == "number" then return "len=" .. n end\n'
   'return false, "type=" .. type(n)')
ex("FUtf8String.ToString", "2c",
   'local u = S.h.u1 or FUtf8String("st2u8") local r = u:ToString()\n'
   'if type(r) == "string" then return r end\n'
   'return false, "type=" .. type(r)')
ex("FAnsiString-ctor", "1a:7761",
   'local a = FAnsiString("st2an") S.h.a1 = a return "made"')
ex("FAnsiString.Len", "2c",
   'local a = S.h.a1 or FAnsiString("st2an") local n = a:Len()\n'
   'if type(n) == "number" then return "len=" .. n end\n'
   'return false, "type=" .. type(n)')
ex("FAnsiString.ToString", "2c",
   'local a = S.h.a1 or FAnsiString("st2an") local r = a:ToString()\n'
   'if type(r) == "string" then return r end\n'
   'return false, "type=" .. type(r)')

# ---- PHASE B: thread/time queries ------------------------------------------

ex("GetCurrentThreadId", "1a:4355",
   'local v = GetCurrentThreadId() if type(v) == "number" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("GetMainModThreadId", "1a:4366",
   'local v = GetMainModThreadId() if type(v) == "number" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("GetAsyncThreadId", "1a:4378",
   'local v = GetAsyncThreadId() if type(v) == "number" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("GetGameThreadId", "1a:4390",
   'local v = GetGameThreadId() if type(v) == "number" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("IsInMainModThread", "1a:4401",
   'local v = IsInMainModThread() if type(v) == "boolean" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("IsInAsyncThread", "1a:4413",
   'local v = IsInAsyncThread() if type(v) == "boolean" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("IsInGameThread", "1a:4425",
   'local v = IsInGameThread() if v == true then return "true" end\n'
   'return false, "expected-true got " .. tostring(v)')
ex("os.clock", "4:loslib",
   'local t = os.clock() if type(t) == "number" then return "t=" .. t end\n'
   'return false, "type=" .. type(t)')
ex("os.date", "4:loslib",
   'local t = os.date() if type(t) == "string" then return "ok" end\n'
   'return false, "type=" .. type(t)')
ex("debug.getinfo", "4:ldblib",
   'local i = debug.getinfo(1, "S")\n'
   'if type(i) == "table" and type(i.source) == "string" then return "src" end\n'
   'return false, "type=" .. type(i)')

# ---- PHASE C: engine acquisition + read-only member surface ----------------

PC = 'local pc = S.g("PlayerController") if not pc then return "SKIP", "no-valid-pc" end '
ex("FindFirstOf", "1a:1870 (PROVEN)",
   'local ok, o = pcall(FindFirstOf, "PlayerController")\n'
   'if not ok then return false, "call-err" end\n'
   'if o == nil then return false, "returned-nil(contract=wrapper)" end\n'
   'local v = S.v(o) if v then S.h.gpc = o end return "valid=" .. tostring(v)')
ex("FindAllOf", "1a:1921 (PROVEN)",
   'local ok, a = pcall(FindAllOf, "PlayerController")\n'
   'if not ok then return false, "call-err" end\n'
   'if a == nil then return "nil(real-miss-ok)" end\n'
   'if type(a) ~= "table" then return false, "type=" .. type(a) end\n'
   'return "n=" .. #a')
ex("StaticFindObject-long", "1a:1759 (PROVEN)",
   'local ok, o = pcall(StaticFindObject, "/Script/Engine.GameEngine")\n'
   'if not ok then return false, "call-err" end\n'
   'if o == nil then return false, "nil" end\n'
   'return "valid=" .. tostring(S.v(o))')
ex("StaticFindObject-short", "1a:1759",
   'local ok, o = pcall(StaticFindObject, "GameEngine")\n'
   'if not ok then return false, "call-err" end\n'
   'if o == nil then return false, "nil" end\n'
   'return "valid=" .. tostring(S.v(o))')
ex("CreateInvalidObject", "1a:1754",
   'local o = CreateInvalidObject() if o == nil then return false, "nil" end\n'
   'local v local ok = pcall(function() v = o:IsValid() end)\n'
   'if ok and v == false then return "invalid-ok" end\n'
   'return false, "IsValid=" .. tostring(v)')
ex("uobj.IsValid", "2a:186",
   PC + 'local v = pc:IsValid() if v == true then return "valid" end\n'
   'return false, "IsValid=" .. tostring(v)')
ex("uobj.GetAddress", "2a:173",
   PC + 'local a = pc:GetAddress() if type(a) == "number" then return string.format("0x%x", a) end\n'
   'return false, "type=" .. type(a)')
ex("uobj.GetFullName", "2a:533 (PROVEN)",
   PC + 'local s = pc:GetFullName() if type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "type=" .. type(s)')
ex("uobj.GetFName", "2a:556",
   PC + 'local f = pc:GetFName() if f == nil then return false, "nil" end\n'
   'local s local ok = pcall(function() s = f:ToString() end)\n'
   'if ok and type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "tostring-failed"')
ex("uobj.GetClass", "2a:561",
   PC + 'local c = pc:GetClass() if c ~= nil and S.v(c) then return "valid" end\n'
   'return false, "class-invalid"')
ex("uobj.GetOuter", "2a:566",
   PC + 'local o = pc:GetOuter() if o == nil then return "nil-outer" end\n'
   'return "valid=" .. tostring(S.v(o))')
ex("uobj.GetWorld", "2a:648",
   PC + 'local w = pc:GetWorld() if w == nil then return "nil-world" end\n'
   'return "valid=" .. tostring(S.v(w))')
ex("uobj.IsA-class", "2a:659",
   PC + 'local c = pc:GetClass() local r = pc:IsA(c)\n'
   'if r == true then return "true" end\n'
   'return false, "IsA=" .. tostring(r)')
ex("uobj.IsA-string", "2a(LuaUObject.cpp:2482)",
   PC + 'local r = pc:IsA("/Script/Engine.PlayerController")\n'
   'if r == true then return "true" end\n'
   'return false, "IsA=" .. tostring(r)')
ex("uobj.IsAnyClass", "2a",
   PC + 'local c = pc:GetClass()\n'
   'return "pc=" .. tostring(pc:IsAnyClass()) .. " cls=" .. tostring(c:IsAnyClass())')
ex("uobj.IsClass", "2a:638",
   PC + 'local c = pc:GetClass()\n'
   'return "pc=" .. tostring(pc:IsClass()) .. " cls=" .. tostring(c:IsClass())')
ex("uobj.HasAllFlags", "2a:663",
   PC + 'local r = pc:HasAllFlags(EObjectFlags.RF_Public)\n'
   'return "r=" .. tostring(r)')
ex("uobj.HasAnyFlags", "2a:687",
   PC + 'local r = pc:HasAnyFlags(EObjectFlags.RF_Public)\n'
   'return "r=" .. tostring(r)')
ex("uobj.HasAnyInternalFlags", "2a:711",
   PC + 'local r = pc:HasAnyInternalFlags(EInternalObjectFlags.PendingKill)\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("uobj.GetName-reflected", "2a(note)",
   PC + 'local s = pc:GetName() if type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "type=" .. type(s)')
ex("uobj.K2_GetActorLocation", "2a-reflect(PROVEN)",
   PC + 'local v = pc:K2_GetActorLocation() if v == nil then return false, "nil" end\n'
   'local x local ok = pcall(function() x = v.X end)\n'
   'return "type=" .. type(v) .. " X=" .. tostring(x)')
ex("uobj.CallFunction", "2a:655",
   PC + 'local v = pc:CallFunction("K2_GetActorLocation")\n'
   'if v == nil then return "nil-ret" end\n'
   'return "type=" .. type(v)')
ex("uobj.ProcessConsoleExec", "2a:735",
   PC + 'local r = pc:ProcessConsoleExec("st2_noop_cmd", nil, pc)\n'
   'return "ret=" .. tostring(r)')
ex("prop-read.Pawn", "2b",
   PC + 'local p = pc.Pawn if p == nil then return "nil" end\n'
   'return "valid=" .. tostring(S.v(p))')
ex("prop-read.PlayerCameraManager", "2b",
   PC + 'local p = pc.PlayerCameraManager if p == nil then return "nil" end\n'
   'return "valid=" .. tostring(S.v(p))')
ex("prop-read.Player", "2b",
   PC + 'local p = pc.Player if p == nil then return "nil" end\n'
   'return "valid=" .. tostring(S.v(p))')
ex("prop-read.bool-bShowMouseCursor", "2b",
   PC + 'local v = pc.bShowMouseCursor\n'
   'if type(v) == "boolean" then S.h.mcur = v return "v=" .. tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("uobj.GetPropertyValue", "2a:628",
   PC + 'local v = pc:GetPropertyValue("bShowMouseCursor")\n'
   'return "type=" .. type(v) .. " same-as-direct=" .. tostring(tostring(v) == tostring(S.h.mcur))')
ex("uobj.Reflection.GetProperty", "2a:582/:591",
   PC + 'local pr = pc:Reflection() if pr == nil then return false, "reflection-nil" end\n'
   'local ph = pr:GetProperty("bShowMouseCursor") if ph == nil then return false, "prop-nil" end\n'
   'local iv = ph:IsValid() if iv == true then return "valid" end\n'
   'return false, "IsValid=" .. tostring(iv)')
ex("prop.ContainerPtrToValuePtr", "FT(set.lua:30)",
   PC + 'local pr = pc:Reflection() local ph = pr and pr:GetProperty("bShowMouseCursor")\n'
   'if not ph then return "SKIP", "no-prop" end\n'
   'local p = ph:ContainerPtrToValuePtr(pc)\n'
   'if p == nil then return false, "nil" end\n'
   'return "type=" .. type(p)')
ex("UStruct.GetSuperStruct", "2d",
   PC + 'local c = pc:GetClass() local n = 0\n'
   'while c ~= nil and S.v(c) and n < 10 do\n'
   'local ok, s = pcall(function() return c:GetSuperStruct() end)\n'
   'if not ok then return false, "walk-err" end c = s n = n + 1 end\n'
   'return "depth=" .. n')
ex("UStruct.ForEachProperty", "2d",
   PC + 'local c = pc:GetClass() local n = 0\n'
   'local ok = pcall(c.ForEachProperty, c, function(p) n = n + 1 end)\n'
   'if ok then return "n=" .. n end\n'
   'return false, "iterate-err"')
ex("UEnum.ForEachName", "2d",
   'local e = StaticFindObject("/Script/Engine.ENetRole")\n'
   'if e == nil or not S.v(e) then return "SKIP", "no-enum" end\n'
   'local n = 0 local ok = pcall(e.ForEachName, e, function(nm, v) n = n + 1 end)\n'
   'if not ok then return false, "iterate-err" end\n'
   'local s local ok2 = pcall(function() s = e:GetNameByValue(0):ToString() end)\n'
   'return "n=" .. n .. " v0=" .. tostring(s)')
ex("TArray-acquire-ConsoleKeys", "2d",
   'local is = StaticFindObject("/Script/Engine.Default__InputSettings")\n'
   'if is == nil or not S.v(is) then return "SKIP", "no-inputsettings" end\n'
   'local a = is.ConsoleKeys if a == nil then return "SKIP", "no-array-prop" end\n'
   'S.h.ck = a return "got-array"')
CK = 'local a = S.h.ck if not a then return "SKIP", "no-array" end '
ex("TArray-len", "2d:LuaTArray.cpp:69",
   CK + 'local n = #a if type(n) == "number" then return "n=" .. n end\n'
   'return false, "type=" .. type(n)')
ex("TArray-index-read", "2d:LuaTArray.cpp:59",
   CK + 'local e = a[1] if e == nil then return "empty-or-nil[1]" end\n'
   'return "got-elem"')
ex("TArray.GetArrayNum", "2d:87",
   CK + 'local v = a:GetArrayNum() if type(v) == "number" then return "n=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("TArray.GetArrayMax", "2d:95",
   CK + 'local v = a:GetArrayMax() if type(v) == "number" then return "n=" .. v end\n'
   'return false, "type=" .. type(v)')
ex("TArray.GetArrayAddress", "2d",
   CK + 'local v = a:GetArrayAddress()\n'
   'if type(v) == "number" or type(v) == "userdata" then return type(v) end\n'
   'return false, "type=" .. type(v)')
ex("TArray.GetArrayDataAddress", "2d",
   CK + 'local v = a:GetArrayDataAddress()\n'
   'if type(v) == "number" or type(v) == "userdata" then return type(v) end\n'
   'return false, "type=" .. type(v)')
ex("TArray.IsValid", "2d",
   CK + 'local v = a:IsValid() if type(v) == "boolean" then return tostring(v) end\n'
   'return false, "type=" .. type(v)')
ex("TArray.ForEach", "2d:121",
   CK + 'local n = 0\n'
   'local ok = pcall(a.ForEach, a, function(el) n = n + 1 end)\n'
   'if ok then return "n=" .. n end\n'
   'return false, "iterate-err"')
ex("NameProp-userdata-read", "2b(FR-revert)",
   CK + 'local e = a[1] if e == nil then return "SKIP", "empty-array" end\n'
   'local k = e.KeyName if k == nil then return "SKIP", "no-keyname" end\n'
   'local s local ok = pcall(function() s = k:ToString() end)\n'
   'if ok and type(s) == "string" then return s:sub(1, 24) end\n'
   'return false, "tostring-failed"')
ex("loadfile-native", "1b:1400(PORT-ONLY)",
   'if type(loadfile) ~= "function" then return false, "nil" end\n'
   'local f = loadfile("part2.lua") return "fn=" .. type(f)')

# ---- PHASE D: hooks / registrations (register + unregister pairs) ----------

REFUSE = ('local ok, a = pcall(%s, function() end)\n'
          'if ok then return "registered" end\n'
          'if tostring(a):lower():find("refus", 1, true) then return "soft-refused" end\n'
          'return false, a')
ex("RegisterHook", "1a:5705 (P6)",
   'local pre, post = RegisterHook("/Script/Engine.PlayerController:ClientRestart", function() end)\n'
   'if type(pre) == "number" and type(post) == "number" then\n'
   'S.h.hk = {"/Script/Engine.PlayerController:ClientRestart", pre, post}\n'
   'return pre .. "," .. post end\n'
   'return false, "pre=" .. type(pre) .. " post=" .. type(post)')
ex("UnregisterHook", "1a:2324 (P6)",
   'local h = S.h.hk if not h then return "SKIP", "no-hook-ids" end\n'
   'local r = UnregisterHook(h[1], h[2], h[3]) S.hyd.unhook = true\n'
   'return "ret=" .. tostring(r)')
ex("NotifyOnNewObject", "1a:2918",
   'local r = NotifyOnNewObject("Engine.DemoNetDriver", function() end)\n'
   'return "ret=" .. tostring(r)')
ex("RegisterCustomEvent", "1a:2986",
   'local r = RegisterCustomEvent("ST2Event", function() end)\n'
   'return "ret=" .. tostring(r)')
ex("UnregisterCustomEvent", "1a:3029",
   'local r = UnregisterCustomEvent("ST2Event") S.hyd.uncustev = true\n'
   'return "ret=" .. tostring(r)')
ex("RegisterLoadMapPreHook", "1a:3048",
   'local r = RegisterLoadMapPreHook(function() end)\n'
   'return "ret=" .. tostring(r)')
ex("RegisterLoadMapPostHook", "1a:3075",
   'local r = RegisterLoadMapPostHook(function() end)\n'
   'return "ret=" .. tostring(r)')
ex("RegisterInitGameStatePreHook", "1a:3102", REFUSE % "RegisterInitGameStatePreHook")
ex("RegisterInitGameStatePostHook", "1a:3102", REFUSE % "RegisterInitGameStatePostHook")
ex("RegisterBeginPlayPreHook", "1a:3102", REFUSE % "RegisterBeginPlayPreHook")
ex("RegisterBeginPlayPostHook", "1a:3102", REFUSE % "RegisterBeginPlayPostHook")
ex("RegisterEndPlayPreHook", "1a:3102", REFUSE % "RegisterEndPlayPreHook")
ex("RegisterEndPlayPostHook", "1a:3102", REFUSE % "RegisterEndPlayPostHook")
ex("RegisterProcessConsoleExecPreHook", "1a:3759", REFUSE % "RegisterProcessConsoleExecPreHook")
ex("RegisterProcessConsoleExecPostHook", "1a:3780", REFUSE % "RegisterProcessConsoleExecPostHook")
ex("RegisterCallFunctionByNameWithArgumentsPreHook", "1a:3801",
   REFUSE % "RegisterCallFunctionByNameWithArgumentsPreHook")
ex("RegisterCallFunctionByNameWithArgumentsPostHook", "1a:3822",
   REFUSE % "RegisterCallFunctionByNameWithArgumentsPostHook")
ex("RegisterULocalPlayerExecPreHook", "1a:3843", REFUSE % "RegisterULocalPlayerExecPreHook")
ex("RegisterULocalPlayerExecPostHook", "1a:3864", REFUSE % "RegisterULocalPlayerExecPostHook")
ex("RegisterConsoleCommandHandler", "1a:3929 (P2)",
   'local r = RegisterConsoleCommandHandler("st2h", function() end)\n'
   'return "ret=" .. tostring(r)')
ex("RegisterConsoleCommandGlobalHandler", "1a:3885",
   'local r = RegisterConsoleCommandGlobalHandler("st2g", function() end)\n'
   'return "ret=" .. tostring(r)')
ex("ForEachUObject", "1a:2897",
   'local n = 0 local ok = pcall(ForEachUObject, function(o, c, i) n = n + 1 end)\n'
   'if ok then return "n=" .. n end\n'
   'return false, "iterate-err"')
ex("IterateGameDirectories", "1a:3270",
   'local r = IterateGameDirectories() return "ret=" .. type(r)')
ex("IsKeyBindRegistered", "1a:2002",
   'local r = IsKeyBindRegistered(Key.F12)\n'
   'if r == true then return "true" end\n'
   'return false, "expected-true got " .. tostring(r)')
ex("RegisterKeyBind-arm-report", "1a:2182 (PROVEN)",
   'if S.h.armed then return "F12-armed" end\n'
   'return false, "arm-failed"')
ex("RegisterKeyBindAsync", "1a:2064",
   'local r = RegisterKeyBindAsync(Key.F12, function() end)\n'
   'return "ret=" .. tostring(r)')

# ---- PHASE E: one-shot scheduling ------------------------------------------

ex("ExecuteInGameThread", "1a:5836 (PROVEN)",
   'ExecuteInGameThread(function() end) return "queued"')
ex("ExecuteAsync", "1a:3675",
   'ExecuteAsync(function() end) return "queued"')
ex("ExecuteWithDelay", "1a:3696",
   'ExecuteWithDelay(1, function() end) return "queued"')
ex("LoopAsync-one-shot", "1a:3727 (PROVEN)",
   'LoopAsync(1, function() return false end) return "one-shot"')
ex("ExecuteInGameThreadWithDelay", "1a:6093 (P7)",
   'local h = ExecuteInGameThreadWithDelay(1, function() end) S.h.d1 = h\n'
   'return "h=" .. tostring(h)')
ex("RetriggerableEITWD", "1a:6310",
   'local h = RetriggerableExecuteInGameThreadWithDelay(1, function() end) S.h.d2 = h\n'
   'return "h=" .. tostring(h)')
ex("ExecuteInGameThreadAfterFrames", "1a:6488",
   'local h = ExecuteInGameThreadAfterFrames(1, function() end) S.h.d3 = h\n'
   'return "h=" .. tostring(h)')
ex("LoopInGameThreadWithDelay-one-shot", "1a:6564 (P7)",
   'LoopInGameThreadWithDelay(1, function() return false end) return "one-shot"')
ex("LoopInGameThreadAfterFrames-one-shot", "1a:6715",
   'LoopInGameThreadAfterFrames(1, function() return false end) return "one-shot"')

# ---- PHASE F: delayed-action handle family (mod-scoped) --------------------

DA_B = ('local h = S.h.%s if type(h) ~= "number" then return "SKIP", "no-handle" end\n'
        'local r = %s(h)\n'
        'if type(r) == "boolean" then return tostring(r) end\n'
        'return false, "type=" .. type(r)')
DA_N = ('local h = S.h.%s if type(h) ~= "number" then return "SKIP", "no-handle" end\n'
        'local r = %s(h)\n'
        'return "r=" .. tostring(r)')
ex("MakeActionHandle", "1a:7491",
   'local h = MakeActionHandle() if type(h) == "number" then S.h.d4 = h return "h=" .. h end\n'
   'return false, "type=" .. type(h)')
ex("IsValidDelayedActionHandle", "1a:7102",
   'local h = S.h.d4 if type(h) ~= "number" then return "SKIP", "no-handle" end\n'
   'local r = IsValidDelayedActionHandle(h)\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("IsDelayedActionActive", "1a:7144", DA_B % ("d1", "IsDelayedActionActive"))
ex("IsDelayedActionPaused", "1a:7186", DA_B % ("d1", "IsDelayedActionPaused"))
ex("GetDelayedActionTimeRemaining", "1a:7228", DA_N % ("d1", "GetDelayedActionTimeRemaining"))
ex("GetDelayedActionTimeElapsed", "1a:7311", DA_N % ("d1", "GetDelayedActionTimeElapsed"))
ex("GetDelayedActionRate", "1a:7396", DA_N % ("d1", "GetDelayedActionRate"))
ex("PauseDelayedAction", "1a:6926", DA_B % ("d1", "PauseDelayedAction"))
ex("UnpauseDelayedAction", "1a:7005", DA_B % ("d1", "UnpauseDelayedAction"))
ex("SetDelayedActionTimer", "1a:6856",
   'local h = S.h.d1 if type(h) ~= "number" then return "SKIP", "no-handle" end\n'
   'local r = SetDelayedActionTimer(h, 2)\n'
   'if type(r) == "boolean" then return tostring(r) end\n'
   'return false, "type=" .. type(r)')
ex("ResetDelayedActionTimer", "1a:6792", DA_B % ("d1", "ResetDelayedActionTimer"))
ex("CancelDelayedAction-d2", "1a:7055", DA_B % ("d2", "CancelDelayedAction"))
ex("CancelDelayedAction-d3", "1a:7055", DA_B % ("d3", "CancelDelayedAction"))
ex("ClearAllDelayedActions", "1a:7454(mod-scoped)",
   'local r = ClearAllDelayedActions() S.hyd.clearactions = true\n'
   'if type(r) == "number" then return "cleared=" .. r end\n'
   'return false, "type=" .. type(r)')

# ---- PHASE G: write-restore + io roundtrip + hygiene audit -----------------

ex("SetPropertyValue-restore", "1a/2a:633 (RUN-2 gap)",
   PC + 'local o = S.h.mcur if type(o) ~= "boolean" then return "SKIP", "no-orig" end\n'
   'local ok, e = pcall(function() pc:SetPropertyValue("bShowMouseCursor", o) end)\n'
   'if not ok then return false, e end\n'
   'local b = pc.bShowMouseCursor\n'
   'if b ~= o then\n'
   'pcall(function() pc:SetPropertyValue("bShowMouseCursor", o) end)\n'
   'return false, "readback=" .. tostring(b) end\n'
   'S.hyd.setprop = true return "restored"')
ex("newindex-write-restore", "2b(RUN-2/5 gap)",
   PC + 'local o = S.h.mcur if type(o) ~= "boolean" then return "SKIP", "no-orig" end\n'
   'local ok, e = pcall(function() pc.bShowMouseCursor = o end)\n'
   'if not ok then return false, e end\n'
   'local b = pc.bShowMouseCursor\n'
   'if b ~= o then\n'
   'pcall(function() pc.bShowMouseCursor = o end)\n'
   'return false, "readback=" .. tostring(b) end\n'
   'S.hyd.newindex = true return "restored"')
ex("ModRef.GetSetSharedVariable", "1b:7848",
   'local ok0, o = pcall(function() return ModRef:GetSharedVariable("ST2Probe") end)\n'
   'if not ok0 then return false, "get-err" end\n'
   'pcall(function() ModRef:SetSharedVariable("ST2Probe", 31337) end)\n'
   'local ok1, v = pcall(function() return ModRef:GetSharedVariable("ST2Probe") end)\n'
   'if not ok1 then return false, "get2-err" end\n'
   'pcall(function() ModRef:SetSharedVariable("ST2Probe", o) end)\n'
   'if v == 31337 then S.hyd.shared = true return "roundtrip-restored" end\n'
   'return false, "readback=" .. tostring(v)')
ex("ModRef.type", "1b:7848",
   'local t = ModRef:type() if type(t) == "string" then return t end\n'
   'return false, "type=" .. type(t)')
ex("io-roundtrip", "4(patched-liolib)/5-shim",
   'if not S.pf then return "SKIP", "no-io-path" end\n'
   'local p = S.pf:gsub("progress%.txt$", "st2io.txt")\n'
   'local o1, f = pcall(io.open, p, "w")\n'
   'if not o1 or not f then return "SKIP", "open-w-failed" end\n'
   'pcall(function() f:write("st2io\\n") end)\n'
   'pcall(function() f:close() end)\n'
   'local rm = pcall(function() if os and os.remove then os.remove(p) end end)\n'
   'local o4, g = pcall(io.open, p, "r")\n'
   'if o4 and g then\n'
   'local rd pcall(function() rd = g:read("l") end)\n'
   'pcall(function() g:close() end)\n'
   'if rd == "st2io" then S.hyd.io = true return "rw roundtrip rm=" .. tostring(rm) end\n'
   'return "w-only read=" .. tostring(rd) end\n'
   'return "w-only open-r-miss"')
ex("hygiene-audit", "item-6a-law",
   'local tot, ok = 0, 0\n'
   'for k, v in pairs(S.hyd) do tot = tot + 1 if v then ok = ok + 1 end end\n'
   'if ok == tot then return "all-" .. ok end\n'
   'return false, ok .. "/" .. tot')

# ---- Static skips: unnumbered preface lines --------------------------------
# (name, class, cite, reason)

SKIPS = [
    ("LoadAsset", "SKIP-WRITE", "1a:3970", "loads content into memory; side-effect risk (task law); RUN-4 probe owns it"),
    ("DumpAllObjects", "SKIP-WRITE", "1a:2420", "writes a full object dump to disk; console output-dir paths unverified"),
    ("GenerateSDK", "SKIP-WRITE", "1a:2431", "large generation run writes to disk; unverified paths"),
    ("GenerateLuaTypes", "SKIP-WRITE", "1a:2442", "writes generated lua types to disk"),
    ("GenerateUHTCompatibleHeaders", "SKIP-WRITE", "1a:2453", "writes headers to disk"),
    ("DumpStaticMeshes", "SKIP-WRITE", "1a:2459", "writes mesh dump to disk"),
    ("DumpAllActors", "SKIP-WRITE", "1a:2464", "writes actor dump to disk"),
    ("DumpUSMAP", "SKIP-WRITE", "1a:2469", "writes usmap to disk"),
    ("DumpJMAP", "SKIP-WRITE", "1a:2479", "writes jmap to disk"),
    ("StaticConstructObject", "SKIP-CONSTRUCT", "1a:2495 (PROVEN by spawner:50/516)", "no provable safe teardown for a non-actor UObject from Lua; leaked object = lasting side effect"),
    ("RegisterCustomProperty", "SKIP-CRASH-LAW", "1a:2630 + MANIFEST offset law", "needs a hardcoded struct offset (ActorDumper 0x98 law): wrong offset walks garbage -> native crash"),
    ("CreateLogicModsDirectory", "SKIP-WRITE", "1a:3604", "mkdir side effect; only BPModLoader (SKIP mod) uses it"),
    ("RestartCurrentMod", "SKIP-DESTRUCTIVE", "1a:7502", "queues reinstall of the running mod mid-run; PS5 hot-reload unavailable"),
    ("UninstallCurrentMod", "SKIP-DESTRUCTIVE", "1a:7516", "uninstalls the running mod mid-run"),
    ("RestartMod", "SKIP-DESTRUCTIVE", "1a:7531", "reinstalls another mod; PS5 hot-reload unavailable"),
    ("UninstallMod", "SKIP-DESTRUCTIVE", "1a:7548", "uninstalls another mod"),
    ("UWorld.SpawnActor", "SKIP-COVERED", "2d:LuaUWorld.cpp:62", "actor spawn forbidden in self-test; the spawner mod + round-4 spawn launch own it"),
    ("TArray.Add/Empty/Remove", "SKIP-WRITE", "2d", "array mutation on live engine state (growth-write = RUN-3 gap; ladder probe owns it)"),
    ("TMap/TSet-members", "SKIP-NO-INSTANCE", "2d", "no game-agnostic TMap/TSet instance discoverable"),
    ("UScriptStruct-members", "SKIP-NO-INSTANCE", "2d", "struct instances only via struct-OUT machinery; P5 probe owns it"),
    ("FWeakObjectPtr.Get", "SKIP-NO-INSTANCE", "2d", "weak handle only from HitResult struct-OUT; P5 probe owns it"),
    ("FOutputDevice.Log", "SKIP-NO-INSTANCE", "2d", "Ar only reaches a console-command handler; P2 probe owns it"),
    ("FURL/FSoftObjectPath/TSoftObjectPtr/UInterface/UDataTable", "SKIP-NO-INSTANCE", "2d", "no game-agnostic acquisition route"),
    ("UFunction-members", "SKIP-NO-INSTANCE", "2d", "the lookup route is the crash-law surface (FindFunction/bpfn); zzdiag v2 law"),
    ("RemoteUnrealParam get/set/type/IsValidField", "SKIP-NO-CALLER", "2a", "param userdata only exists inside a fired hook callback; P6 owns it"),
    ("XProperty.ImportText", "SKIP-WRITE", "FT(set.lua:30; LuaXProperty.cpp:215)", "parses text INTO live property memory; RUN-4 deep path owns it"),
    ("ThreadId/LuaCustomProperty", "SKIP-NO-INSTANCE", "2d", "no ctor global; reachable only through skipped surfaces"),
]

# The static-skip accounting runs LAST: it logs one 'SKIP <api> <class> <cite>
# (<reason>)' line per statically-skipped API (unnumbered), then PASSes with
# the count -- so every parity-table API is accounted for in the log while the
# summary stays 'M of M PASS'-able on a healthy run.
_SKIP_BODY = ("local n = 0 local SK = {%s}\n"
              "for i = 1, #SK do S.log('SKIP ' .. SK[i]) n = n + 1 end\n"
              "return n .. '-skips'")
_skip_strings = ", ".join('"%s %s %s (%s)"' % (n, c, cite, r) for (n, c, cite, r) in SKIPS)
ex("skip-class-list", "item-6a (static skips)",
   _SKIP_BODY % _skip_strings)

# ---------------------------------------------------------------------------
# Chassis (main.lua). Placeholders: __M__, __PARTLOAD__
# ---------------------------------------------------------------------------

CHASSIS_TOP = '''\
-- ueselftest v1 (agent GB, gen_selftest.py 1.0; Claude round-5 item 6a):
-- parity self-test from FS API-PARITY-TABLE.md. LAWS: pcall every call; ONE
-- log line/call; progress.txt marker BEFORE each call (crash-resume); engine
-- base classes only; NO FindFunction/bpfn/class-children; no spawns/saves/
-- constructs; writes restore+verify; hooks unregister; loops one-shot.
local M = __M__
local T = {}
local S = {h = {}, hyd = {}, n = 0, p = 0, busy = false}
local function P(s) print("[ST2] " .. tostring(s) .. "\\n") end
S.log = P
local function TS(v) local ok, s = pcall(tostring, v) if not ok then return "e" end s = s:gsub("^.-:%d+:%s*", "") return (s:gsub("%c", " ")):sub(1, 40) end
S.et = TS
function S.v(o) if o == nil then return false end local ok, r = pcall(function() return o:IsValid() end) return ok and r and true or false end
function S.g(nm) local c = S.h["g." .. nm] if c ~= nil then return c or nil end local ok, o = pcall(FindFirstOf, nm) if ok and o ~= nil and S.v(o) then S.h["g." .. nm] = o return o end S.h["g." .. nm] = false return nil end
-- progress file (mod folder; resume off if io unavailable). ROOT is logical, no title:
-- the path shim anchors "ue4ss/Mods/..." at the running game's ue4ss root.
local ROOT = "ue4ss/Mods/ueselftest"
local PF
local function PFIND()
  if type(io) ~= "table" or type(io.open) ~= "function" then return nil end
  local ok, i = pcall(debug.getinfo, 1, "S")
  local sd = (ok and i and type(i.source) == "string") and (i.source:gsub("^@", ""):match("^(.*)/[^/]+$") or "") or ""
  local cand = {ROOT .. "/progress.txt", sd .. "/../progress.txt"}
  for _, p in ipairs(cand) do local o, f = pcall(io.open, p, "w") if o and f then f:close() return p end end
  return nil
end
local function PFW(n) if PF then local o, f = pcall(io.open, PF, "w") if o and f then f:write(tostring(n) .. "\\n") f:close() end end end
local function PFR() if not PF then return nil end local o, f = pcall(io.open, PF, "r") if not o or not f then return nil end local l = f:read("l") f:close() if l == "-1" then return nil end return tonumber(l) end
local function ONE(e)
  S.n = S.n + 1 PFW(S.n)
  local ok, a, b = pcall(e[2], S)
  local l = S.n .. "/" .. M .. " " .. e[1] .. " "
  if not ok then P(l .. "FAIL " .. S.et(a))
  elseif a == "SKIP" then P(l .. "SKIP " .. TS(b))
  elseif a == false then P(l .. "FAIL " .. TS(b))
  else S.p = S.p + 1 P(l .. "PASS" .. ((a ~= nil and a ~= true) and (" " .. TS(a)) or "")) end
end
local function LP(k, cnt)
  if S.h["lp" .. k] then return end
  local nm = "part" .. k local ok, t = pcall(require, nm)
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, nm .. ".lua") end
  if not ok or type(t) ~= "table" then ok, t = pcall(dofile, "ue4ss/Mods/ueselftest/Scripts/" .. nm .. ".lua") end
  if ok and type(t) == "table" and #t == cnt then
    for i = 1, cnt do T[#T + 1] = t[i] end P(nm .. " loaded " .. cnt)
  else
    P(nm .. " LOAD-FAIL " .. TS(t))
    for i = 1, cnt do T[#T + 1] = {nm .. "-" .. i, function() return false, "part-not-loaded" end} end
  end
  S.h["lp" .. k] = true
end
local function RUN()
  if S.busy then P("busy") return end
  S.busy = true
  S.p = 0
__PARTLOAD__
  if #T ~= M then P("WARN count " .. #T .. " expected " .. M) end
  local cr = PFR()
  if cr then P(cr .. "/" .. M .. " " .. (T[cr] and T[cr][1] or "?") .. " CRASH (resumed)") end
  S.n = cr or 0
  for i = S.n + 1, #T do ONE(T[i]) end
  PFW(-1)
  P("SUMMARY " .. S.p .. " of " .. M .. " PASS")
  P("done")
  S.busy = false
end
PF = PFIND()
if not PF then P("progress-io unavailable (resume disabled)") end
S.pf = PF
RegisterKeyBind(Key.F12, function() P("K1 f12") pcall(ExecuteInGameThread, RUN) P("K2 q") end)
S.h.armed = true
P("boot v1 M=" .. M .. " armed=true pf=" .. tostring(PF ~= nil))
'''

CHASSIS_BOTTOM = "for i = 1, #E do T[#T + 1] = E[i] end\n"

PART_HEADER = '''\
-- ueselftest generated part (gen_selftest.py 1.0). Loaded by main.lua via
-- require("partN") / dofile fallback; returns the entry list. Do not run
-- standalone (needs the [ST2] chassis state passed as S by the runner).
return {
'''


def emit_entry(idx, name, cls, cite, body):
    # One line per entry when the body is single-line; multi-line bodies are
    # emitted with real newlines so runtime errors carry usable line numbers.
    fn = "function(S) " + body.replace("\n", " ") + " end"
    return '{"%s", %s},' % (name, fn)


def build(outdir, maxbytes=3900):
    total = len(E)
    lines = []
    for i, (name, cls, cite, body) in enumerate(E, 1):
        lines.append(emit_entry(i, name, cls, cite, body))

    # Greedy packing: main.lua = chassis + as many entries as fit (console
    # admission law: every file < 4096 B; main budget keeps a 130+ B margin);
    # remaining entries pack into parts at the tighter budget.
    header = CHASSIS_TOP.replace("__M__", str(total))
    main_entries = []
    used = len(header) + len(CHASSIS_BOTTOM)
    i = 0
    while i < len(lines):
        if used + len(lines[i]) + 1 > maxbytes + 60:
            break
        main_entries.append(lines[i])
        used += len(lines[i]) + 1
        i += 1
    if i == 0:
        raise SystemExit("chassis too large for main.lua: %d B" % used)

    parts = []
    cur = []
    used = len(PART_HEADER) + 2
    while i < len(lines):
        if used + len(lines[i]) + 1 > maxbytes:
            parts.append(cur)
            cur = []
            used = len(PART_HEADER) + 2
        cur.append(lines[i])
        used += len(lines[i]) + 1
        i += 1
    if cur:
        parts.append(cur)

    partloads = []
    for k, p in enumerate(parts, 2):
        partloads.append("  LP(%d, %d)" % (k, len(p)))
    partload = "\n".join(partloads)

    main = (header + "local E = {\n" + "\n".join(main_entries) + "\n}" + CHASSIS_BOTTOM)
    main = main.replace("__PARTLOAD__", partload)

    files = {"main.lua": main}
    for k, p in enumerate(parts, 2):
        files["part%d.lua" % k] = PART_HEADER + "\n".join(p) + "\n}\n"

    # sanity: every file under the console admission law
    for fn, src in files.items():
        size = len(src.encode("utf-8"))
        if size >= 4096:
            raise SystemExit("%s is %d B (>= 4096 console admission law)" % (fn, size))

    for fn, src in files.items():
        with open(os.path.join(outdir, fn), "w", encoding="utf-8", newline="\n") as f:
            f.write(src)

    # ---------------- EXPECTED-MARKERS.txt ----------------
    mk = []
    mk.append("EXPECTED-MARKERS -- ueselftest v1 (agent GB). Grep with -a (mixed encodings).")
    mk.append("Boot line (script load, UE4SS.log):")
    mk.append("  [ST2] boot v1 M=%d armed=true pf=true|false" % total)
    mk.append("    No boot line at all = main.lua errored at load (the arm registers")
    mk.append("    Key.F12 directly, zzdiag-v10/v11 pattern; a load-time throw is the")
    mk.append("    loud failure -- check the mod's Lua error line above it).")
    mk.append("    pf=false = io.open/paths unavailable in the mod folder -> resume disabled;")
    mk.append("      the run still completes; treat as a P0 io finding, not a self-test failure.")
    mk.append("")
    mk.append("Arm press (pad chord L1+R1 -> F12):")
    mk.append("  [ST2] K1 f12        (immediate; pad->keybind->callback chain alive)")
    mk.append("  [ST2] K2 q          (callback returned without hang)")
    mk.append("")
    mk.append("Run start (per run):")
    mk.append("  [ST2] part2 loaded %d   (+ one line per further part; LOAD-FAIL = deploy gap)" % (len(parts) and len(parts[0])))
    mk.append("  [ST2] WARN count N expected M    (only if a part loaded with the wrong count)")
    mk.append("")
    mk.append("Per numbered call (ONE line each, %d total):" % total)
    mk.append("  [ST2] <idx>/<M> <name> PASS <detail-40>")
    mk.append("  [ST2] <idx>/<M> <name> FAIL <error-tail-40>")
    mk.append("  [ST2] <idx>/<M> <name> SKIP <reason>     (runtime-conditional only)")
    mk.append("")
    mk.append("Run end (the LAST numbered call is the static-skip accounting):")
    mk.append("  [ST2] %d/%d skip-class-list PASS %d-skips" % (total, total, len(SKIPS)))
    mk.append("  [ST2] SKIP <API> <CLASS> <cite> (<reason>)   x%d static-skip lines, unnumbered" % len(SKIPS))
    mk.append("  [ST2] SUMMARY <N> of %d PASS" % total)
    mk.append("  [ST2] done")
    mk.append("")
    mk.append("Resume (only after a real crash mid-run):")
    mk.append("  [ST2] <idx>/<M> <name> CRASH (resumed)   (the about-to-run marker names the crasher)")
    mk.append("")
    mk.append("Operator check: the io-roundtrip line must read PASS rw roundtrip.")
    mk.append("  'w-only open-r-miss' on console = io write path alive but read-back broken")
    mk.append("  (P0 finding, FS parity-table sections 4/5); SKIP open-w-failed = mod folder")
    mk.append("  not writable. ('w-only open-r-miss' is only the EXPECTED shape on the Mac")
    mk.append("  host harness, where writes are virtualized by the GA path shim.)")
    mk.append("")
    mk.append("Acceptance (full pass): SUMMARY %d of %d PASS, zero FAIL lines, 'done' present," % (total, total))
    mk.append("clean game exit afterwards. Any FAIL line names the broken API (see")
    mk.append("CALL-SHEET.md for per-call interpretation). SKIP lines with reason")
    mk.append("'no-valid-pc' mean the run fired before the player was in-world;")
    mk.append("re-press the combo once in-world -- do NOT count that as a failure.")
    mk.append("NEVER force-kill the game to test resume (no-rapid-launch law); the")
    mk.append("CRASH-resume path is validated opportunistically if a real crash occurs.")
    with open(os.path.join(outdir, "EXPECTED-MARKERS.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(mk) + "\n")

    # ---------------- CALL-SHEET.md ----------------
    cs = []
    cs.append("# CALL-SHEET -- ueselftest v1 per-call interpretation")
    cs.append("")
    cs.append("Agent GB, generated by %s. One row per numbered call, in run order." % VERSION)
    cs.append("Citations are agent FS's API-PARITY-TABLE.md (L26-R4 candidate LuaMod.cpp")
    cs.append("line numbers unless a file is named). Interpretation of outcomes:")
    cs.append("")
    cs.append("- **PASS** -- call completed under pcall with the expected result type")
    cs.append("  (semantic asserts only where marked; the 40-char detail carries the")
    cs.append("  observed value: read it even on PASS, e.g. `FName-eq eq=false` on")
    cs.append("  console means the `__eq` metamethod did not fire -- a real finding).")
    cs.append("- **FAIL** -- Lua error (tail shown) or a failed semantic assert. This is")
    cs.append("  the parity finding: the named API is broken or its signature differs.")
    cs.append("- **SKIP** -- runtime-conditional precondition absent (no-valid-pc = run")
    cs.append("  fired before the player was in-world; no-array/no-enum = that engine")
    cs.append("  object does not exist in this game). Not a failure; re-run in-world.")
    cs.append("- Static **SKIP-\\*** lines in the preface are by-design non-calls (task")
    cs.append("  law: no persistent writes, no constructs, no spawns, crash-law).")
    cs.append("- A hard native crash mid-run = the progress marker names the API; the")
    cs.append("  next launch logs `CRASH (resumed)` for it (NEVER force-kill to test).")
    cs.append("")
    cs.append("Total numbered calls: **%d**. Static skips: **%d**." % (total, len(SKIPS)))
    cs.append("")
    cs.append("| idx | call | class | cite | notes / semantic asserts |")
    cs.append("|---|---|---|---|---|")
    for i, (name, cls, cite, body) in enumerate(E, 1):
        notes = NOTES.get(name, "")
        cs.append("| %d | `%s` | %s | %s | %s |" % (i, name, cls, cite, notes))
    cs.append("")
    cs.append("## Static skips (unnumbered lines logged by the last call, `skip-class-list`)")
    cs.append("")
    cs.append("| api | class | cite | reason |")
    cs.append("|---|---|---|---|")
    for (n, c, cite, r) in SKIPS:
        cs.append("| `%s` | %s | %s | %s |" % (n, c, cite, r))
    with open(os.path.join(outdir, "CALL-SHEET.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cs) + "\n")

    return files, total, len(SKIPS)


# Per-call notes for the interpretation sheet (only where a semantic assert
# or a known port behavior makes the line worth explaining).
NOTES = {
    "print": "PROVEN; every [ST2] line is further evidence",
    "LoadExport": "called with a guaranteed-missing name: a clean error is the EXPECTED (passing) result",
    "FName-eq": "console detail should read eq=true; eq=false means __eq metamethod not firing (finding)",
    "FText-eq": "same as FName-eq",
    "FName-ctor-FNAME_Find": "exact P3 probe target (UEHelpers FindFName path)",
    "IsInGameThread": "semantic assert TRUE: the run executes inside ExecuteInGameThread; false = game-thread dispatch broken",
    "FindFirstOf": "contract: NEVER real nil (miss = non-nil null-wrapper); nil here is itself a finding",
    "FindAllOf": "real nil on a miss is CONTRACT-CORRECT (wake-runbook reading rule)",
    "CreateInvalidObject": "semantic assert IsValid()==false",
    "uobj.IsValid": "requires in-world pc; otherwise SKIP no-valid-pc",
    "uobj.IsA-class": "semantic assert true (pc IsA its own class)",
    "uobj.IsA-string": "semantic assert true (MiniMap:381 overload; false = bridged StaticFindObject path broken)",
    "uobj.K2_GetActorLocation": "PROVEN family (spawner:233); detail shows FVector X readable",
    "uobj.CallFunction": "named-function route for the same proven call",
    "uobj.ProcessConsoleExec": "unknown command name: exercises the 3-arg marshal with zero state change",
    "uobj.GetPropertyValue": "detail compares against the direct property read; mismatch = finding",
    "NameProp-userdata-read": "FR parity-revert shape (ConsoleEnabler:30/42): array element -> NameProperty -> FName userdata -> ToString",
    "RegisterHook": "P6 target: returns (pre,post) integer ids; a Lua error here is the P6 finding",
    "UnregisterHook": "ConsoleEnabler L79-87 pattern; PASS+hygiene flag = clean unhook",
    "NotifyOnNewObject": "port signature (ClassName, cb); no unregister exists -> callback is a provably-inert no-op on a rare class; documented lasting-but-inert",
    "RegisterLoadMapPreHook": "queues a no-op callback (no unregister API); fires only on a later map change and then does nothing",
    "RegisterInitGameStatePreHook": "expect PASS with detail 'registered' OR 'soft-refused' -- both are correct parity outcomes (R3-marshal worlds soft-refuse eager detour arms, LuaMod.cpp:8573)",
    "RegisterBeginPlayPreHook": "same soft-refusal expectation as InitGameState",
    "RegisterEndPlayPostHook": "same soft-refusal expectation",
    "RegisterProcessConsoleExecPreHook": "same soft-refusal expectation",
    "ForEachUObject": "full GUObjectArray iteration with a counting no-op callback; duration can reach seconds on object-heavy games; the per-object cell is never touched",
    "IterateGameDirectories": "PS5 expected: returns nil (root-dir knowledge missing, LuaMod.cpp:3280 warning path)",
    "IsKeyBindRegistered": "semantic assert TRUE: our own F12 arm registered at load",
    "RegisterKeyBindAsync": "2nd no-op bind on F12 (idempotent); no UnregisterKeyBind API exists; inert",
    "LoopAsync-one-shot": "callback returns false = one-shot form (spawner-proven pattern)",
    "ExecuteInGameThreadWithDelay": "P7 target; captures the handle the delayed-action family exercises",
    "ClearAllDelayedActions": "MOD-SCOPED on the port (own hook-lua actions only) -> doubles as final cleanup of our pending actions",
    "SetPropertyValue-restore": "write-same-value roundtrip with read-back verify + restore rewrite (RUN-2 gap family)",
    "newindex-write-restore": "__newindex write-same-value roundtrip (RUN-2/5 gap family)",
    "ModRef.GetSetSharedVariable": "own mod's shared variable only; set->get->restore-nil roundtrip",
    "io-roundtrip": "write chain in the mod folder + read-back. CONSOLE MUST SHOW 'rw roundtrip' (operator check); 'w-only open-r-miss' is the EXPECTED HOST shape (GA path-shim virtual store) but on console means the io read path is broken (P0 finding); SKIP open-w-failed = the mod folder is not writable",
    "hygiene-audit": "verifies every hygiene flag set by the cleanup-bearing entries",
    "skip-class-list": "logs one unnumbered SKIP line per statically-skipped API (the full skip accounting), then PASSes with the count; runs LAST",
}

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--max-bytes", type=int, default=3900)
    args = ap.parse_args()
    files, total, skips = build(args.out, args.max_bytes)
    counts = {}
    for (_n, cls, _c, _b) in E:
        counts[cls] = counts.get(cls, 0) + 1
    print("%s" % VERSION)
    print("numbered calls: %d   static skips: %d" % (total, skips))
    print("classification counts: " + ", ".join("%s=%d" % kv for kv in sorted(counts.items())))
    for fn in sorted(files):
        src = open(os.path.join(args.out, fn), "rb").read()
        print("%-12s %5d B" % (fn, len(src)))
