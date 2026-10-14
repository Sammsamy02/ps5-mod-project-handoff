# agent-HL-osdtoolkit.md — OSD v1 host prototype (capability #11, route (a))

Agent HL (ZCode), 2026-10-10, l25-prep packet. Retry after a provider
outage — no prior progress existed; built from zero this session.
Mac-local Lua 5.5.1 only: no builds, no console contact, no tree edits,
no real mods touched, NO stub extension needed (the toolkit runs entirely
on the stub's existing reflected surface — byte-identity re-proof below).

## Deliverables — outputs/2026-10-08-l25-prep/osd-toolkit/

| file | what |
|---|---|
| `toolkit.lua` (662 lines, 5.2–5.5 subset, luac -p clean) | the toolkit: `BeginWindow/Text/Button/EndWindow` immediate-mode API over a RETAINED widget tree keyed by id-path (`win/item`; buttons by explicit id, texts by per-window ordinal); create-on-first-use, update-on-change, park-and-reuse; dirty-gated emit inside `ExecuteInGameThread`; list-nav focus model (UP/DOWN/RETURN + LEFT/RIGHT window cycling) + virtual cursor (uint8 stick → radial deadzone 0.15 + rescale + curve 2.2 + 1600 px/s + viewport clamp; pure-Lua rect hit-testing); input event queue (`PushInput`: axis/press/click/toggle_cursor); `Tick` ALWAYS returns false (LoopAsync erase law). |
| `host-test.lua` (436 lines) | S1–S5 with 67 assertions, PASS/FAIL lines, exit code. |
| `run.sh` | entry point; `LUA` env override; execs host-test.lua. |
| `HOST-RUN-LOG.txt` | canonical green run (67/67, exit 0), sha256-pinned below. |
| `BRIDGE-NOTES.md` | console bridge: the 8 reflected-call families + status labels, game-thread law, tick dependency (GY cross-refs), stick-axis input feed, staging shape, honesty scope. |

## Scenario results (all PASS; full log: HOST-RUN-LOG.txt)

- **S1 3-window demo** — frame 1 emits exactly **Create×31 / SetText×12 /
  AddChild×15 / SetContent×7 / AddToViewport×3 / SetBrushColor×1 / lock×2 /
  1 game-thread dispatch** (3 shells = 6 constructs each; winA 13, winB 9,
  winC 9 item constructs). 5 identical frames: **total recorded calls
  149 → 149 (ZERO)**. One label change: creates still 31, SetText 12→13.
- **S2 list-nav** — default focus winA/b1; DOWN→b2 with exactly **2
  SetBrushColor per move** (old row + new row); queued events keep order
  (2×DOWN in one tick → b3); clamps at last row; UP returns; RETURN
  round-trips the status line in **1 SetText**; RIGHT skips buttonless
  winB to winC/go; LEFT returns to winA/b1; `Tick()==false` asserted.
- **S3 virtual cursor** — cursor widget = 4 constructs once; sub-deadzone
  deflection (x=138) = ZERO motion; x=200 moves right with curve (one
  SetPosition per MOVING frame, 30/30); warp+click hit-tests winA/b1 and
  activates it; click in empty space activates nothing; full deflection
  clamps at the 2px viewport margin; **pinned at the wall = ZERO
  SetPosition while fully deflected** (update-on-change).
- **S4 100-frame soak (the retained-tree proof)** — **Create 31→31,
  SetText 12→12, brush/addvp/remove/addchild all +0, total engine calls
  149→149, dispatches 1→1**: the draw callback re-runs every frame and the
  diff re-issues nothing. This is the host proof that the tree is
  retained, not rebuilt.
- **S5 close/reopen reuse** — closing a window = exactly **1
  RemoveFromParent** (the root; the subtree parks with it); input lock
  held while other windows stay open, released (2 calls) when the last
  closes; 30 all-closed frames emit ZERO calls; reopen = **1
  AddToViewport, 0 StaticConstructObject, 0 SetText**, lock re-engaged,
  and the **SAME UserWidget object identity** (rawequal across
  close/reopen); dropping/re-adding one button row = 1 RemoveFromParent /
  1 AddChild / 0 creates / 0 SetText, and the row still receives focus +
  activation afterward.

## Calibration law — upheld (27/27, twice)

`host-lua-harness/run-all.sh` (26 logs) + the bonus-stamina repro (27th)
re-run BEFORE work began and AGAIN after completion (no stub extension
was ultimately needed — the toolkit binds only existing surface):
**all 27 logs byte-identical to `runs-baseline/` both times**; aggregate
`shasum -a 256 runs/*.log | sha256sum` =
`9d29049a3123b5cf7e6d1895db241f22563b33fc7424ca0d828eba611ad12d7e`
(matching GN's recorded post-extension aggregate). Files touched this
session: only `outputs/2026-10-08-l25-prep/osd-toolkit/*` (new) and the
regenerated `host-lua-harness/runs/*.log` (byte-identical).

## Key design findings (feed the console ladder)

1. **Idle frames should not queue no-op closures**: the emit is
   dirty-gated — an unchanged frame issues ZERO engine calls AND zero
   `ExecuteInGameThread` dispatches. On console this means a coarse
   LoopAsync cadence (100–250 ms) suffices for spawner-class menus; P7 is
   not a v1 dependency (cross-ref CARRIER-DRIVE).
2. **Window-level park beats item-level teardown on close**: removing
   only the root keeps reopen at exactly 1 AddToViewport with full object
   identity — the cheapest possible close/open cycle.
3. Counting caveat for future host tests: the stub records a method
   LOOKUP (`prop:X.M`) in addition to each INVOCATION (`call:X.M`) —
   family counters must match the `call:` prefix only (this inflated
   first-run counts 2×; fixed in host-test.lua's snapshot()).
4. Stick uint8 center (127/128) makes exact-axis-zero impossible: a
   deflected stick always drifts ~0.04 px/frame on the other axis —
   radial-deadzone behavior, documented and tolerated in S3 assertions.

## Not done / out of scope (by law)

No console contact, no builds, no engine/runtime source changes, no edits
to stub-ue4ss.lua / path-shim / runner / real mods / zzdiag, no
console-staging split of toolkit.lua (needs GB-style <4096 B parts when
its window comes). Engine-side unknowns (FAnchors/FText marshaling, slot
return types, per-row SetBrushColor) remain with console S0–S3 per
CAP11-OSD-DESIGN §5.
