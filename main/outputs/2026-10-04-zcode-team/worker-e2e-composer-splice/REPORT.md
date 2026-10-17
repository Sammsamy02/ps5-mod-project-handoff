# REPORT.md — worker-e2e-composer-splice (2026-10-04)

Task: execute PROMOTION-PREP.md sec 5 step 4 — the COMPOSER SPLICE that
worker-e2e-promotion-apply honestly left undone. The promoted receipt
(`worker-d1-chain-relink/composition/receipts/patches.json`, 54 tuples,
`63c28979…`) pinned endpoint `43ea66c1…` while the vendored core was still
pristine `45c04d08…`, so a tree rebuild would NOT reproduce the promoted
state. That divergence is now closed — and provably stays closed under
rebuild. No console contact, no nested subagents. All writes: this worker dir
+ the explicitly in-scope `worker-d1-chain-relink/` composition tree.

## 1. What was spliced (D1 builder pattern, AST verbatim)

`g1_composer_splice_builder.py` extracted `G1_BLOCK` VERBATIM (AST) from
`worker-e2e-payload-build/g1_block.py` (`9f46b66aeb38a1d4…`, == the
suite-completion promotion copy), verified all 3 tuples against the promoted
receipt's tail pins (occurrences==1, old/new shas), and appended recorded
`MY_SPLICES` entries to the tree driver `compose_merged_relink.py`
(`aedcd8eaf899bd20…` → `dd5fb4283abc8fba…`):

| splice | what |
|---|---|
| `g1-block-append` | generator `EXTRA_PATCHES +=` the 3 G1 4-tuples after `r2-install-status-ok` (19 EXTRA total → 35 base + 19 = 54 applied in order) |
| `g1-receipt-promoted-form-def` / `-seq` | three anchor splices in `adapt_script`'s loop so the REGENERATED `composition/receipts/patches.json` byte-matches the promoted form produced by `worker-e2e-payload-build/g1_tuples_build.py` |

The three receipt-form fixes (each a recorded, count==1-anchored splice):
**(a)** trailing newline after `json.dumps(receipt, indent=2)` — the promoted
file has one, `patch_modinst` wrote none; **(b)** the promoted `patched`
label (`worker-e2e-payload-build/src/modinst_core.c (config-entry revision;
= worker-e2e-config-entry owned/modinst_core.c)`) — the rebuilt
`composition/vendored/modinst_core.c` is byte-identical to exactly that
revision; **(c)** UTF-8 BYTE lengths for the G1 tuples' `new_bytes` (consume
new = 12862 B — contains one em-dash — vs 12860 chars), keeping the inherited
51-tuple prefix's delivered char-length convention untouched.

