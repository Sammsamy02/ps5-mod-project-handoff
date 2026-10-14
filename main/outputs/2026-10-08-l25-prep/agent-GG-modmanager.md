# agent-GG-modmanager.md -- mod manager (Claude round-5 Part 2)

2026-10-08-l25-prep/mod-manager. Battery mode honored: Mac-local
python/bash only, no PRX builds, ZERO console contact (the tool itself
refuses --apply while the console is down; tests probe only a dead
loopback address). No PRX/tree edits; only mod-manager/* files created.

## Deliverables

- `mod-manager/modmanager.py` -- the end-to-end tool (1343 lines,
  python3 stdlib only)
- `mod-manager/DESIGN.md` -- state model, re-pin procedure, conflict
  policy, never-edit law, rollback, 3-pass console acceptance plan
- `mod-manager/test_modmanager.sh` -- offline suite, 49/49 PASS
  (re-verified twice; sandboxed --root, auto-cleaned)

## What the tool does (verified by tests, all offline)

- install <dir|zip> [--enable]: locates the mod root (Scripts/main.lua
  tree; zip wrappers; AND flat diag-style dirs like zzdiag-v11 -- staged
  at Scripts/main.lua, bytes untouched, only main.lua staged), copies
  byte-exact with per-file sha comparison against the ORIGINAL (the
  never-edit law enforced at write time), records provenance in
  store/manifest.json (per-file size+sha256, tree digest, zip sha),
  runs the REAL FU host harness in io-shim mode (lua5.5 runner.lua
  --with-io; LineTraceMod/CheatManagerEnabler = LOADS-CLEAN recorded),
  regenerates the keymap, adds the loadlist rows with exact byte counts.
- enable/disable: row presence in synthesis only; other mods'/base rows
  provably untouched (byte-exact fixtures).
- uninstall: store dir + manifest entry removed; synthesis returns to
  the exact prior bytes.
- deploy (dry-run default): gates (preflight -> keymap regen+check ->
  conflicts -> synthesis) then the upload plan under the order law:
  mod files -> PS5Pad.ini -> loadlist.txt LAST; scope-enforced to those
  three surfaces only (settings/PRX/ploader/UEHelpers never touched).
  --apply requires --i-know-console-is-on AND a live probe
  (http://ip:8084/processes_list); on apply it fetches the LIVE loadlist
  first (sidecar law), uploads, readback-cmps EVERY file, stores
  prev/new receipt pairs. Offline base = last known receipt, else the
  documented 447 B BASE_COMPOSITION.
- keymap: FV generator reuse with size re-pin chain (row == wc -c ==
  --check --expect-size).
- audit: store integrity (tamper -> exit 2, verified), loadlist row
  sizes vs staged files, FV --check mirror, drift report vs last
  receipt (vs LIVE with --console when reachable).
- rollback: restores the receipts/last prev pair; refuses when no
  successful apply exists.

## Key evidence anchors

- Loadlist row format taken verbatim from the l23 harvest receipt +
  zzdiag-v7/LOADLIST-ROW.txt: `file\tconfig\tue4ss/PS5Pad.ini\t<bytes>`
  (re-pin target, replaced IN PLACE) and
  `file\tlua_mod\tue4ss/Mods/<Mod>/...\t<bytes>` (new rows appended
  last, matching the zzdiag sidecar shape).
- Byte-exactness proven against fixtures: base 447 B; +LineTraceMod =
  557 B; zzdiag synthesis = 545 B -- the exact arithmetic of the v7
  sidecar; pad row always == wc -c of the regenerated pad (1728 B for
  {spawner, LineTrace}, deterministic per FV key-identity hashing).
- Conflict policy per spec: zzdiag F7 vs spawner F7 -> FV-style WARN at
  install, deploy REFUSES (exit 2) until resolved (disable one or an
  explicit --allow-cofire); unmapped-key and preflight-fail gates
  likewise refuse with documented override flags.
- ueselftest: NOT in any receipt (zero hits worktree-wide); modeled as
  an ordinary installable mod (--name ueselftest), noted in DESIGN.md.
- Resident baseline: spawner is keymap participant + preserved rows,
  never manager-managed (uninstalling the control via a mod manager
  would be a footgun).

## Not done / limits

- The --apply FTP path is implemented and gate-ordered but NOT
  console-executed (console off -- that is the designed guard, not a
  gap); same for rollback --apply and audit --console live diff.
- 3-pass console acceptance plan is written (DESIGN.md section 7) for
  execution when the console returns.

Files: outputs/2026-10-08-l25-prep/mod-manager/{modmanager.py,DESIGN.md,test_modmanager.sh}
