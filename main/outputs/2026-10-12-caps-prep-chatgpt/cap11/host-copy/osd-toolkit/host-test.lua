-- host-test.lua — S1..S5 host scenarios for the OSD toolkit prototype.
-- Agent HL, 2026-10-10 (l25-prep). Mac-local: drives toolkit.lua against
-- FU's stub-ue4ss recorder and asserts the retained-tree laws.
--
-- Scenarios (tasking):
--   S1 3-window demo: recorder shows Create x31 / SetText x12 ONCE, then
--      zero re-emission until a label actually changes.
--   S2 list-nav focus model: UP/DOWN/RETURN + window cycling move the
--      highlight onto the right ids (2 SetBrushColor per move).
--   S3 virtual cursor: deadzone, stick motion, hit-test click, clamp.
--   S4 100-frame soak: Create-call count CONSTANT after frame 1 (the
--      retained-tree proof).
--   S5 close/reopen + item churn: widget REUSE, zero re-create.
--
-- Prints one "ok/FAIL" line per assertion, one PASS/FAIL line per
-- scenario, exits 0 iff everything passed.

local HERE = (arg and arg[0] and arg[0]:match("^(.*)/[^/]+$")) or "."
local make_stub = dofile(HERE .. "/../host-lua-harness/stub-ue4ss.lua")
local toolkit = dofile(HERE .. "/toolkit.lua")

local pass_n, fail_n = 0, 0
local function check(cond, label, detail)
    local d = ""
    if detail ~= nil then d = " [" .. tostring(detail) .. "]" end
    if cond then
        pass_n = pass_n + 1
        print("  ok   " .. label .. d)
    else
        fail_n = fail_n + 1
        print("  FAIL " .. label .. d)
    end
end

local function scenario(name, fn)
    print("", "")
    print("== " .. name .. " ==")
    local p0, f0 = pass_n, fail_n
    fn()
    if fail_n == f0 then
        print("== " .. name .. ": PASS ==")
        return true
    end
    print("== " .. name .. ": FAIL (" .. (fail_n - f0) .. " assertions) ==")
    return false
end

----------------------------------------------------------------------
-- Recorder snapshot: count the reflected-call families by name pattern.
-- The stub records a method INVOCATION as "call:<display>.<Method>" and
-- the method LOOKUP as "prop:<display>.<Method>" — only the former is a
-- real engine call, so method families are counted from "call:" entries
-- only. Globals ("StaticConstructObject" etc.) and property writes
-- ("set:<display>.<prop> = <type>") record once natively.
----------------------------------------------------------------------
local function ends(n, s)
    return #n >= #s and n:sub(-#s) == s
end

local function snapshot(S)
    local c = { total = S.call_count, create = 0, settext = 0, addchild = 0,
                addvp = 0, remove = 0, brush = 0, setpos = 0, setsize = 0,
                setcontent = 0, anchors = 0, align = 0, widgettree = 0,
                rootwidget = 0, lock = 0, findobj = 0, findfirst = 0,
                dispatch = 0 }
    for _, e in ipairs(S.calls) do
        local n = e.name
        local m = nil
        if n:sub(1, 5) == "call:" then m = n:sub(6) end
        if n == "StaticConstructObject" then c.create = c.create + 1
        elseif n == "StaticFindObject" then c.findobj = c.findobj + 1
        elseif n == "FindFirstOf" then c.findfirst = c.findfirst + 1
        elseif n == "ExecuteInGameThread" then c.dispatch = c.dispatch + 1
        elseif m and ends(m, ".SetText") then c.settext = c.settext + 1
        elseif m and ends(m, ".AddChild") then c.addchild = c.addchild + 1
        elseif m and ends(m, ".AddToViewport") then c.addvp = c.addvp + 1
        elseif m and ends(m, ".RemoveFromParent") then c.remove = c.remove + 1
        elseif m and ends(m, ".SetBrushColor") then c.brush = c.brush + 1
        elseif m and ends(m, ".SetPosition") then c.setpos = c.setpos + 1
        elseif m and ends(m, ".SetSize") then c.setsize = c.setsize + 1
        elseif m and ends(m, ".SetContent") then c.setcontent = c.setcontent + 1
        elseif m and ends(m, ".SetAnchors") then c.anchors = c.anchors + 1
        elseif m and ends(m, ".SetAlignment") then c.align = c.align + 1
        elseif m and (ends(m, "SetIgnoreLookInput") or ends(m, "SetIgnoreMoveInput")) then
            c.lock = c.lock + 1
        elseif n:find(".WidgetTree = ", 1, true) then
            c.widgettree = c.widgettree + 1
        elseif n:find(".RootWidget = ", 1, true) then
            c.rootwidget = c.rootwidget + 1
        end
    end
    return c
