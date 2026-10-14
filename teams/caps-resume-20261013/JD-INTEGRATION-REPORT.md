# Agent JD report — INTEGRATION DESIGN for #7 step 2, #10, #1/#3 (2026-10-13)

Deliverable: `caps-team-work/outputs/INTEGRATION-PLAN.md` (the full plan; this is
the summary). Mac-only, no builds, no console, no source edits. All findings were
verified read-only against the live tree, stagers, drivers, recipes and receipts
today; verification receipts are in the plan's Appendix A.

## What the plan contains

1. **Overlap map** — per-file, per-unit, per-overlay. #7 edits
   {UnrealInitializer.cpp, LuaMod.cpp, CodeSignatureProfile.hpp,
   CodeImageCache.hpp} + 7 additions; #10 edits {UE4SSProgram.cpp} (3 anchors) +
   6 additions (2 TUs deferred); round5-staged patches (lookup-speed, group-b,
   shim-fix, lua-alloc) are NOT in the tree and all their unpatched pins are
   stale against the current bytes. Seven concrete conflict/dependency items
   mapped, including the kMaxPins 8→16 shared-ABI hold and the fs_throw ratchet
   exposure of PakRequestProducer.hpp.
2. **Composition order** — S0 pins/snapshot → S1 #7 read-only stage → S2 #7
   labelled build+package → S3 #10 re-pin + labelled build → S4 round5 items
   (each own labelled build, re-anchored) → S5 #1/#3 nothing (fail-closed).
   #7 and #10 are logically independent; serialization is per-file-chain plus
   the build lock. Independent runtime review slots between each component's
   composition and its --execute.
3. **NEW_UNITS registration** — exact `pakmount` entry (template
   `unreal-unrealinitializer`) now; `mono-owner`/`invocation-boundary` recorded
   but BLOCKED (no Prospero Mono port; argv needs mono include dirs a template
   swap cannot supply); the packaging-driver extension #7 actually needs
   (PakMountCodeProfile.hpp as a new `lifecycle/` lineage overlay, EQ pattern,
   hard-error on absence); census table 395→396 with P8 attribution rules.
4. **Gate design** — G-C7-* (stage pins, identity pins, abi-uniform,
   snapshot-abi host replay, producer refusals, fs-throw-approved, no-detour,
   census, runtime review), G-C10-* (source-repin, observer-embed, probe-image,
   port-admission fail-closed default, boundary exception safety, no-host-config
   leak, runtime review), G-C13-* (selection-empty fail-closed today;
   provider-identity + readpatch-contract for the future).
5. **Runbook** — command-per-step with rollback for every step; full-mode builds
   only; IP-shim-root explicitly forbidden.
6. **Non-conflict guarantee** — #7∩#10 = ∅ proven from stager source; #7→Q
   second-hop composition PROVEN by simulation (patch rc=0, 0 .rej, no fuzz on
   the #7-derived file); token preservation verified; residual risks declared.

## Key findings the integrator must not miss

- **#7 is stageable right now**: all four recipe pins match the current tree
  exactly; the transform was re-validated end-to-end and reproduces all four
  derived hashes.
- **#10's pin is stale**: stager expects UE4SSProgram.cpp `fe9fe7bc…`, tree is
  `ad3ae621…` (CHANGES row synced; anchors still exact-once; no Cap10 markers).
  One-line re-pin + fresh derived sha required; never blind-run the old stager.
- **Q's copyin-probe overlay composes cleanly on top of #7** (verified), so the
  driver's second hop keeps working; re-verify if the #7 review changes source.
- **The shared CodeImageCache ABI hold is real**: kMaxPins 8→16 requires the
  same candidate in the vfs-aliased runtime copy AND the packaging lineage
  overlay, with the G12b consumer cascade (i.e., full build, no --incremental);
  PakMountCodeProfile.hpp additionally needs the new lineage destination.
- **fs_throw_gate will flag #7's producer header**: the PS5-live
  `directory_iterator(directory)` range-for and non-ec filesystem calls need
  error_code forms or visible approved markers before --execute.
- **#1/#3 has nothing to integrate** — the selection must stay empty and a gate
  now enforces that; future work needs the provider identity + read/patch
  contract gates defined in §4.3.

Report ends. No builds, no console contact, no uploads, no tree writes outside
`caps-team-work/outputs/` and this report.
