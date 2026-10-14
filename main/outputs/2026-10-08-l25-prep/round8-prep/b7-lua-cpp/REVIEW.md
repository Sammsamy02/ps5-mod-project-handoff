# REVIEW of track b7-lua-cpp (adversarial, Mac-only, read-only)

## Plain verdict

The track's main work holds up: upstream parity, the E1/E2 defects, the catch inventory, the staged patches and the
gates all check out. But I found one wrong premise that the plan's risk framing and decision 3 rest on, so I mark the
track **refuted on that point**. The claim "no Lua hook callback is live on the PS5, because hooks are refused" is
false for hooks on native UFunctions. Everything else I found is minor or already admitted by the track.

## What I checked and could NOT refute (Mac-only)

- Parity: `LuaRaw/CMakeLists.txt:7` (`LUA_COMPILE_AS_CPP` OFF), `xmake.lua` (default false, `LUA_COMPILED_AS_CPP` public),
  `include/lua.hpp:7-15`, `ldo.c:53-61` (C++ throw chosen by `__cplusplus`), grep for `LUAI_THROW`/`LUA_USE_*`/`COMPILE_AS_CPP`
  outside LuaRaw: 0 hits. All as written.
- Port today: `luaraw-ldo` argv in `l26-A/pass1/COMPOSITION-RECEIPT.json` = `-std=c11 -Os -fPIC ... -D_XOPEN_SOURCE=700 -pthread`. Matches.
- `lapi.c:1248`, `ldo.c:516-533` (`precallC` unlock/lock), `ldo.c:138-150` cited correctly. Lua version 5.4.7.
- Lock: `ue4ss-native-lua-lock-final-1/src/platform_locks.c` `LuaLockFinal` calls `LuaUnlock`, so the probe's `lua_close`
  does not leak the global lock. (I suspected this; the older `original/` copy has a no-op `LuaLockFinal`, the final one does not.)
- E1 (`LuaUObject.cpp:135-150` + `:471-478`) and E2 (`LuaMadeSimple.cpp:1570-1595`): the reasoning is right.
- Catch inventory: all 22 `catch (...)` in `LuaMod.cpp` and the other Lua files are in `CATCH-INVENTORY.json`; the 21 = 6+5+7+3 split adds up.
- Staged patches: `patch -p1 --dry-run` is clean for `b7-on-candidate` and `b7-on-b5final` against the live candidate sources;
  `b7-on-b6` correctly fails against the non-B6 tree. `0001-build-variants...patch` applies clean to `build_variants.py` (sha 60408cef).
- Host-test totals: 65 + 67 + 6 = 138, 0 failed, matches `RESULT-before-void-catch-probe.txt`. Section-size table sums to +132.
- No candidate/original/mod/evidence files edited by this track (live candidate `LuaMod.cpp` is now the final-b5 sha `afaf5caa`
  and its mtime is the driver's B5 snapshot, not this track's).

## Problems

### P1 (major): "no Lua hook callback is live on the PS5" is contradicted by the B4 console log

Claimed in `B7-PLAN.md` verdict item 6, section 6, section 8.3, and `CATCH-AUDIT.md` verdict bullet 4, 3f, and the H4 table
("Hook callbacks ... not live on PS5: hooks refused"; "the data-only thunk route ... route not live"). Evidence against it
(decoded B4 launch-1 log, `evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log`, console-proven):

- `D:807` and `D:818`: `[RegisterHook] Registered native hook (3, 4) / (5, 6) for Function /Script/Engine.PlayerController:ClientRestart`
  (CheatManagerEnablerMod and ConsoleEnablerMod). Code path: `LuaMod.cpp` RegisterHook native branch (`~:6020-6032`) calls
  `UFunction::RegisterPreHook(&lua_unreal_script_function_hook_pre ...)`, which goes through the `ScriptHookThunks` Func swap.
- `D:866-D:894`: both mods' callbacks ran (CheatManager constructed and enabled, ConsoleEnabler removed its own hook,
  "Unregistering native pre-hook (5)" at `D:893-894`).
- `D:918`: `[l25] marshal hook first invocation (carrier fired)` = the thunk route fired from game code.
- The report (`REPORT-l26-B4-launch1-20261012.md` lines 99 and 197) says only **Blueprint**-function hooks are refused
  (`HookProcessInternal=0`). `D:780` lists the built-in detour-style script hooks as unregistered; it does not say native-UFunction `RegisterHook` is refused.

So the H4 path (Lua hook callback running below game frames via `Thunk<Index>` -> `Dispatch` -> `FirePreCallbacks` -> `lua_unreal_script_function_hook_pre`)
is **live today**, and `ScriptHookThunks.cpp:75-86` catches `std::exception&` only. Practical effect looks small (a Lua raise
there has no `errorJmp` on the hook thread, so Lua panics/aborts in both builds, `lms_lua_panic` at `LuaMadeSimple.cpp:97`;
the C++-only difference is when the main thread is mid-pcall), but the plan's wording, the "parked, not live" framing, the gate's
LATENT-H4 classification (warning only) and decision 8.3 all need restating. Fix: correct the text, move
`script_hook`, `lua_unreal_script_function_hook_pre/post` and `Thunk<Index>` from "latent" to "live", and decide whether `--strict-h4`
should be wired in before B7b rather than after.

### P2 (minor): allow-list reason for `UE4SSProgram.cpp:1899` is inaccurate

`gate/catch-allowlist.txt` says the loop's only Lua-reaching callee is the noexcept `Poll`. The same try block runs queued
`event()` callables (`UE4SSProgram.cpp ~:1820-1838`), `m_input_handler.process_event()` (keybind lambdas -> `call_function`) and
`mod->fire_update()`; all are indirect calls the name-based rule cannot see. They are probably safe (keybind lambdas use `lua_pcall`
inside `call_function` and convert to `std::runtime_error`), but the stated reason is wrong and the "130 other sites read by hand" claim did not cover them.

### P3 (minor): small factual slips

- `B7-PLAN.md` section 9: "`lua_close` never called". It is called at `LuaMod.cpp:8576` and `:8624`; only `Lua::~Lua` has it commented out (`LuaMadeSimple.cpp:644-645`).
- `B7-PLAN.md` section 2 says the `catch (void*)` is added "in front of 12 catch-alls"; the patches add 11 (E1-E10, E7 twice) plus the probe's own handler.
- `CATCH-AUDIT.md` line numbers are B4 lines; the live candidate `LuaMod.cpp` is now the final-b5 file, so the numbers no longer match the tree (the doc does say "B4 candidate").
- The `.eh_frame`/PRX numbers in section 3 (`l26-package-A/pass-1/bare-sdk`, sha d044cf0b) cannot be re-checked: that directory was overwritten at 10:16 by a newer build (`.eh_frame 0x6d8c58` now). The qualitative claim (PT_GNU_EH_FRAME, `.eh_frame`, `.gcc_except_table` present) still holds.

### Already admitted by the track (not counted against it)

Extended host test and PS5-toolchain compile of the 5 edited files did not run (2 GB rule); syntax of the `LuaMod.cpp`
edits and the probe's `catch (void*)` are therefore uncompiled. The "tested" wording in the plan must be read as "E1/E2 and
the first probe were tested". The PS5 unwinder behaviour is proven only by the probe line at launch.

## Not in this track's scope

The refused-hook list (reviewer item 4) was not part of this task; note that it must also include the native-UFunction hooks
that DO register (P1), so it does not repeat this mistake.
