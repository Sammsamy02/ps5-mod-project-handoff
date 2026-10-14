# agent-FV-keymap -- AUTO CONTROLLER MAPPING (keymap-gen + baseline)

Agent FV, 2026-10-10. Offline only: no console contact, no builds, no tree
edits. Created exactly three files under `outputs/2026-10-08-l25-prep/keymap/`
(plus this report): `keymap-gen.py`, `GENERATED-BASELINE.ini`, `MAPPING.md`.

## Deliverables

1. **keymap-gen.py** (pure Python 3 stdlib). Parses each mod's main.lua for
   `RegisterKeyBind`/`RegisterKeyBindAsync` uses -- direct calls AND
   table-driven `["Key"]=Key.X / ["ModifierKeys"]={...}` entries (Keybinds
   mod), with Lua-comment stripping so commented binds are ignored
   (zzdiag-v10:7 test case). Emits a parser-legal PS5Pad.ini:
   - stable hash assignment: `slot = ladder[sha256("FV1|"+key_identity) % N]`
     over the KEY IDENTITY only, so same key -> same button on every
     regeneration regardless of enabled-mod set (verified by regen-diff);
     collisions resolve by deterministic linear-probe suffix; pool exhaustion
     leaves the key UNMAPPED with a WARN block, never a reserved-button
     fallback.
   - reserved, never auto-assigned: OPTIONS (pause), CROSS/CIRCLE/SQUARE/
     TRIANGLE, d-pad, L3/R3 (L3+R3 = universal mod-menu chord, key F7).
   - console-proven rows preserved verbatim via `--pin-pad` (today's L3+R3=F7,
     UP/DOWN/CROSS/LEFT/RIGHT = arrows/RETURN).
   - `--check` mode: mirrors the C++ parser rule-for-rule (sections, 16-button
     table, KeyDef name table, modifier rules, one-'=' law, no trailing
     comments, 128/16384/ASCII limits, chord-containment law), reports
     unmapped/orphan/reserved violations + `--expect-size` MFA gate; exit
     0/1/2. Self-validates its own output on every generate.
2. **GENERATED-BASELINE.ini** for the 12 staged mods (MANIFEST paths) +
   zzdiag-v11 (F7) + TownfallItemSpawner console copy. 2,422 B (ASCII, LF,
   trailing newline; sha256 1f07a85a3b60393789a87112db7e683837e5e98db34f1a790d00679dcb5c59c3).
   16 bindings = 6 pinned + 10 auto; 6 shared-key groups ride pinned rows for
   free (F7: spawner+zzdiag; UP_ARROW/DOWN_ARROW/RETURN: spawner+ModMenu;
   LEFT_ARROW/RIGHT_ARROW: spawner). 4 keys unmapped by deterministic pool
   exhaustion, all on never-to-run mods: Keybinds CONTROL+NUM_SIX/FIVE
   (LOAD-ONLY; callables absent from the port), BPModLoaderMod INS (SKIP),
   ActorDumperMod CONTROL+NUM_THREE (SKIP). Every RUN-ladder bind is mapped:
   F3=TOUCH_PAD+R1, CONTROL+Y=TOUCH_PAD+L2, CONTROL+U=TOUCH_PAD+R2,
   CONTROL+I=L1+R1, BACKSPACE=L1+R2 (ladder pairs10).
3. **MAPPING.md** -- schema (with parser line pins), reserved table + Townfall
   assumptions, free ladders (ergonomic 7 / pairs10 10; C(5,2)=10 is the hard
   antichain ceiling over the 5 free buttons), stability/conflict/suffix
   rules, and the manager integration sketch (atomic local swap + MFA
   re-admission steps).

## Key findings (answers to the task's open questions)

- **PS5Pad.ini IS modforge MFA size-admitted.** Loadlist row
  `file config ue4ss/PS5Pad.ini 3718`; MFA re-checks every row's size at
  `module_start` (`modforge_activation.c:185-204`); mismatch = activation
  fails all-or-nothing, `mfa_activate_failed code=-8`, game continues
  unmodded. Any regenerated ini MUST have its loadlist row re-pinned
  (`wc -c`) before upload + readback `cmp` after (plain text: the ftpsrv
  SELF-decrypt hazard does not apply). keymap-gen prints the re-pin reminder
  whenever output != 3718 B, and `--check --expect-size` re-verifies offline.
  Byte discipline implemented: deterministic bytes (no timestamps; identical
  inputs -> identical file -> no re-pin), ASCII <= 0x7e, LF, trailing \n.
- **Today's F7/L3+R3 evidence preserved exactly.** The pinned block is carried
  verbatim from the l23 staged 3,718 B profile (md5 5510caf1...; before/after/
  harvest identical). zzdiag's F7 and ModMenu's nav keys share those rows
  (UE4SS binds are non-exclusive key-level notifications), so nothing about
  tonight's working path changes.
- **Parser gotchas encoded**: no trailing comments on data lines; exactly one
  `=`; `[PS5Pad]` before `[Bindings]`; chord-containment law forbids mixing a
  single button and any chord containing it (ladders are antichains); 16-bit
  button space only -- no back button/gyro exists for this profile; the
  16,384-B cap; em-dash incident history (non-ASCII once nearly killed a
  launch).
- **Divergence documented**: CANDIDATE-SHEETS/TEST-PLAN treated
  TRIANGLE/CIRCLE/SQUARE as free (proposed SQUARE=F10); this mapping reserves
  all face buttons per the task. ConsoleEnablerMod's Tilde/F10 are ENGINE
  ConsoleKeys additions, not RegisterKeyBind keys -- invisible to the
  extractor by design; a pad row for F10 would be a reviewed manual pin.

## Verification performed (local only)

- Extraction cross-checked against MANIFEST.md's per-mod bind facts for all
  14 inputs (spawner 6, zzdiag F7, LineTrace F3, SplitScreen CTRL+Y/U/I,
  ModMenu BACKSPACE+3 shared, Keybinds 7, BPModLoader INS, ActorDumper
  CTRL+NUM_THREE; MiniMap/CheatManager/ConsoleEnabler/ConsoleCommands/BPML/
  jsbProfiler: none).
- Determinism: two regenerations of the full set = byte-identical to the
  baseline (cmp).
- Stability: regenerating without Keybinds/SKIP mods leaves every surviving
  row byte-identical (key-identity hashing).
- Cross-validation: the live 3,718 B console-accepted ini parses clean under
  the tool's mirror parser (6 bindings), and `--check` on GENERATED-BASELINE
  with `--expect-size 2422` gives 0 violations / 4 documented warnings; wrong
  expect-size correctly trips a VIOLATION (MFA gate demo). zsh note: pass mods
  as separate argv (arrays), since zsh does not word-split scalar variables.

## Caveats

- GENERATED-BASELINE.ini is a STAGING CANDIDATE ONLY. Nothing was uploaded; it
  must not be deployed without its loadlist row re-pin (2,422 B) and the
  mod-ladder gating (the 10 auto rows only matter once their mods actually
  ride a launch; mapping SKIP-mod keys in the file is harmless since those
  mods are staged per MANIFEST but "never run, never replay").
- The 4-key overflow is a hard ceiling of the 5-free-button antichain; mapping
  them would require either un-reserving a face button (reviewed decision,
  game-co-fire cost) or a bigger button space (parser change, out of scope).
- Lua extraction is regex-based over the known mod shapes (direct calls +
  quoted-table entries); exotic indirection (binds assembled at runtime from
  computed Key values) would be missed -- none exists in the staged set.
