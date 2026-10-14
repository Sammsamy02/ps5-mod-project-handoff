# STAGE-CARD — diagnostic ploader (rc-naming FAIL popup), one-launch observation

Worker: worker-loader-instrument · 2026-10-05 · host-side build + verification + THIS CARD ONLY.
**No console contact was made by this worker; the controller stages and the user launches.**
No game claims. Precedent staging law reused verbatim: worker-staging-next `STAGING-PLAN-NEXT.md`
(Gate A0, pre-images, `.bak`, readback-verify-everything, ini freeze).

## 0. What is being staged, and why

The loading regression: the composition module is rejected by `sceKernelLoadStartModule` in
~85 ms (FAIL popup) while path_test loads OK — and the console's baseline ploader payload
(sha256 `edf61245…`, 142,104 B) prints `FAIL (<ms>): <file>` and **discards the kernel return
code**. The diagnostic lineage changes exactly one source line so the FAIL popup names the rc:

```
[ploader] [PPSA33286] FAIL (85ms, rc=0x80020064): <file>.prx
```

One launch with this build names the exact kernel error class. That is the whole purpose.

**Artifact to stage (choose ONE, never mix):**

| candidate | bytes | sha256 | receipt depth |
|---|---|---|---|
| `ploader-return-message-1/diagnostic/ploader.elf` (recommended) | 142,968 | `009445ab35ecf447696a325c88c617d469d5b6cc966ccece914bdc09577f706d` | full spike chain: commands.json, verification.json, inputs.json (2847 pins), REPORT.md |
| `worker-loader-instrument/build-diagnostic/ploader.elf` (equivalent rebuild) | 142,904 | `279c14532be884ef79009362eede432a58175ae7cc9c179b2ebf2c71af485353` | this worker's BUILD-RECEIPT.json; `.text`/`.rodata` byte-identical to the recommended artifact (delta = `-g` build-path metadata only) |

Both are plain ELF (`7f 45 4c 46`, NOT FSELF — no decrypt-oracle gate; expected readback is
full byte identity).

## 1. Upload path

```
/data/pldmgr/payloads/ploader/ploader.elf
```

This is the pldmgr-armed ploader payload slot (same path the 2026-10-05 `stage-next` phases
A+B used). Arm with `GET http://ps5-host-not-configured.invalid:8084/loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`
or the Payload Manager UI. Game CLOSED during staging; ONE launch per staged build.

Context (not a substitute for the assignment): a `PloaderReturnCode.elf` payload
("1.1.0-rc-log", local_build, "Existing FAIL notification only: rc hexadecimal output") is
already listed in pldmgr from an earlier cycle, and the 2026-10-03
`middle-import-fail-f9272a95…` observation came through a return-code loader. This card
stages the diagnostic into the ploader slot itself per the assignment.

## 2. Pre-image / rollback law (current file is the edf61245 class)

1. **Gate A0 write probe, exactly once**: STOR+RETR `/data/modbridge/intake/.writeprobe`;
   roundtrip body must equal the 16 bytes of console `/etc/hostname` (last observed:
   `modforge-loadlis`). No roundtrip → STOP, no evidenced write route.
2. **PRE-IMAGE (RETR only, BEFORE any write)**: fetch `/data/pldmgr/payloads/ploader/ploader.elf`,
   record sha256+bytes in the run dir.
   - Last recorded pre-image: `edf612451dd0b22a70f18e6c2dab19075060ed91b13cc3ceb17f36816220a7ef`,
     142,104 B (`worker-staging-next/runs/20261005-200504-stage-next/ploader.elf.pre.sha256`,
     run start 2026-10-05 20:05 local). That run recorded verdict GO for phases A+B, which
     would have overwritten the slot with `4c339235…` (144,248 B). **Do not assume which bytes
     are live now** — the fresh RETR in step 2 is authoritative; record it either way.
3. **Rollback artifact BEFORE overwrite**: STOR the pre-image bytes to
   `/data/pldmgr/payloads/ploader/ploader.elf.$TAG.bak` (one new tag per staging attempt;
   never overwrite a prior `.bak`).
4. **Upload**: ONE STOR of the chosen diagnostic artifact → the ploader path. No DELE anywhere.
5. **Readback verify**: RETR and `shasum -a 256`. Expected FULL byte identity. The ONLY accepted
   fallback is the known SHT-strip tail class: length == max(p_offset+p_filesz) = **98,332 B**
   for these artifacts, body bytes 64..end identical, magic intact. ANY mismatch → **ONE
   bounded restore of the saved pre-image** (STOR the `.bak` bytes back, readback-verify the
   restore) → then refuse. Never blind-retry.
