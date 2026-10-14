# REPORT.md — worker-zip-to-mfb: F-R1 bridge (archive quarantine -> modforge bundle)

Origin: worker-zip-to-mfb, 2026-10-04 (ZCode team, PS5 mod-manager).
Goal: close worker-e2e-rehearsal finding F-R1 — a downloaded real zip prepares
fine but `install --mods <archive-mod>` refuses with "not a modforge bundle".
Host-side only; no network; no console; no original file modified; everything
under `outputs/2026-10-04-zcode-team/worker-zip-to-mfb/`.

## 1. Result

**F-R1 closed on host.** A downloaded archive mod now flows
download -> prepare-archive (auto-bridge composes a kind-1 mfb from the
quarantine) -> install -> runtime-ack -> uninstall, all green with correct
record states, on 3 real worker-6 corpus zips; a no-mod-content archive
refuses with a specific reason; `--no-auto-bundle` restores the old
reporting-only behavior (including the original F-R1 refusal).

```
python3 compose_bridge.py --flavor host      # OK (vendored chain rebuild)
sh test/run_bridge_tests.sh                  # PASS=75 FAIL=0, exit 0
```

Test evidence (all paths in this directory):

| Artifact | Meaning |
|---|---|
| `test/bridge-e2e-summary.log` | 75 PASS / 0 FAIL, per-case, plus transcript sanity |
| `test/bridge-e2e.log` | full command transcript (every run + exit code + tool output) |
| `test/run/<case>/state/records.txt` | final durable records per case |
| `test/run/<case>/state/bundles/<id>.mfb` | the composed bundles (magic `MFBUNDL1`, kind 1) |
| `e2e-receipts/`, `composition/receipts/` | vendor manifests + full patch-chain receipts |
| `composition/build/host/modinst_composed` | rebuilt binary, sha256 `947affc87b422197021fdf9ef35a036ca73d0f43d7ab34602035a4feccc95c92` |

Per-case outcomes (each: fresh state/roots; corpus sha cross-checked against
worker-6 `zip-roundtrip.json` pins before use):

| Case | Corpus zip | Bridge output | Install evidence | Final state |
|---|---|---|---|---|
| zb-lua (Lua mod) | `infinite-stamina-v1.zip` (61cd9c81…) | files=2 bytes=2811 | `ue4ss/Mods/Infinite Stamina/Scripts/main.lua` + `enabled.txt` staged; runtime ack `running=1`; uninstall removes files | prepared |
| zb-pak (pak mod) | `movement-speed-2x.zip` (3850a2b5…) | files=3 bytes=2747 | `mods/~mods/PlayerMovementSpeed_2x_P.{pak,ucas,utoc}` staged | prepared |
| zb-ini (ini fragment) | `performance-pak original.zip` (27db7e1b…) | files=2 bytes=5927 | `mods/~mods/zzz_UniversalPerformance_P.pak` + `config/Windows/Engine.ini` staged; loadlist shows `file config config/Windows/Engine.ini 78` and `file pak mods/~mods/zzz_... 5849` | prepared |
| zb-neg (negative) | synthetic `notamod.zip` (ReadMe + png only) | none | prepare-archive exit 2: `error bad_request auto-bundle refused: no mod content recognized (…); quarantine retained`; record stays `downloaded`; quarantine + manifest intact; no bundle written | downloaded |
| zb-off (flag control) | infinite-stamina again | none (`--no-auto-bundle`) | records prepared, then install refuses with the ORIGINAL F-R1 reason `not a modforge bundle` | prepared (reporting-only) |

e2e transcripts: positive cases 8 begins / 8 ends / 0 failed / 0 unmatched
(the bridge refusal path emits its end line — it does not add a dangling
begin); zb-neg 3/3 with 1 failed end (the intentional refusal, exit 2);
zb-off has 1 unmatched begin = the known F-R2 install-refusal behavior
(cmd_install exits before the end line), unchanged by this worker.

## 2. Design (summary — full detail in BRIDGE-DESIGN.md)

- The quarantine already contains everything needed: `member-%08u.bin` files
  plus `manifest-v1.bin`, the intake's own durable member map (row index,
  size, sha256, member name, original name, SAFE path). The bridge parses
  that manifest (format pinned from the vendored intake.c `emit_manifest`)
  — it never re-scans or re-decompresses anything.
