# FETCH-PLAN — selection criteria + acquisition plan for additional PC Lua mods

Agent-EF, 2026-10-08. Task 2 of the multimod-prep packet. **DOCUMENTED FOR
LATER — NOT EXECUTED.** No network access was used for this packet. The local
pool (CANDIDATE-INVENTORY.md) already covers the first three test slots; this
plan governs any future widening of the pool (e.g. to reach "popular Nexus
mod" diversity beyond the one resident Nexus example, TownfallMiniMap).

## 1. Hard selection criteria (all must hold)

1. **Size**: total staged payload < 30 KB per mod (sum of all files that need
   loadlist rows). Rationale: the acceptance evidence for each mod is a
   line-by-line API audit against agent-CI-marshaling-audit.md's table; the
   review surface must stay hand-auditable, and size-exact admission staging
   stays cheap.
2. **Shape**: `Mods/<Name>/Scripts/main.lua` (+ optional sibling scripts) +
   `enabled.txt`. No DLLs, no .pak/BP assets, no custom C++/detours, no GUI
   frameworks (anything bundling imgui/console-render C++ is out), no
   external HTTP, no save-file writes.
3. **API allowlist** (the machinery proven by the spawner milestone):
   - `require("UEHelpers")` (UEHelpers.lua already resident, 9,962 B row)
   - `FindFirstOf` / `FindAllOf` / `StaticFindObject`
   - `RegisterKeyBind` — single key preferred; `{ModifierKey.*}` second
     (the composed runtime binds the modifier overload; pad side needs a
     MODIFIERS+KEY row in PS5Pad.ini)
   - `ExecuteInGameThread` (+ `…WithDelay`/`…AfterFrames` acceptable when
     type-guarded)
   - Basic UFunction calls with primitive/struct/object params and returns;
     reflected property Get/Set; `IsValid`
   - `LoopInGameThreadWithDelay` acceptable only for phase-3 candidates
     (sustained game-thread ticking is a new stress class — MiniMap is the
     designated probe for it)
4. **API disqualifiers** (reschedule, don't fetch):
   - `RegisterCustomProperty` with hardcoded offsets (ActorDumperMod class —
     version-fragile, crash-grade)
   - `RegisterHook`-driven mods (phase 2 — first prove RegisterHook via
     CheatManagerEnablerMod)
   - BPModLoader / pak dependencies, `RegisterCustomEvent` BP contracts
   - Mods requiring UnrealVersion branches below 5.0 (Townfall is UE5)
5. **Popularity** (proof-of-generality needs real-world mods): Nexus page
   with visible endorsement/download counts, or the official UE4SS
   recommended-mods list. Record author + URL + counts in the receipt.
6. **License/redistribution**: mod must permit personal use transfer to
   owned hardware; keep the archive + receipt, never re-host.

## 2. Search targets (when network is next available)

- Nexus game section for the same title as the resident game
  (the receipt-proven source: DavidWallacee minimap came from there).
- UE4SS official docs "Lua mods that work with UE4SS" list — prefer entries
  flagged "no C++ required".
- GitHub topic search `ue4ss lua mod` filtered to Lua, <30 KB, MIT/none.
- Slot priorities: (a) one item/economy mod, (b) one movement/teleport mod,
  (c) one HUD-lite mod — three different authors than the resident pool.

## 3. Acquisition mechanics (Mac side only; console never fetches)

1. Download archive on the Mac (user-network only; no console-side network).
2. Record `source-receipt.json` entry in the SAME format as the audit-1
   receipt: archive filename, archive sha256, per-member sha256 + bytes +
   local_path. Extend the existing receipt or start a dated spike dir
   `.superpowers/native-dependency-spike/<date>-multimod-fetch-1/`.
3. Stage extracted files under that spike dir mirroring
   `ue4ss/Mods/<Name>/…` layout. Do NOT touch the console staging roots.
4. Quarantine gate before any loadlist work:
   - `luac -p` syntax check on every .lua;
   - grep the full forbidden-API list (RegisterCustomProperty, raw
     detour/library load (`require` of non-UEHelpers), `os.`/`io.` filesystem
     use, `Socket`/http);
   - complete the CANDIDATE-SHEETS.md row set for it (loadlist rows, flow,
     markers, riskiest call mapped to the CI table). A mod with a row landing
     on an unproven class not scheduled for its rung is REJECTED for that
     rung, rescheduled or archived.
5. Only then hand the staged dir to the packaging worker for size-exact
   loadlist row generation (TREE-B LF normalization applied first, `wc -c`
   the exact upload bytes).

## 4. Standing laws that bind this plan

- PS5 rules (AGENTS.md): no uploads of downloaded executables (Lua-only
  here anyway); every upload readback-verified; one launch per clean boot.
- Admission is size-exact (MFA law) — the loadlist row byte count must equal
  the on-console file size; re-measure after ANY edit.
- Never modify fetched mod sources: the milestone is proof-of-generality for
  UNMODIFIED PC mods. If a mod needs a change to run, it is evidence of a
  runtime gap, which is itself a finding — file it, don't patch the mod.