6. **ini freeze**: `/data/plugins/ploader.ini` read at start AND end, sha256 byte-equal
   (last frozen: `1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0`, 263 B,
   2-plugin config: composition mode 2 first, path_test mode 2 last). This flow NEVER writes
   the ini; external drift is the controller's reconciliation (`ploader.ini.DRIFT.sha256`).

## 3. Expected popup formats (sceKernelSendNotificationRequest; notify() prepends `[ploader] `)

| launch outcome | exact popup text |
|---|---|
| injection round starts for the title | `[ploader] [PPSA33286] Injecting N plugin(s)` |
| **module rejected (THE observation)** | `[ploader] [PPSA33286] FAIL (<dt>ms, rc=0x********): <filename>` |
| module accepted | `[ploader] [PPSA33286] OK (<dt>ms): <filename>` |
| target died during readiness wait | `[ploader] [PPSA33286] The case died while waiting '<filename>'` (no rc — different failure, record as such) |
| ptrace attach failed | `[ploader] [PPSA33286] Attach failed for '<filename>'` (no rc) |
| baseline comparison (current edf61245-class binary) | `[ploader] [PPSA33286] FAIL (<dt>ms): <filename>` — rc absent |

`<dt>` is elapsed wall time of the inject attempt, not an error number. `<filename>` is
`get_filename(path)` of the ini entry. The rc printed is the raw low-32 bits the shellcode
collected in RAX at the `INT3` trap, i.e. the kernel's own `sceKernelLoadStartModule` return
reinterpreted via `(unsigned)rc`.

## 4. Decision table — sceKernelLoadStartModule rejection codes

Decode source (in-tree, pinned): `.superpowers/native-dependency-spike/ploader-rc-80020063-1/public-orbis-error.h`
(retained shadPS4 public decode, GPL-2.0-or-later; **not Sony normative documentation**; the
retained list ends at `ESTART 0x80020064`). Observation receipts cited per row.
Status legend: **EVIDENCED** = in-tree decode + console observation or in-tree source proof;
**DECODE-ONLY** = public decode present, no console observation; **INFERRED** = no in-tree
decode, semantics proposed by the task brief only.

