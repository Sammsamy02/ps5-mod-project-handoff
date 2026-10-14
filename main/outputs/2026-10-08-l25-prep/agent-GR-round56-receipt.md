# agent-GR — round-5+6 combined receipt delivery report

Agent GR (ZCode), 2026-10-10. Scope honored: DOCS ONLY, battery-safe,
read-only except the three writes below. Zero console contact, zero builds,
zero tree edits.

## Writes (the only ones)

1. `outputs/2026-10-08-l25-prep/CLAUDE-ROUND5-6-RECEIPT.md` — the single
   combined receipt for the next Claude review: round-5 directive rows
   (parity revert FR; API parity table FS; static scan FT; path shim GA;
   host harness FU; self-test GB 6a; ladder-v2 GC 6b; cleanup FW; parked
   FX; build speed FY; next-tracks FZ; item-9 policy) + ADDENDA rows
   (GI first-launch/audit, GJ Blade prep, GD/GF phase B/C/A, GG manager,
   GH engines) + round-6 rows (FX build order, Blade scheduling, pak track
   GK/GL/GM/GN) — each with artifact citation, honest status, and the
   console test that will prove it.
2. `outputs/2026-10-08-l25-prep/WAKE-RUNBOOK-20261010.md` — appended dated
   §12 "AC-power build staging order": step 0 shim set-aside (round-4 lock)
   -> 1 round-4 gated build -> 2 serial-equivalence -> 3 restore shim ->
   4 P1 labelled build -> 5 P2 labelled build -> 6 round-5 build for the
   ladder window; with the exact set-aside/restore file list (5 surfaces +
   CHANGES.json reconciliation) and pre-build checks.
3. This report.

## Root-action verification (all four, on disk)

1. **Rule-10 fix VERIFIED**: `typescan-r4/candidate/include/LuaPathShim.hpp`
   has NO `PPSA33286` (grep rc=1; :77-79 documents the no-default law);
   `candidate/src/LuaMod.cpp:7842-7862` derives the root at runtime from
   `m_program.get_mods_directory()` (weakly_canonical, strip Mods + ue4ss)
   and passes it to 3-arg `LuaPathShim_install`; `LuaPathShim.cpp install()`
   (~:726-734) disables itself loudly
   (`[SHIM] path-resolver disabled reason=no-root …`) on an empty root,
   leaving stock io/os. Residual comment-only mentions remain (cpp:89 etc.)
   — scrub-at-release, per GI's audit.
2. **kstuff-lite pinned VERIFIED**: `outputs/kstuff-lite-pinned/`
   `kstuff-lite-v1.11.elf` sha256 `ab9a6cb4d3b1daf…dc779` == the CONSOLE
   pldmgr catalog checksum (worker-boot-protocol list_payloads.json meta
   checksum, itsPLK mirror URL) == FZ's recorded upstream mirror sha — the
   resident payload is the official release; source zip sha `b4fcf169…ddf6`.
   GP source verification still in flight (no report).
3. **Round-4 scope law documented**: §12 now carries the explicit staging
   procedure; set-aside = LuaPathShim.cpp + LuaPathShim.hpp + LuaMod.cpp
   block 7842-7862 (post-check wc -l = 9509) + `lua-pathshim` NEW_UNITS row
   (build_variants.py ~:112) + 3 markers (`shim-installed`, `shim-io-open`,
   `shim-write-rejected`; 18→15). GR additionally verified CHANGES.json
   carries `round5_row_sync` + LuaPathShim.hpp hash rows — documented as a
   required 6th reconciliation surface or the G2 gates fail round-4.
4. **GJ provenance VERIFIED**: `blade-prep/per-version-table.json:4`
   `generated_by` contains "Stellar Blade blind-test prep" — packer
   provenance-whitelist recorded as the future work item (GI finding).

## Notable session facts

- **GN and GL landed DURING this task** (logicmods harness Stage 0 done,
  27/27 calibration byte-identical; PAK-RUNTIME-MOUNT-DESIGN.md). **GK and
  GM remain in flight** (partial artifacts only: `pak-overlay/overlay_builder.py`
  in progress, `shader-fallback/classify_materials.py`; no reports) —
  receipt marks them in flight with a re-read instruction.
- FS has no separate report file; `API-PARITY-TABLE.md` self-identifies and
  is the data of record (receipt gap §4.1).
- Honest-gaps section covers: all-Mac-only status, verbatim round-5
  numbering not on disk (reconstructed from report-quoted item numbers),
  FX/FY unproven-until-AC-power, GI audit receipts predating the Rule-10
  fix, the ueselftest 1736 B pad-ini deviation, Lua 5.5.1-vs-5.4.7 harness
  caveat, and gameplay still at frozen menu-up state.
- Two real questions for Claude surfaced (already-on-record decision
  points, not invented): FX's fallback-pump removal boundary; FR's delegate
  `FunctionName` pass-back semantics.
