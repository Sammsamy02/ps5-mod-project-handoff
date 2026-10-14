# MAPPING.md -- auto controller mapping: PS5Pad.ini schema, policy, and manager integration

Agent FV, 2026-10-10 (l25-prep/keymap). Everything below is verified against
the RETAINED sources the console payload actually runs, not docs alone:

- Parser: `.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp` (`parse_ps5_pad_config`; identical copy at `native-coherent-pad-runtime-1/build-1/source/PS5PadConfig.cpp`)
- Key names: `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Input/src/KeyDef.cpp` (`string_to_key`, lines 12-171)
- Live staged profile (3,718 B, md5 5510caf11fb9307931fe26602245205d): `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/PS5Pad.ini` (before/after/harvest identical; agent-AI 1.3)
- Provider edge semantics: `outputs/2026-10-08-l22-runtime/candidate/PS5PadSource.cpp:119-177` (conjunction rising-edge, one event per chord completion)

Artifacts in this directory:

- `keymap-gen.py` -- the generator + `--check` auditor (pure Python 3 stdlib, no builds, no console)
- `GENERATED-BASELINE.ini` -- output for the 12 staged mods + zzdiag-v11 + TownfallItemSpawner (2,422 B, sha256 1f07a85a...c59c3)

---

## 1. EXACT ini schema the payload consumes

```
[PS5Pad]
User = -1

[Bindings]
CHORD = MODIFIERS+KEY
```

Enforced by `parse_ps5_pad_config`; ALL violations are hard refusals with line
numbers (a present-but-invalid file makes the runtime start throw; an absent
file means no pad bridge at all -- `NativePadInput.cpp:13-23`):

| Rule | Detail |
|---|---|
| Sections | `[PS5PAD]` then `[BINDINGS]` (case-insensitive), each at most once, in that order; any other section header is an error |
| `[PS5Pad]` | exactly one option: `User = -1` (first logged-in user) or a nonnegative id |
| Rows | `LEFT = RIGHT` with EXACTLY ONE `=` on the line; empty name/value is an error |
| LEFT (chord) | one button or `'+'`-joined buttons, no repeats; names (case-insensitive): `L3 R3 OPTIONS UP RIGHT DOWN LEFT L2 R2 L1 R1 TRIANGLE CIRCLE CROSS SQUARE TOUCH_PAD` -- exactly 16 bits, nothing else exists (no back button, no gyro, no touch swipe) |
| RIGHT (key) | zero or more modifiers from `SHIFT CONTROL ALT` (each at most once, `'+'`-joined) then a final key name resolved by `string_to_key` -- the full UE4SS KeyDef set (F1..F24, A..Z, ZERO..NINE, NUM_ZERO..NUM_NINE, arrows `UP_ARROW/DOWN_ARROW/LEFT_ARROW/RIGHT_ARROW`, `RETURN`, `BACKSPACE`, `INS`, `SPACE`, ...); names are case-insensitive; `reserved_start_of_enum` is rejected |
| Comments | full lines only, first char `;` or `#`. **No trailing comments on data lines** (a `;` after the key makes the key name unknown -> parse refusal) |
| Text discipline | 1..16384 bytes total; every byte <= 0x7e; of control bytes only `\n \r \t`. NO em-dash / non-ASCII -- a stale em-dash pad config once nearly killed a launch (agent-I audit, l23 checkpoint incident) |
| Cardinality | at least 1 binding, at most 128 |
| Containment law | no chord's button mask may be a subset or superset of another row's mask (`old ⊆ new || new ⊆ old` both rejected); no duplicate chord |
| Delivery | rising-edge conjunction: `data.buttons & chord == chord`; hold one button, tap the other; exactly one key event per chord completion (no auto-repeat); dispatched on the UE4SS update thread |

Keybind side (Lua): `RegisterKeyBind(Key.X)` / `RegisterKeyBind(Key.X, {ModifierKey.CONTROL,...}, cb)`. Binds are NON-EXCLUSIVE notifications: a mapped button still reaches the game, and every mod registered for the delivered key fires. Mods must compensate with `SetIgnoreMoveInput`/`SetIgnoreLookInput` (spawner already does, RUNTIME-REQUIREMENTS C4).

## 2. Reserved buttons (game controls -- NEVER auto-assigned)

