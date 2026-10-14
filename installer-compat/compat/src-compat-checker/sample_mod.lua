--------------------------------------------------------------------------
-- ItemSpawnerPlus -- sample mod for compat-checker testing.
-- Fixture only: scanned statically on the Mac host; never executed.
-- Exercises ~25 distinct APIs across all ps5_runtime_status values.
--------------------------------------------------------------------------

local ModName = "ItemSpawnerPlus"
local Version = GetUnrealVersion()                -- global: implemented+console-proven
local Spawner = {}                                -- mod-local table (must be ignored)

local Inventory = FindFirstOf("AFairyInventory")  -- global: proven; Inventory typed AFairyInventory
local Items = FindAllOf("AFairyItemBase")         -- global: implemented-untested; Items is T[]

-- Helpers: plain functions, never registered -> NOT main code path ----------

local function Describe(obj)                       -- param: untyped
    if obj:IsValid() then                          -- unresolved (untyped variable)
        return obj:GetFullName()                   -- unresolved (untyped variable)
    end
    return "<invalid>"
end

local function NameOf(id)
    local n = FName(tostring(id))                  -- FName constructor: proven
    return n:ToString()                            -- FName:ToString: proven
end

local function LogLen(msg)
    local s = FString(msg)                         -- FString constructor: implemented-untested
    return s:Len()                                 -- FString:Len: MISSING (peripheral)
end

local function DebugAddress()
    local u = FindFirstOf("UObject")               -- proven; u typed UObject
    if u:IsValid() then                            -- UObject:IsValid: proven (exact)
        return u:GetAddress()                      -- UObject:GetAddress: MISSING (peripheral)
    end
    return 0
end

-- Wiring: registration calls + handlers (main code path) --------------------

RegisterHook("AFairyInventory:GiveItem", function()   -- RegisterHook: proven
    ExecuteInGameThread(function()                    -- behaves-differently
        local pc = GetPlayerController()              -- proven; pc typed APlayerController
        if pc:IsValid() then                          -- via UObject base: proven
            print(string.format("%s spawned for %s", ModName, "player"))
        end
        for _, item in pairs(Items) do                -- item typed AFairyItemBase (array elem)
            local loc = item:GetActorLocation()       -- via AActor base: proven
            item:K2_SetActorLocation(loc)             -- via AActor base: implemented-untested
        end
        ExecuteAsync(function()                       -- MISSING, central (inside handler)
            table.insert(Spawner, "saved")
        end)
    end)
end)

RegisterKeyBind("F6", function()                      -- proven; handler
    ImGui.Begin("ItemSpawnerPlus")                    -- MISSING static, central
    ImGui.Text("Press to spawn")                      -- MISSING static, central
    ImGui.SetNextWindowSize(200, 120)                 -- MISSING static, central
    ImGui.Begin("panel")                              -- ImGui.Begin count = 2
end)

Loop(function(dt)                                     -- Loop: implemented-untested; handler
    math.floor(dt)
    NotifyOnNewObject("AFairyItemBase", function(o)   -- MISSING, central (inside handler)
        o:IsValid()                                   -- unresolved (untyped param)
    end)
end)

RegisterConsoleCommandHandler("ispawn", function()    -- MISSING registration, central
    return true
end)

-- Uncataloged APIs (grade-affecting unknowns) --------------------------------

ModConfig_Load(ModName .. ".json")                    -- UNKNOWN global (not in catalog)
local manager = ModServices.Manager()                 -- UNKNOWN static (CamelCase namespace)

-- One-off uses ---------------------------------------------------------------

local Pressed = EInputEvent.IE_Pressed                -- constant reference: proven
local ItemName = NameOf(42)                           -- local call (ignored)
local Desc = Describe(Inventory)                      -- local call (ignored)
local Len = LogLen("hello")                           -- local call (ignored)
local Addr = DebugAddress()                           -- local call (ignored)

-- The scanner must ignore comments and string contents:
-- ExecuteAsync("comment") ImGui.Begin("comment")
local Ignored = "FindFirstOf( FName( RegisterHook( inside a string"