- Members map to the SAME final-path/role shapes the proven supplier bundles
  use: ue4ss Lua mods -> `ue4ss/Mods/<X>/…` (`lua_mod`/`config`), paks ->
  `mods/<tail after Content/Paks>/` (`pak`; `.ucas/.utoc/.sig` ->
  `asset_override`), ini fragments -> `config/<tail after Saved/Config>/`.
  Non-mod files (ReadMe, dlls) are skipped; zero recognizable members =
  specific refusal.
- Identity defaults, documented: id = the download `--mod` id, version
  1.0.0, priority 100, engine unreal, load_order manifest_priority, no
  invented framework/requires/plugin records.
- Cross-mod conflicts/ordering reuse `mfx_compose` unchanged (priority, then
  id; later mod wins a path; loadlist last). In-archive duplicates are
  refused, never silently resolved.
- Record states: `downloaded` = bundle field is the intake zip (NOT
  installable — the old refusal is correct there); `prepared` = bundle field
  is `<state>/bundles/<id>.mfb` (installable); refusal leaves `downloaded`
  + quarantine retained. The bridge runs AFTER a successful intake and
  BEFORE the `downloaded->prepared` transition, inside the existing
  prepare-archive dispatch.
- **No new format**: the bridge emits the same kind-1 mfb bytes
  `cmd_make_bundle` writes; `mfx_load` validates it like any supplier
  bundle (proven by the green installs).

## 3. Patch chain (recorded, anchored, sha-logged)

`compose_bridge.py` vendors `worker-e2e-final/compose_e2e.py` (sha
cross-checked against its provenance receipt; its 3 authored owned adapters
vendored sha-verified — `e2e-receipts/vendored-e2e-owned.json`), applies 4
recorded splices (`e2e-receipts/bridge-script-patches.json`) producing
`compose_bridge_e2e.py`, which builds this directory's composition and
appends 4 modinst.c patch tuples to the chain
(`composition/receipts/patches.json`, 45 patches total, apply order
preserved; every anchor + old/new sha logged):

1. `zb-inc-bridge` — `#include "modinst_zip_bridge.h"`
2. `zb-flag-no-auto-bundle` — main flag loop skips the value-less opt-out
3. `zb-dispatch-bridge` — in the prepare-archive dispatch: on intake success
   and `mi_bridge_wants_auto` (--mod given, no `--no-auto-bundle`), run
   `mi_bridge_run` and repoint the record's bundle to the composed mfb
   before `records_mark_archive_prepared`
4. `zb-usage-bridge` — usage text

New owned code: `owned/modinst_zip_bridge.c/.h` (manifest reader,
classifier, mfb writer; bounded static scratch; compiled clean — empty
compiler stderr in `composition/receipts/host-compile-modinst_zip_bridge.*`).
Defects found and fixed during this worker (in MY files only):
manifest row parse initially skipped the row_index field (fixed; caught by
the suite's hash-check refusal), and 3 compiler warnings cleaned.

## 4. Remaining console-only / future steps

- Console flavor: the bridge TU compiles under the console flag set too
  (same core_flags, ps5io ABI), but the console link was NOT run here;
  the controller step is the same pinned-closure console build + payload
  install as e2e-final's (compose_bridge.py --flavor console + re-verify
  size/imports or re-pin sha).
- Real runtime: `running=1` on host is still the simulate-runtime ack-file
  format proof only; on console the injected runtime's module_start ack is
  the truth (unchanged from the runbook).
- v1 bridge bounds (documented in BRIDGE-DESIGN.md §6): <=256 members,
  <=64 MiB member bytes, <=1 MiB manifest — larger archives (e.g. leon-2k
  100 MB texture paks) are refused honestly; a streaming variant is future
  work if the corpus needs it.
- The supplier-provided `.mfb` path (2b) remains preferable when available:
  the bridge's identity defaults are conventions, not extracted metadata;
  F-R2 (install's dangling begin on refusal) is still open from
  worker-e2e-rehearsal (my zb-off case reproduces it; not a bridge defect).

Bounds respected: 44 tool calls budgeted, ≤45 used; no nested subagents; no
network; existing files untouched (vendored copies + patches only; the only
writes outside this dir were the suites' ephemeral test trees, all inside
this directory).