Assumptions for Townfall (PPSA33286, UE5, standard DualSense action layout;
documented, not all individually console-verified):

| Button | Owner | Basis |
|---|---|---|
| `OPTIONS` | game pause | proven tonight: l12 deliberately returned OPTIONS to the game; stay unbound forever |
| `CROSS` `CIRCLE` `SQUARE` `TRIANGLE` | game actions (confirm/interact, cancel/back, context) | standard action layout; CROSS also carries the pinned RETURN row (legacy, menu-scoped) |
| `UP` `RIGHT` `DOWN` `LEFT` | game menus / d-pad | standard; UP/DOWN/LEFT/RIGHT also carry pinned nav rows (legacy, menu-scoped) |
| `L3` `R3` | stick clicks; **L3+R3 = universal mod-menu chord** | the console-proven menu combo; no other row may use L3 or R3 in any combination |

Divergences from earlier team docs, deliberately stricter here:
- `multimod-prep/CANDIDATE-SHEETS.md` treated TRIANGLE/CIRCLE/SQUARE as free singles and TEST-PLAN proposed `SQUARE = F10`; this mapping reserves all face buttons (task rule: no conflicts with game controls). ConsoleEnablerMod needs no row anyway -- its Tilde/F10 are ENGINE `InputSettings.ConsoleKeys` additions (main.lua `KeysToAdd`), not UE4SS RegisterKeyBind keys; if a pad chord for F10 is ever wanted it must be a reviewed manual pin, not an auto row.
- The pinned legacy rows themselves use d-pad + CROSS. They predate this policy, are console-proven (l12/l22/l23 sessions; `[l22] keybind callback entry` fired every run), and are menu-scoped with game-side compensation. New assignments never touch these buttons; the generator carries the rows verbatim from `--pin-pad`.

## 3. Free-button pool and ladders

Free buttons (not game-reserved, not in the universal chord):
`TOUCH_PAD L1 R1 L2 R2` -- five buttons. Touchpad click is assumed unused by
Townfall (safe: it is the least game-coupled input; assumption documented).

Because of the containment law, singles and chords over the same button cannot
mix. A ladder is a fixed antichain (no member contains another, and no member
shares a superset/subset relation with any pinned row):

- **ergonomic** (default, 7 slots): `TOUCH_PAD`, `L1+R1`, `L2+R2`, `L1+R2`, `L2+R1`, `L1+L2`, `R1+R2` -- one easy single plus two-handed shoulder pairs
- **pairs10** (10 slots): all pairs over the five free buttons -- `TOUCH_PAD+L1`, `TOUCH_PAD+R1`, `TOUCH_PAD+L2`, `TOUCH_PAD+R2`, `L1+R1`, `L1+R2`, `L1+L2`, `R1+L2`, `R1+R2`, `L2+R2`

C(5,2) = 10 is the maximum possible antichain over 5 buttons, so **10
auto-assigned rows is the hard ceiling** of this pad profile (16 total with the
6 pinned rows; parser ceiling is 128). There is no back button in the parser's
button space; "hold combos" are exactly the pair chords above.

## 4. Assignment: stability, conflicts, suffixes

