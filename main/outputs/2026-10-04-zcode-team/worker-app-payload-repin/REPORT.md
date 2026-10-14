# REPORT.md — worker-app-payload-repin

Origin: worker-app-payload-repin, 2026-10-04 (ZCode team, PS5 mod-manager).
Host-side only: no console contact, no nested subagents, no provider contact.
Applied the ONE required app delta from worker-app-track-closure
`APP-SESSION-INTEGRATION.md` **P0**: the app's `PAYLOAD_IDENTITY` no longer
pins the D1 MODE-A payload; staged manifests and the batch tool's `mode_gate`
now agree on the MODE=B config-entry payload.

## 1. Result

**P0 is APPLIED and re-proven: 42/42 API suite green, 52/52 UI-flow checks
green, and every staged manifest row now carries exactly the pin the batch
tool's `mode_gate` admits for cmd files.**

```
PAYLOAD_IDENTITY (app_core/modmgr_api.py, all three owned vendored copies):
  OLD  e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436 / 681528
       MODE=A (D1 argv) — mode_gate: cmd files against it = STOP
  NEW  a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054 / 681888
       MODE=B (config-entry, MODBRIDGE_INST_E2E alias) — the gate's only GO
Backend 42/42 offline regression:      PASS=42  FAIL=0
UI flow Level 1 (real server+session): PASS=34  FAIL=0
UI flow Level 2 (real app.js, Node):   PASS=18  FAIL=0   (34+18 = 52/52)
Staged manifest rows after re-run:     13 rows, 1 distinct pin (the NEW one),
                                       console_executed:false, consumed:false
```

## 2. The delta (exact, receipt-cited)

- **Code (in-place, owned vendored copies; pre-edit md5 `c08e6a0e…`,
  byte-identical across all three; post-edit md5 `000d238e…`, byte-identical):**
  - `worker-app-backend-integration/vendored/repo/outputs/ps5-mod-bridge/native-host/app_core/modmgr_api.py` (owner-chain copy)
  - `worker-app-download-runtime/vendored-app/repo/outputs/ps5-mod-bridge/native-host/app_core/modmgr_api.py` (copy)
  - `worker-app-ui-flows/vendored/repo/outputs/ps5-mod-bridge/native-host/app_core/modmgr_api.py` (build/test tree copy)
  - Diff: `receipts/repin-modmgr_api.diff` (pin sha/bytes/source + honest
    comment citing worker-e2e-payload-build build-r1 and the batch gate;
    modmgr.py `2590e0e4…` and console_link.py untouched).
- **Suite twin (my harness copy only):** `S5-payload-pin-recorded` assertion
  retargeted `e3489297` → `a21771f8` — see `receipts/harness-diff-run_api_tests.patch`.
- **Gate cross-check:** `worker-batch-final-ini-adoption/console_batch.py`
  lines 128–132 (E2E payload block) and 875–884 (`mode_gate`: MODE=B on
  a21771f8/681888; MODE=A e3489297/681528 on cmd files = STOP).

## 3. Binary artifact note (honest correction of the task premise)

The task expected the UI final binary sha `a677e7b2…` to change. It does NOT,
and this is now proven, not assumed: the payload pin lives in the RUNTIME
driver (`app_core/modmgr_api.py`, fork/exec'd via `MB_MODMGR_SCRIPT`), and
`app_core/` is not in `build.py`'s `SOURCE_INPUTS`. A full in-place rebuild of
`build/final/desktop_fixture-download` with the exact retained inputs (fixture
OAuth config CA `89b9f708…`/534 B verified, curl profile + curl source,
fresh `dependencies-download` rebuilt from the 7 pinned inputs) reproduced:

- build_id `9b46b161947b8f33e1ef0970c8eb1c83eeaf60500e6910f5d561f938c015c56a` (identical)
- artifact sha256 `a677e7b273019f3accf9d29bb072bc44cf1b2061c2db9875fffaf29e272e91a9` (identical, 1,682,216 B, `cmp`-verified)
- `manifest.json` byte-identical to the pre-repin manifest

**Artifact note for the session docs: the binary identity `a677e7b2…` /
build_id `9b46b161…` REMAINS VALID post-repin — no artifact-note change is
needed; it is re-verified byte-identical this session.** Pre-image binary +
manifest preserved at `receipts/binary.pre-repin.a677e7b2` and
`receipts/manifest.pre-repin.json`; the superseded deps build moved to
`build-pre-repin/dependencies-download` (replaced in place by the fresh
deterministic rebuild).

## 4. Verification detail

- **42/42 API suite** (my copy of the UI worker's retargeted harness; uses the
  re-pinned vendored driver + REAL composed installer `f2215f2f…`): log
  `receipts/api-regression-42-postrepin.log`, receipt `receipts/api-tests.json`.
- **52-check UI flow suite**: fixture tree `/tmp/mb-ui-flows-fix` recreated
  fresh (zb-lua seeded through the proven CLI, generation 1), rebuilt binary
  served at `--port 62283` with the `MB_MODMGR_*` env pointing at the
  RE-PINNED driver; Level 1 34 checks + Level 2 18 checks against the live
  server; server stopped after the runs (identified task only).
  Logs/receipts: `receipts/ui-flow-tests-postrepin.log` +
  `ui-flow-tests.json`, `receipts/ui-flow-harness-postrepin.log` +
  `ui-flow-harness.json`, summary `receipts/postrepin-pin-evidence.json`.
- **Pin agreement proof**: after the re-run, all 13 staged manifest rows in
  the fixture queue carry exactly one `payload_identity`:
  `a21771f8…/681888` (source string = the batch gate), all
  `console_executed:false`, `consumed:false`. This is the P0 goal: staged
  manifests and `mode_gate` agree before anything is queued.

## 5. Notes for the session-docs owner (not edited by me)

- `worker-app-track-closure/APP-SESSION-INTEGRATION.md`: P0's "NOT yet
  applied" is now stale — the re-pin IS applied (this worker); its §1 table
  rows for the binary (`a677e7b2…`) and driver shas (`2590e0e4…`) remain
  correct as-is; the D1/MODE-A rows remain correct as gate descriptions.
- `worker-app-backend-integration/test/run_api_tests.py:173` still asserts the
  OLD pin (`e3489297`) — that is the delivered pre-repin evidence and was
  left untouched; running THAT copy verbatim against the re-pinned tree will
  fail `S5-payload-pin-recorded` by design. The retargeted assertion lives in
  my harness copy (`test/regression/run_api_tests.py` in this worker dir).
- The UI worker's delivered tree/receipts were not modified except the owned
  `app_core` copy and the in-place deterministic rebuild of `build/final`
  (byte-identical outputs, pre-images preserved in this dir).

## 6. Bounds

~31 tool calls (target 25-45). CLAIM.json first, this REPORT.md last.
Writes confined to: this worker dir; the three OWNED `app_core/modmgr_api.py`
copies; `worker-app-ui-flows/build/final` (deterministic rebuild, byte-identical
artifacts, deps dir swapped through preservation); process-local
`/tmp/mb-ui-flows-fix` fixture. Upstream worker dirs otherwise read-only.
No console dispatch, no provider contact; `console_executed:false` everywhere.
