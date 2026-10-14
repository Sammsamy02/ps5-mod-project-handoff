# CURRENT STATE — Townfall UE4SS / Item Spawner (read in 3 minutes)
Day 2026-10-04 15:15Z → 10-05 02:31Z. Full detail: INDEX.md next to this file.
GOAL: run the real Item Spawner mod under actual UE4SS on the PS5 via our owned
modbridge runtime — no PC, no AI in normal use.
WHERE IT STANDS: everything is built and staged; the runtime has never yet reached
"engine-ready". Eight launches root-caused seven distinct blockers, each fixed once; launch 9
is the first that can plausibly reach the UE4SS startup chain.
1. LADDER: rung 1→2 PROMOTED with in-game evidence — GUObjectArray validated (layout A)
   at 0x97ba110 (71,959 objects, read+write); image correspondence 4/4 immutable data
   windows at base 0x400000; axes SHIPPING/STATS-OFF + identity 5.6.1-0+++UE5 from the
   game. Rung 3+ NOT promoted. No spawner seen yet.
2. WHAT TODAY PROVED: a ploader late-load PRX runs real code inside Townfall; the full
   memory map was recovered (21,721 records); the 196-import composition loads and
   module_start runs; six DLL families (InitOnce, Mutex, FileIO, CRT, ModuleEnum, VM)
   are console-proven (combined build passed).
3. LAUNCH HISTORY: L1 no image anchor→r3 window anchor; L2 VMMAP ENOMEM with save→r4
   16k buffer; L3 fixed-stride parse fail→r5 per-record walk; L4 anchor verify failed
   →r6 walk-contract fix; L5 ANCHOR VERIFIED but game_executable_path required→r7
   multi-window gate; L7 kernel path absent→r9 discovery; L8 conversion+context PASSED
   first time; read-lease then threw — extent clamp not page-aligned → r11 page-quantized.
4. CURRENT STAGED BUILD: composition-5 r11, fself 0911af0c…, live+v2 offline gates 8/8
   GO both (zcode_fresh_r11_gate_pair). Slot /data/plugins/modbridge_supplier_
   composition.prx; aligned_entry=2, path_test=2, ingame_facts=0.
5. LAUNCH 9 WILL TEST composition-6 = configured roots (Resolution B: explicit
   game/working dirs; PRX never substitutes the exe path) + r11 lease fix + D2
   text-base guards. Scaffold + change manifest on disk; the D2-only dry build ALREADY
   passed the offline gate GO (35ba028e…). Controller runs comp6_one_pass.py: ODR
   pre-flight → build → gate v2 → stage (never auto-launches).
6. HONEST OUTCOMES: (a) best — conversion passes → context → lease+D2 hold →
   engine-ready → mods-started → OPTIONS at a loaded save opens the Spawner; (b) likely
   honest stop — PS-scan can't match anchors in exec-only text ("PS scan timed out";
   required-slots worker IN FLIGHT at close, no REPORT); (c) loud failure — entry r4
   stamps failures timestamp+pid, runtime-status.txt has the stage code; (d) fail-open.
7. AFTER MODS-STARTED: main.lua runs AFTER engine-ready (not in module_start); the
   popup needs a LOADED SAVE ("Load a save first." expected without one). Ritual: game
   closed → send ploader → WAIT for OK popup (1–60 s; HTTP OK ≠ armed) → launch →
   records ~1 min in; FTP reads wait 15–30 s with a fresh connection.
8. RIDEALONGS: observer PRX (1afa0997…) + qdrv1 (52c32216…) ready to ride =2 in the
   same session; 5 of 10 ploader slots used → ≥4 spare; pre-image sha 8d331b3f.
9. ALSO PENDING (game-closed windows): DLL family run with Static TLS — 8-family build
   preferred, 7-family (83ee2ae9) frozen fallback; installer E2E (a21771f8) after
   patches.json promotion apply + ACK-exit-6 schema amendment; optional rung-3 probe.
   Do NOT launch the FName FC1 probe: direct exec-page reads crash the module (igf6),
   an inline-syscall probe crashed the console (igf5) — decide the exec-span approach first.
10. DLL TRACK: SEH64 host-verified only; MSVCP140 assessed; third-party DLL mod
    acquisition needs real-Nexus OAuth rebuild + your sign-in + explicit GO. APP
    TRACK: backend 61/61, modmgr 18/18, 5 web-UI patches ready. Team: 12 agents incl.
    controller (user decision; 30-simultaneous hit 1302s).
11. TODAY'S INCIDENTS (why safety rules exist): argv double-dispatch crashed the
    console; igf5 crashed console+game; a power outage killed 9 subagents (all
    re-dispatched, nothing lost). ≥30 s dispatch gaps, one-dispatch rule,
    timeout=crash-signal (CONSOLE-SAFETY-PROCEDURE.md).
RETURN-SESSION SEQUENCE:
  i.   Console sanity: reachable; staged files intact vs INDEX Track-8 inventory.
  ii.  Finish composition-6: run/verify comp6_one_pass.py → gate GO → stage via
       stage_composition5_cycle.py → readback band check.
  iii. Launch 9 with a loaded save; post_launch_check.py + rung_check.py; menu
       runbook CP0–CP7 (banner → OPTIONS → CROSS spawn).
  iv.  If engine-ready/mods-started: collect rung-1/2 evidence via qdrv1 records;
       decide the exec-span approach before any code-read probe.
  v.   Game-closed windows: 8-family DLL test (7fam fallback), then installer E2E.
HONESTY: nothing above claims completion, parity, or a passed launch. Every claim traces
to the ledger (zcode_* keys) or an on-disk receipt; 20 spot-checks with citations are in
REPORT.md. Launch 6 has no dedicated ledger entry (gap). Open: required slots, FName ctor.
