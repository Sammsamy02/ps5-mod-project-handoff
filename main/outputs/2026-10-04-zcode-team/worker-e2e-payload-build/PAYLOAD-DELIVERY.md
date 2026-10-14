# PAYLOAD-DELIVERY.md — worker-e2e-payload-build, 2026-10-04

The ACTUAL revised console payload (config-entry revision, RUN-CONFIG B reader)
built per `worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md` sec 8 with the
UNCHANGED D1 console recipe. Full gate evidence: `REPORT.md` +
`receipts/console-build-config-entry.json`.

## 1. Final payload

| | |
|---|---|
| Path | `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf` |
| sha256 | `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054` |
| Bytes | `681888` |
| ELF magic | `7f 45 4c 46` verified |
| Source chain | pristine D1 core `45c04d08…` + `worker-e2e-config-entry` diff (`4bab84ab…`) → core `43ea66c1…` (byte-exact vs owned) |
| Recipe | D1 `receipts/console-build.json` argv verbatim (prospero-clang, same `-I`/resource-dir/static closure/linkerscript `6704c97b…`); only `-c` source, `-o`, `-MF`, `-Map` live in this worker dir; `LLVM_CONFIG` env = D1 composer's |
| Determinism | r2 rebuild `ec8acdfc19161ac716e0a7b74a3b012230f39827b0dc58784c2b356c07df5fc3` (681888 B) — identical to r1 in **681872/681888** bytes; the ONLY variance is the per-link random 16-byte `.note.gnu.build-id` descriptor (proved, not assumed) |
| Zeroed-build-id canonical sha | `c40346c0268f2aede0f554697939b3587344f0b7db5ceb14896567d1b13537bd` |
| Census | 116 dynamic imports, set byte-identical to the D1 payload and to `console-imports.stdout`, no plain `strtol`; missing=[] by name-set identity + D1 `console-import-providers.json` |
| Size law | PT_LOAD prefix `606256` bytes (max Offset+FileSize = 0x94030, recomputed from THIS payload) + ELF magic |
| Host proof transfer | the host-test binary rebuilt this session byte-reproduces `4659c24c…` EXACTLY, so config-entry's HOST-TEST-RECEIPT (PASS, 32 checks) applies verbatim to this source revision |

IMPORTANT (build-id): any future relink of the same sources produces a
DIFFERENT sha (16 random note bytes). The controller must upload THESE exact
bytes (path above) and pin `CONFIG_ENTRY_SHA` to THIS file's sha. Never
re-derive the sha from a rebuild.

## 2. Step P re-pin (worker-e2e-staging-final/STAGED-E2E-COMMANDS.md §1)

Replace EXACTLY these two lines:

```
CONFIG_ENTRY_SHA=REPLACE_WITH_REPINNED_SHA_OF_CONFIG_ENTRY_REVISION   # RUN-CONFIG B only
  → CONFIG_ENTRY_SHA=a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054
CONFIG_ENTRY_BYTES=REPLACE_WITH_REPINNED_BYTES
  → CONFIG_ENTRY_BYTES=681888
```

`D1REL_SHA=e3489297…` / `D1REL_BYTES=681528` stay untouched as the MODE A
fallback. With these values the §3 identity gate resolves MODE=B only when the
staged readback is exactly these bytes, and can never pass MODE B with the
D1-only build.

## 3. Console dispatch sequence (controller; MODE B path from STAGED-E2E-COMMANDS.md)

Conventions already in that doc apply verbatim (`HTTP=http://ps5-host-not-configured.invalid:9223`,
`FTP=ftp://ps5-host-not-configured.invalid:2121`, `cmd_put`/`cmd_gone`/`dispatch`/`evid` helpers).
The ONLY change is `$PL`:

```sh
PL=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf
```

1. Step P upload (ONCE, alias unchanged):
   `curl -s -X POST --data-binary @"$PL" "$HTTP/manage:upload?filename=MODBRIDGE_INST_E2E.elf"`
2. Identity gate (unmodified): `manage:check` → FTP readback →
   `RB_SHA=a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`,
   `RB_SZ=681888` → `MODE=B` (else NO-GO; do NOT fall through to running cmd
   files against an unverified payload).
3. Per stage, in the §4 order: `cmd_put "$CMDDIR/<stage>.json"` →
   `dispatch` = `curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf"`
   → `cmd_gone` (consumed proof) → `evid` readback.
4. Expected report path: `FTP data/modbridge/state/e2e-status.jsonl`
   (downloaded by `evid` into `$EV/e2e-status.jsonl`; closing receipt copy
   `$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl`). MODE B additionally expects the
   consumed twin at `data/modbridge/intake/e2e-cmd.json.consumed` per dispatch,
   and — per the receipted deltas (design sec 7) — possible one-line
   `stage:"e2e-cmd" outcome:"refused"` events on refusals with exit 6.

Controller-flagged deltas before RUN-CONFIG B (design sec 7): EX_CONFIG 6 for
present-but-refused (schema §6/§8 say EX_BAD — needs ack or one-line schema
amendment), the refusal report line, the `--e2e-cmd FILE` override slot.

## 4. Tuple promotion (controller step; my writes stop at this dir)

`patches.json.new` (54 tuples) + `g1_block.py` (literal anchors) are delivered
HERE. `g1-cmd-entry-{parse,allowlist,consume}` sit directly after
`r2-install-status-ok` with `patched_sha256=43ea66c1…`; every anchor verified
occurrences==1 and the chain reproduces the owned core byte-exactly. Promotion
into `worker-d1-chain-relink/composition/receipts/` and any suite re-runs
(bridge 75 / F-R2 87 / fault 186 + the new config-entry cases, design sec 8.6)
remain controller/staging work — NOT run by this worker.

## 5. Honesty

No console contact, no target execution, no writes outside this worker dir.
The 16-byte build-id variance is an honest measured property of the pinned lld
link (two relinks of identical inputs differ in exactly those bytes; ALL other
bytes reproduce D1 bit-exactly). Console behavior remains [TARGET] until the
controller run above.
