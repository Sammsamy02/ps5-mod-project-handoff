# RUNCONFIG-B-RESULTS.md — worker-e2e-runconfig-b, 2026-10-04

Host-side execution of RUN-CONFIG B against the PROMOTED tree. No console
contact, no target execution, no nested subagents. Raw transcripts:
`logs/01…08-*.log` in this directory.

## 0. What RUN-CONFIG B is (precise definition)

**RUN-CONFIG B = the MODE B (config-file-entry) E2E dispatch sequence** of
`worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` (§1 conventions + §2
pre-flight + §3 Step P identity gate + §4 dispatch order + §5 Step C evidence
+ §6 reset), which materializes `worker-e2e-console-dry/CONSOLE-E2E-EXECUTION.md`
+ `worker-e2e-console-dry/E2E-CMD-SCHEMA.md` as one copy-paste script.
Mechanically: 13 sha-verified command JSON files are STORed one at a time to
`/data/modbridge/intake/e2e-cmd.json`, the payload is bare-dispatched via
`GET $HTTP/loadpayload:…/MODBRIDGE_INST_E2E.elf`, the installer's compiled-in
`e2e_cmd_entry()` reader consumes the file (atomic rename to
`.consumed` BEFORE any verb runs) and synthesizes the argv vector; per-dispatch
proof = `cmd_gone` (original gone + `.consumed` hash-equal) + `evid` readback
of `e2e-status.jsonl`/`records.txt` checked against the §4 verbatim blocks
(35-line corrected ledger). Stage 6 order fixed mm-lua → mm-skin → movspeed;
stages 4-5 are SIMULATED ack (labeled, not runtime acceptance).

**Hard gate (§0):** MODE B is lawful ONLY against the re-pinned config-entry
payload revision (`CONFIG_ENTRY_SHA`/`CONFIG_ENTRY_BYTES`); it can NEVER pass
against the D1-fixed argv-flavor build `e3489297…` (681528 B), which has no
cmd reader. `MODE=A` (same stage order via argv grammar) is the fallback for
that build.

**Preconditions (E2E-CMD-SCHEMA §8 + STAGED §0):** (1) implementing revision
exists with distinct tuple IDs appended after `r2-install-status-ok`;
(2) imports re-proven 116, missing=[]; (3) suites re-run green (bridge 75,
F-R2 87, fault 186) PLUS config-entry host cases; (4) new build receipt +
STAGING re-pin values; (5) 13 staged cmd files byte-exact under
`worker-e2e-staging-final/cmd/`.

**A PASS produces (§5):** `$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl` +
`-records.txt` copies, on-console bridge-artifact sha, `cmdfiles.log`, and a
receipt JSON with dispatch list (mode + cmd-file sha per dispatch), payload
sha/bytes, readback shas, per-stage pass/fail vs §4, the SIMULATED label on
4-5, the stage-6 CORRECTION reference, F-R2 notes — 35/35 lines, 0 unmatched
begins, 0 failed ends.

## 1. Execution form

**PROMOTED-RECEIPT + VENDORED-PRE-SPLICE.** `worker-e2e-composer-splice` has
NOT delivered (no such directory under `2026-10-04-zcode-team/` at run time;
verified). `composition/vendored/modinst_core.c` is still the pristine
51-tuple core `45c04d08…` (Step 2 below) — promotion changed the RECEIPT only
(promotion-apply REPORT §5.3/§7: the splice is an explicit separate controller
step). The build-r1 payload was built from the patch-apply form of the SAME
bytes (`43ea66c1…`), so the splice does not change these payload bytes; it only
lets future tree rebuilds reproduce them. A post-splice re-run of Steps 1-5
would be expected to produce identical results.

## 2. Per-step verdicts (host-executable portion)

### STEP 1 — §2 pre-flight: 13 staged cmd files — **PASS**
`shasum -a 256 -c SHASUMS.txt` → **13 OK, 0 failed, exit 0** (log `01`).
Files match `SHASUMS.txt` exactly; §2's expected-sha block in STAGED matches
line-for-line (recomputed shas in log `01`). Nit, no gate impact: the STAGED
header says SHASUMS.txt carries "NO trailing newline"; the actual file ends
`0x0a` (log `01`). The 13 dispatch JSONs themselves are newline-free compact
bytes (STEP 3), which is the claim that matters.

### STEP 2 — promoted receipt + vendored state — **PASS** (pre-splice form)
(log `02`)
- `receipts/patches.json` sha256 = `63c28979d21a9a563f29334cd094568f11fd6cc2d56abfe31158ec14a42c80f9`,
  `cmp`-identical to `worker-e2e-payload-build/patches.json.new`.
