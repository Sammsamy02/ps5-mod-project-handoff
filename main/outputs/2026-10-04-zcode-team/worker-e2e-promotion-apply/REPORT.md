# REPORT.md — patches.json 51->54 promotion APPLIED + EXCONFIG6 schema amendment

Worker: worker-e2e-promotion-apply, 2026-10-04 (UTC timestamps in logs are
2026-10-05Z; local date 2026-10-04). Applied the package prepared (and NOT
applied) by `worker-e2e-suite-completion/PROMOTION-PREP.md` to the frozen
`worker-d1-chain-relink/composition/` tree, plus the EXCONFIG6 memo's schema
amendment. No console contact, no nested subagents.

## 1. Input re-verification (before any write)

All gates of `worker-e2e-suite-completion/promotion/apply-promotion.sh`
re-verified independently first (log `inventory/00-input-verification.log`):

| input | required | actual |
|---|---|---|
| `worker-e2e-payload-build/patches.json.new` sha256 | `63c28979d21a9a56…80f9` | MATCH |
| == `promotion/patches.json.new.copy` (cmp) | identical | MATCH |
| `promotion/g1_block.copy.py` sha256 | `9f46b66aeb38a1d4…` (PROMOTION-PREP sec 4) | MATCH |
| `composition/vendored/modinst_core.c` | pristine `45c04d08…` | MATCH |
| current receipt | 51 tuples, `patched_sha256=45c04d08…`, last id `r2-install-status-ok` | MATCH |
| no prior `patches.json.51.bak` | absent | absent |

## 2. BEFORE inventory (archive)

`inventory/01-before-inventory.txt` — full sha256 inventory of ALL 123 files
under `worker-d1-chain-relink/composition/receipts/`, plus the 13 staged cmd
files (`worker-e2e-staging-final/cmd/*.json`), the two docs to be amended, and
the vendored core. Key rows:

- `receipts/patches.json` = `252ad699d183a1b6…` (51-tuple receipt)
- `worker-e2e-console-dry/E2E-CMD-SCHEMA.md` = `97a33efdfa5005a4…`
- `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` = `778860973d7950c6…`

## 3. Application (gated helper)

`sh worker-e2e-suite-completion/promotion/apply-promotion.sh` →
`PROMOTION-OK: 54 tuples; prefix == 51-tuple backup; tail
g1-cmd-entry-{parse,allowlist,consume}; patched_sha256 43ea66c1...`
(helper exit 0; log `inventory/02-apply-promotion.log`). The helper performs
its own gates BEFORE any write, then: backup `patches.json` →
`patches.json.51.bak`, install `patches.json.new` as `patches.json`,
verify-after (54 tuples, prefix identity vs backup, tail ids, endpoint sha).

## 4. AFTER inventory + diff

`inventory/03-after-inventory.txt` (full 124-file sha inventory),
`inventory/09-full-inventory-diff.txt`. The ENTIRE delta is 3 inventory lines:

| path | before | after |
|---|---|---|
| `receipts/patches.json` | `252ad699d183a1b6…` | `63c28979d21a9a56…` (byte-identical to `patches.json.new`) |
| `receipts/patches.json.51.bak` | — (new) | `252ad699d183a1b6…` (the original, preserved) |

All other 122 receipts files are byte-identical. Promoted receipt facts
(`inventory/03-after-inventory.txt`): 54 tuples; tuple 51 = `r2-install-status-ok`
(last of the unchanged prefix); tail = `g1-cmd-entry-parse`,
`g1-cmd-entry-allowlist`, `g1-cmd-entry-consume` (delivered order, NOT
reordered); `patched_sha256=43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0`;
`source`/`source_sha256`/`bridge` unchanged.

## 5. Re-verification on the promoted tree

1. **G1 block replay** (`g1_replay_promoted_tree.py`, log
   `inventory/10-g1-replay-promoted-tree.log`) — G1_BLOCK imported VERBATIM
   from `promotion/g1_block.copy.py` (`9f46b66a…`) and applied sequentially to
   a scratch copy of the tree's still-pristine core `45c04d08…`:
   `45c04d08… (74749 B) -> 330d47f2… (86235 B) -> 91a122a8… (88006 B) ->
   43ea66c1… (100238 B)`, every anchor occurrences==1 (asserted), final ==
   the PROMOTED receipt's `patched_sha256`, byte-identical to
   `promotion/g1-chain-replay-core.c` (== owned config-entry revision), and
   all 3 receipt tail tuples' occurrences/byte-counts/old/new shas ==
   G1_BLOCK literals (receipt format note: `old_bytes`/`new_bytes` are
   LENGTHS, literals pinned by the shas — same as the existing 51 tuples).
   **PASS.**
2. **13 cmd files unchanged** — inventory diff vs before-record: empty
   (`inventory/13-post-verification.log`); `shasum -a 256 -c
   worker-e2e-staging-final/cmd/SHASUMS.txt`: **13 OK, 0 failed**.
3. **Vendored core untouched** — still `45c04d08…` (receipt-only promotion;
   the optional composer splice of PROMOTION-PREP sec 5 step 4 was NOT
   performed — it is explicitly a separate controller step).

## 6. Schema amendment (EXCONFIG6-MEMO recommendation: ACK exit 6)

