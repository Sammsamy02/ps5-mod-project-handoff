# PROMOTION-PREP.md — patches.json 51 -> 54 promotion package (PREPARED, NOT APPLIED)

Worker: worker-e2e-suite-completion, 2026-10-04. Per tasking and
`worker-e2e-config-entry/CONFIG-ENTRY-DESIGN.md` sec 8.5, the controller owns
the frozen `worker-d1-chain-relink/composition/` tree — NOTHING was applied.
This document + `promotion/` hold the ready-to-promote delta, placement, and
verification evidence.

## 1. What changes (exact)

| | current (`worker-d1-chain-relink/composition/receipts/patches.json`) | promoted (`patches.json.new`) |
|---|---|---|
| tuples | 51 | 54 |
| prefix (first 51 ids+payloads) | — | BYTE-IDENTICAL, same order (verified programmatically) |
| appended tail | — | `g1-cmd-entry-parse`, `g1-cmd-entry-allowlist`, `g1-cmd-entry-consume` (in that order, directly after `r2-install-status-ok`) |
| `source` / `source_sha256` | `composition/vendored/modinst.c` / `ccdd3e35…` | unchanged |
| `bridge` | ps5_prepare_bridge `da770544…` | unchanged |
| `patched` | `composition/vendored/modinst_core.c` | `worker-e2e-payload-build/src/modinst_core.c (config-entry revision; = worker-e2e-config-entry owned/modinst_core.c)` |
| `patched_sha256` | `45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d` | `43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0` |

Placement follows design sec 8.5 verbatim ("append … AFTER
`r2-install-status-ok`"). Tuple shape (`id/occurrences/old_sha256/new_sha256/
old_bytes/new_bytes`) is identical to the existing receipt; tuple shas are
ANCHOR-LITERAL shas (e.g. `r2-exit-shim` old 44 B), same semantics as the
current 51 — confirmed by cross-check below.

Order note: design sec 8.5 lists the ids as "parse, consume, allowlist" but
the DELIVERED file order is parse -> allowlist -> consume. This matches
`g1_block.py`'s header ("Sequential unique-anchor replacements (order
matters): parse -> allowlist -> consume") and PAYLOAD-DELIVERY.md sec 4. The
delivered order is the verified-application order; do NOT reorder.

## 2. Delta tuples (from `patches.json.new`)

| id | occurrences | old anchor | old sha256 | new block | new sha256 |
|---|---|---|---|---|---|
| g1-cmd-entry-parse | 1 | `int main(int argc, char **argv) {` (33 B) | `50a408c16213ef21…` | 11519 B parser machinery (ej_*) | `5e54df659ffdc079…` |
| g1-cmd-entry-allowlist | 1 | same 33 B anchor (pre-state) | `50a408c16213ef21…` | 1804 B e2e_refuse/e2e_verb_legal/e2e_opt_flag | `36db9f7d78928d27…` |
| g1-cmd-entry-consume | 1 | 630 B anchor | `089098a7074b7f01…` | 12862 B consume/override/main-consult | `2cf5e204dfb0fd72…` |

## 3. Independent verification (this worker, this session)

Log: `promotion/g1-chain-replay-verify.log`; replayed source:
`promotion/g1-chain-replay-core.c`.

1. Sequential replay of `G1_BLOCK` (imported VERBATIM from
   `worker-e2e-payload-build/g1_block.py`, sha `9f46b66a…` as copied) onto a
   copy of the pristine 51-tuple core `45c04d08…`:
   `45c04d08… -> 330d47f2… (86235 B) -> 91a122a8… (88006 B) -> 43ea66c1… (100238 B)`.
2. Every anchor occurrences==1 in its pre-state (asserted, replay aborted on
   any != 1).
3. Final replay == `worker-e2e-config-entry/owned/modinst_core.c`
   BYTE-EXACT (cmp true).
4. All 6 literal shas + 4 byte counts in `patches.json.new` match the
   `g1_block.py` literals (LITERAL-LEVEL CROSS-CHECK: OK).
5. First 51 tuples of `patches.json.new` identical to the current receipt
   (ids, order, payloads).

This independently reproduces the delivery's own gate
(`worker-e2e-payload-build/receipts/gate-tuples-verify.log`: "sequential
application == owned bytes: True").

## 4. Artifacts in `promotion/` (copies; originals untouched in
worker-e2e-payload-build/)

| file | sha256 | what |
|---|---|---|
| `patches.json.new.copy` | `63c28979d21a9a56…` | the 54-tuple receipt to promote |
| `g1_block.copy.py` | `9f46b66aeb38a1d4…` | literal anchors backing the 3 tuples |
| `g1-chain-replay-core.c` | `43ea66c1c5dcf52b…` | replayed chain endpoint (== owned) |
| `g1-chain-replay-verify.log` | `ad191bc7d01521e7…` | verification transcript incl. the (corrected) cross-check note |
| `apply-promotion.sh` | (see file) | OPTIONAL controller-run script: re-verifies every sha/hash THEN installs the receipt (backup + verify-after). Not executed here. |

## 5. Controller application steps (NOT run by this worker)

Minimal (receipt promotion, design sec 8.5):

1. Verify the promoted file's identity BEFORE installing:
   `shasum -a 256 worker-e2e-payload-build/patches.json.new` must equal
   `63c28979d21a9a563f29334cd094568f11fd6cc2d56abfe31158ec14a42c80f9`, and it
   must be byte-identical (cmp) to
   `worker-e2e-suite-completion/promotion/patches.json.new.copy`.
2. Verify the frozen pre-state BEFORE replacing:
   `worker-d1-chain-relink/composition/receipts/patches.json` still 51 tuples,
   `patched_sha256=45c04d08…`; `composition/vendored/modinst_core.c` still
   `45c04d08…` (unless the controller ALSO promotes the source, step 4).
3. Back up the old receipt (e.g. `patches.json.51.bak` inside the same tree),
   then install `patches.json.new` as
   `worker-d1-chain-relink/composition/receipts/patches.json`; re-verify 54
   tuples + `patched_sha256=43ea66c1…` after install.
4. Recommended (tree reproducibility, matches the D1 splice pattern):
   also splice `G1_BLOCK` into the composer's `EXTRA_PATCHES`
   (`composition_zb_console_inner.py`) exactly the way
   `worker-d1-chain-relink/compose_merged_relink_builder.py` spliced
   UF_BLOCK/R2_BLOCK (AST verbatim extraction), and regenerate
   `composition/vendored/modinst_core.c` to `43ea66c1…`. The receipt alone
   records the chain; the spliced composer is what makes the tree rebuild
   byte-reproducibly. This can be a separate controller step; the suites in
   SUITE-RESULTS.md were run against the patch-apply form of the same bytes.
5. Only after 1-3: the controller may treat `patches.json.new`'s
   `patched_sha256=43ea66c1…` as the pinned chain endpoint everywhere
   (staging manifest notes, RUN-CONFIG B receipt references).

Honesty: nothing outside `worker-e2e-suite-completion/` was written; the
frozen tree is untouched; the promotion is a prepared, fully verified
package.