- Structure: 54 tuples; tuple 51 (last unchanged prefix) = `r2-install-status-ok`;
  tail = `g1-cmd-entry-parse`, `g1-cmd-entry-allowlist`, `g1-cmd-entry-consume`
  (delivered order); `patched_sha256 = 43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0`
  (== expected). Bridge/source fields unchanged.
- Vendored core: `45c04d08…` (74749 B) — pristine, splice NOT applied.
- `patches.json.51.bak` present (rollback anchor per promotion-apply §8).

### STEP 3 — schema grammar validation of the 13 staged bytes — **PASS 13/13**
(log `03`) Each file checked against `E2E-CMD-SCHEMA.md` §2/§3/§5/§6: size
≤4096, no BOM, single-line ASCII, `schema==1`, verb allow-list, exact
`state_root`, mods rules (legal only for prepare/install; never with
`options.remove`), options keys within the §3 map and legal for the verb,
deny-list flags absent — AND byte-equality vs the §4 exact compact templates:
**13 pass / 0 fail, validator exit 0.** (Independent of, and consistent with,
the 14/14 refusal-matrix host proof in HOST-TEST-RECEIPT.)

### STEP 4 — §3 identity gate, host form — **PASS → MODE=B resolvable**
(log `04`)
- `build-r1/modbridge-installer.payload.elf`: sha256
  `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`, 681888 B,
  magic `7f 45 4c 46` — **exactly** the re-pin values (PAYLOAD-DELIVERY §2,
  receipt `step_P_repin`). Gate arithmetic: `MODE=B`.
- Determinism twin `build-r2` = `ec8acdfc…` (681888 B); r1 vs r2 differ in
  **exactly 16 contiguous bytes** (offsets 605681–605696 — the per-link
  `.note.gnu.build-id`), matching the receipt's documented exception;
  zeroed-build-id canonical sha `c40346c0…` matches the receipt pin.
- MODE-A fallback D1 payload still present: `e3489297…` / 681528 B.
- On-console gate resolution stays [TARGET]: requires upload + FTP readback.

### STEP 5 — independent G1 replay on the promoted tree — **PASS**
(log `05`; re-run of `worker-e2e-promotion-apply/g1_replay_promoted_tree.py`,
read-only on the tree)
`45c04d08… (74749 B) → g1-cmd-entry-parse → 330d47f2… (86235 B) →
g1-cmd-entry-allowlist → 91a122a8… (88006 B) → g1-cmd-entry-consume →
43ea66c1… (100238 B)`; every anchor occurrences==1; final == promoted receipt
endpoint; byte-identical to `promotion/g1-chain-replay-core.c`; all 3 tail
tuples' literals == G1_BLOCK. `G1-REPLAY-ON-PROMOTED-TREE: PASS`, exit 0.

### STEP 6 — precondition ledger (schema §8)
| precondition | evidence | verdict |
|---|---|---|
| §8.1 tuples appended, receipt extended | STEP 2 + STEP 5 | MET |
| §8.2 imports 116, missing=[] | `console-build-config-entry.json` gates.census_116 PASS (set byte-identical to D1 census; no plain strtol) | MET |
| §8.3 suites re-run green + config-entry cases | `worker-e2e-suite-completion/SUITE-RESULTS.md` on the build-r1 chain, host binary `4659c24c…` byte-exact: bridge75 75/0, F-R2-87 87/0, fault-186 186/0, config-entry suite OVERALL PASS (46 OK check lines, 0 bad; 15 dispatch replays == argv twins, transcript sha `31a4764a…`; refusal matrix 13x exit 6 + absent 2) | MET |
| §8.4 new build receipt + STAGING re-pin values | `receipts/console-build-config-entry.json` (all gates PASS); re-pin values delivered in PAYLOAD-DELIVERY §2 (`a21771f8…`/`681888`) | MET (values delivered; see STEP 7 note on the STAGED placeholders) |
| 13 staged cmd files byte-exact | STEP 1 + STEP 3 | MET |
| EXCONFIG6 exit-6 delta acked | schema amendment applied by promotion-apply (`E2E-CMD-SCHEMA.md` §1/§6/§8(3) + STAGED §4 mirror); suite 4 confirms implemented 13x exit 6 | MET |

### STEP 7 — re-pin substitution state (finding, no action taken)
`STAGED-E2E-COMMANDS.md` §1 lines 49-50 STILL contain the
`REPLACE_WITH_REPINNED_…` placeholders (log `06`). The values ARE delivered
and independently recomputed here; the **substituted, operative paste-block
already exists** in `worker-e2e-console-stage-prep/CONSOLE-STAGE-PLAN.md` §2
(8084 conventions + `CONFIG_ENTRY_SHA=a21771f8…`, `CONFIG_ENTRY_BYTES=681888`,
`$PL` → build-r1). I did NOT edit the staged doc — the literal substitution is
the controller's §1 edit; no gate depends on it once the stage-prep block is
used.

