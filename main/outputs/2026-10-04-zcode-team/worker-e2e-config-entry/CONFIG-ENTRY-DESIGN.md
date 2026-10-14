# CONFIG-ENTRY-DESIGN.md — e2e-cmd.json reader for modinst_core.c (G1 fallback)

Worker: worker-e2e-config-entry (FINISH pass), 2026-10-04.
Governing design: `worker-e2e-console-dry/E2E-CMD-SCHEMA.md` sec 1-8 (verbatim
grammar/allow-deny lists). Emulation reference:
`worker-e2e-staging-final/host-test/host_cmd_entry_test.py` (MODE B entry-rule
emulation that the C code mirrors line-for-line).
Origin of tasking: worker-e2e-staging-final found that the D1-fixed installer
payload sha `e3489297…` contains NO e2e-cmd.json reader — RUN-CONFIG B
(config-file dispatch) has nothing to run against. This revision supplies it.

## 0. Deliverables in this directory

| File | sha256 | What |
|---|---|---|
| `owned/modinst_core.c` | `43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0` | fully patched source, compile-ready |
| `patch/modinst_core-config-entry.diff` | (apply-check below) | unified diff vs pristine base |
| `host-test/host_cmd_entry_patched_test.py` | — | host validation through the COMPILED-IN entry |
| `host-test/HOST-TEST-RECEIPT.md` | — | PASS receipt, 32 checks |
| `host-test/build/modinst_core_cfgentry.o` | `51b881715d9a43f7c29886622f5d74becee5b38f02a93198f02549688c52ec40` | host .o, D1 receipt argv |
| `host-test/build/modinst_composed_cfgentry` | `4659c24c4a4d829dc13a59813e7b137bf4d0efeaf0bca45b21e064b589eb2aa1` | full host binary, D1 link argv |

- Pristine base: `worker-d1-chain-relink/composition/vendored/modinst_core.c`
  sha256 `45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d`
  (= `patches.json.patched_sha256`).
- Apply-check (done): `patch -p0` of `patch/modinst_core-config-entry.diff`
  onto a pristine copy reproduces the owned file BYTE-EXACT (sha
  `43ea66c1…`). Diff shape: +624/-1 lines, 3 hunks (entry machinery before
  `main`; two insertions inside `main`).

## 1. Trigger slots (schema sec 1)

1. **Bare dispatch (argc<2)** — the console loader forwards no argv. Reads the
   compile-time fixed path `E2E_CMD_PATH` = `/data/modbridge/intake/e2e-cmd.json`.
2. **Override slot `--e2e-cmd FILE`** — first occurrence wins, value required.
   Runs the IDENTICAL validate/consume/synthesize rule against FILE. Purpose:
   (a) host harness entry point (cannot write `/data` on the Mac); (b) a
   controller-side manual trigger. Today that string is an unknown flag
   (usage + EX_BAD), so no existing argv invocation changes meaning.
3. **Legacy precedence** — `argc>=2` WITHOUT the flag never consults the entry;
   a leftover cmd file stays untouched (schema sec 6 row 6; proven by test E).
4. `--e2e-cmd` without a value stays legacy grammar (unknown flag → usage +
   EX_BAD; proven by test G). `make-bundle` bypass predates the entry and is
   unchanged.

## 2. Exit taxonomy (controller-requested DELTA vs schema §6/§8)

| Situation | Exit | Note |
|---|---|---|
| dispatch ok | 0 | identical transcript to argv twin |
| no file at the slot | 2 (EX_BAD) | schema sec 1/6 verbatim: `say("error bad_request e2e-cmd: file absent")` + `usage()` — identical to today's bare `usage()` shape; also used for consume-rename failure (sec 6: `error e2e-cmd consume failed`) |
| file present but refused (BOM / oversize / non-UTF-8 / parse / unknown key / bad type / deny-listed verb or option / allow-list violations) | **6 (EX_CONFIG)** | DISTINCT refusal code — controller-requested delta. Schema §6 and §8(3) say EX_BAD for these; see §7 Deltas. Verb NEVER executes; file LEFT in place for controller inspection |

All refusal/absent paths RETURN from main (never the `exit()` macro — the
F-R2 `r2-exit-shim` routes exit() through `e2e_status_end`, and a refusal must
not leave a dangling end line). No begin line is ever emitted on a refusal.

## 3. Refusal report line (one line, existing report path)

On refusals where `state_root.state` itself validated (string, absolute,
≤ LINE_MAX_), exactly ONE line is appended to the existing
`<state_root.state>/e2e-status.jsonl` via the existing
`e2e_status_set_dir` + `e2e_status_event` API:

```json
{"event":"status","stage":"e2e-cmd","outcome":"refused","detail":"<reason>"}
```

