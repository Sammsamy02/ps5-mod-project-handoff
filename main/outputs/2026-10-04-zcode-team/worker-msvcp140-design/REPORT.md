# REPORT.md — worker-msvcp140-design

2026-10-04. Assessment-only task, completed within bounds (≤40 calls; ~15 used; no nested subagents, no network, no execution of anything).

## Deliverable
`MSVCP140-ASSESSMENT.md` (this directory) — import classification, semantics of the essential surface, coverage analysis against the heap / CRT-startup / SEH64 / sync families, gap list, ownership recommendation.
Evidence extract kept alongside: `ue4ss-msvcp140-symbols.txt` (the 207 MSVCP140.dll slots of UE4SS.dll, from the frozen readobj).
`CLAIM.json` written first per contract.

## Key findings
1. **The family is 3 DLLs and 282 import slots**: dwmapi.dll imports 61 (all MSVCP140.dll); UE4SS.dll imports 207 + MSVCP140_ATOMIC_WAIT.dll (13) + MSVCP140_CODECVT_IDS.dll (1). Both counts match worker-modreqs SERVICES-BACKLOG §10 exactly.
2. **Two census corrections** (from direct frozen-read evidence): there is **no `std::_Execute_once` symbol in either image** (census phrase unsupported), and UE4SS imports **no MSVCP140_1.dll**.
3. **No `operator new`/`operator delete` imports exist in either DLL** — the heap family has zero direct MSVCP140 overlap. The real allocation-adjacent gap is `_callnewh` (imported by both, unimplemented, ~10 lines, belongs to the UCRT heap slice).
4. **Essential surface is much smaller than the slot count**: dwmapi 53/61 (streambuf backbone 34 + locale/codecvt bootstrap 19); UE4SS ~150/221 (adds wchar_t streams, `_Mtx/_Cnd/_Thrd` thread support, atomic-wait forwarders). Optional 29–37 slots: `_X*` throwers, error maps, `__ExceptionPtr*`, `Concurrency::details` PPL (6 of which are pure ETW no-op loggers), tzdb.
5. **Coverage**: sync family already owns `_Mtx/_Cnd` semantics (SRW/CV implemented + verified); ATOMIC_WAIT is 6 direct forwarders to implemented `WaitOnAddress`; SEH64 (design-only today) owns the throw machinery the `_X*` helpers call into; CRT startup (`_initterm/_initterm_e/_seh_filter_dll/_cexit` in worker-crt-startup-design) is a prerequisite, not overlap. Genuinely new = the ABI-exact iostream/locale compatibility core (~125–150 slots, char+wide), `_Fiopen` (depends on file-I/O + UCRT stdio), thread-exit hook, `_X*` catchable-throw shims.
6. **Recommendation**: no full-breadth standalone owner. One bounded owner for the stream/locale core (dwmapi's narrow-only 61 slots first — no thread support there), with `_callnewh` → UCRT heap slice, `_Mtx/_Cnd/_Thrd`+atomic-wait → sync owner addendum, `_X*` stubs → SEH64 owner. Sequencing is P4 for a structural reason: the family is downstream of CRT-startup (P1), file-I/O (P1), UCRT stdio/locale, and SEH64 (P3) and cannot be end-to-end verified before them. The loader's current refuse-unknown-requirements behavior is the correct interim failure mode.

## Bounds compliance
- Writes confined to `outputs/2026-10-04-zcode-team/worker-msvcp140-design/` (CLAIM.json, MSVCP140-ASSESSMENT.md, REPORT.md, ue4ss-msvcp140-symbols.txt).
- Read-only elsewhere (frozen readobj dumps, worker-modreqs census, RETAINED-IMPORT-COVERAGE.json, windows-services inventory).
- Assessment only; no implementation, no engine/verify scripts touched.
