# #1, #3 and DLL transfer supplement — 2026-10-14

The original private upload already included all 62 isolated `cg-*` stage snapshots,
all 95 scratch scripts listed in the transfer inventory, and the matching chat
reports/patches/manifests. This follow-up compared 328 inventory-listed existing text
files with their original source hashes: none were missing or changed.

It adds the new location inventory, the previously omitted untracked provider review
notes, and reviewed offline evidence text. All 80 evidence ZIPs were inspected; their
safe text was unpacked or linked to an identical file already in this repository.
Compiled objects, ELF images and DLL binaries remain outside the upload. This is a
private source transfer, not a new claim that #1/#3/DLL work runs on the PS5.

## Source entry points

| Work | Where to begin |
| --- | --- |
| Latest DLL loader, stage 62 | [Loader source](../../teams/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/src), [headers](../../teams/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/include), [tests](../../teams/caps-resume-20261013/caps-team-work/cg-guest-critical-sections-20261013-62/loader/tests) |
| Latest #3 transaction, stage 57 | [DetourTransaction.cpp](../../teams/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57/src/DetourTransaction.cpp), [tests](../../teams/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57/tests) |
| #1 read probe | [KernelReadProbe-v2.c](../../teams/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01/KernelReadProbe-v2.c), [classifier tests](../../teams/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01/tests) |
| Current status | [CURRENT-STATUS-1-3-DLL-20261013.md](../../sessions/2026-10-13/can-x20/outputs/CURRENT-STATUS-1-3-DLL-20261013.md) |
| Reviewer handoff | [CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md](../../sessions/2026-10-13/can-x20/outputs/CONSOLE-WAIT-REVIEWER-HANDOFF-20261013.md) |
| Original project handoff | [CHATGPT-HANDOFF-1-3-13.md](../../teams/caps-resume-20261013/CHATGPT-HANDOFF-1-3-13.md) |
| Chat reports, patches, manifests | [Chat outputs](../../sessions/2026-10-13/can-x20/outputs) |
| Build/reproducibility scripts | [Chat work](../../sessions/2026-10-13/can-x20/work) |
| Full location inventory | [GITHUB-TRANSFER-LOCATIONS-20261014.md](../../sessions/2026-10-13/can-x20/outputs/GITHUB-TRANSFER-LOCATIONS-20261014.md) |

## Evidence and exclusions

`ARCHIVE-REVIEW.json` accounts for every file member of every evidence ZIP, including
references to already transferred source and reasons for excluded binary artifacts.
`TRANSFER-SUMMARY.json` records this supplement's counts. Extra retained records from
the stage folders remain under their original stage-relative `evidence` directories.
Additional archive text is in `archive-text/<original archive stem>/`.

Owned offline compiler/harness logs are stored as `.log.txt` so they stay distinct
from excluded raw console logs. They describe historical host checks or offline
target compilation. No console connection, kernel probe execution, upload or launch
occurred during this transfer. Runtime source behavior was not changed.

Private home paths and literal private network addresses were removed from retained
text. The source manifest preserves both original and exported hashes. References
to original archives, excluded binaries, and original Mac paths are provenance;
they are not present binary artifacts or current release receipts. Regenerate pins
and receipts before using them as proof for a new build.

Do not merge old stages into the current main runtime wholesale. Use the latest
anchors above and review changes against the current main source. Third-party mod
copies, game data, proprietary source, credential files and raw console captures
continue to be excluded under the original transfer rules.
