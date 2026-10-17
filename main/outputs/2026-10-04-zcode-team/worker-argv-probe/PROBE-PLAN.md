# PROBE-PLAN.md — G1 ARGV dispatch probe (controller-executed)

Origin: worker-argv-probe, 2026-10-04. Resolves GO-NOGO gate **G1** (hard
gate, top risk): does the pldmgr `/loadpayload` route forward arguments to
the dispatched payload? Conventions follow
`worker-e2e-console-staging/STEP-BY-STEP.md` and CONSOLE-LOG grammar.

- `HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121`
- Payload: `outputs/2026-10-04-zcode-team/worker-argv-probe/build-1eb64d8964c6/ARGV_ECHO.elf`
  sha256 `53075bd7cadbebbdfc11df03e08df740a57b81148a270c68652cc5dc81104e6f` (111,016 B)
- Preconditions: G0 reachability OK; game NOT running (G4); no Townfall
  launch (HOLD); evidence dir
  `outputs/2026-10-04-zcode-controller/console/` for collected files.
- **Dispatch budget: ≤3 total.** Never repeat a dispatch whose outcome was
  already collected; a failed/uncollected dispatch is investigated offline,
  not replayed.

## Step A — upload ONCE, verify identity

```sh
curl -s -X POST --data-binary @outputs/2026-10-04-zcode-team/worker-argv-probe/build-1eb64d8964c6/ARGV_ECHO.elf \
     "$HTTP/manage:upload?filename=ARGV_ECHO.elf"        # CONSOLE-LOG item 2/8 pattern
curl -s "$HTTP/manage:check"; curl -s "$HTTP/list_payloads"
curl -s "$FTP/data/pldmgr/payloads/ARGV_ECHO/ARGV_ECHO.elf" | shasum -a 256
```
PASS: readback sha = `53075bd7…04e6f`, 111,016 bytes, catalog shows
`/data/pldmgr/payloads/ARGV_ECHO/ARGV_ECHO.elf`. Never dispatch unverified
bytes. We own this new alias; a failed upload may be re-uploaded to the SAME
new name only.

## Step B — dispatch ONCE, bare (the documented route, no arguments)

```sh
curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/ARGV_ECHO/ARGV_ECHO.elf"
```
Exactly the CONSOLE-LOG item 4/15 grammar: `GET /loadpayload:<path>`, no
query string, no arguments. Dispatch once.

## Step C — collect run 1

```sh
curl -s "$FTP/data/modbridge-argv-probe.jsonl" > console/ARGV_ECHO-run1.jsonl
wc -l console/ARGV_ECHO-run1.jsonl
```
PASS: exactly 1 JSONL line, `"marker":"modbridge.argvprobe"` present.

## Step D — dispatch ONCE again (bare, identical) + collect run 2

The second bare dispatch is the **two-run determinism run** (new evidence,
not a replay: run 1 was collected). Then:

```sh
curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/ARGV_ECHO/ARGV_ECHO.elf"
curl -s "$FTP/data/modbridge-argv-probe.jsonl" > console/ARGV_ECHO-run2.jsonl
```
PASS: 2 lines, line1 == line2 byte-identical. A mismatch = nondeterminism or
per-run environment leakage — STOP, report; the probe result is not usable
for G1 until explained.

## Step E — variant test (ONLY if the manager UI exposes an arguments field)

The user checks the manager's payload page for ARGV_ECHO for an
args/arguments input box.
- **If a free-text args field exists**: enter exactly
  `alpha beta "three four"` and dispatch via that UI once; collect run 3:
  `curl -s "$FTP/data/modbridge-argv-probe.jsonl" > console/ARGV_ECHO-run3.jsonl`
  Expected argv tokens: `alpha`, `beta`, `three four` (argc 3 or 4/5
  depending on whether the route synthesizes argv[0] — the record settles
  the convention empirically).
- **If the UI exposes a URL template instead**: the SDK-documented argument
  grammar is the percent-encoded `?args=` query its own deployer uses
  (`file:/path?args=Hello%2C%5C%20world%21`, `samples/hello_argv` Makefile);
  try it only through whatever input the manager actually provides — never
  invent a new route.
- **If no args field exists anywhere**: variant is N/A; document "no UI
  argument mechanism" — that absence is itself G1 evidence.

## Interpretation (settles G1)

| Observation | Verdict |
|---|---|
| Lines 1==2, `argc==0`, or `argc==1` with argv[0] = payload path and nothing else | **NO forwarding. G1 NO-GO for argv-driven installers.** Apply the fallback design note below. |
| Variant run (if performed) shows the entered tokens | Forwarding works via THAT exact mechanism; G1 GO conditional on using its verbatim grammar in the E2E `$SVC`. |
| File absent despite sha-verified upload + dispatch OK | Payload did not run or could not write `/data` — a distinct failure, NOT an argv verdict. Check the dispatch HTTP response and sink writability (INITONCE lesson: log paths must be under `/data`, FTP refuses `/tmp`). |
| Line1 != line2 | Nondeterminism; unusable for G1 until explained. |
| `argv_capped`/`str_trunc` true | The route passed >64 args or >64 B strings; unbounded-input risk must be noted in any G1-GO design. |

## Fallback design note (adopt on NO forwarding)

**E2E should adopt CONFIG-FILE-DRIVEN ENTRY**: upload
`/data/modbridge/intake/e2e-cmd.json` via FTP STOR (the exact route G3
gates) with `{"state":…,"roots":…,"verb":…,"args":[…]}`; `$SVC` collapses to
the bare `GET /loadpayload:<installer>` dispatch; the installer reads its
command from that file at startup. Chosen over a **baked-args wrapper**
(which compiles the argument string into a per-stage ELF): one payload
serves every stage instead of a new build/upload/alias/sha per command; the
command input is sha-verified by readback BEFORE dispatch; STOR intake is
already the E2E's own gated transport; and the installer-side change is the
same shape either way (read command from file vs argv). The config file
itself is argv-echo-probeable later if the route ever gains an args field.

## Collection summary (controller receipt)

Store run1/run2(/run3) JSONL, the upload readback sha, each dispatch's HTTP
response line, and the UI-args-field existence finding under
`outputs/2026-10-04-zcode-controller/console/` with an ARGV-probe receipt
JSON. We own only: the `ARGV_ECHO` payload alias and
`/data/modbridge-argv-probe.jsonl`. Nothing else is written or deleted.