| rc | decode | cause class | next action if observed | status |
|---|---|---|---|---|
| `0x80020064` | `ESTART` | kernel attempted the start and the start failed/aborted (module accepted far enough to be started; NOT a format/stamp rejection at load) | compare against the Oct-3 precedent below; if the composition reproduces it, the failure is on the start/initializer/import-resolution side — pull `native-runtime-import-diagnostics-1` (ledger records `ONE_EXACT_UNRESOLVED_IMPORT_IDENTIFIED`) and the owned-log absence law; do NOT touch the SDK stamp on this evidence | **EVIDENCED** — console observation `FAIL 85 ms rc=0x80020064` for `modbridge_runtime_import_probe.prx` (23,735 B, sdk 0x11000000), receipt `.superpowers/native-observations/middle-import-fail-f9272a95ff6242ac88f31d1a654b9143/receipt.json`, ledger meaning: "Public error list names ESTART; specific startup failure not yet known". Most likely first-launch outcome for the composition given the ~85 ms baseline signature. |
| `0x80020063` | `ESDKVERSION` | module/dependency SDK-version stamp rejected before start | repackage the module with the console-proven SDK stamp `0x11000000` (+ companion `0x08050001`); verify statically via package_prx before any restaging | **EVIDENCED** — `modbridge_log_probe.prx` v2 baseline returned 0x80020063; the 0x11000000 stamp turned it into `OK 126ms`, `module_start_confirmed` (ledger `working_probe_result`; receipt `sdk-version-probe-user-ok-60f346e25c714b70bab9fc8f0010d036`). Corroborated by `worker-prx-packaging/STAGING-CHECKLIST.md` and composition5 package-profile `sdk_stamp_provenance: "console-proven fix for ESDKVERSION 0x80020063"`. |
| `0xffffffff` | NOT a kernel code | `inject_prx` returned `-1`: local pre-check failed — `access(prx)`, NID resolve of `sceKernelLoadStartModule`, `pt_mmap`, copyin, or `pt_call` setup/trap collection | loader-side problem, NOT a module rejection: verify the prx path is readable in the target's jail, that the game process reached readiness, and that ptrace attach worked (a prior `Attach failed` popup would corroborate) | **EVIDENCED by source** — diagnostic `main.c` lines 423–456 (`inject_prx` early `-1` paths) and 260–290 (`pt_call` `-1` paths); `ploader-rc-80020063-1/return-path-review.json` return_path facts 2–3. |
| `0x8002000c` | `ENOMEM` | kernel out of memory mapping the ~30 MB image | reduce module footprint or free console memory; plausible specifically because the composition is ~30 MB while all prior observations are ≤ a few hundred KB | DECODE-ONLY (public-orbis-error.h line 21); attribution to this module INFERRED (no console observation). |
| `0x80020011` | `EEXIST` — **correction**: the retained decode says EEXIST (already loaded/exists), NOT a "module/auth" class | duplicate/already-present module | indicates a second injection attempt in one boot (ploader's known-list one-shot makes this unlikely per boot; per launch-day law mode-2 entries need re-send per launch, so a stale second entry could collide) | DECODE-ONLY for EEXIST (public-orbis-error.h line 26); occurrence INFERRED. The task brief's "module/auth" label for this code is UNSUPPORTED by the retained decode — auth-family codes are `EPERM 0x80020001`, `EAUTH 0x80020050`, `ENOTCAPABLE 0x8002005d`. |
| `0x80020002` / `0x8002000d` | `ENOENT` / `EACCES` | path not found / not accessible from the TARGET process context | fix staging path/jail visibility of the prx; check the ini path spelling and mode-2 timing | DECODE-ONLY (public-orbis-error.h lines 11, 22); occurrence INFERRED. |
| `0x80020016` | `EINVAL` | invalid argument — e.g. ELF the kernel's loader refuses (format/mach-o-class problem) | static ELF-class comparison of the composition vs the accepted path_test module (same pattern as worker-staging-next STRUCTURAL-CHECKS) | DECODE-ONLY (public-orbis-error.h line 31); occurrence INFERRED. |
| `0x800200cb` / `0x800200d1` / `0x800200d3` | **NOT in the retained public list** (ends at 0x80020064); no in-tree definition anywhere | task-brief proposals only: missing-dependency / no-memory / module-auth — UNVERIFIED | treat as UNKNOWN: capture the popup, extend the retained decode from a fresh pinned public source (same discipline as the 80020063 spike), and only then classify; do not act on the brief's guessed semantics | **INFERRED, unsupported** — any such popup value is outside every retained decode and outside all in-tree observations to date. |

Generic law for ANY observed rc: record the popup verbatim (photo or transcription) with the
launch timestamp, keep the `.bak` rollback until the controller reconciles, never replay the
launch to "see it again", and never accept a semantics not present in the pinned decode or an
in-tree observation.

## 5. Secondary instrument (prepared, NOT recommended for the first launch)

`worker-loader-instrument/build-filelog/ploader.elf` — 143,224 B,
sha256 `9dccfef1b04b303a628cbbac87d1030e5c6f1a8d16706cba03f474a28ee17628`.
Diff vs the diagnostic source is minimal (one static `log_result()` + one call site):
appends, never replaces, one line per plugin attempt to
`/data/plugins/ploader-results.log`:

```
OK|FAIL <TITLEID> path=<full ini path> rc=0x******** dt=<dt>ms loader=ploader-diag-filelog built=<__DATE__ __TIME__>
```

Notes: `main(void)` takes no argv, so the "argv0" slot is the `__DATE__ __TIME__` build tag
plus the static `loader=` marker (this is the only honest loader-identity available). Open
failure returns silently — logging cannot alter injection behavior. Build: exit 0, empty
stderr, same toolchain. Host-review only. **The first observation launch must use the plain
diagnostic build** (§0 table) — one variable per launch; the file-log variant is a separately
staged follow-up only if the user wants persistent per-attempt evidence.

## 6. Evidence index

- Source diff (one line): `notify-return.patch`, `ploader-return-message-1/REPORT.md`
- Build receipts: `receipts/BUILD-RECEIPT.json` (this worker), `ploader-return-message-1/{commands,verification,source-receipt,inputs}.json`
- Structural: `receipts/STRUCTURAL-CHECKS.json` + raw dumps in `structural-checks/`
- Error-code decode: `.superpowers/native-dependency-spike/ploader-rc-80020063-1/{public-orbis-error.h,return-path-review.json}`
- 0x80020064 observation: `.superpowers/native-observations/middle-import-fail-f9272a95ff6242ac88f31d1a654b9143/receipt.json`; mirrored in the composition transport ledgers (`hardware_result`)
- 0x80020063 fix chain: ledger `working_probe_result` (OK 126 ms), `sdk-version-probe-user-ok-60f346e25c714b70bab9fc8f0010d036/receipt.json`, `worker-prx-packaging/STAGING-CHECKLIST.md`
- Staging law precedent: `worker-staging-next/STAGING-PLAN-NEXT.md`, `worker-staging-next/receipts/STRUCTURAL-CHECKS.json`
