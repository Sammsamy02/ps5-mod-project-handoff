-- toolkit.lua — OSD v1 host prototype: immediate-mode-flavored API over a
-- RETAINED UMG widget tree (capability #11, route (a)).
-- Agent HL, 2026-10-10 (l25-prep packet). Mac-local Lua only: no console,
-- no builds; runs against FU's host-lua-harness stub-ue4ss recorder.
--
-- Public API (HC's CAP11-OSD-DESIGN.md 1(a)):
--   OSD.SetDraw(fn)                 per-frame draw callback
--   OSD.BeginWindow(id,x,y,w,h,t)   open/issue a window (reuses retained tree)
--   OSD.Text(str)                   text row (create once, SetText on change)
--   OSD.Button(id,label) -> clicked text row + Border highlight + our own
--                                   activation logic (v1 law: never UButton)
--   OSD.EndWindow()
--   OSD.PushInput(ev)               input event queue:
--                                     {kind="axis",x=,y=}   stick uint8 sample
--                                     {kind="press",key=}   UP/DOWN/LEFT/RIGHT/RETURN
--                                     {kind="click"}        cursor activate
--                                     {kind="toggle_cursor"}
--   OSD.Tick(dt)                    one frame; drains queue, integrates the
--                                   cursor, runs draw, diffs, emits. ALWAYS
--                                   returns false.
--   OSD.WarpCursor(x,y)             place the cursor (engine-cursor bridge)
--   OSD.FocusWindow(id)             set list-nav focus context
--   OSD.GetCursor()/OSD.Debug()     introspection (host tests)
--
-- Laws honored:
--  * Retained tree keyed by id-path (window id .. "/" .. item key; buttons
--    by explicit id, texts by per-window text ordinal "#tN"): create on
--    first use via reflected calls, update ONLY on change, remove via
--    RemoveFromParent, park + REUSE (never per-frame recreation, never
--    re-create for identical content, widget identity preserved across
--    close/reopen).
--  * Every engine call is a console-proven reflected family, issued inside
--    ExecuteInGameThread (game_thread below) per the design's game-thread
--    law. Engine base classes only (/Script/UMG.*): game-agnostic.
--  * PORT LAW (GY, CARRIER-DRIVE): this build's LoopAsync ERASES a loop
--    whose callback returns true, so Tick ALWAYS returns false.
--  * Input semantics: axis events set the stick sample (last in queue
--    wins); the cursor integrates ONCE per Tick with full dt; press/click
--    events are evaluated in queue order against last frame's layout
--    (standard one-frame immediate-mode semantics).
--
-- Lua: 5.2-5.5 common subset only (parses under the console's 5.4).

local M = {}

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

function M.new(env, opts)
    opts = opts or {}
    local vw = opts.viewport_w or 1920
    local vh = opts.viewport_h or 1080
    local max_speed = opts.max_speed or 1600   -- px/s (design 2.1)
    local deadzone = opts.deadzone or 0.15     -- radial, of half-range
    local curve_exp = opts.curve_exp or 2.2    -- response exponent
    local margin = opts.margin or 2            -- viewport clamp edge
    local z_win = opts.z_windows or 9999       -- proven spawner Z
    local z_cur = opts.z_cursor or 10000       -- above the windows
    local TITLE_H = opts.title_h or 30
    local ROW_H = opts.row_h or 24
    local PAD = opts.pad or 6

    local COL_NORMAL = { 0.12, 0.12, 0.14, 0.92 }
    local COL_FOCUS  = { 0.25, 0.50, 0.90, 1.0 }
    local COL_HOVER  = { 0.30, 0.30, 0.34, 1.0 }

    local S = {
        env = env,
        frame = 0,
        draw_fn = nil,
        draw_error = nil,
        windows = {},     -- id -> window node
        order = {},       -- window ids in first-issue order
        current = nil,
        text_ord = 0,
        queue = {},
        axis = { x = 128, y = 128 },  -- PS5_PadData uint8 center 127/128
        cursor = { x = vw / 2, y = vh / 2, shown = false, built = false,
                   on_screen = false, root = nil, tree = nil, canvas = nil,
                   border = nil, slot = nil, moved = true },
        focus_win = nil,
        focus_idx = 0,
        hover_key = nil,
        activated = {},  -- key -> true for this frame (draw reads it)
        locked = false,
        pc = nil,
        classes = nil,
        -- dirty-gate: emit (and the ExecuteInGameThread dispatch itself)
        -- only when the diff is non-empty. Over-approximate generously;
        -- never skip a frame that needs an emission.
        dirty = true,
        last_fk = nil,
        last_hover = nil,
    }

    ------------------------------------------------------------------
    -- Engine seam (console bridge: BRIDGE-NOTES.md)
    ------------------------------------------------------------------
    local function game_thread(fn)
        if env.ExecuteInGameThread then
            env.ExecuteInGameThread(fn)
        else
            fn()
        end
    end

    local function construct(class, outer, name)
        return env.StaticConstructObject(class, outer, env.FName(name))
    end

    local function classes()
        if not S.classes then
            S.classes = {
                UserWidget  = env.StaticFindObject("/Script/UMG.UserWidget"),
                WidgetTree  = env.StaticFindObject("/Script/UMG.WidgetTree"),
                CanvasPanel = env.StaticFindObject("/Script/UMG.CanvasPanel"),
                Border      = env.StaticFindObject("/Script/UMG.Border"),
                VerticalBox = env.StaticFindObject("/Script/UMG.VerticalBox"),
                TextBlock   = env.StaticFindObject("/Script/UMG.TextBlock"),
            }
        end
        return S.classes
    end

    local function player_controller()
        if not S.pc then S.pc = env.FindFirstOf("PlayerController") end
        return S.pc
    end

    local function set_lock(on)
        if on == S.locked then return end
        S.locked = on
        local pc = player_controller()
        pc:SetIgnoreLookInput(on)   -- proven spawner :446-452 family
        pc:SetIgnoreMoveInput(on)
    end

    ------------------------------------------------------------------
    -- Retained-tree queries (input stage uses LAST frame's issue state)
    ------------------------------------------------------------------
    -- use_now=false -> LAST frame's issue state (input stage, one-frame
    -- immediate-mode semantics); use_now=true -> THIS frame's alive set
    -- (emit stage, so the very first frame already highlights).
    local function live_buttons(win, use_now)
        local t = {}
        for _, it in ipairs(win.items) do
            local on = use_now and it.alive or it.live
            if on and it.kind == "button" then t[#t + 1] = it end
        end
        return t
    end

    local function focus_window_node(use_now)
        local win = S.windows[S.focus_win]
        local won = win and (use_now and win.alive or win.live)
        if won and #live_buttons(win, use_now) > 0 then return win end
        for _, id in ipairs(S.order) do
            local w = S.windows[id]
            local on = use_now and w.alive or w.live
            if on and #live_buttons(w, use_now) > 0 then return w end
        end
        return nil
    end

    local function focus_key(use_now)
        local win = focus_window_node(use_now)
        if not win then return nil end
        local btns = live_buttons(win, use_now)
        if #btns == 0 then return nil end
        if S.focus_idx < 1 then S.focus_idx = 1 end
        if S.focus_idx > #btns then S.focus_idx = #btns end
        return btns[S.focus_idx].key
    end

    local function hit_test(x, y)
        -- topmost first: reverse issue order, reverse item order
        for i = #S.order, 1, -1 do
            local win = S.windows[S.order[i]]
            if win.live then
                for j = #win.items, 1, -1 do
                    local it = win.items[j]
                    if it.live and it.kind == "button" and it.rect then
                        local r = it.rect
                        if x >= r.x and x < r.x + r.w
                            and y >= r.y and y < r.y + r.h then
                            return it.key
                        end
                    end
                end
            end
        end
        return nil
    end

    ------------------------------------------------------------------
    -- Input stage
    ------------------------------------------------------------------
    local function process_input(dt)
        local q = S.queue
        local i = 1
        while i <= #q do
            local ev = q[i]
            local k = ev.kind
            if k == "axis" then
                S.axis.x = ev.x or 128
                S.axis.y = ev.y or 128
            elseif k == "press" then
                local key = ev.key
                if key == "UP" or key == "DOWN" then
                    local win = focus_window_node()
                    if win then
                        local btns = live_buttons(win)
                        if #btns > 0 then
                            local d = (key == "DOWN") and 1 or -1
                            S.focus_win = win.id
                            S.focus_idx = clamp(S.focus_idx + d, 1, #btns)
                        end
                    end
                elseif key == "LEFT" or key == "RIGHT" then
                    local withb = {}
                    for _, id in ipairs(S.order) do
                        local w = S.windows[id]
                        if w.live and #live_buttons(w) > 0 then
                            withb[#withb + 1] = w
                        end
                    end
                    if #withb > 0 then
                        local pos = 1
                        for pi, w in ipairs(withb) do
                            if w.id == S.focus_win then pos = pi end
                        end
                        local d = (key == "RIGHT") and 1 or -1
                        local nw = withb[((pos - 1 + d) % #withb) + 1]
                        S.focus_win = nw.id
                        S.focus_idx = 1
                    end
                elseif key == "RETURN" then
                    local fk = focus_key()
                    if fk then S.activated[fk] = true end
                end
            elseif k == "click" then
                local hk = hit_test(S.cursor.x, S.cursor.y)
                if hk then S.activated[hk] = true end
            elseif k == "toggle_cursor" then
                S.cursor.shown = not S.cursor.shown
                if S.cursor.shown then S.cursor.moved = true end
            end
            i = i + 1
        end
        S.queue = {}
        -- stick -> cursor (once per tick, last sample wins; radial
        -- deadzone + rescale-to-full + response curve, design 2.1)
        if S.cursor.shown then
            local nx = (S.axis.x - 127.5) / 127.5
            local ny = (S.axis.y - 127.5) / 127.5
            local mag = math.sqrt(nx * nx + ny * ny)
            if mag > deadzone then
                local rescaled = (mag - deadzone) / (1 - deadzone)
                local curve = rescaled ^ curve_exp
                local f = curve / mag
                local cx = clamp(S.cursor.x + nx * f * max_speed * dt,
                                 margin, vw - margin)
                local cy = clamp(S.cursor.y + ny * f * max_speed * dt,
                                 margin, vh - margin)
                if cx ~= S.cursor.x or cy ~= S.cursor.y then
                    S.cursor.x = cx
                    S.cursor.y = cy
                    S.cursor.moved = true
                end
            end
        end
        S.hover_key = S.cursor.shown
            and hit_test(S.cursor.x, S.cursor.y) or nil
    end

    ------------------------------------------------------------------
    -- Draw-time API (issues the widget list; no engine calls happen here)
    ------------------------------------------------------------------
    local function begin_window(id, x, y, w, h, title)
        if S.current then
            S.draw_error = "BeginWindow inside window: " .. tostring(id)
            return
        end
        local t = tostring(title or id)
        local win = S.windows[id]
        if not win then
            win = { id = id, items = {}, index = {},
                    x = x, y = y, w = w, h = h, title = t,
                    alive = true, live = false, shown = false,
                    root = nil, geom_dirty = false, last_title = nil }
            S.windows[id] = win
            S.order[#S.order + 1] = id
            S.dirty = true
        else
            win.alive = true
            if win.x ~= x or win.y ~= y or win.w ~= w or win.h ~= h then
                win.x, win.y, win.w, win.h = x, y, w, h
                win.geom_dirty = true
                S.dirty = true
            end
            if win.root and not win.shown then
                S.dirty = true -- parked window reissued -> AddToViewport
            end
        end
        if t ~= win.last_title then S.dirty = true end
        win.title = t
        S.current = win
        S.text_ord = 0
    end

    local function find_item(win, key, kind)
        local it = win.index[key]
        if not it then
            it = { kind = kind, sub = key, key = win.id .. "/" .. key,
                   alive = true, live = false, added = false, added_frame = 0,
                   hl = nil, label = nil, last_text = nil,
                   tb = nil, border = nil, host = nil, rect = nil }
            win.index[key] = it
            win.items[#win.items + 1] = it
            S.dirty = true
        else
            it.alive = true
        end
        return it
    end

    local function issue_text(str)
        local win = S.current
        if not win then
            S.draw_error = "Text outside window"
            return
        end
        S.text_ord = S.text_ord + 1
        local it = find_item(win, "#t" .. S.text_ord, "text")
        it.label = tostring(str)
        if it.label ~= it.last_text then S.dirty = true end
    end

    local function issue_button(id, label)
        local win = S.current
        if not win then
            S.draw_error = "Button outside window"
            return false
        end
        local it = find_item(win, tostring(id), "button")
        it.label = tostring(label)
        if it.label ~= it.last_text then S.dirty = true end
        return S.activated[it.key] == true
    end

    local function end_window()
        if not S.current then
            S.draw_error = "EndWindow without BeginWindow"
            return
        end
        S.current = nil
    end

    ------------------------------------------------------------------
    -- Emit: the retained-tree diff (all engine calls inside this closure)
    ------------------------------------------------------------------
    local function build_window(win)
        local c = classes()
        local pc = player_controller()
        win.root = construct(c.UserWidget, pc, "OSDWin_" .. win.id)
        win.tree = construct(c.WidgetTree, win.root, "WidgetTree")
        win.root.WidgetTree = win.tree          -- proven tree-assembly write
        win.canvas = construct(c.CanvasPanel, win.tree, "Canvas")
        win.tree.RootWidget = win.canvas        -- proven tree-assembly write
        win.frame = construct(c.Border, win.canvas, "Frame")
        win.slot = win.canvas:AddChild(win.frame)
        win.slot:SetAnchors(0, 0, 0, 0)         -- proven-aggregate slot family
        win.slot:SetAlignment(0, 0)
        win.slot:SetPosition(win.x, win.y)
        win.slot:SetSize(win.w, win.h)
        win.vbox = construct(c.VerticalBox, win.frame, "VBox")
        win.frame:SetContent(win.vbox)
        win.title_tb = construct(c.TextBlock, win.vbox, "Title")
        win.vbox:AddChild(win.title_tb)
        win.title_tb:SetText(env.FText(win.title))  -- proven DRY-6 family
        win.last_title = win.title
        win.root:AddToViewport(z_win)           -- proven spawner show path
        win.shown = true
    end

    local function build_item(win, it)
        local c = classes()
        if it.kind == "button" then
            it.border = construct(c.Border, win.vbox, "Row_" .. it.sub)
            win.vbox:AddChild(it.border)
            it.tb = construct(c.TextBlock, it.border, "TB_" .. it.sub)
            it.border:SetContent(it.tb)
            it.host = it.border
        else
            it.tb = construct(c.TextBlock, win.vbox, "TB_" .. it.sub)
            win.vbox:AddChild(it.tb)
            it.host = it.tb
        end
        it.tb:SetText(env.FText(it.label or ""))
        it.last_text = it.label
        it.added = true
        it.added_frame = S.frame
    end

    local function build_cursor()
        local c = classes()
        local pc = player_controller()
        local cur = S.cursor
        cur.root = construct(c.UserWidget, pc, "OSD_Cursor")
        cur.tree = construct(c.WidgetTree, cur.root, "WidgetTree")
        cur.root.WidgetTree = cur.tree
        cur.canvas = construct(c.CanvasPanel, cur.tree, "Canvas")
        cur.tree.RootWidget = cur.canvas
        cur.border = construct(c.Border, cur.canvas, "CursorBorder")
        cur.slot = cur.canvas:AddChild(cur.border)
        cur.slot:SetAnchors(0, 0, 0, 0)
        cur.slot:SetAlignment(0, 0)
        cur.slot:SetPosition(cur.x, cur.y)
        cur.slot:SetSize(8, 8)
        cur.border:SetBrushColor(1, 1, 1, 1)
        cur.root:AddToViewport(z_cur)
        cur.built = true
        cur.on_screen = true
        cur.moved = false
    end

    local function layout_and_emit()
        -- Pure-Lua layout: rects feed this frame's highlight emit and the
        -- NEXT frame's hit-testing (toolkit-owned rects = design 2.1 route 1)
        for _, id in ipairs(S.order) do
            local win = S.windows[id]
            if win.alive then
                local y = win.y + TITLE_H
                for _, it in ipairs(win.items) do
                    if it.alive then
                        it.rect = { x = win.x + PAD, y = y,
                                    w = win.w - 2 * PAD, h = ROW_H }
                        y = y + ROW_H
                    end
                end
            end
        end

        if not S.dirty then
            -- Nothing changed since last frame: skip the game-thread
            -- dispatch entirely (console: no no-op queueing per frame).
            return
        end

        game_thread(function()
            local fk = focus_key(true)
            -- create / re-show / update windows and items
            for _, id in ipairs(S.order) do
                local win = S.windows[id]
                if win.alive then
                    if not win.root then
                        build_window(win)
                    elseif not win.shown then
                        win.root:AddToViewport(z_win)  -- parked -> reuse
                        win.shown = true
                    end
                    if win.geom_dirty then
                        win.slot:SetPosition(win.x, win.y)
                        win.slot:SetSize(win.w, win.h)
                        win.geom_dirty = false
                    end
                    if win.title ~= win.last_title then
                        win.title_tb:SetText(env.FText(win.title))
                        win.last_title = win.title
                    end
                    for _, it in ipairs(win.items) do
                        if it.alive then
                            if not it.added then
                                if it.tb then
                                    -- parked row: re-parent the SAME widgets
                                    win.vbox:AddChild(it.host)
                                    it.added = true
                                else
                                    build_item(win, it)
                                end
                            end
                            if it.label ~= it.last_text then
                                it.tb:SetText(env.FText(it.label))
                                it.last_text = it.label
                            end
                        end
                    end
                end
            end
            -- removals (dead windows park; dead items park — but items of
            -- a DEAD window stay parented: the whole subtree parks with
            -- the root, so reopen only re-Adds the root to the viewport)
            for _, id in ipairs(S.order) do
                local win = S.windows[id]
                if not win.alive and win.shown then
                    win.root:RemoveFromParent()      -- proven hide path
                    win.shown = false
                end
                if win.alive then
                    for _, it in ipairs(win.items) do
                        if not it.alive and it.added then
                            it.host:RemoveFromParent()
                            it.added = false
                            it.rect = nil
                        end
                    end
                end
            end
            -- highlight diff: only CHANGED rows emit SetBrushColor
            for _, id in ipairs(S.order) do
                local win = S.windows[id]
                if win.alive then
                    for _, it in ipairs(win.items) do
                        if it.kind == "button" and it.added then
                            local want = nil
                            if it.key == fk then want = "focus"
                            elseif it.key == S.hover_key then want = "hover" end
                            if want ~= it.hl then
                                local col = COL_NORMAL
                                if want == "focus" then col = COL_FOCUS
                                elseif want == "hover" then col = COL_HOVER end
                                it.border:SetBrushColor(col[1], col[2],
                                                        col[3], col[4])
                                it.hl = want
                            end
                        end
                    end
                end
            end
            -- cursor widget
            local cur = S.cursor
            if cur.shown then
                if not cur.built then
                    build_cursor()
                elseif not cur.on_screen then
                    cur.root:AddToViewport(z_cur)
                    cur.on_screen = true
                end
                if cur.moved then
                    cur.slot:SetPosition(cur.x, cur.y)
                    cur.moved = false
                end
            elseif cur.on_screen then
                cur.root:RemoveFromParent()
                cur.on_screen = false
            end
            -- input lock follows visibility (proven pc family)
            local any = false
            for _, id in ipairs(S.order) do
                local win = S.windows[id]
                if win.alive and win.shown then any = true end
            end
            set_lock(any)
            S.dirty = false
        end)
    end

    ------------------------------------------------------------------
    -- Tick
    ------------------------------------------------------------------
    local function tick(dt)
        S.frame = S.frame + 1
        process_input(dt)
        -- reset diff flags, then re-issue the widget list
        for _, id in ipairs(S.order) do
            local win = S.windows[id]
            win.alive = false
            for _, it in ipairs(win.items) do it.alive = false end
        end
        if S.draw_fn then
            local ok, err = pcall(S.draw_fn)
            if not ok then S.draw_error = tostring(err) end
        end
        if S.current then
            S.draw_error = "missing EndWindow for " .. S.current.id
            S.current = nil
        end
        -- dirty scan (generous over-approximation; never miss an emission):
        -- membership changes, lock transitions, focus/hover moves, cursor.
        local any_visible = false
        for _, id in ipairs(S.order) do
            local win = S.windows[id]
            if win.alive then any_visible = true end
            if win.alive ~= win.live then S.dirty = true end
            for _, it in ipairs(win.items) do
                if it.alive ~= it.live then S.dirty = true end
            end
        end
        if any_visible ~= S.locked then S.dirty = true end
        if focus_key(true) ~= S.last_fk then S.dirty = true end
        if S.hover_key ~= S.last_hover then S.dirty = true end
        if S.cursor.shown and S.cursor.moved then S.dirty = true end
        if S.cursor.shown ~= S.cursor.on_screen then S.dirty = true end
        layout_and_emit()
        -- publish liveness for the next frame's input stage
        for _, id in ipairs(S.order) do
            local win = S.windows[id]
            win.live = win.alive
            for _, it in ipairs(win.items) do it.live = it.alive end
        end
        S.last_fk = focus_key(true)
        S.last_hover = S.hover_key
        S.activated = {}
        -- PORT LAW (GY/CARRIER-DRIVE): returning true would ERASE this
        -- port's LoopAsync driver. Always false.
        return false
    end

    ------------------------------------------------------------------
    -- Public surface
    ------------------------------------------------------------------
    local OSD = {}
    OSD.SetDraw = function(fn) S.draw_fn = fn end
    OSD.PushInput = function(ev) S.queue[#S.queue + 1] = ev end
    OSD.Tick = tick
    OSD.BeginWindow = begin_window
    OSD.Text = issue_text
    OSD.Button = issue_button
    OSD.EndWindow = end_window
    OSD.GetCursor = function() return S.cursor.x, S.cursor.y end
    OSD.WarpCursor = function(x, y)
        S.cursor.x = clamp(x, margin, vw - margin)
        S.cursor.y = clamp(y, margin, vh - margin)
        S.cursor.moved = true
    end
    OSD.FocusWindow = function(id)
        S.focus_win = id
        S.focus_idx = 1
    end
    OSD.Debug = function()
        local wins = {}
        for _, id in ipairs(S.order) do
            local win = S.windows[id]
            local items = {}
            for _, it in ipairs(win.items) do
                if it.alive or it.live then
                    items[#items + 1] = it.key ..
                        (it.kind == "button" and ":btn" or ":txt")
                end
            end
            wins[id] = { alive = win.alive, live = win.live,
                         shown = win.shown, title = win.title,
                         items = items, root_ref = win.root }
        end
        return {
            frame = S.frame,
            focus = { window = S.focus_win, index = S.focus_idx,
                      key = focus_key() },
            hover = S.hover_key,
            cursor = { x = S.cursor.x, y = S.cursor.y,
                       shown = S.cursor.shown },
            windows = wins,
            draw_error = S.draw_error,
        }
    end
    return OSD
end

return M
