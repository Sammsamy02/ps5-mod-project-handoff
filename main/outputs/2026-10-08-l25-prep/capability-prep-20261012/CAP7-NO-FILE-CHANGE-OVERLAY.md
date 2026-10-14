# CAP7 — load PC pak mods WITHOUT changing any game file (overlay/redirect)

Prep summary, 2026-10-12 (Mac-only, read-only review of existing design docs).
Goal restated: a PC UE4SS pak mod loads on the jailbroken console while every
byte of the game dump stays identical before and after. Support is per ENGINE,
never per game or per mod.

Sources reviewed (all pre-existing; nothing re-run this session):
`../PAK-RUNTIME-MOUNT-DESIGN.md` (agent GL), `../PAK-PIN-PASS.md` +
`../pak-pin-pass-pins.json` (agent HJ), `../pak-overlay/` (agents GK+HI:
`overlay_builder.py`, `test_overlay_builder.py`, `UNSUPPORTED.md`,
`mod-manager/INTEGRATION.md`, `leon-replay-drift.json`), and the #7 row in
`../CLAUDE-CAPABILITY-STATUS.md` + `../agent-GV-capability-relabels.md`.

## What exists today

- **Mac-only (tool + tests):** the overlay builder `../pak-overlay/overlay_builder.py`
  (~2,886 lines) with 32/32 passing tests (`test_overlay_builder.py`). It builds
  ONE derived `.utoc` index plus ONE partition-1 sidecar `.ucas` from N mods,
  keeping base rows pointed at the untouched base `.ucas`; it also reads classic
  `.pak` v11 and orchestrates the pinned retoc binary for pak->zen. Determinism,
  conflict ledgers, journal/uninstall, and substitutions are all Mac-tested only.
  Receipt `../pak-overlay/leon-replay-drift.json` records a Mac replay
  (derived-utoc / sidecar shas differ from the old trial by design; semantic
  equivalence asserted in-test).
- **Mac-only (static RE pins):** `../pak-pin-pass-pins.json` pins, from the
  decrypted Townfall eboot (sha `3b6cfad4…`), the engine mount machinery:
  `GPlatformFileManager` singleton accessor `0xb8ed0`, the FPakPlatformFile
  vtable `0x7fee8c0` (FileExists +0x80, FileSize +0x88, OpenRead +0xb8,
  IterateDirectory +0x1c0), the mount-one-pak helper `0x176ec30` with a refined
  ABI, and the `*.pak` scan visitor (no subfolder recursion). All re-derived
  byte-identically across 4 eboot snapshots; NONE has console confirmation.

## Proven vs only designed

- **console-proven (cite `.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json`):**
  ONE mod, ONE game (Townfall, UE 5.6), base `.ucas` untouched, mod appeared
  in game. BUT that trial REPLACED the on-disc `pakchunk0-ps5.utoc` index with
  the derived index — i.e. it DID change a game file. It is the content-shape
  proof the no-file-change design builds on, not a no-file-change proof itself.
- **Mac-only / designed only:** every mechanism in `PAK-RUNTIME-MOUNT-DESIGN.md`
  that leaves the dump untouched — A1 (call the engine's own mount helper at
  runtime for classic paks), B1 (pre-scan enumeration "lie" so the engine's own
  boot discovers virtual pak names backed by our mod root), B2 (post-init
  OpenRead/FileSize intercept that serves our derived index + sidecar from
  memory). None has run on console.

## Open questions (from the design, unresolved)

1. B1 pre-scan timing window (is our code resident before the Paks scan?) —
   unmeasured; miss degrades gracefully to vanilla.
2. B2 seam coverage — whether PS5 `.utoc`/`.ucas` reads actually flow through
   the hookable IPlatformFile chain or bypass it via direct kernel handles
   (HJ §4: the exact read slots +0x108/+0x110 were not isolated statically).
3. A2 native IoStore container late-add — the helper has a utoc-companion
   registration tail (HJ §3), but whether a LATE call merges into an
   already-resolved package store is unproven.
4. Foreign-container acceptance (B1's gamble) is unproven; only the merged-index
   shape is content-proven (the Leon trial).

## Per-engine-rule risks (places the design would need game-specific values)

- **Hardcoded chunk filename.** B2 decides by exact name `pakchunk0-ps5.utoc` /
  `pakchunk0-ps5_s1.ucas` (HJ §6). The chunk index (`0`) and platform suffix
  are a per-title/per-cook naming, not an engine constant. A per-engine build
  must DISCOVER which index container holds the target packages at runtime, not
  assume chunk 0. FLAGGED.
- **"Minimal set = chunk-utoc + sidecar"** is a Townfall-cook observation (the
  Leon trial touched only chunk-utoc; `global.utoc` was not needed). Another
  title may key imports differently; treat the minimal set as discovered, not
  fixed. FLAGGED (lower risk).
- **Pins tie to one image sha.** The offsets above admit only against eboot
  `3b6cfad4…`; the per-version-table + prologue-signature plan re-pins per
  engine BUILD (acceptable, this is per-engine not per-game) and fails closed
  on mismatch. Not a violation, but the admission path is unproven on console.
- The overlay builder itself holds the line: `overlay_builder.py` keeps ZERO
  game names in logic (rule-10 re-verified in `agent-HI-overlay2.md`); game
  identifiers appear only in docstrings/fixtures. Good.

## Exact next Mac-only step

Make B2's target-container selection engine-generic: replace any assumption of
`pakchunk0` with a discovery pass over the captured `.utoc` set
(`.superpowers/native-observations/townfall-main-utoc-2026-10-01/`) that finds
which container(s) hold the packages a given mod overrides, and add a test to
`test_overlay_builder.py` asserting no hardcoded chunk name reaches the hook
decision. Pure Mac, no console, no new fetch.

## First console step it would need (PROPOSAL — needs explicit user OK)

Phase 0 observation launch per `PAK-RUNTIME-MOUNT-DESIGN.md` §4: the frozen PRX
plus an OBSERVE-ONLY arm (no mount calls, no substitution) that timestamps
`module_start_entered` vs the first Paks enumeration and logs which
IPlatformFile layer/handle serves the `.utoc`/`.ucas` opens. This single
reviewed, read-only launch decides B1-vs-B2 feasibility (open questions 1 and
2). It requires a full before/after dump hash manifest and one launch per boot
under the AGENTS.md / 3-pass rules. Not authorized here — reported as the
proposal that would open the ladder.
