# Launch card - capability #5: crash report

**What we test:** when the game crashes, can we tell exactly where it crashed?
**How:** our own small test mod (`zzcrashtest`) crashes the game on purpose, once. Then a Mac tool reads
the PS5's crash record and must name the exact spot.

- One launch only. Downloaded mods are not changed. The game's code is not patched.
- This launch crashes on purpose, so it does **not** count as one of Townfall's "2 clean launches".
- Run it on its own. Do not combine it with another test launch.

## Before the launch (Claude does this; each console step needs your OK)

- [ ] New build made once; all checks PASS; the build's `FINAL-SYMBOLS-STATUS` says `PASS`.
- [ ] One reviewer approved the runtime change.
- [ ] New PRX uploaded and checked (your OK).
- [ ] `zzcrashtest` test mod copied to the console and the load list checked (your OK).
- [ ] Console offline. The Mac crash-log recorder (klog tap) is running BEFORE the game starts.
- [ ] Fallback ready: the last good build, R11 (`bfd95db6`).

## What you do

1. Start Townfall as usual.
2. Stay on the title / main menu. **Do not load a save.**
3. Wait about 3 minutes. The game closes by itself with an error. **This is expected.**
4. Press OK. If the PS5 shows "creating crash report", let it finish. Do not send anything.
5. Tell Claude "the game is closed".

## How we decide

**PASS** = the Mac tool prints both of these:
- `EXPECT PASS: RIP inside 'modbridge_cap05_crashtest_fault'` (it named the right spot)
- `EXPECT PASS: fault VA 0xc08` (it saw the exact bad address we planted)

Helpful but not required: the game's mod log may also show `[CAP05] GATE-CHECK PASS` and
`[CAP05] CRASHING NOW`. If those log lines are missing (the crash can cut the log short), that alone
is **not** a FAIL - the two lines above decide.

| If we see | It means |
|---|---|
| `ModBridgeCrashTest is missing` | the console has the wrong build |
| `GATE-CHECK FAIL` | the safety check misbehaved; no crash happened; fix first |
| the game never closes | the planted crash did not fire |
| no crash record from the recorder | the recorder was not running; nothing captured |
| `CONVENTION-MISMATCH` or `no local artifact with build-id` | the Mac tool used the wrong build's files |
| a different function named | the Mac tool named the wrong spot: capability FAIL |

## Afterwards

Remove the `zzcrashtest` mod and its load-list rows, and check the load list (your OK).
If anything went wrong, put R11 (`bfd95db6`) back.