Where the schema lives: the owning doc is
`worker-e2e-console-dry/E2E-CMD-SCHEMA.md` (confirmed; the memo cites it by
that path). Applied there + the memo's required mirror:

- `E2E-CMD-SCHEMA.md` (`97a33efd…` → `e940137d3f440969ef39991e2b044ee60dd22fad8cc1cbf8a1083d1e1cb7d461`):
  - §1 present-but-invalid bullet: `EX_BAD` → `EX_CONFIG` 6 (points to §6
    amendment; absent-file stays `EX_BAD`).
  - §6: AMENDMENT block added (provenance: worker-e2e-promotion-apply
    2026-10-04, per `worker-e2e-suite-completion/EXCONFIG6-MEMO.md`, ACKing
    the implemented build-r1 delta in
    `worker-e2e-config-entry/owned/modinst_core.c` sha `43ea66c1…`, promoted
    to the D1 receipts this date): present-but-fails-validation exits
    **6 (`EX_CONFIG`)**; absent-file and consume-failure remain `EX_BAD` (2);
    all other refusal semantics unchanged. Row 3 exit updated to `EX_CONFIG` 6.
  - §8(3): refusal-matrix evidence wording now distinguishes absent-file
    (EX_BAD) from validation refusals (`EX_CONFIG` 6).
- `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` §4 mirror (`77886097…` →
  `873ad49003721166273cae5415a799ab63957ae2f40d2397ba856af406d0ba00`): one
  expected-exits paragraph after the §4 rollback-ownership note — cmd file
  PRESENT but invalid exits 6, file LEFT in place (inspect, DELE, never
  auto-retry); absent-file/verb-level refusals still exit 2, so the existing
  `{"event":"end","exit":2}` = refusal-path STOP guidance is unchanged.

The 13 staged cmd JSON files were NOT modified (proven unchanged above).

## 7. Honest scope

- The promotion changes the RECEIPT ONLY. `composition/vendored/modinst_core.c`
  remains the 51-tuple core `45c04d08…`; a tree rebuild would NOT yet
  reproduce `43ea66c1…` until the composer splice (PROMOTION-PREP sec 5 step
  4) is done as a separate controller step. The suites in SUITE-RESULTS.md
  were run against the patch-apply form of the same bytes (43ea66c1).
- No new host/console runs were performed; no console contact.
- RUN-CONFIG B remains pending its controller dispatch; exit-6 semantics now
  documented in both amended docs.

## 8. ROLLBACK instructions (from the archived inventory)

1. Receipt (exact restore of the frozen tree's pre-promotion state):
   ```sh
   cd .../2026-10-04-zcode-team/worker-d1-chain-relink/composition/receipts
   cp -p patches.json.51.bak patches.json     # restore 252ad699… (51 tuples)
   rm patches.json.51.bak                      # helper gate treats its presence as "already promoted"
   ```
   Verify: `shasum -a 256 patches.json` = `252ad699d183a1b63cc2118edff51b8f0fbf5cb48a22dfd0f40b1d7341340c3b`;
   `python3 -c "import json;d=json.load(open('patches.json'));assert len(d['patches'])==51 and d['patched_sha256']=='45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d'"`.
   Full-state check: re-run the BEFORE inventory and diff against
   `inventory/01-before-inventory.txt` — must be identical (123 files).
2. Schema amendment revert: restore the two docs to their before-shas —
   `E2E-CMD-SCHEMA.md` `97a33efdfa5005a4bca0f11a5a59223c0b2ea0851ea8ccdb4a8ef73c3c5d72a4`,
   `STAGED-E2E-COMMANDS.md` `778860973d7950c6cecdaa58183f5b735089a8fbadad8aeab0ab7d7597fc3b6a`
   (undo the three §1/§6/§8(3) edits + §4 mirror paragraph; exact inserted
   text is quoted in sec 6 above). Only revert these if the EX_CONFIG 6 ACK
   itself is withdrawn (which would also require reverting the build-r1
   payload — a new composition revision, per EXCONFIG6-MEMO sec 5).
3. The 13 cmd files need NO rollback action (never modified; 13/13 shas OK).

## 9. Artifacts in this worker dir

| file | what |
|---|---|
| `CLAIM.json` | claim (written FIRST, updated to APPLIED) |
| `g1_replay_promoted_tree.py` | the replay/cross-check script (read-only on the tree) |
| `inventory/00-input-verification.log` | gate re-verification transcript |
| `inventory/01-before-inventory.txt` | BEFORE: full receipts-dir sha inventory (123 files) + cmd files + docs |
| `inventory/02-apply-promotion.log` | helper run transcript (PROMOTION-OK, exit 0) |
| `inventory/03-after-inventory.txt` | AFTER: full inventory (124 files) + promoted receipt summary |
| `inventory/04…06-*.txt` | patches.json-only before/after + diff |
| `inventory/07…09-*.txt` | full-inventory diff (exactly 3 changed lines) |
| `inventory/10-g1-replay-promoted-tree.log` | G1 replay PASS transcript |
| `inventory/11…13-*.txt/.log` | cmd-file before/after + SHASUMS -c 13 OK + doc after-shas |
| `REPORT.md` | this report (written LAST) |
