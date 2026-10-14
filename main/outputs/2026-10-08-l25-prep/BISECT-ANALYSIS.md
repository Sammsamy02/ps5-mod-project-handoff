# BISECT ANALYSIS — launch-2/3 F7 + stutter root causes (2026-10-11)

## RESULTS SO FAR (round-4 build, 59b0528b, all tests)

| Config | F7 spawner | Stutter | Keybind entries |
|---|---|---|---|
| spawner + zzdiag only | ✅ works | ✅ clean | ✅ fires |
| + ueselftest | ✅ works | ✅ clean | ✅ fires |
| + CheatManager + Console + Stamina + MiniMap | ❌ fails | ❌ stutters | ✅ fires (launch-2, new build) |
| + CheatManager + Console + Stamina (no MiniMap) | ❌ fails | ✅ clean | ❌ ZERO entries |
| spawner + zzdiag + ueselftest (re-verified) | ✅ works | ✅ clean | ✅ fires |

## STUTTER ROOT CAUSE: MiniMap (SOLVED)
The minimap's 50ms tick causes the stutter. Removed = clean.
Next step: profile the minimap's tick to identify the specific call.

## F7 BLOCKER: one of {CheatManager, ConsoleEnabler, Stamina}

### GetWorld fallback analysis (Claude item 2): NOT the cause
The IK 02b fallback is an OUTER-CHAIN POINTER WALK (2-3 hops: Actor→Level→World),
NOT an object-array scan. O(depth), not O(40K). Additionally:
- Round-4 does NOT have the fallback (GetWorld throws → pcall catch → fast)
- Round-4 STILL fails with all mods → the fallback is irrelevant to the failure
- The stutter existed on BOTH builds with the minimap → same conclusion

### Suspect ranking (for the F7 blocker):

**1. ConsoleEnablerMod (HIGHEST)**
- Writes to the ConsoleKeys TArray (the deepest write in the pool per GC)
- Array-growth write: ConsoleKeys[#ConsoleKeys+1].KeyName = FName("Tilde")
- If the FName-array growth corrupts adjacent memory (the TArray reallocation
  path), it could damage the keybind dispatch table or the input handler
- The write fires at LOAD TIME (before any keybind can be tested)

**2. CheatManagerEnablerMod (MEDIUM)**
- Hooks ClientRestart via RegisterHook (the UFunction Func-swap)
- Constructs a CheatManager object → changes the game's input processing
- In UE, a CheatManager takes over console command processing and can
  intercept/redirect input handling
- The hook fires on every world load → cumulative effect

**3. InfiniteStamina (LOW)**
- LoopAsync at ~1.5s intervals, modifies stamina values
- No input/hooks/UI interaction — purely a value modifier
- Cannot see a mechanism for blocking keybind dispatch

### The zero-keybind-entries anomaly
In the "no minimap" session (round-4 + CM + CE + Stamina), there were
ZERO keybind callback entries in the entire log — not even for keys the
user certainly pressed (the session lasted minutes of gameplay). This is
DIFFERENT from launch-2 (new build, all mods) where key=118 DID fire.
Possible explanations:
a) The ConsoleEnabler's array write corrupted the keybind table
b) The CheatManager changed the input mode blocking all pad input
c) The log was captured mid-session (game was still running during harvest)
   — the game-closed harvest would show more

## NEXT BISECT STEPS (staged, one per launch):
1. + CheatManagerEnabler (STAGED — ready for user launch)
2. + ConsoleEnabler (if CM is clean, this is next)
3. + InfiniteStamina (if CE is clean, this is next)
4. If ALL three are individually clean → interaction between two of them

## AFTER BISECT: the fix
Once the blocking mod is identified, the fix depends on the mechanism:
- ConsoleEnabler: route the array write through a safer path (single
  pre-allocated entry, or a different console-enabling method)
- CheatManager: delay the hook or make the CheatManager construction
  safe for concurrent keybind dispatch
- Stamina: unlikely to be the cause; if it is, inspect the LoopAsync
  callback for hidden engine calls

## Claude items completed in this analysis:
- Item 1 (d90563f5 log): harvested; 0 F7 presses (user didn't press F7
  in that short session before the next issue was reported)
- Item 2 (GetWorld fallback): analyzed — outer-chain walk, NOT a scan;
  not the cause (round-4 also fails, round-4 has no fallback)
- Item 3 (round-4 + all mods): executed — same failure, confirming
  mod interaction not build regression

## DEEP ANALYSIS (while user away): the TArray corruption theory

