# EVE-STATE — session-eve snapshot (2026-10-04, worker-session-eve-coherence)

One page. Workspace root: `.codex/worktrees/translation-core/what`; W = `outputs/2026-10-04-zcode-team`;
HTTP = `http://ps5-host-not-configured.invalid:8084` (manager; never 9223); FTP = `ftp://ps5-host-not-configured.invalid:2121` (anonymous).
**Nothing below has been executed on the console.** Every "Local and ready" hash was recomputed
on disk this pass and MATCHES its documented identity. Doc staleness fixes live in CORRECTED-LINES.md (same dir).

## Staged on console NOW (identities from last VERIFIED reads — re-verify at open, nothing re-sent)

| # | Item | Console path | Identity |
|---|------|--------------|----------|
| S1 | r11 payload slot | `/data/pldmgr/payloads/` (composition-5 fself) | sha256 `0911af0cd5b4a1f22eac49a79f8b3c9a3d9cac7358b246e16ae2a969b537cab9`, 30,792,151 B |
| S2 | Activation root | `/data/modbridge/roots/PPSA33286/` | v2-era payloads staged (834 B settings generation) |
| S3 | ploader.ini pre-image | `/data/plugins/ploader.ini` | sha256 `8d331b3f…`, 247 B (INDEX Track 8; Phase-0 gate) |

## CONSOLE-DOWN NOTE

The console homebrew servers went DOWN before comp6 staging (FTP refused / HTTP empty while the user
was away). Nothing was staged after that point: the slot still holds r11 (S1), the activation root
still holds the v2 generation (S2), and ploader.ini must still match the S3 pre-image. No dispatch,
retry, or replay was made against the down console. At session open, confirm pldmgr/FTP are back
(command 1) BEFORE any batch step; command 3 retries the comp6 upload + readback under the guarded law.

## Local and ready (sha256 recomputed on disk 2026-10-04 — 12/12 MATCH)

| # | Artifact | Local path (under root) | sha256 | Bytes |
|---|----------|-------------------------|--------|-------|
| L1 | comp6 staged fself (gate-approved 13/13) | `outputs/2026-10-04-zcode-controller/console/comp6-staged-fself.prx` | `76ee6af39b407ab73f78c59a3a3d38979397636bb988d3585cf6dc987098e553` | 30,773,943 |
| L2 | xwp armed (exec-write probe, post-D-1) | `…/worker-writability-probe-build/modbridge_exec_write_probe_armed.prx` | `48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e` | 20,013 |
| L3 | xwp skeleton | `…/worker-writability-probe-build/modbridge_exec_write_probe.prx` | `38b532155934537ff56b37bd11f7be91c3266c8510076f79d0fbbc30de2a37e5` | — |
| L4 | fc1 pool PRX | `…/worker-fc1-validator-probe/modbridge_fc1pool.prx` | `d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96` | 22,733 |
| L5 | qdrv1 driver PRX | `…/worker-record-sink-addon/modbridge_qh_driver.prx` | `52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4` | 26,157 |
| L6 | observer PRX | `…/worker-observer-packaging/modbridge_runtime_aligned_observer.prx` | `1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30` | 30,152,151 |
| L7 | settings v3 ini | `…/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini` | `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038` | 1448 |
| L8 | settings v3 loadlist | `…/worker-settings-v3-package/package-v3/loadlist.txt` | `4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d` | 447 |
| L9 | 8fam ELF (ALLFAM8_DLL_TEST.elf) | `…/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf` | `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb` | 336,552 |
| L10 | E2E payload (r1) | `…/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf` | `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054` | 681,888 |
| L11 | FINAL ploader.ini canonical (9-entry) | `…/worker-final-ini-layout/ploader.ini.canonical` | `fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507` | 440 |
| L12 | final-ini law + constants brief | `…/worker-final-ini-layout/FINAL-INI-LAYOUT.md` + `CONSOLE-BATCH-CONSTANTS-BRIEF.md` | — | — |

## First five commands at session open (exact order; game fully CLOSED throughout 1–5)

1. `curl -sS --connect-timeout 10 "http://ps5-host-not-configured.invalid:8084/list_payloads"` → expect 200 + catalog OK
   (confirms pldmgr/FTP are back; CONSOLE-LOG §1 reachability). Refused/empty = console still DOWN:
   STOP, report, no dispatches.
2. `curl -sS --connect-timeout 10 --max-time 60 "ftp://ps5-host-not-configured.invalid:2121/data/plugins/ploader.ini" -o /tmp/pl.now && shasum -a 256 /tmp/pl.now`
   → expect `8d331b3f…` (247 B, S3). Divergent = STOP + preserve + reconcile; never re-send.
3. `python3 outputs/2026-10-04-zcode-controller/comp6_one_pass.py` → ODR pre-flight → gate v2 GO →
   stage comp6 `76ee6af3…` into the composition slot (upload + readback band check via
   `stage_composition5_cycle.py`; guarded law). `JOB FAILED` anywhere = STOP for the whole launch path.
4. `python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py dll8` →
   ALLFAM8_DLL_TEST.elf `79281191…` 336,552 B; upload, readback EXACT, dispatch ONCE, wait 15 s,
   verdict vs EXPECTED-DELTA-8FAM.
5. `python3 outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py e2e` →
   13/13 cmd SHASUMS + payload `a21771f8…` 681,888 B; MODE=B gate; 15-dispatch stage 0..6 cycle,
   ≥30 s gaps; interrupted stage-3 commit → ONE permitted `recover.json` dispatch then HALT.

Then (not in the first five, same game-closed window per the master plan): `stage-qdrv1` (L5, appends
`2=/data/plugins/modbridge_qh_driver.prx`), the observer ridealong (L6) + FINAL 9-entry ini adoption
(L11/L12 — registered count 8), and the settings v3 restage (L7/L8, readback 1448 B + 447 B). See
CORRECTED-LINES.md B2/C1–C4 for the doc lines these supersede.

## Post-correction gate quick-reference

- CP2 tick: expected line `GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)` — NOT `0x37ac8c0`.
- ploader.ini final state: 8 entries / **exactly 8 registered** (`modbridge_ingame_facts.prx = 0` never registers).
- Settings readback: 1448 B `d8faa4ec…` + 447 B `4d98ecd7…` (v3). v2 (834 B `9570ead3…` + 446 B `3afd10ce…`) is superseded.
- patches.json promotion: APPLIED (`252ad699`→`63c28979`); operative E2E gate stays the tool's own 13/13 + payload preflight.
- Comp2 expected line per decision brief CP2 row corrected per A2; launch anomalies: runbook §6 triage, honest-stop shapes.
