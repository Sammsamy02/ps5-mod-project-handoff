# agent-EF — multimod prep (post-menu-acceptance milestone)

2026-10-08. Scope: prepare the NEXT milestone — testing 2-3 DIFFERENT
unmodified PC Lua mods on the console (proof of generality). No network, no
console writes; everything written under
`outputs/2026-10-08-l25-prep/multimod-prep/` (4 docs) + this report.

Inputs consumed: agent-CI-marshaling-audit.md (amber/red table §5),
agent-CK-acceptance.md + ACCEPTANCE-LADDER.md + RUNBOOK-MOD-MENU-UP.md
(ladder pattern), zzdiag-v3/v7 sidecars, PS5Pad.ini (l23 after-copy, 3,718 B),
the composed console LuaMod global surface (typescan-r4 candidate), the two
local mod trees + their source-receipt.json, and the resident UEHelpers.lua.

## Deliverables

| File | Content |
|---|---|
| multimod-prep/CANDIDATE-INVENTORY.md | Task 1: every locally staged mod with sizes/hashes/provenance, API-surface summary, binding-availability check, thinness verdict |
| multimod-prep/FETCH-PLAN.md | Task 2: selection criteria (<30 KB, proven-API allowlist, no C++/GUI/offsets) + documented-for-later acquisition mechanics |
| multimod-prep/CANDIDATE-SHEETS.md | Task 3: per-candidate loadlist rows (exact byte sizes), keybind→menu→action flows, exact marker strings, riskiest call mapped to CI's table; S1–S10 |
| multimod-prep/TEST-PLAN.md | Task 4: cumulative launch order M1→M3, per-mod rung ladders (CK pattern) + FM1–FM12 failure-owner table, per-mod rollback via loadlist row removal |
| multimod-prep/ZZDIAG-RETIREMENT.md | Task 5: exact zzdiag removal state — pull-live-first, delete 2 rows + 2 files, 12-line/7-file-row post state, never-re-upload law |

## Key findings

1. **Local pool is NOT thin** — 12 distinct unmodified PC Lua codebases with
   full provenance: the stock UE4SS mod set (LF copies in
   `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/`),
   plus two real game-community mods in
   `townfall-ue4ss-real-script-audit-1/` (TownfallMiniMap v3.1 196,743 B by
   DavidWallacee; TownfallModMenu v0.66 128,009 B) with archive sha256
   receipts. The fetch plan is therefore a complement, not a substitute.
2. The composed console LuaMod already binds the FULL upstream global
   surface (RegisterHook, NotifyOnNewObject, LoopInGameThreadWithDelay,
   UnrealVersion, EFindName, ModifierKey overload of RegisterKeyBind,
   RegisterCustomProperty, …) — so candidacy is limited by PROVEN-ness, not
   by binding availability.
3. Line endings matter for staging: TREE-A copies are CRLF, TREE-B are LF
   (same content, different byte counts). Plan pins the TREE-B LF copies and
   re-measures upload bytes (MFA size-exact law).
4. Pad-chord budget: only TOUCH_PAD/TRIANGLE/CIRCLE/SQUARE/L1/R1/L2/R2 are
   free (no-chord-contains-chord law; OPTIONS stays deliberately unbound);
   every PS5Pad.ini edit requires re-pinning its config loadlist row.
5. TownfallModMenu's nav binds (UP/DOWN/RETURN) co-fire with the spawner's —
   it must run in a spawner-free boot; documented as reserve S10.
6. ActorDumperMod (RegisterCustomProperty @ hardcoded 0x98), Keybinds
   (C++ dumper tools), and BPML_GenericFunctions (BP-loader dependency) are
   rejected for this milestone with reasons on record.

## Final message content (relayed)

Recommended first three (one new marshaling class each, cumulative launches):
see "Recommended first three" in the final report message.
