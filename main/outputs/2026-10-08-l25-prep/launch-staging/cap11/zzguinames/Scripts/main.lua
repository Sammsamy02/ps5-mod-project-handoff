-- zzguinames v1 (cap11 diagnostic). OUR OWN read-only diagnostic mod: it never calls a game function and never
-- writes anything. It prints the REAL reflected parameter / member names of every engine function and struct the
-- #11 GUI backend uses, so a wrong name (e.g. "parameter not found: Width") is found from the game itself.
-- Lines: [gn] FN <class>:<fn> found=<yes|no>   then   [gn] P <fn> <name> <type> off=<n> size=<n> parm=<0|1>
local TICK_MS = 5000
local RUN_AFTER_TICKS = 4 -- about 20 s after mods start
local CPF_Parm = 0x80
local FUNCS = {
  {"/Script/Engine.CanvasRenderTarget2D", "CreateCanvasRenderTarget2D"},
  {"/Script/Engine.KismetRenderingLibrary", "ClearRenderTarget2D"},
  {"/Script/Engine.KismetRenderingLibrary", "BeginDrawCanvasToRenderTarget"},
  {"/Script/Engine.KismetRenderingLibrary", "EndDrawCanvasToRenderTarget"},
  {"/Script/Engine.KismetRenderingLibrary", "ImportBufferAsTexture2D"},
  {"/Script/Engine.Canvas", "K2_DrawTriangle"},
  {"/Script/Engine.PlayerController", "GetViewportSize"},
  {"/Script/Engine.Controller", "SetIgnoreMoveInput"},
  {"/Script/Engine.Controller", "SetIgnoreLookInput"},
  {"/Script/UMG.CanvasPanel", "AddChildToCanvas"},
  {"/Script/UMG.CanvasPanelSlot", "SetPosition"},
  {"/Script/UMG.CanvasPanelSlot", "SetSize"},
  {"/Script/UMG.Image", "SetBrushResourceObject"},
  {"/Script/UMG.UserWidget", "AddToViewport"},
  {"/Script/UMG.Widget", "SetVisibility"},
}
local STRUCTS = {"/Script/Engine.CanvasUVTri"}
local function P(s) print("[gn] " .. tostring(s) .. "\n") end
local function valid(o) local ok, v = pcall(function() return o ~= nil and o:IsValid() end) return ok and v end
local function field_line(tag, p)
  local name, typ, off, size, parm = "?", "?", "?", "?", "?"
  pcall(function() name = p:GetFName():ToString() end)
  pcall(function() typ = p:GetClass():GetFName():ToString() end)
  pcall(function() off = tostring(p:GetOffset_Internal()) end)
  pcall(function() size = tostring(p:GetSize()) end)
  pcall(function() parm = p:HasAnyPropertyFlags(CPF_Parm) and "1" or "0" end)
  P(tag .. " " .. name .. " " .. typ .. " off=" .. off .. " size=" .. size .. " parm=" .. parm)
end
local function probe()
  local nf, np = 0, 0
  for _, e in ipairs(FUNCS) do
    local fn = StaticFindObject(e[1] .. ":" .. e[2])
    if valid(fn) then
      nf = nf + 1
      P("FN " .. e[1] .. ":" .. e[2] .. " found=yes")
      pcall(fn.ForEachProperty, fn, function(p) np = np + 1 field_line("P " .. e[2], p) end)
    else
      P("FN " .. e[1] .. ":" .. e[2] .. " found=no")
      local cls = StaticFindObject(e[1])
      if valid(cls) then
        local want = string.lower(e[2])
        pcall(cls.ForEachFunction, cls, function(f)
          local n = "?"
          pcall(function() n = f:GetFName():ToString() end)
          if string.find(string.lower(n), string.sub(want, 1, 8), 1, true) then P("  near " .. e[1] .. ":" .. n) end
        end)
      else
        P("  class " .. e[1] .. " not found")
      end
    end
  end
  for _, sp in ipairs(STRUCTS) do
    local st = StaticFindObject(sp)
    P("STRUCT " .. sp .. " found=" .. (valid(st) and "yes" or "no"))
    if valid(st) then pcall(st.ForEachProperty, st, function(p) np = np + 1 field_line("M " .. sp, p) end) end
  end
  P("SUM functions_found=" .. nf .. "/" .. #FUNCS .. " fields_listed=" .. np)
end

P("zzguinames v1 loaded: listing reflected GUI names in about " .. (TICK_MS * RUN_AFTER_TICKS // 1000) .. " s")
local n = 0
LoopAsync(TICK_MS, function()
  n = n + 1
  if n < RUN_AFTER_TICKS then return false end
  local run = function() local ok, err = pcall(probe) if not ok then P("ERR " .. tostring(err)) end end
  if ExecuteInGameThread then ExecuteInGameThread(run) else run() end
  return true
end)