1. Extract: for every enabled mod's `main.lua`, strip Lua comments, then match
   direct `RegisterKeyBind(Key.X, [{ModifierKey.*}], ...)` /
   `RegisterKeyBindAsync(...)` calls AND table-driven entries
   (`["Key"] = Key.X` + `["ModifierKeys"] = {ModifierKey.*}` -- the Keybinds
   mod's shape). Commented-out calls are ignored (zzdiag-v10 line 7 test case).
2. Group by key identity (`MODIFIERS+KEY`, e.g. `CONTROL+Y`). A key wanted by
   several mods is ONE row -- all their callbacks fire (UE4SS semantics); the
   output lists every consumer.
3. Pinned rows are carried verbatim; their keys are "served". Any mod wanting a
   served key (zzdiag F7; ModMenu's UP_ARROW/DOWN_ARROW/RETURN via the spawner
   nav rows) gets it for free -- shared note, no slot consumed.
4. Each remaining key gets `slot = ladder[sha256("FV1|" + key_identity) mod len(ladder)]`.
   Hashing the KEY IDENTITY ONLY (not the mod set) makes assignments stable:
   same key -> same button on every regeneration, regardless of which mods are
   enabled (verified: dropping Keybinds/SKIP mods left every surviving row
   byte-identical). This subsumes the "same mod+key -> same button" rule.
5. Collision (two keys hashing to one slot): deterministic suffix = forward
   linear probe through the ladder (wrap). Pool exhausted: the key stays
   UNMAPPED and is listed in the ini's WARN block + stderr -- never a silent
   drop, never a reserved-button fallback.
6. Priority: `--mod` argument order. When the pool runs short, earlier-listed
   mods keep slots (baseline order: spawner, zzdiag, then MANIFEST RUN order,
   then LOAD-ONLY, then SKIP -- so overflow lands on never-run mods first).

Baseline outcome (pairs10): 6 pinned + 10 auto rows; 14 distinct new keys from
14 mods; the 4 unmapped are exactly `Keybinds` CONTROL+NUM_SIX / CONTROL+NUM_FIVE
(LOAD-ONLY, callables reference C++ globals absent from the port) and SKIP mods
BPModLoaderMod `INS`, ActorDumperMod `CONTROL+NUM_THREE`. Every RUN-ladder mod
(LineTrace F3; SplitScreen CONTROL+Y/U/I; ModMenu BACKSPACE) is mapped.

## 5. Mod-manager integration sketch

On enable/disable of any mod (or zzdiag version bump):

1. Rebuild the `--mod NAME=PATH ...` list from the enabled set (order = the
   pinned priority list above; keep it in the manager config, not ad hoc).
2. Run `keymap-gen.py ... --pin-pad <current console-proven profile> --ladder pairs10
   --out <staging>/PS5Pad.ini` -> validate (the tool self-parses its output and
   hard-fails on any reserved-button use, containment breach, >128 rows,
   >16384 B, or non-ASCII byte).
3. Atomic swap locally: write `PS5Pad.ini.tmp`, fsync, `mv` over the staged
   file (same filesystem), then `wc -c` the final bytes.
4. **MFA size re-admission (mandatory)**: PS5Pad.ini IS size-exact admitted.
   The modforge loadlist carries `file<TAB>config<TAB>ue4ss/PS5Pad.ini<TAB><bytes>`
   (today 3718). MFA re-checks all rows at `module_start`
   (`modforge_activation.c:185-204` size gate); any mismatch fails activation
   ALL-OR-NOTHING with `mfa_activate_failed code=-8` and the game silently
   continues unmodded. So: pin the new byte count in the loadlist row BEFORE
   upload, upload binary-safe (LF endings, no BOM, keep trailing newline --
   quirks: settings INI law applies identically to this INI), then readback
   `cmp` the file (plain text, so the ftpsrv SELF-decrypt hazard does not
   apply). `keymap-gen.py --check <ini> --expect-size <pinned>` re-verifies the
   whole chain offline.
5. Determinism note: regenerating the SAME enabled set yields byte-identical
   output (no timestamps; inputs-sha256 header) -> same size -> NO re-pin
   needed. A changed set changes the size -> step 3-4 mandatory.
6. Rollback: keep the previous generated ini + its loadlist row pair; disabling
   the offending mod and regenerating reproduces the old bytes exactly
   (key-identity hashing), so rollback is also deterministic.

Bounded extra (documented, not implemented): manual pins for keys outside
RegisterKeyBind visibility (e.g. engine ConsoleKeys F10) should go through a
reviewed `--pin-pad` profile update, never through hand edits to the generated
file -- hand edits break the size-pinned provenance chain.

## 6. --check mode

`keymap-gen.py --check <ini> [--pin-pad <pinned>] [--expect-size N] --mod ...`

Reports: parser-rule violations (with line numbers, mirroring the C++ parser),
reserved-button use by non-pinned rows, chord-containment breaches, unmapped
mod keys, orphan rows no enabled mod wants, shared-key co-fire disclosures,
byte size vs `--expect-size` (the MFA gate) and vs the 16384 B law. Exit 0
clean / 1 warnings-with---strict / 2 violations. Cross-validated: the live
3,718 B console-accepted profile parses clean under the tool's mirror parser,
and GENERATED-BASELINE.ini passes with 0 violations (4 documented unmapped
warnings).