### ConsoleEnabler's array write — the mechanism
```lua
ConsoleKeys[#ConsoleKeys + 1].KeyName = KeyName
```
This triggers the port's TArray growth path. In the port's implementation:
1. Lua reads `#ConsoleKeys` (array size)
2. Lua writes to `ConsoleKeys[N+1]` — this must REALLOCATE the TArray
3. Then writes `.KeyName` to the new element

If step 2's reallocation is buggy (writes past the current allocation without
properly growing, or the FName assignment at step 3 uses the wrong stride),
it corrupts whatever follows the array in the UObject's memory layout.

The InputSettings object's ConsoleKeys TArray is followed in memory by
other UInputSettings fields — potentially including input-related state.

### Why host tests pass but console fails
The host harness (FU) stubs the array write (the stub's `__index`/`__newindex`
metamethods handle the growth). On console, the REAL TArray member functions
run — the same wide-libc/name-seam class that bit us with FName. The
`.KeyName =` assignment writes an FName through the marshaling layer.

### CheatManager input interference — the secondary theory
In UE, `PlayerController.CheatManager = CreatedCheatManager` changes the
input processing pipeline:
- The engine's InputKey path checks for a CheatManager
- A CheatManager can intercept keys meant for the game/mod handlers
- The CheatManager is constructed on EVERY ClientRestart (world load)
- In the no-minimap session, "Enabled CheatManager" was logged — confirming
  the assignment happened — and ZERO keybinds fired in the entire session

### Testing priority (bisect order):
1. +CheatManagerEnabler (STAGED — tests the input theory)
2. +ConsoleEnabler (tests the TArray corruption theory)
3. +Stamina (long shot — but its 1.5s LoopAsync + UEHelpers chain could
   saturate the game-thread queue)

### If BOTH are individually clean:
The interaction: CheatManager changes input mode → ConsoleEnabler's array
write under the changed mode corrupts different memory. Test with CM+CE pair.

### THE FIX (once identified):
- ConsoleEnabler: use StaticConstructObject to pre-allocate entries, or
  write the KeyName through the FName bridge (avoiding the TArray growth)
- CheatManager: delay the hook to after the first keybind test, or
  make the construction happen on a non-input thread

## INPUT DISPATCH CHAIN (traced while user away)

The PS5 pad → keybind → Lua callback pipeline:

1. **PS5PadNative.cpp** (the composed PRX): opens a borrowed pad handle,
   reads `scePadReadState` → PS5PadSource::poll() → produces InputEvents
2. **Input::Handler::process_event()** (Handler.hpp:77): consumes events
   from the pad source, calls the registered keydown callbacks
3. **LuaMod.cpp:2207**: the keybind callback lambda fires, logging
   `[l22] keybind callback entry key={}`
4. The lambda calls the Lua callback function

### The gate: `allow_input`
PS5PadSource::poll() line ~140: `if (!baseline && down && !m_down[i] && allow && subscribed[b.key])`
- `allow` comes from `get_allow_input()` on the Handler
- The console-state check that would set it false is **COMMENTED OUT** in UE4SSProgram.cpp (line 102 and 1825: "Commented out because this system doesn't work properly")
- BUT the `allow` parameter is still passed to poll()

### The critical question: what else can make `allow` false?
Looking at the Handler interface: `set_allow_input()` exists. If nothing
calls it (the console check is commented out), `allow` should default to
true. UNLESS:
- The Handler's constructor initializes `m_allow_input` to false
- Something else calls `set_allow_input(false)`
- The pad source's `is_available()` returns false (a different failure —
  the handle was never opened)

### NEXT DIAGNOSTIC: check the Handler's default `allow_input` state
If the default is false, and the console-state check (which would set it
to true when the console is closed) is commented out, then `allow` is
always false and keybinds NEVER fire. But they DID fire in the proven
config... unless something in the additional mods changes the Handler's
state.

### REVISED THEORY: it's not `allow_input` at all
The keybinds DID fire in the proven config (spawner+zzdiag) and in
launch-2 (all mods, new build). They did NOT fire in:
- round-4 + CM + CE + Stamina (no minimap)
- round-4 + CM + CE + Stamina + MiniMap (all mods)

The difference between "fires" and "doesn't fire" is:
CheatManagerEnabler + ConsoleEnabler + InfiniteStamina being loaded.

The most likely mechanism is NOT the allow_input gate but rather:
1. The **CheatManager's UFunction hook on ClientRestart** interfering with
   the R3 carrier dispatch (the same game-thread pipeline that delivers
   keybind callbacks)
2. The **ConsoleEnabler's TArray write** corrupting the Handler's
   subscribed-keys array or the PS5PadSource's binding table
3. A **deadlock or mutex contention**: the additional mods' callbacks
   holding the Lua state mutex when the keybind tries to acquire it

The bisect tests are staged and will discriminate between these.
