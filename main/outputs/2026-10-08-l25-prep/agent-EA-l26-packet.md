# Agent EA — l26 Claude-review packet assembled (2026-10-08 night)

Task: assemble the l26 round packet in the attachments-20261008 pattern
(main packet <25 MB with filtered logs + separate per-variant PRX zips).
Read-only over all sources; written ONLY under
`packet/attachments-l26/` plus this report.

## Deliverables (all under .../2026-10-08-l25-prep/packet/attachments-l26/)

| Attachment | Size | sha256 |
|---|---:|---|
| `l26-MAIN-PACKET.zip` (+ `.sha256`) | 450,179 B | `6daab5512d2fb7d982d8051b105a200df0457ec9198e99dc1beb583289abb58e` |
| `prx-variantA.zip` (+ `.sha256`) | 9,636,518 B | `83a7857a4db930e1beebd57cdcdd1ce94452279b5a5643a8e4dac5d9196db854` |
| `prx-variantB.zip` (+ `.sha256`) | 9,624,365 B | `699d528d2f7964a48e4d08232d0e0dc830c1bba759d0ade51ea00c267d05bc04` |

- `ATTACHMENTS-MANIFEST.md` — 3-row attachment table (sizes/shas/contents).
- `REASSEMBLE-PRX.md` — per-variant verify commands (A already uploaded to
  console: re-arm notice; B UNSAFE, not uploaded).
- `main-packet/` — 74 files: cover `claude-review/REVIEW-FOR-CLAUDE-l26.md`,
  `INDEX.md` + `MANIFEST.json` (every file hashed from disk), `PRX-FINGERPRINTS-l26.md`,
  22 agent reports in `analysis/`, evidence in `evidence/` (incl. 6 filtered
  readable session logs), build receipts in `build-receipts-{root,variant-a,variant-b}/`,
  tree-state (3 CHANGES.json snapshots + REVERT-POSTMORTEM-FNAME-ADD.md).

## Verification performed

- Main zip 450,179 B < 25 MB: PASS. No `4f153d1d`/`5414f5ee`/`7f454c46`
  magics anywhere inside (PRX binaries live only in the two variant zips): PASS.
- PRX zip round-trips: A → `5ea2c845ba4a01dde4599c08f1930262b394e76279d1bfbaa235a19499dbfc51`
  (31,412,727 B, `4f153d1d`); B → `dd6bab629f2a1f07466c9ddd92d8d2f4c53478d40b478625ce41dfe1bdaf535c`
  (31,374,487 B, `4f153d1d`); both recomputed from the zips AND from the
  FINAL-RECEIPT pass-1 pins: PASS.
- INDEX/MANIFEST internal hashes recomputed after the final cover edit and
  re-verified against the zipped tree: PASS (74 files, 0 missing).
- Fingerprints cross-checked: STATIC-CHECKS 20/20 per pass per variant,
  exactly 207 import identities each, ctor arrays 238/2, twins identical,
  A≠B (VARIANT-PAIR-PACKAGING-RECEIPT) — all cited in PRX-FINGERPRINTS-l26.md.

## Cover document's decision list (§7 of REVIEW-FOR-CLAUDE-l26.md)

(a) RETROACTIVE APPROVAL of the l26 approach (user authorized
    fix-without-review): CP Find-first, DA verified-query + string-walk
    fallback, CN hardening items 1-4+6 with the FNAME_Add item-5 revert,
    CL logwriter, staging/header routing, zzdiag-v7 methodology.
(b) The adapter root-fix plan (CJ): bind engine `FName::ToStringInternal`
    @0x63FA00 with slot-2-style admission; keep FNAME_Find via the bound
    ctor; NEVER Store/Add (GATE-1 family) — adopt as l27 root fix or
    diagnostic-only.
(c) Variant C + Q-gating integration timing (both still un-integrated
    proposals in the l26 tree; registration now console-proven safe).
(d) The decisive-test gate for the already-uploaded 5ea2c845: ploader
    re-arm from STEP 1 + LIVE-ploader assertion, one launch, save-load,
    in-world zzdiag-v7 cycle, harvest; acceptance reading per DA's marker
    semantics ([FFO] query-fallback = fix working; add-fallback = GATE-1
    family; neither + nil = N1 falsified → CR §4 A2 next).

## Key content facts bound into the packet (assembled from receipts/logs)

- Build state: l26-A `5ea2c845…` uploaded to console 23:09 CDT with
  decrypt-OFF readback PASS; launch ABORTED at ploader arming (step 6) —
  decisive in-world test PENDING (LAUNCH-PREP-FAILED-A-20261009T040918Z.md
  included with its full 27-step ledger). B `dd6bab62…` not uploaded, UNSAFE.
- The in-world confirmation is captured in `evidence/logs/now3.log.filtered`:
  save-load churn calls=2 → 35419 (22:47:58→22:48:59), then probes
  22:49:04-06: F1/F2/F3 `r n`, A1 `r n` — FindXOf nil WITH a save loaded
  (menu-timing red herring excluded; the original "UFunction nullptr"
  framing was CA's null-wrapper misread, amendment included).
- The FNAME_Add GC-storm crash + revert postmortem is
  `tree-state/REVERT-POSTMORTEM-FNAME-ADD.md` (verbatim revert comment,
  storm receipts 2→6181→13970, CZ's no-kernel-crash qualification, build
  lineage 23c8483d-never-ran / e13b1d0b-post-revert / 5ea2c845-final).
- Honesty box flags: fix not console-tested; l26-v1 crash attribution
  plausible-not-proven (user-quit end path in klog); S-probe
  "GetPackageNameFromLongName: Name wasn't long." error UNDIAGNOSED;
  now3 is n=1; Z15/variant-C not compiled into any build; ploader arming
  unreconciled (nothing re-dispatches the one-shot).

Raw sources untouched; filtered logs carry derivation headers (mixed
ASCII/UTF-16LE decode, only ~420 BuildProfile PA-check noise lines dropped);
7.5 MB per-variant SOURCE-SELECTION.json tables replaced by sha-pinned
summaries (A `4e82a156…`, B `302b3c5e…`) to keep the main packet text-only.

— agent EA (ZCode), 2026-10-08 ~23:30 CDT.
