# REPORT.md — worker-re-pc-ps5-comparison (2026-10-04)

CLAIM.json first; this report last. All work read-only local analysis plus authoring of
the four deliverables in this directory; no console contact, no target execution, no
frozen/wiring file modified, no nested subagents.

## What was asked and what was delivered

Mission: build the PC-vs-PS5 correspondence for the functions/structures the Townfall
Item Spawner mod needs, as the seed of a reusable three-layer adapter stack. STEP 0
(inventory) was to decide CASE A (PC executable exists → correspondence) vs CASE B
(none → framework + protocol, no fabrication).

**STEP 0 result: CASE B.** No usable PC Townfall executable exists on this machine —
verified across the workspace, `/WORKSPACE/Projects`, `/Applications`,
`~/Library/Application Support`, the Steam library, Epic manifests, and whole-drive
Spotlight (INVENTORY.md §1). The ledger's `windows_*` entries were followed to their
directories: they are **owned PE-runtime fixtures**, never PC game material; the only
PC-side PEs the project holds are the Win64 UE4SS release DLLs (`dwmapi.dll` proxy
279,552 B sha `4e3e5324…` + payload), which are adapter-layer reference material
(INVENTORY.md §2–3).

Delivered instead of CORRESPONDENCE.md, per the mission's Case B instruction:

1. **ADAPTER-FRAMEWORK.md** — the compat-runtime / engine-adapter / game-profile
   framework mapped onto this project's actual artifacts (all ledger entries named in
   the mission classified, each with reuse note and its own recorded gap).
2. **COMPARISON-PROTOCOL.md** — the exact PC acquisition protocol (same-update version
   law, capture receipt shape, read-only intake), tooling status (lief + capstone
   ready; Ghidra downloading; BinDiff absent with the free Ghidra version-tracking
   fallback specified), and the prioritized 5-item comparison list with the PS5 half
   filled from pinned, provenance-cited derivations so the PC half slots in immediately.

## What is NOT delivered and must not be inferred

- **No correspondence table.** With zero PC-side material, any PC evidence column would
  be fabrication; none was written. Every PC-side value in the protocol is marked
  TO-COLLECT.
- **No working translator, adapter, or Item Spawner fix.** This is analysis only. The
  PS5-side runtime's own honesty labels carry over unchanged: no engine game-thread
  execution, no validated reflected call, no on-target EXEC — the mod's measured F7
  blocker (`ExecuteInGameThread`, main.lua line 695) remains blocked, exactly as
  recorded in `ue4ss_current_engine_focus`.
- **No completion claims about translation capability.** The deliverable is the
  scaffolding that makes a future PC-vs-PS5 comparison a fill-in exercise, plus the
  honest statement of what the PC side must be.

## Key findings worth the controller's attention

1. **The seed-table slot model IS the comparison scaffold.** PC and PS5 profiles fill
   the SAME named slots (GUA / FName ctor / FNamePool / SCO / GMalloc), so the reuse
   path is "same table, second column" — not a new methodology (ADAPTER-FRAMEWORK §4,
   COMPARISON-PROTOCOL §5).
2. **Three PS5-side sub-gaps the PC comparison should be planned around**: (a) PS5
   StaticFindObject/FindObjectSlow path not yet derived; (b) FNamePool block size /
   field offsets beyond +0x18 not fully enumerated; (c) ProcessEvent/CallFunction
   address not derived (the widest gap). These are listed in the protocol so the PC
   evidence lands where the PS5 side is weakest, not where it is already proven.
3. **Method asymmetry is itself the main finding**: the PS5 RELA model (no absolute
   data pointers; 1,661,599 relative relocs) makes the PC and PS5 derivations differ
   structurally at the vtable step, and the PS5 execute-only text mapping forces
   data-side-only validation where the PC side can validate by code reads. Both
   asymmetries are mechanical and now documented where the next worker will see them.
4. **A retained PC prior exists legitimately**: the Win64 UE4SS release DLLs in the
   Downloads zip (and retained under
   `.superpowers/native-dependency-spike/windows-image-original-inputs-1/`) can seed
   PC-side pattern priors — usable as priors only, never as game evidence.

## Artifacts

`CLAIM.json`, `INVENTORY.md`, `ADAPTER-FRAMEWORK.md`, `COMPARISON-PROTOCOL.md`,
`REPORT.md` — all inside
`outputs/2026-10-04-zcode-team/worker-re-pc-ps5-comparison/`. Nothing else written.
