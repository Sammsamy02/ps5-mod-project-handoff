-- test_selftest_root.lua - Mac-only. Where does the self-test's progress
-- file land on a game that is NOT Townfall?  Uses the host mirror of the
-- path shim (host-lua-harness/path-shim.lua, read-only; it mirrors
-- LuaPathShim.cpp normalize_logical :187-311 and derive_writable_root
-- :476-483). Prints how many checks it ran; exits 1 on any failure or zero.
-- Usage: lua test_selftest_root.lua <path-shim.lua>
local shim_path = arg[1]
local PathShim = dofile(shim_path)
local checks, failed = 0, 0
local function check(cond, what)
  checks = checks + 1
  if cond then print("  PASS " .. what) else failed = failed + 1 print("  FAIL " .. what) end
end
local OLD = "/data/modbridge/roots/PPSA33286/ue4ss/Mods/ueselftest"  -- shipped today
local NEW = "ue4ss/Mods/ueselftest"                                 -- staged 0002
local WANT = "ue4ss/Mods/ueselftest/progress.txt"
local k_old = PathShim.normalize(OLD .. "/progress.txt", "ueselftest")
local k_new = PathShim.normalize(NEW .. "/progress.txt", "ueselftest")
print("key(old literal) = " .. tostring(k_old))
print("key(new logical) = " .. tostring(k_new))
check(k_old == WANT, "with the shim, the OLD Townfall literal is re-anchored to the running game's ue4ss root (title dropped)")
check(k_new == WANT, "the NEW logical ROOT gives the same key (behaviour unchanged where the shim runs)")
check(not NEW:find("%u%u%u%u%d%d%d%d%d"), "the NEW ROOT holds no title id")
-- Writable area (where w-mode opens land): today it drops the title.
local tf = PathShim.derive_writable_root("/data/modbridge/roots/PPSA33286")
local sb = PathShim.derive_writable_root("/data/modbridge/roots/ABCD12345")
print("writable(Townfall root) = " .. tostring(tf))
print("writable(other root)    = " .. tostring(sb))
local s_tf = PathShim.sandbox_of_key(tf, WANT)
local s_sb = PathShim.sandbox_of_key(sb, WANT)
print("progress.txt write target, Townfall: " .. tostring(s_tf))
print("progress.txt write target, other:    " .. tostring(s_sb))
-- FINDING check (asserts CURRENT behaviour so a silent fix is noticed):
check(tf == sb and s_tf == s_sb, "FINDING: both games share one writable area today (title-free /data/modbridge/writable)")
-- Proposed rule (DESIGN.md A9): keep the title -> /data/modbridge/writable/<title>
local function proposed(root)
  local prefix, title = root:match("^(.*)/roots/([^/]+)$")
  if prefix and #prefix > 0 then return prefix .. "/writable/" .. title end
  return root .. "-writable"
end
check(proposed("/data/modbridge/roots/PPSA33286") ~= proposed("/data/modbridge/roots/ABCD12345"),
      "proposed per-title writable root separates the two games")
check(proposed("/data/modbridge/roots/ABCD12345") == "/data/modbridge/writable/ABCD12345", "proposed shape")
print(string.format("SELFTEST-ROOT TEST: checked %d; failed %d", checks, failed))
if checks == 0 or failed > 0 then print("SELFTEST-ROOT TEST: FAIL") os.exit(1) end
print("SELFTEST-ROOT TEST: PASS")
