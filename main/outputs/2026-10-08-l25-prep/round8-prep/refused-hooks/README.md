# refused-hooks (round 8 prep, Mac-only, nothing applied)

Reviewer request: refused hooks are a parity gap. This folder is the complete list for the fix queue.

| File | What it is |
|---|---|
| `REFUSED-HOOKS.md` | Start here. Plain verdict first, then the table (22 rows), then the routes and risks |
| `refused-hooks.json` | Same list, machine-readable (one object per row, plus gate/source tables) |
| `staged/refused-log-vs-B4-candidate.diff` | Uniform `[REFUSED] kind=... target=... mod=... reason=...` logging, as a diff against the B4 candidate `LuaMod.cpp` (sha 4d6bc63b). NOT applied |
| `staged/apply_refused_log.py` + `staged/refused_log_helper.inc` | Same change as an anchor-based script (use this one for B5/B6: the line diff has 4 failing hunks against B5). Usage: `python3 -I apply_refused_log.py <LuaMod.cpp copy> refused_log_helper.inc` |
| `staged/ueselftest-gaps-from-runtime-counter.diff` | Self-test GAPS line counts real gaps via the new `GetRefusedHookCount()` global. Changes ueselftest sha, so regenerate the row-check pins |
| `host-test/` | Host test of the helper (mock engine types): 18 checks, 0 failures |
| `evidence/vtable_extent_check.py` / `.out` | Static replay on the local decrypted image: a hypothesis for why the copied-vtable pump is refused (`non-text-ent`) |
| `evidence/vtable_extent_sensitivity.py` / `.out` | Same replay with stricter code-range stand-ins: shows the hypothesis depends on the runtime's code classifier (not in the tree) |
| `REVIEW.md` | The reviewer's findings on the first version (kept for the record) |

Manifest marker if the staged patch is built: id `refused-log`, text `[REFUSED] kind=`, encoding utf-8 (narrow literal in `LuaMod.cpp` `refused_log::emit`). The startup line adds `kind=detour-block`.

Labels: console-proven (cites a line of the B4 log) / Mac-only / comparison (earlier session, never a verdict). No console contact was made. Written only inside this folder.

Second pass (2026-10-12, after review): R21/R22 corrected (extent fix alone does not unblock them), R01 refusal site marked unknown with a resolver step added, LoadMap slot relabelled one source, vtable cause relabelled hypothesis. See the last section of `REFUSED-HOOKS.md`. Staged patches unchanged (apply with `patch -p1` from the folder that contains `src/`, or use the script).
