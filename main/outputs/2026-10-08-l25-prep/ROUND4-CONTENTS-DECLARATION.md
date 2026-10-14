# ROUND-4 CONTENTS DECLARATION (Claude round-7 note 3) — 2026-10-10

Tree state as staged, verified against the round-3 build's own candidate
pins (l26-A CHANGE-SELECTION, the 56a9b1a0 build):

## IN round-4 (the agreed contents)
- Everything the round-3 build pinned, byte-identical where untouched:
  UObjectGlobals/NameTypes/LuaFName/LuaFText/LuaLibrary/LuaUScriptStruct
  all MATCH their r3 pins exactly; LuaMod.cpp restored byte-exact to its
  r3 pin aac03fbc after removing the round-5 shim insertion (block
  captured verbatim in round4-setaside/LuaMod-insertion-block.txt).
- FR parity revert: candidate/src/LuaUObject.cpp (aa10e4ea) — property
  reads return upstream userdata; bridge inside :ToString/__tostring.
- FW cleanup: tree include/LuaFNameBridge.hpp + candidate/src/
  LuaFNameBridge.cpp (contiguous-literal family markers), candidate/
  include/LuaType/LuaUnrealString.hpp (FString/FUtf8/FAnsi __tostring +
  routed members), candidate/include/LuaType/LuaUObject.hpp
  (ProcessConsoleExec echo route). MODBRIDGE_RELEASE_DIAG_OFF stays OFF.

## OUT (set aside to round-5; files in 2026-10-08-l25-prep/round4-setaside/)
- GA io/path shim: LuaPathShim.{cpp,hpp} + the LuaMod.cpp insertion +
  the lua-pathshim NEW_UNITS row + the 3 [SHIM] manifest entries
  (manifest now 15) + CHANGES rows/added_headers.
- GT first-launch TU: FirstLaunchProfile.{cpp,hpp} + firstlaunch
  NEW_UNITS row + added_headers.
- HM KB probe TU: KernelBridgeProbes.{cpp,hpp} + added_headers
  (compiles to nothing ungated, excluded by the default-waits law).
- Round-5 restore procedure: move files back, reinsert the captured
  block, re-add the two NEW_UNITS rows, re-add the 3 [SHIM] manifest
  entries, resync CHANGES.

## Build-time-only changes ALLOWED in round-4 (not PRX content)
- FY concurrent-twins driver (equivalence proof runs during play per the
  round-7 order law); EZ incremental (unused); HH's packager symbols
  sidecar + P12/P13 gates + provenance whitelist (packaging pipeline);
  FK's driver stamp generator. None alter PRX bytes' sources.

## Naming (Claude round-7 note 1)
- efbfc656 = THE MENU-UP BUILD, console-resident FALLBACK (console-
  proven; carries FM's plain-string pushers).
- 56a9b1a0 = round-3 (these FR/FW contents' predecessor), never
  uploaded, superseded by round-4.
- READBACK LAW: decrypt-off readback after EVERY power event.

## LAUNCH-1 EXTRAS CONFIRMATION (Claude round-7 pre-launch check) — verified

1. L1+R1 trigger: launch-manifests/MANIFEST-LAUNCH-1.md row 14 pins the
   byte-copy of GB's PS5Pad-round4-proposed.ini (1736 B, sha 957d3e25...,
   FV --check 0 violations 0 warnings, 7 bindings = 6 pinned + L1+R1/F12;
   loadlist pad row re-pin 3718->1736 documented). No shim dependency.
2. ueselftest: NO expected-without-shim marks needed — its io usage is
   ABSOLUTE PHYSICAL paths only (ROOT constant /data/modbridge/.../
   ueselftest/progress.txt + the st2io roundtrip alongside it), which the
   raw patched-liolib serves without the shim (the shim only adds logical
   resolution + write-confinement). Parts load via the native module
   searcher (shim-free, GB's canonical-key fallback). Progress/resume and
   io-roundtrip degrade to SKIP (never FAIL) if io is absent.
   RESIDUAL (not a launch-1 issue): the ROOT deploy constant names
   Townfall's root — allowed for diagnostics under the Rule-10 boundary,
   but MUST be parameterized before the Stellar Blade blind test
   (round-5 item).
3. zzdiag-v11: cb= rate line (pure-Lua LoopAsync counter) + [MAP]
   first-LoadMap lines (reflected World:GetName) — zero io/shim use.
   Current 4094 B; GQ's manifest re-measure-at-wake step covers it.

Battery round-4 build: APPROVED by Claude round-7 (round-4 alone; serial
proof + P1 + P2 + round-5 wait for AC).
