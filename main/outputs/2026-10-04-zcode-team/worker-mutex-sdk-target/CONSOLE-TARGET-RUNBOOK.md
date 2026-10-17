# CONSOLE-TARGET-RUNBOOK.md — MUTEX_DLL_TEST.elf (controller-executed)

Origin: worker-mutex-sdk-target, 2026-10-04. Prepared from the ACTUAL InitOnce
target run evidence (`outputs/2026-10-04-zcode-controller/CONSOLE-LOG.md`
items 7-10, `console/1a6b37b7-console-receipt.json`,
`console/1a6b37b7-console-result-2026-10-04.jsonl`) and the full-composition
host evidence (builds 68690d14 and 78bf885d). No console contact was made by
this worker; all `$HTTP`/`$FTP` steps are controller-executed.

## 0. HARD PREREQUISITE — do not stage anything yet

The staging artifact does not exist. The full SDK composition currently fails
at `native-owned_runtime` (`ms_abi` on `x86_64-sie-ps5`); see
`SDK-COMPOSITION-RESULT.md` + `build.py.proposal.diff`. Required before ANY
console step:

1. Controller applies the one-line proposal (distinct revision) and re-runs the
   FULL composition (same flags, same `--report-path`, fresh uuid output).
   Receipt must say `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED` with
   `warnings: {}`, host modes normal+release green (imports 51/10, mutex+
   init_once enabled, cleanup retired), worker-failure controls green, and
   `import_providers` fully mapped (every undefined symbol has >=1 provider).
   Record the new build_id as `<BUILD_ID>` and the artifact `sha256`/`bytes`.
2. Console reachability: `GET $HTTP/list_payloads` -> 200.
3. Catalog check: `MUTEX_DLL_TEST` must NOT already exist. Precedent: a fresh
   alias is created by the upload; re-upload to the same alias is only
   legitimate for an alias this track already owns (CONSOLE-LOG item 9).
   Never overwrite any pre-existing payload name.

Conventions (CONSOLE-LOG / STEP-BY-STEP):
`HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121`.

## 1. Staging — upload ONCE, verify readback

```sh
curl -s -X POST --data-binary @<BUILD_DIR>/owned-dll-candidate.elf \
     "$HTTP/manage:upload?filename=MUTEX_DLL_TEST.elf"          # expect OK
curl -s "$HTTP/manage:check"                                    # expect ok/file_exists
curl -s "$FTP/data/pldmgr/payloads/MUTEX_DLL_TEST/MUTEX_DLL_TEST.elf" | shasum -a 256
```

- Readback sha256 MUST equal the receipt's `artifact.sha256`; FTP byte count
  MUST equal `artifact.bytes`. Expect ~242-250 KiB (1a6b37b7 was 241784; the
  mutex family adds a few KiB — the receipt value is authoritative, do not
  assume).
- The ELF remains a payload ELF staged under pldmgr; it is never renamed as a
  PRX and no game files are touched (game-path HOLD honored).

## 2. Known-answer (decryption-oracle) expected values

The result record's fixed constants are the oracle. Every value below is
profile-invariant and was proven identical in the InitOnce TARGET run
(1a6b37b7, 4 records) and the full-mutex HOST runs (68690d14, 78bf885d), so
any mismatch on target means target-specific divergence:

| record field | expected exact value | meaning |
|---|---|---|
| marker / schema | `modbridge.owned-dll` / `1` | record identity |
| build_id | `<BUILD_ID>` (uuid hex, 32 chars) | this build only |
| sequence | 0,1,2,… contiguous from 0 | no gaps/duplicates |
| phase set | `[1,2,3,5]` | no phase 4 (not a release run) |
| execution | stage 11, started/run_attempted/completed = 1/1/1, dll_calls 3/3 | full lifecycle |
| entry | attach [7,7], detach [0,0], detach_result [0,0], contract_errors 0 | DllMain BOOL preserved |
| clock | sessions [1,1] begin / [1,1] end | QPC/QPF sessions retire |
| worker | enabled 1, calls 2/2 | worker export ran and returned |
| worker.begin | `[5, 1, 258, 259]` | handle token, tid slot, 258 WAIT_TIMEOUT re-wait, 259 alertable wait |
| worker.result | `[2166572391, 1234605616436508552, 3782448555, 0]` | `0x81234567` exit, cookie `0x1122334455667788`, last_error `0xe17395ab`, final wait OK |
| results | `[86, 93]` | consumer+helper export call results |
| synchronization | `{enabled:1}` | CS/SRW/address-wait family |
| init_once | `{enabled:1}` | InitOnce family |
| cleanup | `{pending:0, retired:1}` | no leaked tokens, cleanup done |
| first_failure | all zeros (stage/native_status/windows_error/module_cause) | zero failures |
| memory | owner_status 0; last {operation 3, native_result 0, length 98304}; first_failure zeros | mprotect telemetry clean |
| delivery | file_errors 0/0, file_records 3; stdout error 32 only (ENOTTY-class, delivery-only) | report reached /data |

## 3. Dispatch — single, under standing authorization

```
GET $HTTP/loadpayload:/data/pldmgr/payloads/MUTEX_DLL_TEST/MUTEX_DLL_TEST.elf   # expect OK
```

- Authorization basis (same pattern as CONSOLE-LOG item 9): changed
  implementation vs INITONCE_DLL_TEST — new mutex family through real PE
  imports plus the combined CloseHandle. This is NOT an unchanged-diagnostic
  repeat; no re-rating of unchanged evidence is involved.
