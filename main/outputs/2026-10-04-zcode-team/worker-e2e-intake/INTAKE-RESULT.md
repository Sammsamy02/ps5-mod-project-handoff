# INTAKE-RESULT.md — live-console intake blocker probes (worker-e2e-intake, 2026-10-05)

Console: `ftp://ps5-host-not-configured.invalid:2121` (ftpsrv.elf pid 90, banner: `Version: v0.21.1 (built Aug 20 2026 17:02:36)`) + `http://ps5-host-not-configured.invalid:8084` (pldmgr). Anonymous login, PASV only (EPSV → `502 Command not recognized`, curl falls back automatically).

## 0. Pre-state (read-only LIST)

- `/data/` exists; `/data/modbridge/` exists (`drwxrwxrwx`, mtime Oct 05 11:37) containing **only** `roots/` (mtime Oct 04 22:22).
- `/data/modbridge/intake/` did **NOT** exist.
- `/data/pldmgr/payloads/` exists with prior imported payloads incl. `MODBRIDGE_INST_E2E` (earlier staging; untouched this run).

## 1. Task 1 — confirm the blocker: MKD + STOR against /data/modbridge/intake

### 1a. MKD

```
$ curl -sv --max-time 10 -Q "MKD /data/modbridge/intake" "ftp://ps5-host-not-configured.invalid:2121/data/" -o /dev/null
> MKD /data/modbridge/intake
< 226 Directory created
curl rc = 0
```

**MKD SUCCEEDED.** ftpsrv replies `226 Directory created` — a nonstandard reply code for MKD (RFC 959 says 257), an ftpsrv quirk worth remembering when parsing replies. Post-LIST confirms:

```
/data/modbridge/:
drwxrwxrwx 2 0 0 65536 Oct 05 19:49 intake
drwxrwxrwx 2 0 0 65536 Oct 04 22:22 roots
```

### 1b. STOR into the (previously missing) intake dir — after creation

Local probe file: 88 bytes, sha256 `818dafb31d5836f9cebd80381e71f9fab2ad7dee143baac16b5a63a15a455783`.

```
$ curl -sv --max-time 15 -T /tmp/modbridge-intake-test.txt "ftp://ps5-host-not-configured.invalid:2121/data/modbridge/intake/test.txt"
> CWD data  < 250 OK
> CWD modbridge  < 250 OK
> CWD intake  < 250 OK
> TYPE I  < 200 Type set to I
> STOR test.txt
< 150 Opening data transfer
< 226 Data transfer complete
curl rc = 0
```

Readback: `RETR` rc 0, `diff` local vs readback → **byte-identical**, sha256 matches (`818dafb3…5783`).

**Conclusion for task 1:** the parked blocker does not reproduce in the current console state. MKD works (given an existing parent) and STOR into the intake dir returns rc 0 with verified readback. The KB fact (`KNOWLEDGE-BASE.md` §2.1, source `Lreturn_e2e_blocked`: "MKD is refused and STOR-into-missing-dir returns rc 9 silently") is reconciled in §4 below.

## 2. Task 2 — alternative creation routes

### 2a. pldmgr `manage:upload` with path-carrying filename

```
$ curl -sS -X POST "http://ps5-host-not-configured.invalid:8084/manage:upload?filename=%2Fdata%2Fmodbridge%2Fintake%2Fprobe.txt" \
       --data-binary "e2e-intake probe worker-e2e-intake 2026-10-05"
HTTP_STATUS=500
body: Error during upload
```

**REJECTED — HTTP 500 `Error during upload`.** Post-probe LISTs of `/data/modbridge/intake/`, `/data/modbridge/` and `/data/pldmgr/payloads/` are unchanged: **pldmgr creates no parent dirs and does not accept absolute-path filenames.** This matches the upstream handler semantics already documented (`outputs/2026-09-30-native-sdk-host-route.md`: server writes `/data/pldmgr/payloads/<filename>.tmp` then imports into a derived payload dir — `filename` is a basename, not a path). Route (a) is dead for intake purposes; `manage:upload` can never place `e2e-cmd.json` at the fixed path.

### 2b. Flat/writable-parent alternative

`/data/modbridge/` itself is `drwxrwxrwx` and therefore a candidate host for a flat intake. Design check (read-only):

- `worker-e2e-console-dry/E2E-CMD-SCHEMA.md` §1: `#define E2E_CMD_PATH "/data/modbridge/intake/e2e-cmd.json"` — "The fixed path is a **compile-time constant** … No env vars, no relative resolution".
- `worker-d1-chain-relink/composition/vendored/modinst_core.c` `mkdirs()` creates only `state`/`roots`/journal dirs — never `intake/`.

So a flat `/data/modbridge/e2e-cmd.json` consumption would require a **payload rebuild (design change)**. Bundle/archive/quarantine paths inside the cmd JSON (`options.bundle` / `options.archive` / `options.quarantine`) are data and were always adjustable — but the cmd-file location is not. Route (b) was **not needed**: route MKD succeeded first, so the design stays as-is.

## 3. Task 3 — created state + readback + reconciliation

Console artifacts created (exactly the bounded set):

| Artifact | Method | Verification |
|---|---|---|
| `/data/modbridge/intake/` | FTP `MKD` | LIST: `drwxrwxrwx … Oct 05 19:49 intake` |
| `/data/modbridge/intake/test.txt` | FTP `STOR`, 88 B | RETR byte-identical, sha256 `818dafb31d5836f9cebd80381e71f9fab2ad7dee143baac16b5a63a15a455783` |

`/data/modbridge/intake/` currently contains ONLY `test.txt` — `e2e-cmd.json` is absent, so the controller's pre-STOR absence check (CONSOLE-E2E-EXECUTION.md §2) passes as-is. The controller may `DELE` `test.txt` during its own normal intake cleanup if it wants a pristine dir; I deliberately did not delete anything (DELE was outside my bounded writes).

**Reconciliation outcome: the dir now exists → the installer design is consumed unchanged.** The `E2E_CMD_PATH` constant stays `/data/modbridge/intake/e2e-cmd.json`; no payload rebuild; no installer-side change. The staged E2E plan's STOR probe gate (PREREQS P1/G3: `curl -s -T … "$FTP/data/modbridge/intake/movspeed.zip"` + sha readback) is exactly the operation proven green in §1b.

## 4. Historical-fact reconciliation (MKD "refused" vs today's success)

- Today: MKD of `/data/modbridge/intake` returns `226 Directory created` — because its parent `/data/modbridge/` exists (roots/ was created 2026-10-04 22:22, consistent with an earlier installer dispatch's `mkdirs()`; the KB `Lreturn_e2e_blocked` episode predates that parent existing).
- Most probable reading: the earlier "MKD refused" was an MKD whose **parent chain did not yet exist** (or predated the current ftpsrv v0.21.1). Not re-tested here: a parent-missing MKD experiment would require creating a directory outside the task bounds.
- The rc-9-silent STOR finding was not re-triggered either (the dir now exists). For the E2E controller this is moot: the STOR-probe gate in the staged plan already requires an explicit post-STOR sha readback, which is what catches a silent failure.

## 5. What was NOT done (bounds)

- No dispatch of the installer payload; no ELF upload; no `e2e-cmd.json` staged (that is the controller's sha-verified per-dispatch job).
- No `ploader.ini`, `plugins/`, or mod-tree access. No DELE/REN. No other console writes of any kind; everything else read-only LIST/RETR plus the single rejected task-2a POST.
