# agent-GH — OTHER ENGINES research receipt

2026-10-08. RESEARCH ONLY honored: Mac on battery, no builds, no code-to-tree,
no console, no web. Two files written (this + `OTHER-ENGINES-RESEARCH.md`).
Inputs consumed: API-PARITY-TABLE.md, FZ-PHASEB-KSTUFF-LITE.md,
PS5-MODDING-MISSION.md (PHASE B/C + capability defs), agent-FZ-next-tracks.md
(Astro Bot 0/244 refusal), agent-FT-static-scan.md (offset/crash laws),
mod-engine-runtime-options-2 notes (pinned ModEngine2/me3 facts),
2026-09-30-roadmap-technical-audit.md Phase 7 (pinned IL2CPPInterop facts),
2026-10-01-bethesda-header-inspection.md, ps5-mod-manager-feasibility.md.
No local REFramework/BepInEx/SKSE/SHV sources exist in the trees (surveyed).

## Per-framework capability one-liners (MUST-needs; # = mission capabilities)

- **REFramework (RE Engine)**: MUST #2 (small bootstrap-sig set per game+
  patch — RE RTTI keeps the offset surface tiny), MUST #3 (Lua hook callbacks
  = detours), #4 timing already solved by ploader process-start injection;
  #6/#7 HELPS (pad-chord input route; NativeModSource pattern for addons);
  #1/#5/#8 OPTIONAL. UI/present hook is the one new subsystem (PS5 = GNM, no
  ImGui backend; defer via game-UI route). Nothing hard-blocked.
- **BepInEx (Unity)**: MUST #2 (PS5 IL2CPP eboots export nothing —
  il2cpp_init/domain/class-from-name via sigs), MUST #3, #4 solved-by-ploader
  (verify module_start precedes Unity main); #7/#8 HELPS. OFF-TABLE BLOCKER:
  managed runtime — PS5 console Unity = IL2CPP AOT only, no Mono/CoreCLR, no
  JIT; real PC C# plugins cannot run (research-scale Mono-interpreter port).
  Honest PS5 scope = our own native IL2CPP interop (UE4SS-for-Unity), not
  BepInEx.
- **SKSE-class (Bethesda)**: MUST #2 (no Address Library on PS5 — pinned
  per-title profiles replace it), MUST #3 (biggest hook surface of the five:
  VM/allocator/Scaleform), MUST #7 (esp/bsa overlay into encrypted-PFS data
  scan), #6 HELPS→MUST for MCM-class UI. COLD: plugins are native Win PE DLLs
  (reimpl only) and NO PS5-native title exists (PS4-BC = different injection
  environment). Local TES4 header inspector = data layer already built.
- **DSMS/ModEngine-class (FromSoft)**: MUST #7 (the framework IS the overlay:
  me3 packages=asset-overrides vs natives=DLLs, pinned local source), MUST-
  lite #2 (fd-layer anchors only), #3 HELPS→MUST for the in-process fd-hook
  route (kernel sysentvec route is the #1-gated alternative), #4 solved-by-
  ploader, #6/#8 OPTIONAL/HELPS. Lowest hook budget; ME1/ME2 self-test = the
  mission's pending in-game #7 proof. "Natives" entries out of scope.
- **RAGE/ScriptHookV**: MUST #2 (per-build natives table + script-tick sigs),
  MUST #3 (tick hook + invoker stubs), #4 HELPS (most timing-tolerant),
  #6/#7/#8 HELPS. COLD: SHV closed source (clean-room rebuild), PS5 "Enhanced"
  layout divergence, per-build DB nobody maintains, highest legal heat.
  Offline-only.

## Self-test shapes (full lists in the research doc; ueselftest laws applied)

- REFramework: via.* reflection base surface — thread-context, find_type_
  definition("via.Object") round-trip, base native singleton, type-DB count;
  opt managed-singleton discriminator + register/unhook round-trip. 4-of-5.
- BepInEx: IL2CPP precondition layer now (domain/classes/"System.String"
  round-trip, metadata sanity); true Harmony-on-System-method round-trip
  gated BLOCKED. 4-of-5.
- Skyrim: TESForm query core — DataHandler read, GetFormByID(0x14) player
  form, form-count, Papyrus stack count, known-global range check; read-only
  hook-site prologue verify (no install). 4-of-5.
- FromSoft: overlay PAIR law — baseline fd read hash-pins + register/serve/
  unregister redirect round-trip must BOTH pass; fd-registry count, version
  global, FSM/AI-table lookup. 4-of-5 (+ME1&ME2 mandatory pair).
- RAGE: natives table count, benign native hash resolve, script-handler pool
  count, invoker call round-trip, read-only player-handle native. 4-of-5.

## Ranking + recommendation

1. REFramework (RECOMMENDED) — highest machinery overlap: reflection-driven
   like UE (Lua stack, name-table seams, hook soft-refusal safety, injection
   timing all port); open ecosystem; many PS5-native RE titles; medium risk
   (UI route only).
2. ModEngine-class — lowest-risk parallel data track; no Lua reuse but ME
   self-test doubles as the in-game #7 capability proof.
3. BepInEx — biggest mod universe, honest BLOCK on managed runtime; IL2CPP
   probe layer shares machinery today.
4. SKSE-class — no PS5-native title; data layer locally done; extender
   runtime cold.
5. RAGE/SHV — closed source, per-build DB, legal heat; last.

First three steps (REFramework): (1) Mac-local dry run — decrypt-off eboot of
a user-owned PS5 RE title via the FZ-GAME2 recipe + new RE anchor pack
(via::reflection/RETypes markers) through the existing classifier
(pin-or-refuse, Astro-Bot-precedent, zero console); (2) read-only RE self-test
PRX (RF1-RF5) under ueselftest laws, one-launch ladder + harvest; (3) first
bounded register/unhook round-trip (RF6) after three clean boots.

Staleness disclosure: REFramework API/license, DSMS identity, PS5 title file
layouts, native-hash stability are [TRAINING-KNOWLEDGE] and flagged in the
doc — pin from primary sources before any code. Version-specific facts were
labeled throughout.
