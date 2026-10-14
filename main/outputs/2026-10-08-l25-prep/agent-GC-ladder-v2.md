# agent-GC — LADDER-V2 (Claude round-5 item 6b) delivery

2026-10-10. Docs/analysis only — no builds, no code, no console contact.

## Deliverables

| File | Content |
|---|---|
| `mod-ladder/LADDER-V2.md` | The V2 plan: type→mod selection (final five + rejected alternatives with reasons), per-type PASS/FAIL markers (exact mod print strings), the STANDARD CHECK canonical procedure (profile → self-test → ~3 mods + 3-passes-across-restarts law), and the one-launch console test plan (staging table, harvest commands, verdicts, rollback) |
| `mod-ladder/MANIFEST.md` | First line only: superseded comment (data within still cited) |
| `mod-ladder/VERDICT-SCHEMA.md` | First line only: superseded comment (data within still cited) |

FT's STATIC-SCAN, FU's RESULTS, FS's API-PARITY-TABLE and FV's keymap docs
were NOT touched — they remain the cited data of record.

## The four picks (five with the spawner)

| Type | Pick | Core reason |
|---|---|---|
| event/hook | **CheatManagerEnablerMod** (1,407 B) | one unguarded RegisterHook(ClientRestart) fired by save load; hook-param get + object-ref Set; FU load-clean + hook fired; marker-producing PASS shapes (`Constructed CheatManager [0x…]` / `Enabled CheatManager` / skip-shape) |
| value/array-write | **ConsoleEnablerMod** (3,173 B) | deepest write in the pool (TArray growth + struct-field Set + FName roundtrip) WITH the mod's own read-back print (`ConsoleKey[%d]: %s` listing appended Tilde/F10); load-time trigger, FU full-path clean |
| config/io | **TownfallMiniMap v3.1** (196,743 B CRLF) | the only runnable io consumer (FT gap #5, FS §5); settings read nil-degrades, write pcall-swallowed; io verdict tiered FULL/PARTIAL pending the io shim |
| sustained-tick | **TownfallMiniMap primary** (LIGWD 50 ms :4268, observable = live HUD updating over time) **+ StaminaMod secondary** (2,811 B, 13th candidate, LoopAsync 1500 ms proven engine, zero footprint; honest limit: loop body no-ops, evidence = registration + survival) | MiniMap legitimately covers two type slots with DISTINCT evidence streams — matching Claude's "~3 mods" economy |
| menu (control) | **TownfallItemSpawner** (resident, original bytes) | proven baseline; co-residency control law (EF) |

## Sustained-tick overlap findings (as ordered)

- **SplitScreenMod does NOT tick** — zero Loop*/delay calls (FT census); it is
  a value-write candidate only, kept as the write-slot FALLBACK.
- **ModMenu ticks (LIGWD 100 ms, pcall-guarded) but is REJECTED** for the
  combined launch: BACKSPACE/UP/DOWN/RETURN co-fire with spawner nav (EF
  finding 5 spawner-free-boot law — incompatible with a spawner-resident
  launch), input-mode/pause mutations, heaviest hook surface in the pool
  (9 hooks), 128 KB, and its tick is silent without its menu. Documented as
  the spawner-free follow-up session if needed.
- MiniMap is the ONLY candidate with an OBSERVABLE sustained tick → primary;
  stamina adds LoopAsync engine breadth + de-risks MiniMap's unguarded :4268
  load call being the sole tick provider.

## Interference verdict: GO

- **Zero new PS5Pad.ini rows** — none of the four picks registers any
  RegisterKeyBind key (ConsoleEnabler's Tilde/F10 are engine ConsoleKeys,
  invisible to the pad extractor per FV). The live 3,718 B console-proven
  ini stays byte-identical; no MFA re-pin; no accidental chord can fire
  anything. GENERATED-BASELINE.ini stays unused.
- No console-command handlers in the set; no shared keybinds; only MiniMap
  touches io (self-contained degradation).
- Hook surface ≈ 5 registrations. ONE WATCH-ITEM: CheatManager and
  ConsoleEnabler BOTH hook ClientRestart and ConsoleEnabler unhooks after
  first fire — if UnregisterHook mis-removes the chain, CheatManager goes
  silent on later restarts; the 3-restart law is the detector.
- Second watch-item: MiniMap's guarded SetGamePaused/SetInputMode paths
  (reachable only via its pcall-guarded FKey input polling — BOUND-UNTESTED,
  degrades to S.pc=nil). Session guidance: one UI open at a time.
- Ticks: one LIGWD (50 ms) + two LoopAsync — different carriers, no
  collision; spawner F7 pump doubles as the pump-wide control.
- Per-mod Lua states keep attribution clean (a MiniMap load error cannot
  corrupt the other four mods' markers).

## Standard check (codified in LADDER-V2 §3)

Profile (decrypt-on acquisition → ref_axes refuse-if-not-UE → anchor pins →
two-source layout facts → generate_profile units → live-base resolution) →
self-test (PASS bar: >=95% rows PASS AND 0 FAIL in the four type-critical
families: hooks / property+array writes / io / delay+loop; red critical
family = STOP, mods never run against it) → ~3 mods (one real mod per type,
smallest-primary-function mod, built-in observable required, exclusion laws:
offsets/crash-law/BP-loader/profilers; overlap allowed with distinct
evidence; 3-5 mods range) → **3-passes-across-restarts law**: type ACCEPTED
only when the same markers pass in 3 clean-exit launches (1 + up to 2
restarts), byte-identical staging; any FAIL = FAIL with owner named; a build
change resets the cycle.

## Console plan (one launch + up to 2 restarts)

Round-4 PRX (decrypt-off verified) + the four mods staged (+8 size-exact
loadlist rows: 1407/3173/196743/2811 + zero-byte enabled.txt each; UEHelpers
and PS5Pad.ini untouched) + resident spawner. Launch → save → in-world →
self-test combo (GB) → spawner F7 control → 60-90 s MiniMap HUD captures →
clean exit → harvest `--label ladder2-combo-r1`; repeat r2/r3 byte-identical.
Rollback = the 8 rows off + row-count check (PS5Pad.ini has nothing to roll
back). Never rapid launches; crash → harvest + reboot; klog gates every run.