Provenance is recorded by the tree's own receipts: `receipts/
console-bridge-script-patches.json` (driver level: the 3 new MY_SPLICES) and
`e2e-receipts/script-patches.json` (generator level: the promoted-form
splices), plus `worker-e2e-composer-splice/receipts/
g1-splice-provenance.json`.

## 2. Dry run caught real divergence BEFORE any tree write

The builder first simulates the ENTIRE chain in-memory/sandbox: spliced
driver exec → `adapt_driver` (ZB replay-proof `897cf0eb…` re-verified) →
generator exec (EXTRA_PATCHES 19, tail = G1 ids) → `adapt_script`
consolidation (54-tuple inner) → 54-tuple sequential core rebuild from
worker-7's pristine `modinst.c` → sandbox `patch_modinst` receipt. Run 1
FAILED the receipt check: without fixes (b)/(c) the regenerated receipt was
`78befccf…` — content-different from the promoted `63c28979…`. A naive
newline-only splice would have silently traded the old divergence for a new
one. After the recorded fixes: `FULL DRY RUN: PASS` — core chain tail
`45c04d08 → 330d47f2 → 91a122a8 → 43ea66c1` (the verified G1 chain, in-tree),
final == owned config-entry revision byte-exact, sandbox receipt == promoted
bytes.

## 3. Rebuild (the tree's own build, exact D1 recipe) + reconciliation

`MODBRIDGE_ROOT=<what-root> python3 compose_merged_relink.py --flavor all`,
cwd = the tree (the builder's own invocation form). Note: the first apply-run
build died at `link_layout.write_linker_script`'s exclusive-create against
STALE D1-era `build/console/` artifacts — vendor()+patch_modinst() had
already run. Stale `build/console/` (23 files incl. the D1 ELF `e3489297…`,
sha-listed) was moved ASIDE into this worker's `archive/
stale-51chain-build-console/` (nothing deleted) and the build re-run clean:

- **BUILD exit=0**; all 14 host compile+link commands exit 0
  (`composition/receipts/host-build.json`).
- Console payload ELF `composition/build/console/modbridge-installer.payload.elf`
  = `b0d73e93de55a0d55a35e19995a6ee0f00eaedf32e05130636ae7bcfb9476598`
  (681888 B), **116 dynamic imports, missing=[]** (provider check re-run),
  ELF sha re-verified on disk vs `console-import-providers.json`.
- Host regression binary `composition/build/host/modinst_composed` =
  `bbc7f96f4445a32401b0b05eaf986452e87678e3889f74e181e07cd75695f3d7`
  (matches `host-build.json`).
- Regenerated inner: `composition_zb_console_inner.py`
  `de3a4a3d… → abab60d38fe422ad…` (54-tuple consolidated form).

**Reconciliation (all three agree):**

| party | value |
|---|---|
| promoted receipt (installed by promotion-apply; unchanged) | `63c28979d21a9a56…` / endpoint `43ea66c1…` |
| spliced tree's regenerated receipt | `composition/receipts/patches.json` = `63c28979…`, **byte-identical** to `worker-e2e-payload-build/patches.json.new` (cmp) |
| byte-exact rebuild | `composition/vendored/modinst_core.c` = `43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0`, **byte-identical** to `worker-e2e-config-entry/owned/modinst_core.c` (cmp); full 54-tuple chain replayed in-tree with occurrences==1 everywhere |

Divergence closed (`receipts/06-post-verification.log`, all POST PASS).

## 4. Change scope (full-tree sha diff, BEFORE 402 / AFTER 404 files)

Exactly 15 changed/new files (`receipts/05-inventory-diff.txt`): the spliced
driver (+ its in-tree `.51chain.bak`), the vendored core (`45c04d08 →
43ea66c1`), the regenerated inner, the build artifacts for BOTH flavors
(advanced to the promoted 54-chain revision; stale 51-chain artifacts
archived aside with shas), the core's `.o` files, and 4 provenance receipts.
`composition/receipts/patches.json` appears in NEITHER delta side
(byte-stable at `63c28979…` — promoted before this worker started,
reproduced byte-exactly by the rebuild). Every other vendored input
regenerated byte-identically (modinst.c, ps5_prepare_bridge.c, closure, owned).

## 5. Honest scope

- The G1 core bytes were ALREADY suite-proven (`worker-e2e-suite-completion/
  SUITE-RESULTS.md`, run against the patch-apply form of the same bytes).
  This worker proves TREE REPRODUCIBILITY of those bytes — no new
  gameplay/acceptance claims, no console, no new suites run.
- The tree's build/ artifacts now hold the promoted 54-chain revision; the
  D1-era payload `e3489297…`/host `8dffe3cf…` pins remain in their own
  receipts and archived artifacts. STAGING-MANIFEST-REPIN.md and RUN-CONFIG B
  staging references were NOT touched — re-staging/re-pin is a separate
  controller decision (the new revision is proposal-complete here, with
  full build receipts).
- First apply attempt: one builder bug (path-% precedence) crashed BEFORE any
  tree write; the interrupted build left the tree at an intermediate
  (already-correct core+receipt) state, completed and verified by
  `finish_rebuild_verify.py`. All raw transcripts preserved.

## 6. ROLLBACK instructions

Archives: in-tree `worker-d1-chain-relink/compose_merged_relink.py.51chain.bak`
(= `aedcd8ea…`) and `worker-d1-chain-relink/composition/receipts/
modinst_core.c.45c04d08.pristine.bak` (74749 B); worker-dir
`archive/compose_merged_relink.pristine-aedcd8ea.py`,
`archive/composition_zb_console_inner.pristine-de3a4a3d.py`,
`archive/modinst_core.pristine-45c04d08.c`,
`archive/stale-51chain-build-console/` +
`archive/stale-51chain-build-artifacts-shasums.txt`.

1. Undo the SPLICE only (tree back to the promotion-apply end state — 54-tuple
   receipt, pristine 51-chain core):
   ```sh
   cd .../2026-10-04-zcode-team/worker-d1-chain-relink
   cp -p compose_merged_relink.py.51chain.bak compose_merged_relink.py
   shasum -a 256 compose_merged_relink.py        # expect aedcd8eaf899bd20...
   cp -p composition/receipts/modinst_core.c.45c04d08.pristine.bak \
         composition/vendored/modinst_core.c      # expect 45c04d08...
   cp -p ../worker-e2e-composer-splice/archive/composition_zb_console_inner.pristine-de3a4a3d.py \
         composition_zb_console_inner.py          # expect de3a4a3dbaaf7200...
   # optional: restore the D1-era console build dir (shas in
   # worker-e2e-composer-splice/archive/stale-51chain-build-artifacts-shasums.txt;
   # host binary 8dffe3cf... reproduces via the restored driver, --flavor host)
   rm compose_merged_relink.py.51chain.bak \
      composition/receipts/modinst_core.c.45c04d08.pristine.bak
   ```
   Verify: re-run `shasum -a 256` checks above; the promoted receipt
   (`63c28979…`, 54 tuples) is left untouched — that state is exactly the
   honestly-divergent post-promotion state this worker started from.
2. Undo PROMOTION + SPLICE together (full pre-promotion frozen tree): do step 1,
   then the receipt rollback of `worker-e2e-promotion-apply/REPORT.md` sec 8
   (restore `composition/receipts/patches.json` from `patches.json.51.bak`
   `252ad699…`, 51 tuples, and remove the backup), then rebuild via the
   restored 51-chain driver (`--flavor all`) and diff against
   `worker-e2e-composer-splice/receipts/02-before-inventory.txt` — the
   only acceptable residual deltas are the two in-tree `.bak` removals and
   build artifacts reproducing the archived 51-chain shas
   (`e3489297…` / `8dffe3cf…`).
3. The receipt-form splices (a/b/c) revert together with the driver restore —
   they live only in the spliced `compose_merged_relink.py`.

## 7. Artifacts in this worker dir

| file | what |
|---|---|
| `CLAIM.json` | claim (written FIRST; status DONE + result) |
| `g1_composer_splice_builder.py` | the splice builder (dry-run + --apply; gates, AST verbatim extraction, pin checks, full chain simulation) |
| `finish_rebuild_verify.py` | completes the interrupted rebuild: stale-build archive, re-run, inventory diff, post checks |
| `receipts/00-builder-transcript.log`, `01-dry-run-proof.log` | dry-run gates incl. the caught receipt-content divergences |
| `receipts/g1-splice-provenance.json` | splice + apply provenance (anchors, pins, chain, final shas, changed-file list) |
| `receipts/02-before-inventory.txt` / `04-after-inventory.txt` / `05-inventory-diff.txt` | full-tree BEFORE/AFTER sha inventories + 15-line diff |
| `receipts/03-build-all.log` | the tree's own build transcript (flavor=all, exit 0) |
| `receipts/06-post-verification.log` | 10/10 POST PASS + reconciliation statement |
| `receipts/apply-stdout.log`, `finish-stdout.log` | raw apply/finish transcripts (incl. the honest failures) |
| `dryrun/` | sandbox outputs of the dry-run `patch_modinst` (core `43ea66c1…`, receipt `63c28979…`) |
| `archive/` | pristine driver/inner/core + stale 51-chain build console (moved aside, sha-listed) |
