# Agent IC — Claude round-8 items 2+3 (2026-10-10)

Lua-only edits + host validation. No console/build contact. Files touched:
zzdiag-v11/main.lua, zzdiag-v11/EXPECTED-MARKERS.txt, ueselftest/main.lua,
ueselftest/EXPECTED-MARKERS.txt.

## 1. zzdiag-v11 P7 fname-eq (item 2)

Added to the ids() ladder after P4 (main.lua:44-47), main target AND
per-target repeats, label `[PRB-7]` + sfx:

```lua
local o7, r7, t7 = pcall(function() local f = FName(o4 and s4 or s3) return f == ii, str(f) == s3 end)
d(L(7) .. " fname-eq " .. (o7 and "ok=" .. bo(r7) .. " w=" .. bo(t7) or "e"))
if not o7 then el(r7) end
```

- Builds FName from the SAME string ToString returned (s4; falls back to
  the tostring text s3 when ToString failed — visible from P4's ok= flag
  one line above), then ONE pcall'd `==` (r/ok=) plus the labeled weaker
  tostring round-trip tostring(FName(text))==tostring(id) (w=), both on
  one line; indented el() tail on throw.
- Verdict semantics (full decision tree in EXPECTED-MARKERS): ok=1 is
  DEFINITIVE real-name-text — a freshly constructed userdata can never be
  identity-equal and Lua 5.x consults __eq from either operand's
  metatable, so ok=1 is only reachable via a registered bridged __eq
  (FL's ComparisonIndex compare) returning true. ok=0 w=1 = weak-positive
  fallback (no usable __eq; tostring round-trip only — labeled). ok=0 w=0
  with P4 ok = index-fallback signature. `e` + nil-global tail = the port
  does not expose the Lua FName constructor at all.
- SKIP ladder widened to match: no-target -> PRB-1..7 SKIP, no-valid ->
  PRB-2..7 SKIP (was ..6).

## 2. ueselftest RUN start line (item 3)

main.lua:53 `P("RUN start 1.." .. M)` right after the busy guard — prints
exactly `[ST2] RUN start 1..167` as the FIRST line of the run. Run still
ends with SUMMARY + done (lines 72-73).

## 3. Byte counts (cap 4096, size-exact law)

- zzdiag-v11/main.lua: 4094 -> **4086** (room made by compressing
  comments ONLY — two-line FJ header, ids() ladder comment, st() legend
  now "-- e/n/v/i"; logic untouched. Relocated notes + new size recorded
  in the IC addendum of EXPECTED-MARKERS.txt. Z0 marker unchanged; the
  revision is identified by the new size.)
- ueselftest/main.lua: 3961 -> **3987**
- ueselftest part2..part12: unchanged, max 3869. All 12 deployed files
  under 4096. (validate.lua 14619 B is host-only, not deployed.)

## 4. Host validation (lua 5.5.1 host)

- luac -p clean on both edited files.
- ueselftest/validate.lua: exit 0, **45 PASS, ALL SCENARIOS PASS**
  (S1 primary/S2 resume/S3 io-off). Supplemental ordering check:
  K1@3 < RUN-start@4 < part2@5 < idx1@17 — start line is unambiguous and
  pre-index.
- zzdiag stub harness (preserved evidence/tmp-preserved-20261010/tmp/
  zzdiag-v11-harness.lua, run as-is): exit 0, all scenarios + WD clean;
  P7 appears in every ladder walk; with the harness's missing FName
  global it reads `e` + stripped indented "attempt to call a nil value
  (global 'FName')" (fixed the initial ts() form which printed the chunk
  path prefix instead of the reason).
- Supplemental /tmp stubs (not repo files): FName with bridged __eq ->
  `[PRB-7] fname-eq ok=1 w=1`; __eq removed -> `ok=0 w=1`. All three
  verdict shapes (strong/weak/error) exercised.

## 5. Trigger-path confirmation (item 4)

L1+R1 -> F12 -> ueselftest has NO dependency on anything in
round4-setaside/ (FirstLaunchProfile.{cpp,hpp}, KernelBridgeProbes.{cpp,hpp},
LuaPathShim.{cpp,hpp}, LuaMod-insertion-block.txt — all C++ engine-side):

1. PS5Pad-round4-proposed.ini:19 [Bindings], :31 `L1+R1 = F12`; pad rows
   are the pinned console-proven l23 profile (comment :20-22, md5
   5510ca...) — pad driver shipped, not set-aside. Keybinds non-exclusive
   (:13-14) so the game still gets L1/R1.
2. ueselftest/main.lua:79 `RegisterKeyBind(Key.F12, ...)` — pure Lua;
   RegisterKeyBind is shipped LuaMod.cpp:2118, ExecuteInGameThread :5745
   (citations from FJ's v11 markers header). Callback is K1 log ->
   pcall(ExecuteInGameThread, RUN) -> K2 log.
3. RUN (main.lua:51-75) is pure Lua: part loading via require/dofile,
   optional io progress, print. No GATE-1/FName-pool, no FindFunction/
   bpfn, no spawns.
4. Honest nuance: the set-aside LuaPathShim would have improved io/
   require logical-path resolution if installed; without it the run
   still triggers and completes — pf=false is a documented tolerated
   degradation (EXPECTED-MARKERS boot section) and any part-load gap
   names itself via LOAD-FAIL. The K1-without-RUN-start signature now
   isolates a game-thread drain stall from everything downstream.
