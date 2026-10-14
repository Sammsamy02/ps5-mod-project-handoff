# Adversarial review: refused-hooks track

Verdict (plain): the list itself is sound. All 22 rows, the 12 self-test hooks, the minimap's 3 hooks, the spawner's LoadMap hook and the log-line citations check out. But two conclusions in it are wrong or unproven, and both would misdirect the fix queue. Refuted: yes (2 major, 5 minor).

Labels: Mac-only = read from code or logs in this review. `C` = candidate/src (LuaMod.cpp sha 4d6bc63b). `D` = B4 decoded ue4ss log.

## What was checked and holds

- Candidate sha unchanged (4d6bc63b). Nothing was written outside the track dir. No console IP in the deliverables.
- All 12 self-test GAPS registrations (part8.lua:14-20, part9.lua:5-9) are listed (R07-R14, R17-R20). main.lua:44 and :101 behave as described. The pass2 comparison log (lines 1247-1281) shows the `PASS registered` / `PASS ret=nil` / FAIL pattern as claimed.
- Minimap hooks (main.lua:515 inside the 506-519 loop, :4249) and the spawner's hook (main.lua:731) are the only non-self-test hook calls in the pinned mod set. The other mods use only native `ClientRestart` hooks (D:807, 818, 893-894). The mod-side list is complete.
- Code citations spot-checked and correct: LuaMod.cpp 5994, 6017, 6043, 2435, 3062-3103, 8871, 8896-8983, 9002, 9104, 9230, 9331, 9691-9701. Log citations D:621, 623, 658, 668, 764-766, 776-778, 780, 807, 818, 893-894, 908-918, 920-922 are correct.
- `staged/ueselftest-gaps-from-runtime-counter.diff` applies cleanly (`patch -p1 --dry-run` on a scratch copy). `refused-log-vs-B4-candidate.diff` applies cleanly with `-p1`.

## Problems

### 1. MAJOR: R21/R22 (frame delays) are not unblocked by the vtable extent fix (Mac-only, code)

REFUSED-HOOKS.md plain verdict item 5 and the R21 row say fixing the extent rule "would unblock the Tick pump, the LoadMap hook and frame delays together". It will not.

- `ExecuteInGameThreadAfterFrames` and `LoopInGameThreadAfterFrames` throw at LuaMod.cpp:6782-6784 and :7011 whenever `funcptr_marshal_usable()` is true.
- `funcptr_marshal_usable()` (:4669-4683) is just `R3MarshalPumpPending()`.
- `pump_pending` is set true at UnrealInitializer.cpp:2941, and the comment at :2948-2951 says it "stays true" even after `vmt_pump_arm`. So with the VMT pump armed the throw is unchanged.
- Even past that gate, `VmtEngineTickDrain` (:5542-5600) calls `process_delayed_actions<kFuncPtrMarshalExecutor>`. That executor skips frame-based actions (:4891-4905: marshal executor only takes marshal-tagged actions, and `delay_frames > 0` with a non-EngineTick method logs "should not happen" and continues).
- Console comparison log pass2 lines 1279/1281 show the exact refusal text "frame-ba[sed delays need an EngineTick pump; only the R3 FuncPtr-marshal pump exists...]", which is this branch.

Needed instead: change the two gates (6782, 7011), add frame counting to the VMT drain (new executor tag or frame counter), and keep it out of the marshal path. R21/R22 data-only verdict should say "needs code changes in LuaMod, plus the extent fix", not "same blocker as R05".

### 2. MAJOR: R01 route skips the function-lookup precondition, and the refusal site is asserted without evidence (Mac-only)

- The MD states the minimap's RegisterHook throws at LuaMod.cpp:6043. The first thing RegisterHook does is `StaticFindObject<UFunction*>` by path (:5992), and it throws at :5994 if that fails. The JSON lists 5994 too, but the MD and the proposed log reason (`bp-script-function-needs-processinternal-hook`) assume 6043.
- The runtime's own comments say lookups of Blueprint function names by path do not resolve on this console: LuaMod.cpp:5686-5693 ("on console the StaticFindObject lookups cannot resolve at all (GATE-1 numbered duplicates ... 'not interned')") and :5855-5860 ("long-name lookups cannot resolve: GATE-1 numbered duplicates"). The carriers avoid this by scanning the object array, not by path.
- So the P1 "YES, Func swap" route may never be reached: a branch change at 6017 does nothing if the call dies at 5994. The report does not mention GATE-1 or a scan-based resolver anywhere (zero hits for StaticFindObject/GATE-1 in MD and JSON).
- No B4 line shows which throw the minimap hit (the mod pcall swallows it), and the report does not say the refusal site is unknown. The staged logger's `reason` split should separate not-found from wrong-branch, and R01 needs a "resolve path" item in its needs list.

### 3. MINOR: LoadMap slot "two independent sources" is not independent (rule: CLAUDE.md section 3)

The two sources are ENGINE-SLOT-FACTS-v2.json (0x4F8) and D:616 (`GameEngine::LoadMap address 0x3c4d460`). D:616 is the runtime reading the same profile offset, so it confirms the table content, not that the slot is LoadMap. The facts file itself says `live_validation: false` and "Method identity is an inference from named native callers". The report's own replay (slot 159 -> 0x384d460) also reuses 0x4F8. Say "one static inference plus consistency check" until a second identification exists.

### 4. MINOR: existing world-change watcher not mentioned for R05/R06

LuaMod.cpp:5110-5135 and :5247 contain a bounded 1 Hz World-identity watch (`[FL-MAP] ... src=marshal-watch`) that already detects map loads with data reads only. It logs only, but it is the nearest existing data-only source for a LoadMap "post" delivery and is not in the R05/R06 route analysis. (No `[FL-MAP]` lines are in the B4 log, so it did not fire there; the report could have said so.)

### 5. MINOR: small omissions

- `UnregisterCustomEvent` (LuaMod.cpp:3105, self-test part8.lua:11) is not in the table. It works (a no-op on an empty list) but should be noted next to R03.
- `ExecuteInGameThread` itself throws at :6136/:6184 when no pump or carrier is armed yet; not a hook, but it is a conditional refusal next to the frame-delay ones.
- UE4SS-settings.ini:15 has `bUseUObjectArrayCache=false`, so the class create listener (UnrealInitializer.cpp:604) is not registered in this config. Relevant to the R04 route 1/2 discussion; not mentioned.

### 6. MINOR: vtable extent finding is stated as fact in the plain verdict

Item 5 says the pump "fails because its 512-entry scan runs past the end". The body labels it Mac-only, but the verdict text does not. The replay uses `text_lo=0` as its code range, so relocated small values (entry 175 = 0x120, entry 184 = 0xe0) are counted as code, which the runtime classifier may not do. The B4 log has no entry index. Reasonable hypothesis; label it as such and keep the proposed index-and-value log line as the way to confirm.

### 7. MINOR: staged-patch and rule slips

- REFUSED-HOOKS.md:128 says `patch -p0`. That fails (29 of 29 hunks ignored). It works with `-p1` run from the directory that contains `src/`.
- One host-test compile ran while vm_stat showed about 1.1 GB free (task rule: skip under 2 GB). Self-disclosed; no harm found, but it is a rule breach.

## Not refuted

R03-R20 silent-refusal classification, the GAPS-counter finding, the 66-carrier console proof for Func swap on script functions, the capacity numbers (256 total), and the "detour-only" calls for CallFunctionByNameWithArguments and exact Pre-hooks.
