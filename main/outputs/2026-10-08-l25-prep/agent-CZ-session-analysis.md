# Agent CZ — l26-v2 + v6 session analysis report (2026-10-08 night)

Full detail: `outputs/2026-10-08-l25-prep/SESSION-L26V2-ANALYSIS.md` (same dir).
Mission-doc appendix appended (run matrix + corrections). Raw logs untouched;
readable normalized copies in /tmp/*.norm.log.

## Timeline headline

Five game launches tonight, not two. Console boot ~19:14 CDT (no reboots
after): (1) l26-v1 19:34:38–19:39:09 — save LOADED (delete-listener counters
2→6,181→13,970 ≈100–128/s = level-load churn), UE4SS log tail died 19:38:57
under the OLD logwriter, game itself ended by NORMAL user quit — NOT a kernel
crash (klog: suspend path, no signal/coredump); (2) a 20-second ghost boot at
20:58:56 — the ONLY real crash of the night (pid 305, RTHeartBeat SIGSEGV
executing libkernel.sprx+0x10ec, xotext page-not-present, coredump failed
0x80aa000c), launched with the ploader already dead → effectively unmodded,
pre-UE4SS-logging, klog-only evidence (monitor polls missed it); (3) 20:59–
21:01 unmodded again (ploader dead), clean user quit — PRX 23c8483d (20:46
build) NEVER executed on console; (4) **l26-v2** 21:23:10–21:26:10 on the
post-revert 21:18 build (PRX e13b1d0b, the ONLY source delta vs 20:46 being
the root's fix-2a revert at LuaUObject.hpp:821-829: FNAME_Add member names →
upstream FNAME_Find) — first fully-readable UE4SS log ever, 3 listener Adds
(Num 1→4), engine-called counts 2→**349** (≈6.4/s menu churn — **GC storm did
NOT survive the revert**), 6 probe cycles all db/ks=invalid with the clean CN
null-wrapper error, EIGT 13/13/13, normal user quit; (5) **v6** 21:29:26–
21:31:43 same PRX, zzdiag v6: warm pump (self-test drain 7 ms vs 11600 ms
cold), 2 cycles N1pc/N2db/N3en/N4ks **all -1**, zero [FFO]/[FN]/[UF] markers
anywhere, normal user quit overlapping the 2nd probe.

## White-dot hypotheses (ranked)

The mod drew NOTHING — proven: F7 → scanItems fails (line 66, caught) →
buildUI() still runs but exits at the getPlayerController() gate (UEHelpers
path AND FindFirstOf("PlayerController") both nil) BEFORE any widget
construction; zero buildUI log lines in any session; navigation keybinds
no-op (menuOpen never true). Ranked: (1) the game's own title-screen UI
(cursor/pip/loading dot) — session never left the main menu; (2) PS5 system
overlay (notification/controller hint) — invisible to logs; (3) partially-
created mod widget — REJECTED (above); (4) UE4SS GUI — REJECTED ("GUI …
unavailable" at init); (5) R3 carrier BP-override side effect — interceptors
only, no draw path; (6) HEN GPU/compositor glitch — no GPU faults in klog,
can't fully exclude.

## UEHelpers verdict

Silent / indeterminate BY CONSTRUCTION: zero UEHelpers-pattern lines in every
readable session (l25-B-final, l26-v1, l26-v2, v6); the spawner requires it
lazily under pcall inside getPlayerController, so module-missing, lookup-error
and nil-return are all swallowed; its cached lookups ride the same FindXOf
machinery probed empty tonight. zzdiag-v7's U1/U2/U3 tokens exist to
discriminate — run them IN-GAME.

## Key analytical caveats (for the parent)

1. ALL FindXOf probes tonight were MENU-TIMED (~18–21 s after UE4SS start; no
   save loaded in l26-v2/v6 — their delete churn is 6/s vs 100/s during v1's
   save load). db/ks invalid and v6 all-nil are the CA-Predicted menu-state
   results (CDO-only, IsValidObjectForFindXOf excludes CDOs). **No in-game
   FindXOf datapoint exists in ANY session to date** — v7's header premise
   ("FindXOf is empty for EVERY class in-game") is not established by tonight's
   evidence. The decisive experiment (same probes with a save loaded) is still
   unrun.
2. Zero [FFO] with the CP fix in the binary = every probed name RESOLVED via
   FNAME_Find and the walk still returned nothing (or no live non-CDO instance
   existed) — consistent with CO #2/#3 territory OR plain menu-state absence;
   only an in-game run separates them.
3. The l26-v1 "crash" should be re-labeled in records: old-logwriter tail loss
   + user quit; the kernel never saw a crash. The real crash of the night
   (20:58:56, unmodded RTHeartBeat/libkernel) must NOT be attributed to any
   PRX.
4. Walk cost estimate: 4 full GUObjectArray walks + spawner scan + 66-override
   carrier re-arm < 1 s (cycle 1, same-second); second cycle crossed the 1 s
   boundary (≤2 s). Per-walk ≈ 100–400 ms at ~72k live objects — matches the
   perceptible hitch per F7 press.

## Anomalies worth flagging

Ghost crash (see headline); "CRASH KILL_END" klog event = NORMAL quit path
(naming trap — verify with surrounding suspend context before calling any
future end a crash); "SIGSEGV: normal" klog lines = Mono/PSM banner, not
crashes; ploader can die between arm and launch (assert a live ploader
immediately before each user launch — PRX 23c8483d never ran); pool-hook and
LoadMap hook failures are pre-existing (also in l25-B-final); 'N2RC6Unreal18FOb'
size-exposure failure (benign, first readable sighting); carriers print raw
FName ids (fname_tostring unbound by design); ClassCastFlags 0xD8 staged value
live (port 0xD0 never surfaces); v6 second probe overlapped the quit.

— agent CZ (ZCode), 2026-10-08. Read-only over logs/trees; wrote only the two
l25-prep analysis files + this mission-doc appendix.
