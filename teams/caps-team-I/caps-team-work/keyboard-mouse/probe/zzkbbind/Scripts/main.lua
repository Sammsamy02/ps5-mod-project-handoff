-- zzkbbind v1 (Team I2): for capability launch K2 ONLY (needs the runtime
-- build with the engine-keys source). Prints when a REAL key fires a normal
-- UE4SS key bind. Does nothing else.
local function d(s) print("[kbb] "..tostring(s).."\n") end
local n=0
local function on(label) return function() n=n+1 d("FIRE "..label.." n="..n) end end
RegisterKeyBind(Key.F8, on("F8"))
RegisterKeyBind(Key.F8, {ModifierKey.SHIFT}, on("SHIFT+F8"))
RegisterKeyBind(Key.NUM_FIVE, on("NUM_FIVE"))
RegisterKeyBind(Key.RIGHT_MOUSE_BUTTON, on("RIGHT_MOUSE_BUTTON"))
d("Z0 v1 binds=4")
