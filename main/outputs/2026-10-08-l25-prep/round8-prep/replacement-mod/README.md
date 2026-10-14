# replacement-mod track (round 8 prep, revised after review)

Question: which value-changing UE4SS Lua mod replaces InfiniteStamina in the test set (it is inert on PC too)?

- `CANDIDATES.md` is the deliverable: plain verdict first, then top 3 with links, current game version evidence,
  ModMenu Lua API table, complete ModMenu hook list (section 5.2), key clash with the spawner (5.4), what the user must do,
  other candidates, local archive hashes, open uncertainties.
- `REVIEW.md` is the reviewer's note on the first version (kept for the record). Its major points are fixed in `CANDIDATES.md`.
- Short answer: no candidate has written proof of working on the current version (Nexus blocked my tools; no
  comments readable). Best available pick is TownfallModMenu v6 (already on this Mac, source read), but it is NOT hook-free
  (19 hook targets, 10 Blueprint ones refused today, 5 map-load callbacks undelivered), it needs B5/B6 or later, it must not
  share an open menu with the spawner, and its God Mode health write uses a PC-derived hard-coded offset (+0x67C) with no PS5 source.
  Test Speed first. SpeedHackMod (Nexus 10) and Comfort Camera (Nexus 59) need a download and their hook use is unknown;
  if SpeedHackMod has no RegisterHook it becomes the better first choice.
- The InfiniteStamina original is already local (zip sha 61cd9c81..., Nexus mod 19 file 30), so the user supplies nothing for it.
- Mac/web only. No console contact, no mod downloaded or edited, nothing written outside this folder.
