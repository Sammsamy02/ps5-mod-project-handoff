# REPORT — worker-e2e-payload-build, 2026-10-04

Task: build the ACTUAL revised console payload (config-entry revision) from
`worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md`, following its sec 8
sha-pinning steps with the SAME toolchain/flags/argv as the D1-fixed build
(`worker-d1-chain-relink/composition/receipts/console-build.json`), and enforce
the sec 8 gates: 116-import census re-proof, host-test binary byte-reproduction,
tuple updates, two-build determinism. CLAIM.json first; this report last.
No console contact, no target execution, no nested subagents; every write
inside `outputs/2026-10-04-zcode-team/worker-e2e-payload-build/`.

## DELIVERABLE

`build-r1/modbridge-installer.payload.elf`
sha256 `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`,
**681888 bytes**, ELF magic verified. Step P re-pin values and the exact
console dispatch sequence: `PAYLOAD-DELIVERY.md`.

## Gate ladder (all enforced with hard STOP on mismatch; script: run-gated-build.sh)

1. **8.1 Patch apply — PASS.** Pristine `45c04d08…` + `patch -p1` of the
   delivered diff → `src/modinst_core.c` = `43ea66c1…`, byte-exact vs
   `worker-e2e-config-entry/owned/modinst_core.c`. Note: the actual diff is
   **2 hunks** (+624/-1 counting the diff's own `+++`/`---` headers = +623/-0
   content), not the "3 hunks" the design sec 0 describes; the byte-exact
   apply-check is unaffected.
2. **Host-test binary byte-reproduction — PASS, byte-exact.** My host flavor
   build (D1 `host-build.json` compile argv + D1 host objects + pinned host
   closure) reproduces `51b88171…` (.o) and `4659c24c…`
   (`modinst_composed_cfgentry`) EXACTLY. This transfers config-entry's
   HOST-TEST-RECEIPT (PASS, 32 checks) to this source revision without re-run.
3. **Console toolchain reproduction — PASS.** Compiling the PRISTINE source
   with the exact D1 console argv reproduces D1's `modinst_core.ps5.o`
   (`dafd4214…`) byte-exactly (same 3 pre-existing warnings at 643/1365/1379,
   zero new). Linking the UNCHANGED D1 object set reproduces the D1 payload in
   **681512/681528** bytes.
4. **Revised builds r1/r2 — PASS.** Only `modinst_core.ps5.o` is replaced
   (mine); every other object/library/linkerscript is the D1 set. r1 =
   `a21771f8…` (681888 B), r2 = `ec8acdfc…` (681888 B).
5. **Census re-proof — PASS.** New payload: exactly **116** dynamic imports;
   `llvm-nm -D --undefined-only` output byte-identical to BOTH a fresh census
   of the D1 payload and the D1 `console-imports.stdout` receipt; no plain
   `strtol` (exact-symbol check — `strtold`/`strtoll` are pre-existing
   imports). missing=[] follows from name-set identity + D1
   `console-import-providers.json`. Dynamic layout unchanged in shape: 21
   entries, terminal DT_NULL, BIND_NOW.

## THE one honest finding: per-link random `.note.gnu.build-id`

The recorded D1 payload `e3489297…` **cannot be byte-reproduced by any
relink** — including D1 rebuilding itself. The linker emits a 16-byte
`.note.gnu.build-id` descriptor whose value is per-link random. Evidence: two
relinks of the IDENTICAL pristine inputs (this session,
`receipts/gate-console-repro/pristine-link{,2}/`) differ from each other in
exactly those 16 bytes, and each differs from `e3489297…` in exactly those 16
bytes; with the descriptors zeroed, all three images are **bit-identical**
(`BID` lines in the run log; `bidcmp` in the script). The determinism gate was
amended to this documented form, not bypassed: r1 vs r2 are identical in all
681872/681888 non-build-id bytes; the delivered artifact's sha covers its own
random bytes, so the controller must upload THESE exact bytes and re-pin the
sha from THIS file (never from a rebuild). Canonical content sha with the
descriptor zeroed: `c40346c0…` (receipts/r1-zeroed-buildid.sha).

## Tuple update (design sec 8.5)

`patches.json.new` (54 tuples; written INSIDE this dir per write scope —
promotion into `composition/receipts/` is the controller's step) + `g1_block.py`
(literal anchors, splicer-ready). Order: `g1-cmd-entry-parse`,
`g1-cmd-entry-allowlist`, `g1-cmd-entry-consume` appended directly after
`r2-install-status-ok`; `patched_sha256 = 43ea66c1…`. Content-truthful mapping:
parse = ej_* parser machinery; allowlist = e2e_refuse/e2e_verb_legal/
e2e_opt_flag (the per-verb option allow/deny legality); consume =
e2e_consume/e2e_cmd_override_path/e2e_cmd_entry + the main consult block (one
spanning unique anchor covers both regions). Each anchor verified
occurrences==1 in its pre-state; sequential application pristine → owned
byte-exact; independently re-verified from the delivered `g1_block.py`
literals. The design enumerates the ids as parse/consume/allowlist; the
application order above is required by textual contiguity and is documented in
the block header.

## Environment fact discovered

`prospero-clang` resolves the real toolchain through `LLVM_CONFIG`
(prospero-llvm-config line 21); the D1 composer sets it to the pinned
LLVM-21.1.8 `llvm-config`. Without it the SDK wrapper fails with
`/llvm-clang: No such file or directory`. Recorded in the build receipt so the
controller can reproduce the build.

## NOT run (out of my scoped gates; remains controller/staging work)

- Suite re-runs on the new chain (design sec 8.6: bridge 75 / F-R2 87 /
  fault 186 + the new config-entry cases). My host byte-reproduction
  transfers the existing 32-check receipt only.
- Any console STOR/readback/consumed behavior, the Step P identity-gate run,
  the patches.json promotion, and the schema §6/§8 amendment or waiver for the
  EX_CONFIG-6 delta. All console results remain [TARGET].

## Honesty

No gate was bypassed; the single deviation (build-id) is a measured property
with saved evidence, and the delivery doc pins the exact artifact bytes the
controller must upload. Never claimed: console behavior, AAA/full parity, or
completion of the E2E run. All writes stayed in this worker dir; D1
composition and all other worker dirs read-only.