- EXACTLY ONE dispatch. Never re-dispatch while a run may be live.
- Collect afterwards: `curl -s "$FTP/data/modbridge-owned-dll-candidate.jsonl"`
  — the file is append-only history (16 lines after the InitOnce run);
  filter by `build_id == <BUILD_ID>`; expect exactly 4 new records
  (phases 1,2,3,5). Never truncate or rewrite the log.
- Store result copy + console-receipt JSON under
  `outputs/2026-10-04-zcode-controller/console/` (pattern:
  `1a6b37b7-console-receipt.json`): upload readback sha, dispatch count=1,
  final-record oracle table, `expected_values_matched: true/false`.

## 4. Expected deltas vs the InitOnce target run (1a6b37b7)

| field | InitOnce observed | MUTEX_DLL_TEST expected | why |
|---|---|---|---|
| counts.consumer_imports | 46 | **51** | +5 KERNEL32 mutex imports: CreateMutexW, CreateMutexExW, OpenMutexW, ReleaseMutex, WaitForSingleObjectEx (owned_runtime.c:317 census: 18+5+21+2+5) |
| counts.helper_imports | 10 | 10 (unchanged) | helper gains no new imports |
| mutex | key ABSENT | **`{enabled:1}` (new key)** | profile flag now on |
| init_once / synchronization | `{enabled:1}` / `{enabled:1}` | unchanged | — |
| worker.begin / worker.result | `[5,1,258,259]` / `[0x81234567, 0x1122334455667788, 0xe17395ab, 0]` | **identical** | mutex checks run inside prepare and pass silently on host; success-path values unchanged by design |
| cleanup | `{pending:0, retired:1}` | identical | MFR-1 (7/7 keeps) + MFR-2 (retire-on-every-exit) keep teardown green; the old stage-40 BUSY hang must NOT reappear |
| everything else | — | identical | see section 2 |
| ELF artifact | sha bc0574d6…, 241784 bytes, 42 undefined imports | NEW sha; bytes slightly larger; undefined count may grow only by a few pthread/clock names (mb_win_mutex.c native uses pthread cond/mutex/key + clock_gettime already linked by the threads family) | receipt `import_providers` mapping is authoritative — all must resolve |

## 5. Triage table for NEW failure stages (mutex run)

Read `first_failure.stage` + `native_status` (int32; hex like 0xFFFFFFBC =
-68). Fixture code ranges: sync/initonce -1..-53 (unchanged from InitOnce run),
mutex -60..-78 (new). Host passed every check below on the SAME fixture bytes,
so any target failure is adapter/target-specific evidence — record it, give
the next revision a distinct ID; do not re-dispatch unchanged bytes.

| stage | native_status | meaning | first suspects | action |
|---|---|---|---|---|
| 9 | 0 | PE import census != 51/10 on target | mutex KERNEL32 imports not all bound (def/lib mismatch, folded-name resolution) | compare `counts` vs section 4; inspect loader metadata |
| 22 / 26 / 27 / 28 / 29 | workers status | worker-family plumbing around the mutex path | unchanged code — suspect only if InitOnce-identical fields also drift | compare vs 1a6b37b7 record |
| 24 | -60..-63 (0xFFFFFFC4..C1) | unnamed-mutex create/wait/release/alertable-wait check | adapter wait semantics on target | note check number; compare with host green |
| 24 | -64..-67 (0xFFFFFFC0..BD) | named create / folded open / create-existing 183 / not-owner 288 | name folding or error propagation | same |
| 24 | -68 / -69 / -70 (0xFFFFFFBC/BB/BA) | acquire via opened / owner re-wait must TIME OUT (258) / release; second-handle grant | the reordered block from worker-mutex-fix-2 — an owner re-wait GRANTING on target is the classic deviation | capture recursion value recorded in fixture state |
| 24 | -71 (0xFFFFFFB9) | backslash name must fail with 87 | name validation difference | same |
| 24 | -72..-76 (0xFFFFFFB8..B4) | finish: abandon_open open / WAIT_ABANDONED after join / release / re-wait grant / re-release | abandonment transfer semantics on target | worker exits owning the named mutex; creator must see 0x80 |
| 24 | -77/-78 (0xFFFFFFB3/B2) | retirement itself failed (tokens left) | combined CloseHandle path | MFR-1/MFR-2 regression; expect stage 40 next |
| 24 | 0xFFFFFFD8 (-40) | invalid worker parameter | --reject-worker-start leakage — must NOT occur in this profile | composition error; do not dispatch again |
| 25 | 4 | pending worker admission retire | host analog green (expected exactly this in failure controls) — in normal mode means unintended fault | verify mode |
| 39 | mutex status | mb_win_mutex_open failed (native init) | new for this family | first mutex-specific native stage |
| 40 | nonzero (BUSY=3 class) | mutex close at cleanup with outstanding tokens | token leak (MFR-1/MFR-2 gap on target); sdk_driver.c:45 unbounded retry = liveness hazard | if phases stop at 4 and no new record for >2 min: record honestly, NO re-dispatch, new revision |
| (none) | phases stop at 4 / file stalls | cleanup retry loop | as above | same — bounded patience, then report |
| any | delivery.file_errors != 0 | /data report write failed | FTP/log path | check file presence/permissions |

Unknown native_status: convert hex->int32 FIRST (the build-4 "-100" was a
misread of 0xFFFFFFBC = -68); only then map to the fixture ranges above.

## 6. Post-run

- On `expected_values_matched=true`: update CURRENT-PS5-TEST pointer/current
  build notes (controller-owned), preserving 1a6b37b7 artifacts.
- On any failure: preserve the 4 records verbatim, write the console-receipt
  with `expected_values_matched:false` and the triage row filled; the next
  revision gets distinct IDs and its own dispatch.
