# REPORT — worker-mutex-sdk-retry (mutex SDK composition, ms_abi fix applied)

Result: **SUCCESS** — the full mutex SDK composition now passes end-to-end and produced the
console candidate. The previous failure at command 34 (`ms_abi` on `native-owned_runtime`) is gone;
this run completed all 81 commands with exit 0.

## 1. Command executed (workspace `/WORKSPACE/.codex/worktrees/translation-core/what`)

```
python3 outputs/ps5-mod-bridge/windows-candidate/build.py --entry --clock --workers --sync \
  --init-once --mutex --report-path /data/modbridge-owned-dll-candidate.jsonl \
  --output .superpowers/native-dependency-spike/windows-mutex-candidate-1
```

Timeout guard: no `timeout`/`gtimeout` binary exists on this Mac, so the shell guard was not
available; the harness 660 s cap served as the guard. The build finished well inside it (single
invocation, no retry, no replay).

## 2. Receipt verification

- Build dir / build_id: `.superpowers/native-dependency-spike/windows-mutex-candidate-1/6f5bf85a0f314df6acfe2f15e464ef69`
- Receipt: `<build dir>/receipt.json`, status `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`
- Commands: 81 recorded, **all exit 0** (receipt `commands` + per-command .stdout/.stderr reviewed; stderr empty except the two pre-existing lld warnings below)
- Artifact: `owned-dll-candidate.elf`, sha256 `fc4b09afc034525678aee2a5d4544b7a72308eb4cac3141b9a841b067fd055fb`, **259568 bytes** (independently re-hashed with `shasum -a 256` — matches receipt)
- Artifact nature: "Genuine SDK payload crt1 _start, not a PRX module entry"; `prx_created: false`
- Gates: `console_contact: false`, `target_execution: false`, `downloaded_dll_executed: false`
- Dynamic layout validated (21 entries, terminal DT_NULL); fixture entry RVAs helper 16544 / consumer 17136; entry profile admitted as expected
- Host controls green: normal mode `counts {consumer_imports: 51, helper_imports: 10}`, dll_calls 3/3, entry attach [7,7]/detach [0,0], `worker.begin [5,1,258,259]`, `worker.result [2166572391, 1234605616436508552, 3782448555, 0]` (= `0x81234567`, cookie `0x1122334455667788`, `0xe17395ab`, 0), results [86,93], cleanup `{pending:0, retired:1}`, first_failure all zeros, `host_owned_worker_pe_executed: true`; release mode green; worker-failure controls green (failure control ends at stage 25 / native_status 4 — the expected retire, per runbook section 5)

## 3. Import providers and undefined delta vs InitOnce baseline

- ELF undefined symbols: **43**, every one mapped in receipt `import_providers` — 35 via `libkernel_web.so`, 8 via `libSceLibcInternal.so` (abort, calloc, free, malloc, memcmp, memcpy, memmove, memset)
- Baseline: InitOnce full-composition build `windows-initonce-candidate-1/c49108fea0bf4454a886d610b78edc0a` (same status, 241784 bytes; its sibling `1a6b37b7…` is the console-executed InitOnce build, artifact `bc0574d6…`, 241784 bytes)
- Undefined delta: **+1 symbol, `memmove`** (42 → 43), provided by `libSceLibcInternal.so`. The runbook anticipated "a few pthread/clock names"; actual growth is the single `memmove` — no unresolved or unexpected symbols.

## 4. Warnings (honest note)

Receipt `warnings` holds the two rust-lld `/align specified without /driver` messages (helper-link,
consumer-link). These are **byte-identical to the accepted InitOnce baseline** (both `1a6b37b7…`,
which the console already ran, and `c49108fe…`). The runbook prerequisite text says `warnings: {}`,
which is stricter than the accepted precedent; nothing new was introduced by this build.

## 5. Console staging info (for controller; NO console/network contact made by this worker)

Follow `outputs/2026-10-04-zcode-team/worker-mutex-sdk-target/CONSOLE-TARGET-RUNBOOK.md` verbatim:

- **Upload once** as `MUTEX_DLL_TEST.elf`: `POST --data-binary @<build dir>/owned-dll-candidate.elf "$HTTP/manage:upload?filename=MUTEX_DLL_TEST.elf"`; verify `manage:check` ok; FTP readback sha256 must equal `fc4b09af…055fb` and byte count 259568. Staged under `pldmgr` as payload ELF; never renamed as PRX.
- **Dispatch exactly once**: `GET $HTTP/loadpayload:/data/pldmgr/payloads/MUTEX_DLL_TEST/MUTEX_DLL_TEST.elf` (fresh alias, changed implementation vs INITONCE_DLL_TEST — standing-authorization basis per runbook section 3).
- **Collect**: `FTP /data/modbridge-owned-dll-candidate.jsonl`, filter `build_id == 6f5bf85a0f314df6acfe2f15e464ef69`; expect exactly **4 new records, phases [1,2,3,5]**, contiguous sequences, appended (log is append-only — never truncate).
- Staged evidence copy of the candidate: `outputs/2026-10-04-zcode-team/worker-mutex-sdk-retry/MUTEX_DLL_TEST.elf` (sha256 verified identical to receipt artifact).

## 6. Expected result deltas vs the InitOnce target run (1a6b37b7)

| field | InitOnce observed | MUTEX_DLL_TEST expected |
|---|---|---|
| build_id | `1a6b37b7…` | `6f5bf85a0f314df6acfe2f15e464ef69` |
| counts.consumer_imports | 46 | **51** (+5 KERNEL32 mutex imports: CreateMutexW, CreateMutexExW, OpenMutexW, ReleaseMutex, WaitForSingleObjectEx) |
| counts.helper_imports | 10 | 10 (unchanged) |
| mutex | key absent | **`{enabled:1}`** (new key) |
| init_once / synchronization / clock / entry / worker.begin / worker.result / results [86,93] / cleanup {pending:0, retired:1} / first_failure zeros / memory / delivery | observed values | **identical** (host normal mode already reproduced every oracle constant on the same fixture bytes) |
| ELF artifact | `bc0574d6…`, 241784 B, 42 undefined | `fc4b09af…`, 259568 B, 43 undefined (+memmove, provided) |
| failure triage if any | sync/initonce −1..−53 | new mutex fixture range −60..−78 per runbook section 5 (owner re-wait must TIME OUT 258; abandonment 0x80; stage-40 BUSY must NOT reappear) |

## 7. Files

- Receipt/build: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/windows-mutex-candidate-1/6f5bf85a0f314df6acfe2f15e464ef69/`
- This worker's outputs: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-mutex-sdk-retry/` (CLAIM.json, build-stdout.log, MUTEX_DLL_TEST.elf staged copy, elf-undefined.stdout copy, owned_profile.h copy, REPORT.md)
- Runbook governing console steps: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-mutex-sdk-target/CONSOLE-TARGET-RUNBOOK.md`

No console/network access, no nested subagents, no PRX creation, no re-dispatch; existing files outside the write scope untouched.
