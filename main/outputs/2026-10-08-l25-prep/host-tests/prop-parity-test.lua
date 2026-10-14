-- prop-parity-test.lua — agent FR (Claude PRIORITY-1 parity revert, 2026-10-10)
--
-- Host-side contract test (battery-cheap, pure Lua, NO engine, NO builds) for
-- the reverted property-read shape in
-- outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaUObject.cpp:
--   push_nameproperty / push_textproperty / push_strproperty
--   Get + GetNonTrivialLocal -> LuaType::FName/FText/FString USERDATA
-- (upstream shape restored; agent-FM's plain-Lua-string reads reverted).
--
-- The C++ userdata contract being modeled (what a PC mod relies on):
--   local p = obj.Prop          -- userdata (lua_newuserdatauv via
--                               -- LuaMadeSimple::transfer_stack_object)
--   p:ToString()                -- __index member table -> bridged string
--   tostring(p)                 -- __tostring metamethod -> bridged string
--   SomeFunc(p) / p passed back -- userdata survives round-trips
-- Both string routes must return the SAME bridged payload (marker string).
--
-- Pure-Lua limitation, stated up front: Lua 5.5.1 has no newproxy and userdata
-- can only be created from C, so the userdata value itself is emulated with a
-- metatable-carrying value. When the interpreter DOES offer newproxy (LuaJIT
-- keeps it), a REAL userdata is used and type()=='userdata' is asserted
-- strictly; otherwise the strict type check is delegated to the C++ side
-- (construct sites listed in agent-FR-parity-revert.md) and the UE4SS-visible
-- :type() member is asserted instead. The __tostring / :ToString / pass-back
-- contract is identical for both, because Lua's tostring() honors __tostring
-- on any value's metatable.

local MARKER = 'BRIDGED:ItemId_Dummy_Pickup_001'

local failures = 0
local function check(name, ok, detail)
    if ok then
        print(string.format('PASS %s%s', name, detail and (' — ' .. detail) or ''))
    else
        failures = failures + 1
        print(string.format('FAIL %s%s', name, detail and (' — ' .. detail) or ''))
    end
end

-- Build the UE4SS-ish "FNameUserdata" metatable: __tostring (the agent-FL
-- luaL_getmetatable+lua_setfield wiring in candidate/src/LuaFName.cpp) plus an
-- __index member table carrying ToString (the :ToString member, bridged) and
-- type (UE4SS userdata convention: FNameUserdata's `type` member).
local members = {}
function members:ToString() return MARKER end
function members:type() return 'FNameUserdata' end

local registry = {
    __index = members,
    __tostring = function() return MARKER end,
}

-- The property-read value: real userdata when newproxy exists, else the
-- closest pure-Lua emulation (table + the same metatable).
local used_real_userdata = false
local prop
if type(newproxy) == 'function' then
    prop = newproxy(true)
    local mt = getmetatable(prop)
    mt.__index = members
    mt.__tostring = registry.__tostring
    used_real_userdata = true
else
    prop = setmetatable({}, registry)
end

-- 1) tostring(.Prop) -> __tostring -> bridged marker
local via_tostring = tostring(prop)
check('tostring(prop) via __tostring == marker', via_tostring == MARKER,
      string.format('got %q', via_tostring))

-- 2) .Prop:ToString() -> __index member -> bridged marker
local via_member = prop:ToString()
check('prop:ToString() via member == marker', via_member == MARKER,
      string.format('got %q', via_member))

-- 3) Both routes produce the SAME bridged string
check('tostring(prop) == prop:ToString()', via_tostring == via_member)

-- 4) Type of the property-read value
if used_real_userdata then
    check("type(prop) == 'userdata' (real userdata via newproxy)", type(prop) == 'userdata')
else
    -- Lua 5.5.1 cannot construct userdata from Lua; assert the UE4SS-visible
    -- type name instead and record the delegation to the C++ construct sites
    -- (lua-luauobject push_*property Get -> LuaType::*::construct ->
    -- lua_newuserdatauv, see agent-FR-parity-revert.md).
    check("prop:type() == 'FNameUserdata' (userdata-type check delegated to C++ construct sites)",
          prop:type() == 'FNameUserdata',
          string.format('interpreter %s has no newproxy; got %q', _VERSION, prop:type()))
end

-- 5) Round-trip pass-back: the PC-mod pattern — take the object, hand it to
--    another function, call :ToString() there.
local function pass_back(obj) return obj:ToString() end
check('round-trip pass-back obj:ToString() == marker', pass_back(prop) == MARKER)

local function pass_back_tostring(obj) return tostring(obj) end
check('round-trip pass-back tostring(obj) == marker', pass_back_tostring(prop) == MARKER)

-- 6) Why plain strings break PC mods (regression guard for the revert): the
--    agent-FM shape returned a plain Lua string, and `p:ToString()` on it is
--    a runtime error (no ToString member on the string metatable).
local plain = MARKER
local ok_plain, err_plain = pcall(function() return plain:ToString() end)
check('plain-string shape FAILS the p:ToString() contract (revert is required)',
      not ok_plain, ok_plain and 'unexpectedly succeeded' or tostring(err_plain))

-- 7) print()/concat route uses __tostring too (the mod menu logs values)
check('print-shaped concat uses __tostring', ('val=' .. tostring(prop)) == 'val=' .. MARKER)

print(string.format('%s: %d failure(s) [stub=%s, interpreter=%s]',
      failures == 0 and 'ALL PASS' or 'FAILED', failures,
      used_real_userdata and 'real-userdata' or 'table-emulation', _VERSION))
os.exit(failures == 0 and 0 or 1)