No begin/end pair, no store mutation, verb never executed. Parse-level
failures (BOM/oversize/UTF-8/parse/bad schema/verb/state_root itself) have no
validated report dir and log one stdout line only. If the state dir is not
openable (console: dir not yet created; the refusal path never mkdirs), the
event write fails silently — same bounded emit() semantics as the rest of
e2e_status.c. The one-line report is proven by host-adapted refusal cases
(receipt §F: exactly 1 line, no `journal/`, no `records.txt`).

## 4. Consumption (schema sec 6 verbatim)

After validation passes, BEFORE falling into the parse/dispatch loop (thus
before `e2e_status_begin`): `ps5io_rename(e2e-cmd.json → e2e-cmd.json.consumed)`
in the same directory. Overwrite of a stale `.consumed` is legal. Rename
failure → `error e2e-cmd consume failed` + usage + EX_BAD, verb not executed.
Proven per dispatch (receipt §B: all 15 dispatches consumed=True).

## 5. Validate → synthesize (mechanical mapping, mirrors the host harness)

Same bounded JSON subset as `host_cmd_entry_test.py::rd_parse` (schema sec 5):
strict whole-file UTF-8; BOM refuses; >4096 bytes refuses pre-parse;
escapes only `\" \\ \/` (`\uXXXX` refuses); control chars refuse; object
depth ≤ 2; arrays only as top-level `mods`; `true` only (`false`/`null`
refuse); plain ints only (trailing `.eE+-` refuses); duplicate keys refuse.
Integers use a bounded sign-aware saturating decimal fold — see §6 imports.
Validation order and allow/deny lists are byte-for-byte the harness `validate()`:
top-level keys {schema,verb,state_root,mods,options}; schema==1 exactly;
verb allow-list {records, register-mod, prepare, prepare-archive, install,
simulate-runtime, recover}; deny {make-bundle, download}; state_root exactly
{state,roots}, absolute strings ≤ LINE_MAX_; mods ≤16 non-empty strings,
only with prepare/install; options ≤16 keys with per-key verb legality
(eager refusal), deny-list {crash_at,cancel_at,cancel_after,variant,
defaults,state_name,code,detail}, `no_auto_bundle` accepts bool true only,
other values non-empty strings ≤ LINE_MAX_; mods+options.remove refuses.

Synthesized argv (fixed key order = harness `synth_argv`):
`[prog, verb, --state S, --roots R, (--mods a,b), --title, --id, --bundle,
--archive, --quarantine, --mod, --remove, --rt-state, --choices, (--no-auto-bundle)]`
then `argc/argv` are REPLACED and dispatch falls into the EXISTING parse loop —
zero dispatch-path drift (schema sec 7 identity holds: proven byte-identical
35-line transcript, sha `31a4764a…`, same as staging-final's emulation run and
its argv twins). Bounds: ≤64 synthesized slots (`E2E_ARGV_MAX`); the parse
tree + raw buffer (one ≤4096-byte block) intentionally live until process exit
because the synthesized argv points into it.

## 6. Import census — delta ZERO (and the honest strtol incident)

Design rule: stdio/string/malloc/free/`read_file`/`say`/`ps5io_rename`/
`snprintf` and the e2e_status API only — every symbol already imported by the
D1-fixed console payload (116-import census,
`worker-d1-chain-relink/composition/receipts/console-imports.stdout`).

Incident: the PRIOR partial attempt (died mid-work) compiled the entry with
`strtol` for the JSON int branch; its saved census
(`host-test/build/cfgentry-undefined-syms.txt`) showed `+_strtol` vs pristine,
and plain `strtol` is NOT among the 116 console imports — that build would
have busted the delta-zero target. FINISH replaced it with a bounded
sign-aware saturating decimal fold (cap `0x0CCCCCCCCCCCCCCC`, no UB;
only `schema==1` is ever accepted, so a saturated value refuses with the
identical "schema must be exactly 1" line — observably equivalent to the
harness `int()` for every input).

Proof (this session, host):
- `nm -u` of patched .o vs pristine .o: 70 = 70 symbols, diff EMPTY
  (`build/pristine-undefined-syms.txt` vs `build/cfgentry-undefined-syms.txt`).
- `nm -u` of the linked patched binary vs the pristine D1 binary
  (`composition/build/host/modinst_composed`): 104 = 104, diff EMPTY; no
  `strtol` anywhere.
- Compile with the exact D1 `host-build.json` argv: exit 0, warnings = the
  SAME 3 pre-existing warnings as the D1 receipt (`host-compile-modinst_core.stderr`,
  lines 643/1365/1379 — `ids`/`pos`/`argc` in untouched code). ZERO new
  warnings. (CLAIM.json earlier said "0 warnings"; corrected: 3 pre-existing,
  zero new.)

