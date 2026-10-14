-- zzcrashtest v1 (cap05 crash capture). OUR OWN diagnostic mod; it never
-- touches any other mod. It crashes the game ON PURPOSE, once, about 150 s
-- after mods start, so the crash report pipeline can be proven end to end.
-- Needs the cap05 runtime patch (ModBridgeCrashTest). Without it: no crash.
local TICK_MS = 5000
local CRASH_AFTER_TICKS = 30          -- 30 x 5 s = about 150 s
local TOKEN = "CRASH-THE-GAME-NOW"
local function P(s) print("[CAP05] " .. tostring(s) .. "\n") end

P("zzcrashtest v1 loaded: deliberate crash in about " .. (TICK_MS * CRASH_AFTER_TICKS // 1000)
  .. " s. Stay on the title/main menu; do not load a save.")

if type(ModBridgeCrashTest) ~= "function" then
  P("ModBridgeCrashTest is missing (runtime without the cap05 patch): NO crash will happen")
  return
end

-- Gate check first: both calls must be refused and nothing may happen.
local ok1, r1, m1 = pcall(ModBridgeCrashTest, "segv", "wrong-token")
P("gate-check wrong-token: pcall=" .. tostring(ok1) .. " ret=" .. tostring(r1) .. " msg=" .. tostring(m1))
local ok2, r2, m2 = pcall(ModBridgeCrashTest, "bogus-kind", TOKEN)
P("gate-check bad-kind: pcall=" .. tostring(ok2) .. " ret=" .. tostring(r2) .. " msg=" .. tostring(m2))
if not (ok1 and r1 == false and ok2 and r2 == false) then
  P("GATE-CHECK FAIL: the refusals did not behave as designed; NOT crashing")
  return
end
P("GATE-CHECK PASS: 2 refusals, nothing happened")

local n = 0
LoopAsync(TICK_MS, function()
  n = n + 1
  if n % 6 == 0 and n < CRASH_AFTER_TICKS then P("waiting: " .. (n * TICK_MS // 1000) .. " s") end
  if n >= CRASH_AFTER_TICKS then
    P("CRASHING NOW: ModBridgeCrashTest(segv). Expect RIP in modbridge_cap05_crashtest_fault, data address 0xc08")
    local okc, rc, mc = pcall(ModBridgeCrashTest, "segv", TOKEN)
    P("UNEXPECTED: still running after the crash call: pcall=" .. tostring(okc) .. " ret=" .. tostring(rc)
      .. " msg=" .. tostring(mc))
    return true
  end
  return false
end)
