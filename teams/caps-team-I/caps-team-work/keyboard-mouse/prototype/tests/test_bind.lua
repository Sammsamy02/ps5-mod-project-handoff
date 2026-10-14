-- Team I2 host test for probe/zzkbbind/main.lua (Mac-only, plain Lua).
local path = assert(arg[1], "usage: lua test_bind.lua <main.lua>")
local checks, failures = 0, 0
local function check(c, what) checks = checks + 1 if not c then failures = failures + 1 print("  FAIL " .. what) end end
local fh = assert(io.open(path, "rb")) local src = fh:read("a") fh:close()
check(#src < 4096, "size < 4096") check(not src:find("[\128-\255]"), "ASCII only")
local log, binds = {}, {}
local env = setmetatable({}, {__index = _G})
env.print = function(s) log[#log + 1] = (s:gsub("\n$", "")) end
env.Key = setmetatable({}, {__index = function(_, k) return k end})
env.ModifierKey = setmetatable({}, {__index = function(_, k) return k end})
env.RegisterKeyBind = function(key, a, b)
  local mods, cb = a, b
  if type(a) == "function" then mods, cb = {}, a end
  binds[#binds + 1] = {key = key, mods = table.concat(mods, "+"), cb = cb}
end
assert(load(src, "=main.lua", "t", env))()
check(#binds == 4, "4 binds")
check(log[#log] == "[kbb] Z0 v1 binds=4", "startup line")
local want = {"F8|", "F8|SHIFT", "NUM_FIVE|", "RIGHT_MOUSE_BUTTON|"}
for i, w in ipairs(want) do check(binds[i] and (binds[i].key .. "|" .. binds[i].mods) == w, "bind " .. w) end
for _, b in ipairs(binds) do b.cb() end
check(log[#log] == "[kbb] FIRE RIGHT_MOUSE_BUTTON n=4", "FIRE lines")
print(string.format("test_bind: size=%d checks=%d failures=%d", #src, checks, failures))
if checks == 0 or failures > 0 then os.exit(1) end
