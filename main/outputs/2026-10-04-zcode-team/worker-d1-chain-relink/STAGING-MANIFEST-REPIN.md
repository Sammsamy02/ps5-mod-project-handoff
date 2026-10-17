# STAGING-MANIFEST-REPIN.md — D1-safe merged-chain console payload (F-R3)

Origin: worker-d1-chain-relink, 2026-10-04. This file is the F-R3 re-pin of
`worker-e2e-console-staging/STAGING-MANIFEST.md` row #1 for the merged
51-tuple chain (D1 fix before F-R2 fix, per `worker-risk-e2e/D1-PROPAGATION.md`
section 4). The staging directory itself is read-only to this worker; the
controller should apply this row supersession before the next E2E run. All
shas were recomputed with `shasum -a 256` / hashlib this session.

## Superseded payload row #1

| Field | Old (staged) | New (merged chain) |
|---|---|---|
| Local file | `outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/composition/build/console/modbridge-installer.payload.elf` | `outputs/2026-10-04-zcode-team/worker-d1-chain-relink/composition/build/console/modbridge-installer.payload.elf` |
| sha256 | `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9` | `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436` |
| Bytes | 681632 | 681528 (-104; recorded exactly — see REPORT §5 note 1) |
| Chain | 45 tuples (35 base + 6 e2e + 4 zb) | 51 tuples (35 base + 6 e2e + 4 zb + 4 uf + 2 r2) |
| D1 fixed | NO | YES (all 4 uf OLD anchors absent from patched core `45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d`) |
| F-R2 fixed | NO | YES |
| Imports | 116, missing=[] | 116, missing=[] — import NAME set and per-name provider sets byte-identical to the staged payload (`composition/receipts/console-import-providers.json` both sides) |
| Bridge link-map | mi_bridge_* present | 6 `mi_bridge_*` symbols in `composition/build/console/modbridge-installer.payload.map` |

Console destination / upload alias unchanged:
`/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf`
(via `POST /manage:upload?filename=MODBRIDGE_INST_E2E.elf`).

## F-R3 / R-FTP-01 staging law (applies to the new payload)

The console link is not bit-reproducible across relinks, so identity for
staging acceptance is verified by size + import-set identity + link-map
symbols, with the sha re-pinned DELIBERATELY by this receipt — never by
full-hash equality with a previous payload.

- Staged-fself size law: readback must equal the inner-ELF phdr-covered
  prefix + ELF magic `\x7fELF`. For THIS payload:
  **phdr-covered prefix = 606256 bytes** (3 PT_LOAD segments), file size
  681528 bytes, ELF magic verified. Freshness oracle stays the runtime
  aligned-log mtime.
- Per-tuple patch evidence: `composition/receipts/patches.json` (51 patches,
  occurrences==expected for every tuple; the only non-1 is the pre-existing
  base-chain `empty-root-open`==2, also ==2 in the delivered base chain).
- Link/import receipts: `composition/receipts/console-build.json`,
  `composition/receipts/console-import-providers.json` (unresolved imports:
  `[]`).
- Splice provenance: `receipts/merged-script-patches.json` (vendored driver
  `ab9de3dd53fe208a…` verified; uf and r2 block texts verified per-tuple
  against both fix-origin `patches.json` pins before use).

## Unchanged corpus pins (verified byte-identical, not re-staged by this worker)

| File | sha256 | Pin source |
|---|---|---|
| movement-speed-2x.zip | `3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad` | manifest #2 |
| mm-skin.mfb | `7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af` | manifest #3 |
| mm-lua.mfb | `eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec` | manifest #4 |
| infinite-stamina-v1.zip (optional #2) | `61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5` | manifest optional row |

## Expected-output validity

Applying D1 changes NO user-visible command output and no exit codes, so the
STEP-BY-STEP expected stdout/e2e-status lines stay valid. Two identity/notes
for GO-NOGO:

1. GO-NOGO/STAGING identity checks must accept the NEW sha
   `e3489297…` + 681528 bytes (this file is the deliberate re-pin receipt).
2. Merged-chain transcript contract (from F-R2, already designed): every
   CONTROLLED exit — including die()-refusals — now closes its status begin
   with a failed end line; only `_exit(86)` crash probes and real signals
   leave unmatched begins. See REPORT §4 for the one fault-suite pin this
   moves (`uf-p5-poison-pill` unmatched 1→0, revised with a distinct-ID
   documented suite copy).

Honesty gates unchanged: running=1 stays SIMULATED (R-ACK-01); stage-1 native
download still deferred (R-DL-01); G1/G3 still open (R-ARGV-01/R-G1-02,
R-G3-01). No console was executed by this worker.
