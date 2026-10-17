# NEXT-STEP.md — worker-dll-integration (2026-10-05)

Worker: worker-dll-integration. Chosen step: **(c) closure** — EXECUTED
host-side. No console action is proposed; nothing below may be read as a
dispatch request.

## 1. What the dll8 run proved (read from the ledger, not re-measured)

Sources: ledger `zcode_return_phase1_dll8` (2026-10-05T11:38:27Z, superseded
intact by) `zcode_return_session_log` (consolidation, 11:55:21Z) +
`worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md` + `worker-8fam-console-prep/
CONSOLE-STAGE-PLAN-8FAM.md`.

- `EIGHT_FAMILY_CONSOLE_RUN_PASSED`, phase `dll8_allfam8`, verdict
  `PASS_12_FAMILY_KEYS`. Upload identity **79281191 EXACT** (upload NOT
  repeated); dispatch exactly ONCE. Provenance note: the batch tool halted at
  its own gate because its `manage:check` lacked `?filename=` (fixed in place
  at the dll8 and e2e sites; pre-patch snapshots retained); the controller
  verified the upload and dispatched manually once. No replay, per the
  no-duplicate-dispatch rule.
- Final record vs the frozen oracle: **12 family keys ALL enabled** (entry,
  clock, worker, sync, init_once, mutex, fileio, crt_startup, module_enum,
  vm, static_tls, seh64), worker **4/4**, consumer_imports **90**, results
  **[86,93]**, memory.last **163840**, `contract_errors` 0 / cleanup retired
  (per the consolidation record's LEDGER-MATCH statement) — the 8fam
  EXPECTED-DELTA table matched.

## 2. What results [86,93] mean (and do NOT mean)

**Not per-family verdicts.** `report.results = [values[2], values[3]]`
(`worker-dll-8fam-combined/vendor/windows-candidate/owned_runtime.c:618`):
the return values of the two calls to the owned helper's `HelperCompute`
export, resolved by GetProcAddress by name and by ordinal 7
(`vendor/windows-execution/tests/loader_consumer.c:29-35`):

- `HelperCompute(11)` = `11*7 + 9` = **86**
- `HelperCompute(12)` = `12*7 + 9` = **93** (bias 9, malloc/free and
  TlsAlloc/Set/Get/Free exercised per call — the substrate heap/TLS/error
  services).

The pair is constant across the 6/7/8-family console runs by construction:
each combined family's exercise runs inside the same window and must return
exactly 1 or a unique negative family code, "keeping the 86/93 base results"
(`fixtures/entry_consumer.c:25-41`). A family failure would have replaced
[86,93] with an early negative return and set `first_failure` in that
family's documented range (−120..−127 static TLS, −130..−137 SEH64, etc.).

### Per-family verdicts actually established

| Family | Verdict after dll8 |
|---|---|
| InitOnce, Mutex, FileIO, CRT startup | console-proven individually earlier; re-proven inside the combined 6/7/8-family runs |
| Module enum, VM | console-proven inside combined runs only (ALLFAM 133c5e9e; re-proven dll8) — never individual payloads |
| Static TLS | **first console proof** via dll8 (dir-9 post-link admission route; no failure code recorded) |
| SEH64 | **first console proof** via dll8 (dir-3 EXCEPTION admission + −130..−137 fixture census; no failure code recorded) |

### What remains untested / unproven after the PASS (honest)

1. **SEH64 PS5-trampoline pre-execution review NOT discharged** — the named
   open successor item. Naked-trampoline register contracts are host-verified
   only; H2 guard-frame RSP parity, signal→exception delivery,
   resume-execution, vectored handlers, C++ EH, integer-state-only restore
   stay unproven. Required before any reliance on SEH64 beyond the exercised
   fixture census (e.g., real DLL mods raising exceptions).
2. Host-only caveats carried verbatim: no TEB/`__tls_array` emulation, lazy
   TLS blocks for never-notified threads, no loader lock (Static TLS);
   protection changes bookkeeping-only (VM); `_seh_filter_dll` bound not
   exercised (CRT); 5 loader-unique module names only (Module enum).
3. The combined admission proves the composition executes on the target
   route — not Windows-loader parity, not arbitrary-DLL support, not
   DllMain/full-clock profiles beyond the exercised census.
4. **MSVCP140 remains ASSESSED-only** (possible future 9th family; explicitly
   not required for this track).

## 3. Successor options — decision

### (a) dll7 fallback test — NOT NEEDED, not executed (proposal: keep frozen)

The 7-family candidate (`e72f5318` / `83ee2ae9…`, 334,656 B,
ALLFAM7_DLL_TEST.elf) was frozen by `worker-dll-7fam-stage-prep` as a
**conditional attribution aid only**: "only if dll8 trips a SEH64-specific
surface and window budget remains" (DLL-TRACK-STATE §6; TRACK-COMPLETE
criterion 4 carve-out branch). dll8 PASSED, so the condition lapsed. Running
it now would append records 41–44 that answer no open question, cost a
console window, and contradict the no-busywork / no-duplicate-dispatch rules.
**The fallback stays compiled-never-dispatched, as designed.** Re-hashed this
session: on-disk bytes still EXACT vs the frozen receipt.