### STEP 8 — port erratum (adjudicated upstream, verified here)
PAYLOAD-DELIVERY §3 says `HTTP=…:9223`; every proven source (console-context
manager endpoint, STAGED §1, CONSOLE-E2E-EXECUTION §1, batch runner) pins
**8084** — `worker-e2e-console-stage-prep` documented this erratum; the batch
runner acks it in-run ("port 8084 pinned; 9223 never used"; if 8084 does not
answer, STOP — no alternate-port probing).

### STEP 9 — batch-runner host-side rehearsal (cited, not re-run)
`worker-console-batch-runner/runs/fake-drive/e2e/` (2026-10-04 22:04): the
batch runner drove the ENTIRE RUN-CONFIG B flow against a fake drive —
**15/15 dispatches PASS** (d01–d15, per-stage `e2e-verify-d*.json`), final
ledger **35/35 lines, 0 failed ends, 0 refusal lines**, mode gate resolved
MODE=B (`mode-gate.json`), receipt `MODBRIDGE_INST_E2E-receipt.json` carries
the SIMULATED label, stage-6 CORRECTION and F-R2 notes, erratum ack, and §7
reset results. **This is rehearsal evidence, NOT console evidence.**

## 3. [PENDING user console session] — the console-bound remainder

Everything requiring console dispatch/consumption STAYS PENDING. The exact
sequence (already substitution-complete) is
`worker-e2e-console-stage-prep/CONSOLE-STAGE-PLAN.md` §2–§5 + §Step C, driven
by `worker-console-batch-runner/console_batch.py` (flow `e2e`). In outline,
with the conventions block from CONSOLE-STAGE-PLAN §2 (`HTTP=…:8084`,
`FTP=…:2121`, `CONFIG_ENTRY_SHA=a21771f8…`, `CONFIG_ENTRY_BYTES=681888`,
`$PL=…/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf`) and
the §1 helpers `cmd_put`/`cmd_gone`/`dispatch`/`evid` verbatim from STAGED §1:

1. **Pre-flight (host, §3):** `shasum -c SHASUMS.txt` (13/13 — re-run on the
   console host), `shasum "$PL"` = `a21771f8…`, size 681888, ELF magic.
2. **Step P (§4):** `curl -s "$HTTP/list_payloads"` (G0; else STOP) →
   `curl -s -X POST --data-binary @"$PL" "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"`
   (ONCE) → `manage:check` → FTP readback → identity gate → **MODE=B** else
   NO-GO/STOP.
3. **Dispatch sequence (§5, exact order, cycle = cmd_put → dispatch →
   cmd_gone → evid N):** movspeed.zip STOR probe (sha `3850a2b5…`) →
   stage0-records → stage1-register → stage2a-prepare-archive → stage2b-1/2/3
   → stage3-install → stage4-activated → stage0-records → stage5-mods-started
   → stage0-records → stage6-1/2/3-remove → closing stage0-records.
   Expected lines per STAGED §4 (verbatim blocks; exit-6 refusal semantics per
   the amendment; stage-6 CORRECTION expectation).
4. **Step C (§5 STAGED):** evidence copies, on-console `movspeed.mfb` sha,
   `cmdfiles.log`, receipt JSON with per-stage verdicts — 35/35, 0 unmatched
   begins, 0 failed ends.
5. **§6 reset** only if aborting before stage-3 commits; rollback ownership
   strictly `/data/modbridge/**` + alias `MODBRIDGE_INST_E2E`.

## 4. Verdict summary

| # | step | verdict |
|---|---|---|
| 1 | §2 pre-flight 13 cmd files | PASS (13/13, exit 0) |
| 2 | promoted receipt + vendored state | PASS (54 tuples, endpoint match; pre-splice form) |
| 3 | schema grammar validation | PASS (13/13, incl. §4 byte-equality) |
| 4 | identity gate host form | PASS (MODE=B resolvable) |
| 5 | independent G1 replay | PASS |
| 6 | precondition ledger (schema §8) | MET (all five) |
| 7 | STAGED placeholder substitution | OPEN (values delivered; substituted block exists in stage-prep plan; doc edit = controller) |
| 8 | port erratum | adjudicated: 8084 pinned, 9223 never used |
| 9 | console dispatch/consumption/Step C | **[PENDING user console session]** (batch runner ready; fake-drive rehearsal 15/15) |

RUN-CONFIG B is **host-ready and dispatch-ready**: every host-executable gate
re-verified green this session with fresh receipts, and the sole remaining
work is inherently console-bound.
