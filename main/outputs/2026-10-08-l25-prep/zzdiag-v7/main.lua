-- zzdiag v7: alternate-path resolution discrimination (agent-CX, 2026-10-08).
-- FindXOf is empty for EVERY class in-game (walk runs, lag seen). v7 splits
-- hash/path resolution (StaticFindObject) from the class-scan path
-- (FindFirstOf: FName compare + IsValidObjectForFindXOf exclusion).
-- Wrappers are NEVER nil (auto_construct_object is nullptr-safe), so the
-- ONLY resolution test is pcall obj:IsValid(). No GetClass/FindFunction/
-- class-children calls (v2 crash lesson). Tokens, all wide-char-safe short:
--   Z0 load / K1 K2 keybind+queue / Z1 native-bridge sanity
--   S1r S2r S3r StaticFindObject full paths  (S*e = call error)
--   U1 require UEHelpers / U2r CDO lookup / U3r GetEngine(=FindFirstOf)
--   F1r F2r F3r FindFirstOf same short names / A1r FindAllOf nil-contract
local function d(s) print("[zz] " .. tostring(s) .. "\n") end
local function el(e)
    e = tostring(e):gsub("^.-:%d+:%s*", "")
    d("  " .. e:sub(1, 24))
end
local function ok(tag, f)
    local o, r = pcall(f)
    if not o then d(tag .. "e") el(r) return end
    local o2, v = pcall(function() return r:IsValid() end)
    if not o2 then d(tag .. "r?") el(v) return end
    d(tag .. (v and "r y" or "r n"))
end
RegisterKeyBind(Key.F7, function()
    d("K1 f7")
    ExecuteInGameThread(function()
        local zo, zv = pcall(IsKeyBindRegistered, Key.F7)
        if zo then d("Z1 " .. tostring(zv)) else d("Z1e") el(zv) end
        ok("S1", function() return StaticFindObject("/Script/Engine.PlayerController") end)
        ok("S2", function() return StaticFindObject("/Script/Engine.GameEngine") end)
        ok("S3", function() return StaticFindObject("/Script/Engine.KismetStringLibrary") end)
        local uo, UH = pcall(require, "UEHelpers")
        if uo then
            local vo, vv = pcall(function() return UH.GetUEHelpersVersion() end)
            d("U1 y" .. (vo and (" v" .. tostring(vv)) or ""))
        else d("U1 n") end
        if uo then
            ok("U2", function() return UH.GetKismetStringLibrary() end)
            ok("U3", function() return UH.GetEngine() end)
        end
        ok("F1", function() return FindFirstOf("PlayerController") end)
        ok("F2", function() return FindFirstOf("GameEngine") end)
        ok("F3", function() return FindFirstOf("KismetStringLibrary") end)
        local ao, av = pcall(FindAllOf, "PlayerController")
        if ao then d("A1r " .. (av ~= nil and "y" or "n")) else d("A1e") el(av) end
    end)
    d("K2 q")
end)
d("Z0 v7")
