-- zzpathprobe v2 (integrator, #7 step 2 diagnostic). OUR OWN read-only diagnostic: from INSIDE the running game,
-- which folders can the game process still open after start-up? Opens each candidate file read-only, reads at most
-- 16 bytes, closes it, and logs ok/error. Writes nothing, changes nothing. Runs at mod start and again ~20 s later.
local function d(s) print("[pp] "..tostring(s).."\n") end
local CANDIDATES = {
  "/app0/townfall/content/paks/pakchunk1-ps5.utoc",
  "/app0/Townfall/Content/Paks/pakchunk1-ps5.utoc",
  "/data/homebrew/Silent Hill Townfall PPSA33286/townfall/content/paks/pakchunk1-ps5.utoc",
  "/data/modbridge/roots/PPSA33286/pakmods/zzz_Leon_Townfall_2K_P.pak",
  "/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzpathprobe/enabled.txt",
  -- v2: the staged UE command-line file (names the project folder), in both name cases; param.json; folder opens
  "/app0/uecommandline.txt",
  "/app0/UECommandLine.txt",
  "/app0/sce_sys/param.json",
  "/app0",
  "/app0/townfall/content/paks",
  "/app0/townfall/content/paks/~mods",
}
local function probe(tag)
  for i, p in ipairs(CANDIDATES) do
    local ok, f, err = pcall(io.open, p, "rb")
    if ok and f then
      local rok, data = pcall(function() return f:read(64) end)
      pcall(function() f:close() end)
      local shown = ""
      if rok and data and p:lower():match("commandline") then shown = " text="..data:gsub("%c"," ") end
      d(tag.." "..i.." OK bytes="..tostring(rok and data and #data or -1)..shown.." "..p)
    else
      d(tag.." "..i.." FAIL "..tostring(ok and err or f):gsub("%c"," "):sub(1,90).." "..p)
    end
  end
end
d("zzpathprobe v2 loaded: "..#CANDIDATES.." candidates")
probe("T0")
local n = 0
LoopAsync(5000, function()
  n = n + 1
  if n < 4 then return false end
  local run = function() local ok, e = pcall(probe, "T20") if not ok then d("ERR "..tostring(e)) end end
  if ExecuteInGameThread then ExecuteInGameThread(run) else run() end
  return true
end)