Console re-proof remains the build worker's step (§8.2 of the schema): the
census must still read 116 dynamic imports, missing=[].

## 7. Controller-flagged deltas (decisions to ack, not silent)

1. **EX_CONFIG 6 for present-but-refused** — DELTA vs schema §6 table and
   §8(3) ("exit EX_BAD"). Deliberate, per the tasking that demands a distinct
   refusal code. `E2E-CMD-SCHEMA.md` §6/§8 and the STAGED §4 expected-exit
   notes need a one-line amendment (or an explicit controller waiver) BEFORE
   RUN-CONFIG B. absent-file and consume-failure remain EX_BAD 2 exactly as
   §6.
2. **Refusal report line** — a new `stage:"e2e-cmd"` status line shape on the
   existing report path, emitted BEFORE any begin line. Schema §7's
   "no schema drift" concerned dispatch begin/end; this adds one refusal
   event the controller should expect in RUN-CONFIG B evidence.
3. **`--e2e-cmd FILE` override slot** — new token in the argv grammar; today
   it is an unknown flag (usage + EX_BAD), so no legacy invocation changes
   behavior (test E/G prove argv precedence and untouched leftover files).
4. **Import census** — delta zero, but only after the §6 fix; the console
   116-import re-proof is still a REQUIRED re-pin step (schema §8.2).

## 8. Build-worker steps: new payload + sha pinning (schema §8)

1. Verify pristine base sha `45c04d08…`, apply
   `patch/modinst_core-config-entry.diff`, verify result sha `43ea66c1…`.
2. Recompile the console object with the EXACT console argv from
   `composition/receipts/console-build.json` (`prospero-clang -std=c11 -O2
   -Wall -Wextra -resource-dir … -c vendored/modinst_core.c -o
   build/console/modinst_core.o`); expect exit 0 and no NEW warnings beyond
   the D1/console receipts' 3 pre-existing ones.
3. Relink the console payload with the UNCHANGED console-link argv (all other
   objects/libraries exactly the D1 set).
4. Import re-proof: run the same census used for `console-imports.stdout` on
   the new ELF — must still be exactly 116 dynamic imports, missing=[] (§6).
5. Extend `composition/receipts/patches.json`: append tuple(s)
   `g1-cmd-entry-parse`, `g1-cmd-entry-consume`, `g1-cmd-entry-allowlist`
   AFTER `r2-install-status-ok` (chain 51 → N), with the new
   `patched_sha256 = 43ea66c1…`.
6. Re-run suites green on the new chain: bridge 75, F-R2 87, fault 186, PLUS
   the new config-entry cases (the host receipt here is the template: 15
   dispatches via the entry == argv twins byte-compare; 14-case refusal
   matrix; stale re-dispatch; bare-dispatch harmlessness; valueless flag).
7. New build receipt → payload sha256 + byte size → the ONLY manifest values
   the controller re-pins (next section). Never dispatch unverified bytes;
   never widen the identity gate.

## 9. Step P re-pin instructions (worker-e2e-staging-final/STAGED-E2E-COMMANDS.md)

The staging doc already contains the MODE B gate with placeholders. The build
worker (or controller) replaces EXACTLY two lines in `§1 Conventions`:

```
CONFIG_ENTRY_SHA=REPLACE_WITH_REPINNED_SHA_OF_CONFIG_ENTRY_REVISION
  → CONFIG_ENTRY_SHA=<payload sha256 from the NEW build receipt (step 8.7)>
CONFIG_ENTRY_BYTES=REPLACE_WITH_REPINNED_BYTES
  → CONFIG_ENTRY_BYTES=<byte size of the new payload ELF>
```

Nothing else in the doc changes: `D1REL_SHA=e3489297…` / `D1REL_BYTES=681528`
stay as the MODE=A fallback; §3 Step P's identity gate then resolves MODE=B
only when the staged-readback sha AND size both match the re-pinned values,
and can still never pass MODE B with `e3489297…`. New tuple IDs for the run
receipt remain `g1-cmd-entry-*` per §RUN-CONFIG B.

## 10. Bounds + honesty

- Writes confined to this worker dir; D1 composition sources/receipts and the
  13 staged cmd files (`SHASUMS.txt` verified) read-only; scratch state under
  `host-test/scratch/`; no console contact, no nested subagents.
- Everything above is HOST-side evidence on the patched binary through the
  `--e2e-cmd` slot (the designed host entry point). The compile-time
  `/data/...` slot itself, console STOR/readback/consumed behavior, suite
  re-runs, the new build receipt and the re-pin are the build worker's +
  controller's work; console results remain [TARGET] until that run.
