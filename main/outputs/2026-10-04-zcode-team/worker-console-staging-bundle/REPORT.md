# REPORT.md — worker-console-staging-bundle (2026-10-04)

## Delivered

- `MASTER-STAGING-PACKAGE.md` — the ONE controller execution document: complete
  9-entry file manifest (local path + sha256 + bytes + console destination), exact
  curl/FTP pass (Gate A0 write-probe → pre-image backups → mkdir → STOR ×9 with
  loadlist LAST → per-file RETR readback verification → ploader.ini edit → freshness
  reset), ploader.ini update rules, launch procedure, two-tier evidence checklist,
  full rollback procedure, and hash-verification method.
- `loadlist.txt` — the corrected 446-B loadlist (sha256
  `49ab7a3a40ee15d3090a542a6e7150307b4d001a08aa3386de66c04b6981e3bf`), produced by
  this worker because **worker-loadlist-final was NOT delivered** and the
  worker-settings-bundle-merge copy is not stageable (below). TAB-separated, LF
  endings, trailing newline — byte format matches the existing package copy.
- `CLAIM.json` first, `REPORT.md` last. No other files written; no network/console
  contact; no nested subagents.

## Key findings (all verified by reading actual files)

1. **All 8 data-file hashes re-verified on disk and match**
   worker-post-promotion-runtime/RUNTIME-STAGING-PLAN.md §4 exactly: PRX
   `b8bfae77…` (30756791 B), settings ini `9f3fa127…` (779 B), MemberVariableLayout
   `f291d552…` (588 B), VTableLayout `9c20f7c9…` (186 B), PS5Pad `1d919fbf…` (2960 B),
   main.lua `f586043a…` (21415 B), enabled.txt `e3b0c442…` (0 B), UEHelpers.lua
   `5a8f6609…` (9962 B). `PS5Pad.ini` and `UE4SS-settings.ini` are byte-identical
   across their duplicate worker locations.
2. **The settings-bundle-merge loadlist is NOT stageable as-is**: its 360-B loadlist
   (sha `9eef4dbb…`) lacks the `MemberVariableLayout.ini` (588) and `VTableLayout.ini`
   (186) rows; mfa preloads only loadlisted files, so the runtime's layout gate would
   always throw. Corrected loadlist adds exactly those two rows in the plan's §4 order;
   total preloaded bytes recomputed **35890** (the source plan printed 35390 —
   arithmetic slip, corrected in the master doc; both far under the 64 MiB cap).
3. **worker-composition-4 has NOT built** (only CLAIM.json in its dir). Per task
   directive the composition-3 candidate PRX is the placeholder (build receipt gaps
   `[]` verified; local magic `4f 15 3d 1d` = FSELF, so console readback must show
   `7f 45 4c 46` — Gate A1). Contingency for a late composition-4 PRX is specified in
   §C of the master doc, including the ploader-line adjustment rule.
4. **Evidence honesty preserved**: with the placeholder PRX only Tier A evidence
   (`/data/townfall_supplier_composition.log`) is possible — the ack supplier unit is
   absent from composition-3's symbol table (measured in the source plan), so
   `runtime-status.txt` / `UE4SS.log` / `[TownfallItemSpawner]` lines are documented
   as Tier B (composition-4 only). Their absence under the placeholder is a wiring
   state, not a runtime failure.
5. **Console-write honesty preserved**: FTP STOR is not evidenced in workspace
   records — Gate A0 (STOR+RETR roundtrip into `/data/modbridge/intake/.writeprobe`)
   must pass before anything else; the plan's "never mkdir inside roots/ by hand"
   ownership caveat and the installer-route alternative are carried verbatim into §0.

## Source disposition

RUNTIME-STAGING-PLAN.md (read, merged) · staged-package 6 files (5 adopted + its
loadlist superseded) · emission-final pair + receipts (adopted; LIVE-EVIDENCE-EMITTED,
parsepair OK, code-anchor PENDING non-blocking) · PS5Pad.ini (adopted, identical
bytes) · worker-loadlist-final (absent → self-computed) · worker-composition-4
(no PRX → composition-3 placeholder).

Status: staging package complete and internally hash-consistent; nothing has been
executed against the console. No claims of runtime success.