end

local function dstr(t, keys)
    local p = {}
    for _, k in ipairs(keys) do p[#p + 1] = k .. "=" .. tostring(t[k]) end
    return table.concat(p, " ")
end

local KEYS = { "create", "settext", "addchild", "addvp", "remove", "brush",
               "setpos", "setcontent", "lock", "dispatch" }

----------------------------------------------------------------------
-- Shared 3-window demo app (identical shape in every scenario so the
-- frame-1 count contract is one set of numbers: 31 creates / 12 SetText).
----------------------------------------------------------------------
local function fresh()
    local env, S = make_stub({})
    local OSD = toolkit.new(env, { viewport_w = 1920, viewport_h = 1080 })
    local app = { open = { a = true, b = true, c = true }, b3 = true,
                  status = "ready", hits = {} }
    OSD.SetDraw(function()
        if app.open.a then
            OSD.BeginWindow("winA", 40, 40, 300, 220, "Window A")
            if OSD.Button("b1", "Spawn Axe") then
                app.status = "spawn:axe"; app.hits[#app.hits + 1] = "b1"
            end
            if OSD.Button("b2", "Spawn Shield") then
                app.status = "spawn:shield"; app.hits[#app.hits + 1] = "b2"
            end
            if app.b3 and OSD.Button("b3", "Spawn Bow") then
                app.status = "spawn:bow"; app.hits[#app.hits + 1] = "b3"
            end
            OSD.Text("status: " .. app.status)
            OSD.EndWindow()
        end
        if app.open.b then
            OSD.BeginWindow("winB", 400, 40, 260, 160, "Window B")
            OSD.Text("alpha line")
            OSD.Text("beta line")
            OSD.Text("gamma line")
            OSD.EndWindow()
        end
        if app.open.c then
            OSD.BeginWindow("winC", 40, 300, 220, 140, "Window C")
            OSD.Text("info text")
            if OSD.Button("go", "GO") then
                app.status = "go"; app.hits[#app.hits + 1] = "go"
            end
            OSD.EndWindow()
        end
    end)
    return env, S, OSD, app
end

local function healthy(S, OSD, tag)
    check(OSD.Debug().draw_error == nil, tag .. ": no draw error",
          OSD.Debug().draw_error)
    check(#S.errors == 0, tag .. ": zero async/EIT errors", #S.errors)
    check(#S.missing == 0, tag .. ": zero missing globals", #S.missing)
end

----------------------------------------------------------------------
-- S1 — 3-window demo
----------------------------------------------------------------------
local s1_c
local function s1()
    local env, S, OSD, app = fresh()
    OSD.Tick(1 / 30)
    local c1 = snapshot(S)
    s1_c = c1
    check(c1.create == 31,
        "frame1 StaticConstructObject == 31 (3 shells 6 each + 13/9/9 items)",
        c1.create)
    check(c1.settext == 12, "frame1 SetText == 12 (3 titles + 4 labels + 5 texts)",
        c1.settext)
    check(c1.addchild == 15 and c1.setcontent == 7 and c1.addvp == 3,
        "frame1 AddChild/SetContent/AddToViewport == 15/7/3",
        c1.addchild .. "/" .. c1.setcontent .. "/" .. c1.addvp)
    check(c1.widgettree == 3 and c1.rootwidget == 3,
        "tree assembly: WidgetTree= / RootWidget= writes x3/x3",
        c1.widgettree .. "/" .. c1.rootwidget)
    check(c1.brush == 1, "frame1 highlight: exactly 1 SetBrushColor (focus b1)",
        c1.brush)
    check(c1.lock == 2, "input lock engaged (look+move)", c1.lock)
    check(c1.findobj == 6 and c1.findfirst == 1,
        "class resolution: 6 StaticFindObject + 1 FindFirstOf",
        c1.findobj .. "/" .. c1.findfirst)
    local total1 = S.call_count
    for _ = 1, 5 do OSD.Tick(1 / 30) end
    check(S.call_count == total1,
        "5 identical frames emit ZERO engine calls (total unchanged)",
        S.call_count .. " -> " .. total1)
    app.status = "armed"
    OSD.Tick(1 / 30)
    local c2 = snapshot(S)
    check(c2.create == 31, "label change: creates still 31", c2.create)
    check(c2.settext == 13, "label change: exactly ONE new SetText", c2.settext)
    healthy(S, OSD, "S1")
end

----------------------------------------------------------------------
-- S2 — list-nav focus model
----------------------------------------------------------------------
local function s2()
    local env, S, OSD, app = fresh()
    OSD.Tick(1 / 30)
    local d = OSD.Debug()
    check(d.focus.key == "winA/b1",
        "default focus = first alive button of first window", d.focus.key)
    local b0 = snapshot(S)
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.Tick(1 / 30)
    d = OSD.Debug()
    check(d.focus.key == "winA/b2", "DOWN moves focus to winA/b2", d.focus.key)
    check(snapshot(S).brush - b0.brush == 2,
        "highlight move = 2 SetBrushColor (old row + new row)",
        snapshot(S).brush - b0.brush)
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winA/b3",
        "queued events keep ORDER (2x DOWN in one frame -> b3)",
        OSD.Debug().focus.key)
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winA/b3",
        "DOWN clamps at last row", OSD.Debug().focus.key)
    OSD.PushInput({ kind = "press", key = "UP" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winA/b2", "UP moves back to b2",
        OSD.Debug().focus.key)
    local st = snapshot(S)
    OSD.PushInput({ kind = "press", key = "RETURN" })
    OSD.Tick(1 / 30)
    check(app.hits[#app.hits] == "b2" and app.status == "spawn:shield",
        "RETURN activates the focused row (b2 round-trip)",
        tostring(app.hits[#app.hits]) .. "/" .. app.status)
    check(snapshot(S).settext - st.settext == 1,
        "activation -> status SetText round-trip = 1 call",
        snapshot(S).settext - st.settext)
    OSD.PushInput({ kind = "press", key = "RIGHT" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winC/go",
        "RIGHT cycles focus window (winB skipped: no buttons)",
        OSD.Debug().focus.key)
    OSD.PushInput({ kind = "press", key = "LEFT" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winA/b1",
        "LEFT cycles back, index resets to 1", OSD.Debug().focus.key)
    check(OSD.Tick(1 / 30) == false,
        "Tick returns false (LoopAsync erase law)", "false expected")
    healthy(S, OSD, "S2")
end

----------------------------------------------------------------------
-- S3 — virtual cursor
----------------------------------------------------------------------
local function s3()
    local env, S, OSD, app = fresh()
    OSD.Tick(1 / 30)
    local c0 = snapshot(S)
    OSD.PushInput({ kind = "toggle_cursor" })
    OSD.Tick(1 / 30)
    local c1 = snapshot(S)
    check(c1.create - c0.create == 4,
        "cursor widget = 4 creates (UserWidget/WidgetTree/Canvas/Border)",
        c1.create - c0.create)
    check(c1.addvp - c0.addvp == 1, "cursor AddToViewport (Z above windows)",
        c1.addvp - c0.addvp)
    local x0, y0 = OSD.GetCursor()
    for _ = 1, 10 do
        OSD.PushInput({ kind = "axis", x = 138, y = 128 })
        OSD.Tick(1 / 60)
    end
    local x1, y1 = OSD.GetCursor()
    check(x1 == x0 and y1 == y0,
        "sub-deadzone deflection (x=138) = ZERO motion", x1 .. "," .. y1)
    local p0 = snapshot(S).setpos
    for _ = 1, 30 do
        OSD.PushInput({ kind = "axis", x = 200, y = 128 })
        OSD.Tick(1 / 60)
    end
    local x2, y2 = OSD.GetCursor()
    check(x2 > x1 + 100 and math.abs(y2 - y1) < 1.5,
        "right deflection moves cursor right (curve applied)",
        string.format("%.1f,%.1f", x2, y2))
    check(snapshot(S).setpos - p0 == 30,
        "one SetPosition per MOVING frame (30/30)",
        snapshot(S).setpos - p0)
    -- hit-test + click on b1 (row 1 of winA: rect 46,70 -> 334,94)
    OSD.WarpCursor(190, 82)
    OSD.PushInput({ kind = "click" })
    OSD.Tick(1 / 60)
    check(OSD.Debug().hover == "winA/b1",
        "hover hit-test lands on winA/b1 (toolkit rect math)",
        tostring(OSD.Debug().hover))
    check(app.hits[#app.hits] == "b1" and app.status == "spawn:axe",
        "click activates the hovered button",
        tostring(app.hits[#app.hits]) .. "/" .. app.status)
    local st = snapshot(S)
    OSD.WarpCursor(10, 10)
    OSD.PushInput({ kind = "click" })
    OSD.Tick(1 / 60)
    check(snapshot(S).settext == st.settext,
        "click in empty space activates nothing", "")
    -- clamp + update-on-change pinned at the wall
    OSD.PushInput({ kind = "axis", x = 128, y = 128 })
    OSD.Tick(1 / 60)
    OSD.WarpCursor(1918, 540)
    for _ = 1, 60 do
        OSD.PushInput({ kind = "axis", x = 255, y = 255 })
        OSD.Tick(1 / 60)
    end
    local cx, cy = OSD.GetCursor()
    check(cx == 1918 and cy == 1078,
        "cursor clamped at viewport corner (1920x1080 - 2px margin)",
        cx .. "," .. cy)
    local mid = snapshot(S)
    for _ = 1, 10 do
        OSD.PushInput({ kind = "axis", x = 255, y = 255 })
        OSD.Tick(1 / 60)
    end
    check(snapshot(S).setpos == mid.setpos,
        "pinned at the wall = ZERO SetPosition while deflected",
        snapshot(S).setpos - mid.setpos)
    healthy(S, OSD, "S3")
end

----------------------------------------------------------------------
-- S4 — 100-frame soak (the retained-tree proof)
----------------------------------------------------------------------
local function s4()
    local env, S, OSD, app = fresh()
    OSD.Tick(1 / 30)
    local c1 = snapshot(S)
    local total1 = S.call_count
    for _ = 1, 99 do OSD.Tick(1 / 30) end
    local c100 = snapshot(S)
    check(OSD.Debug().frame == 100, "100 frames ran", OSD.Debug().frame)
    check(c100.create == c1.create,
        "SOAK: StaticConstructObject CONSTANT after frame 1",
        c1.create .. " -> " .. c100.create)
    check(c100.settext == c1.settext, "SOAK: SetText CONSTANT",
        c1.settext .. " -> " .. c100.settext)
    check(S.call_count == total1,
        "SOAK: TOTAL recorded engine calls constant across frames 2..100",
        total1 .. " -> " .. S.call_count)
    check(c100.brush == c1.brush and c100.addvp == c1.addvp
        and c100.remove == c1.remove and c100.addchild == c1.addchild,
        "SOAK: brush/addvp/remove/addchild all constant",
        dstr({ brush = c100.brush - c1.brush, addvp = c100.addvp - c1.addvp,
               remove = c100.remove - c1.remove,
               addchild = c100.addchild - c1.addchild },
            { "brush", "addvp", "remove", "addchild" }))
    healthy(S, OSD, "S4")
    print("  [proof numbers] frame1 " .. dstr(c1, KEYS)
        .. " | frame100 " .. dstr(c100, KEYS))
end

----------------------------------------------------------------------
-- S5 — close/reopen + item churn
----------------------------------------------------------------------
local function s5()
    local env, S, OSD, app = fresh()
    OSD.Tick(1 / 30)
    local root_before = OSD.Debug().windows["winA"].root_ref
    local c1 = snapshot(S)
    app.open.a = false
    OSD.Tick(1 / 30)
    local c2 = snapshot(S)
    check(c2.remove - c1.remove == 1, "close winA = 1 RemoveFromParent",
        c2.remove - c1.remove)
    check(c2.lock == c1.lock, "input lock HELD while winB/winC stay open",
        c2.lock)
    app.open.b, app.open.c = false, false
    OSD.Tick(1 / 30)
    local c3 = snapshot(S)
    check(c3.remove - c2.remove == 2, "closing winB+winC = 2 more removes",
        c3.remove - c2.remove)
    check(c3.lock - c2.lock == 2, "last window closed releases input lock",
        c3.lock - c2.lock)
    local total3 = S.call_count
    for _ = 1, 30 do OSD.Tick(1 / 30) end
    check(S.call_count == total3,
        "30 all-closed frames emit ZERO calls", S.call_count - total3)
    app.open.a = true
    OSD.Tick(1 / 30)
    local c4 = snapshot(S)
    check(c4.create == c1.create, "REOPEN: zero new StaticConstructObject",
        c1.create .. " -> " .. c4.create)
    check(c4.addvp - c3.addvp == 1, "reopen = 1 AddToViewport (same root)",
        c4.addvp - c3.addvp)
    check(c4.settext == c3.settext, "reopen: zero re-SetText (labels retained)",
        c4.settext)
    check(c4.lock - c3.lock == 2, "reopen re-engages input lock", c4.lock - c3.lock)
    check(rawequal(OSD.Debug().windows["winA"].root_ref, root_before),
        "SAME UserWidget object identity across close/reopen", "")
    app.b3 = false
    OSD.Tick(1 / 30)
    local c5 = snapshot(S)
    check(c5.remove - c4.remove == 1,
        "dropping b3 removes only its row Border", c5.remove - c4.remove)
    for _ = 1, 10 do OSD.Tick(1 / 30) end
    app.b3 = true
    OSD.Tick(1 / 30)
    local c6 = snapshot(S)
    check(c6.create == c5.create, "b3 re-add: zero new creates",
        c5.create .. " -> " .. c6.create)
    check(c6.addchild - c5.addchild == 1,
        "b3 re-add = 1 AddChild (re-parent SAME widgets)", c6.addchild - c5.addchild)
    check(c6.settext == c5.settext, "b3 re-add: no SetText", c6.settext)
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.PushInput({ kind = "press", key = "DOWN" })
    OSD.Tick(1 / 30)
    check(OSD.Debug().focus.key == "winA/b3",
        "focus reaches the re-added row", OSD.Debug().focus.key)
    OSD.PushInput({ kind = "press", key = "RETURN" })
    OSD.Tick(1 / 30)
    check(app.hits[#app.hits] == "b3" and app.status == "spawn:bow",
        "re-added row still activates", tostring(app.hits[#app.hits]))
    healthy(S, OSD, "S5")
end

----------------------------------------------------------------------
local results = {}
results[1] = scenario("S1 3-window demo (create-once/settext-on-change)", s1)
results[2] = scenario("S2 list-nav focus model", s2)
results[3] = scenario("S3 virtual cursor (deadzone/motion/hit-test/clamp)", s3)
results[4] = scenario("S4 100-frame soak (retained-tree proof)", s4)
results[5] = scenario("S5 close/reopen reuse", s5)

print("", "")
print("assertions: " .. pass_n .. " ok, " .. fail_n .. " failed")
local all = true
for i = 1, 5 do
    print(string.format("S%d %s", i, results[i] and "PASS" or "FAIL"))
    if not results[i] then all = false end
end
if all then
    print("ALL SCENARIOS PASS")
    os.exit(0)
end
print("FAILURES PRESENT")
os.exit(1)
