-- zzdiag v8: Lua->FName bridge menu dry run (agent-EY, 2026-10-09, Claude
-- spec). Runs ONCE at the main menu after the pump is armed - NO user input:
-- LoopInGameThreadWithDelay polls on the game thread, waits for a
-- PlayerController (the menu being up), then executes the four steps and
-- stops the loop. Every engine call is pcall'd; PASS/FAIL per step; the
-- widget is NEVER AddToViewport'd. Wide-char-safe short tokens only
-- (v2/v3/v7 discipline), 24-char error tails.
--   M0 load / W0 wait-armed lines / M1 FindAllOf+IsPlayerController
--   M2 StaticFindObject WidgetBlueprintLibrary
--   M3 widgetLib:Create(pc, UserWidget, pc)
--   M4 StaticConstructObject TextBlock + SetText(FText("test"))
-- Every name here flows through the [LFB] Lua FName bridge; the console log
-- should also show the matching '[LUA-FN] in=...' lines (first 20 uses).
local function d(s) print("[LFB8] " .. tostring(s) .. "\n") end
local function el(tag, e)
    e = tostring(e):gsub("^.-:%d+:%s*", "")
    d(tag .. "e " .. e:sub(1, 24))
end
local function ok(tag, f)
    local o, r = pcall(f)
    if not o then el(tag, r) return nil end
    local o2, v = pcall(function() return r:IsValid() end)
    if not o2 then el(tag .. "?", v) return nil end
    if not v then d(tag .. " n") return nil end
    return r
end
d("M0 v8")

local ran = false
local waits = 0
LoopInGameThreadWithDelay(1000, function()
    if ran then return false end
    waits = waits + 1
    local pcs = ok("W1", function() return FindAllOf("PlayerController") end)
    if pcs == nil or #pcs == 0 then
        if waits % 30 == 0 then d("W0 " .. waits) end
        if waits >= 240 then
            d("W0 timeout")
            ran = true
            return false
        end
        return true
    end
    ran = true

    -- M1: FindAllOf count + IsPlayerController (string-overload IsA also
    -- flows through the bridge).
    local ispc = false
    local e1 = pcall(function() ispc = pcs[1]:IsA("/Script/Engine.PlayerController") end)
    d("M1 " .. #pcs .. " " .. (e1 and ispc and "y" or "n"))

    -- M2: the UMG CDO via full path.
    local widget_lib = ok("M2", function() return StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary") end)

    -- M3: widgetLib:Create(pc, UserWidget class, pc).
    if widget_lib then
        local made = ok("M3", function()
            local wcls = StaticFindObject("/Script/UMG.UserWidget")
            return widget_lib:Create(pcs[1], wcls, pcs[1])
        end)
        if made then d("M3 y") end
    end

    -- M4: StaticConstructObject one TextBlock + SetText(FText("test")).
    -- Outer = the pc (transient); NEVER AddToViewport.
    local tb = ok("M4", function()
        return StaticConstructObject(StaticFindObject("/Script/UMG.TextBlock"), pcs[1], FName("LFB8TextBlock"))
    end)
    if tb then
        local e4 = pcall(function() tb:SetText(FText("test")) end)
        d("M4 " .. (e4 and "y" or "n"))
    end

    d("M9 done")
    return false
end)
