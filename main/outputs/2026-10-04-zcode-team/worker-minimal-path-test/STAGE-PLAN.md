# STAGE-PLAN — worker-minimal-path-test (mpt1-07272f303f12, 2026-10-04)

Purpose: ONE staged launch decides whether `/data/plugins/UE4SS.log` is
writable from a ploader-loaded module (output-path blocker ruling) and whether
`/data/modbridge/roots/PPSA33286/loadlist.txt` is readable from the game
process (activation-path accessibility). No acceptance beyond that is claimed.

## 1. Frozen payload

- File (staged copy, byte-identical in run1/run2):
  `outputs/2026-10-04-zcode-team/worker-minimal-path-test/modbridge_path_test.prx`
  (canonical: `.../worker-minimal-path-test/determinism/run1/modbridge_path_test.prx`)
- Build id: `mpt1-07272f303f12` (also in `build_id` inside profile.json and
  compiled into the module config header).
- fself: **8365 B**, sha256
  `b24f67e5c9a88e28626401f22982543e21775a83db04ba86821749cc71fe6e46`
- Inner ELF (bare-sdk): sha256
  `d51feb99e214ee6c4df9f55084b3e87bc3c92c499dccdb663acadea29f4d7885`
  (verify at stage time against `determinism/run1/build-receipt.json`; the
  ftpsrv readback will be the phdr-covered prefix, not the whole file —
  see worker-v3-startup-diag REPORT section 2 for the channel law).
- Import contract sha256
  `f63b2a473933b964d15a4692eb3222efff156cbe3b7b5edfc4f99389f6fcc73a` —
  8 strong rows, ALL verbatim from the frozen 191-row contract:
  the 7 console-proven igd1 rows (`__error`, `sceKernelOpen`,
  `sceKernelWrite`, `sceKernelClose`, `sysctl`, `getpid`,
  `clock_gettime`) UNTOUCHED + exactly ONE added row: POSIX `read`
  (libScePosix family) for the 16-byte loadlist read.
  **`read` is first-use-in-lineage** — its identity family is from the
  console-loaded aligned-entry 191-row set, but it has never been INVOKED
  by this lineage before. If the module fails to load, suspect this row
  first (removal recipe in section 6).
- Two-run determinism: run1 == run2 byte-identical at fself, inner ELF
  (original + sdk-stamped), combined object, contract — DETERMINISM-PASSED.
- module_start: guard → clock_gettime(CLOCK_MONOTONIC) → open
  `loadlist.txt` O_RDONLY + `read` 16 bytes + close → open `UE4SS.log`
  O_WRONLY|O_CREAT|O_APPEND (mode 0644) → write BOTH lines → close →
  **return 0 ALWAYS**. If the UE4SS.log open FAILS, it writes
  `PATH_TEST: /data/plugins/UE4SS.log OPEN_FAILED errno=<n> ...` + the
  ACTIVATION line into `/data/plugins/modbridge_path_test.fallback.log`
  (only created in that failure case). No threads, no ctor walk, no C++.

## 2. Exact lines the module emits (host-format-test verified)

On success:
```
PATH_TEST: /data/plugins/UE4SS.log writable at <decimal monotonic-ns> monotonic-ns
ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: <32 hex chars>
```
`clock_gettime` failure appends ` CLOCK_FAILED`; loadlist open/read failure
replaces the hex with `OPEN_FAILED errno=<n>` / `OPEN_OK_READ_FAILED errno=<n>`
/ `<hex> (short n=<n>)`. If `UE4SS.log` itself cannot be opened, the fallback
file carries `PATH_TEST: /data/plugins/UE4SS.log OPEN_FAILED errno=<n> at <ns>
monotonic-ns` instead.

## 3. Staging (NEW distinct file + one added ini line; nothing replaced)

Deliberate deviation from the task's "replace modbridge_ingame_facts.prx":
that file is the frozen igd1 diagnostic `igd1-fc26d6c03038` and is DISABLED
(`= 0`) — overwriting it is unnecessary and irreversible-ish. This probe
stages under its OWN name and gets its OWN entry, which is also what "add
this module's ploader entry" requires. If the controller insists on the
same-file variant: back up `/data/plugins/modbridge_ingame_facts.prx` to
`modbridge_ingame_facts.igd1.bak` first and upload the fself over it, then
flip its line to `= 2` — do NOT do both variants in one launch.

1. Upload `modbridge_path_test.prx` via FTP (2121, BINARY) to
   `/data/plugins/modbridge_path_test.prx`.
2. Read back and size-check (fself = 8365 B).
3. Edit `/data/ploader.ini`, section `[PPSA33286]`, add EXACTLY ONE line:
   ```
   /data/plugins/modbridge_path_test.prx = 2
   ```
   Leave every existing line byte-identical (the disabled
   `modbridge_ingame_facts.prx = 0` line stays as-is).
4. Launch per the standing ritual: game fully closed → ploader armed → USER
   launches Townfall (never automated). Enabled plugin count increases by
   exactly one; note ploader's exact OK/FAIL line + ms.

## 4. Collection (after the launch, before any further staging)

- ploader surface text (exact OK/FAIL line + ms).
- `/data/plugins/UE4SS.log` — exists? size/mtime + full tail.
- `/data/plugins/modbridge_path_test.fallback.log` — exists? full content.
- `/data/modbridge/roots/PPSA33286/loadlist.txt` — first 16 bytes (to
  cross-check the ACTIVATION hex offline).

## 5. Outcome matrix (what the launch decides)

| Observation | Decision |
| --- | --- |
| UE4SS.log gains BOTH lines; ACTIVATION hex == loadlist first 16 bytes | Output path WRITABLE from a ploader module; activation path READABLE from the game process. The composed runtime's failure is elsewhere. Path ruled OUT as blocker. |
| UE4SS.log gains both lines; ACTIVATION shows OPEN_FAILED/READ_FAILED errno | Output path writable; activation path blocked at errno (ruled a blocker for activation, not for output). |
| UE4SS.log unchanged; fallback.log appears | UE4SS.log specifically refused (errno in fallback says why: 2=ENOENT parent/path, 13=EACCES, 17=EEXIST-weirdness, 20=NOTDIR...) while /data/plugins itself is writable. The exact UE4SS.log path is the blocker. |
| UE4SS.log unchanged; no fallback; ploader FAIL | Load rejection BEFORE module_start. First suspect: the `read` row (first-use-in-lineage). This result does NOT rule the path in or out. |
| UE4SS.log unchanged; no fallback; ploader OK | Module loaded/started but neither file appeared → /data (or /data/plugins) not writable from the game process, or module_start never actually ran. Compare with igd1 history; treat output path as blocked until contradicted. |

## 6. Failure contingency (only if ploader FAIL)

Rebuild with the `read` row removed (open-only variant): set
`STRONG_VERBATIM` back to the proven 7 in `build_path_test.py` and stub
`path_test_entry.c`'s read block to record `READ_IMPORT_ABSENT`. New build id
(`mpt2-`), new determinism run, THEN restage. Never relaunch the identical
rejected binary hoping for a different result.

## 7. Bounds honored

This worker executed NO console/network operations and modified NO existing
file outside `outputs/2026-10-04-zcode-team/worker-minimal-path-test/`.
ploader.ini is not edited here; the operator applies section 3 step 3 by hand.