### (b) Runtime integration into a composed candidate — NOT called for by any plan

Checked: TRACK-COMPLETE.md, DLL-TRACK-STATE.md §3/§6/§7, the 8fam/7fam stage
plans, the batch runner session order, the launch-10/11 matrices, and the
knowledge base. **No document on the line plans loading the owned-DLL
candidate inside the composition-5/6 PRX.** The independence facts (§7, five
facts, both directions): no acceptance-ladder rung references any DLL
artifact; different mechanism/artifacts/console state (payload-only
game-CLOSED vs ploader PRX running-game); separate build lineages
(worker-fileio-apply base vs bridge r2→r12 + comp6); batch order is
scheduling, not dependency; and the link to UE4SS is **motivation, not
acceptance** — SEH64's census reads UE4SS.dll/dwmapi.dll imports as
forward-looking substrate for real DLL mods, but the current acceptance path
loads Lua mods and does not load owned PE DLLs. Nothing in the launch-10/11
or PC-acquisition work depends on this line. Runtime integration would be
NEW successor work, gated by the trampoline review — parked, not scheduled.

### (c) Closure — CHOSEN and executed

TRACK-COMPLETE.md's own template: the track is COMPLETE when criteria 1–3
hold (already MET), criterion 4 records the dll8 verdict as **PASS** (now
true via the ledger), and 5–8 hold at recording time (5 no-claim-inflation
and 8 provenance discipline are honored by this package; 6 UE4SS-untouched
held by construction — dll8 was payload-only, game closed, touched neither
ploader.ini nor the PRX slot; 7 successor queue documented).

**Executed host-side this session (no console contact):**

- Re-hashed all three combined candidate ELFs; all EXACT vs frozen receipts:
  - 8-family `c3aba79f` = `792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb`, 336,552 B — byte-identical to the dispatched, console-PASSED ALLFAM8 upload (also pinned in the controller session-artifacts-manifest).
  - 7-family `e72f5318` = `83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae`, 334,656 B.
  - 6-family `133c5e9e` = `96296c1c92850623781c0ecf8126f2307c21a1380cc95bb99d277a8d7a07e81c`, 300,456 B.
- Verified the ledger chain (`zcode_return_phase1_dll8` →
  `zcode_return_session_log`, originals preserved with superseded_by markers).
- Recorded the closure verdict in REPORT.md; no source/vendor/plan/tool/ledger
  file modified.

### One outstanding bookkeeping item (controller-owned, not blocking closure)

The 8fam plan's post-PASS bookkeeping names an `allfam8-console-receipt.json`
+ result copy under `C/console/` naming. **No such file exists on the host**
(only a fake-drive selftest copy at
`worker-console-batch-runner/runs/fake-drive/dll8/`). The consolidated ledger
verdict is the record of record; the raw records 37–40 and console receipt
remain retrievable console-side by whoever holds the session transcript. This
worker cannot fabricate them and did not.

## 4. Goal-relevance verdict: INDEPENDENT — line PARKED as COMPLETE

The DLL-compatibility line is **independent of the menu path**. The menu /
spawn path (launch-10 rebuild, launch-11 R3-marshal and IGW row-0 routes,
Lua-mod acceptance) references no DLL-family artifact in any of its oracles,
and the DLL line references none of its artifacts. **Parked**: no further
console work on this line is proposed. If real in-runtime DLL mods are ever
attempted, the re-entry point is the successor queue above, first item the
SEH64 PS5-trampoline pre-execution review (new bounded worker, new IDs, no
replay of anything in this line).
