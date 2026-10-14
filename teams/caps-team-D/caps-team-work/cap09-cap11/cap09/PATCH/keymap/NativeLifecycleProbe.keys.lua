-- Key declaration stub for keymap-gen.py (it scans Lua for RegisterKeyBind).
-- The native C++ probe binds F9 through the ABI; this file only tells the
-- generator to assign a free pad chord to F9. Not a mod; never deployed.
RegisterKeyBind(Key.F9, function() end)
