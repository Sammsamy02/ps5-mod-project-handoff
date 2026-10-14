# REPORT.md — worker-seh64-incident-reconciliation

Worker: worker-seh64-incident-reconciliation (2026-10-05). Local-only read/verify; writes confined to `outputs/2026-10-04-zcode-team/worker-seh64-incident-reconciliation/` plus /tmp scratch. No builds re-run, no receipts rewritten, no quarantined bytes touched, no ledger edits (LEDGER-ADDENDUM.md is proposed text only).

## What was done

1. Read `worker-stray-root-audit/AUDIT.md` (read receipt) and the quarantined stray receipt itself.
2. Hashed all 2,651 files under `worker-seh64-trampoline-fix/` (sha256 + mtime local/UTC + bytes) → `MANIFEST.tsv`; classified every artifact group BEFORE / DURING (05:41:36–05:46:12 local bracket) / AFTER the 05:42:47 misrun → `RECONCILIATION.md` §3.
3. Audited all 13 in-worker receipts (parsed JSON, walked every absolute path string) plus CLAIM/REPORT/OWNING-LINEAGE/run logs for wrong-root evidence; grepped the whole workspace for wrong-root prefixes, the stray build id, the mis-joined lld-link form, the 14 build ids, and the new F-3/F-4 symbols.
4. Read the harness (`vendor/windows-candidate/build.py`, `link_layout.py`) to establish the root-resolution mechanism; verified no marker-walk code exists in the worker dir.
5. Read `launch10-build/EDITION.json` in full and grepped the l10 drivers; swept the ledger entry.
6. Performed three modulo-build-id byte-identity spot-checks (§6 of RECONCILIATION.md), all passed.

## Answer to the open question

**No. The wrong-root condition affected ZERO delivered artifacts of worker-seh64-trampoline-fix.** All 24 artifact groups (and the 4 debug runs) are CLEAN. Two independent reasons, either sufficient:

- Mechanism: the worker's harness resolves its root statically (`parents[2]`), not by marker walk — the stray marker could never change any path it used. The only marker-walk consumer in the tree (`worker-prx-link-gaps/package_prx.py`) is absent from the worker dir.
- Evidence: every receipt records true-root tool paths and worker-dir outputs; zero wrong-root absolute paths, zero stray-id references, zero mis-joined lld-link strings exist in the worker dir.

Additionally, no artifact has ANY downstream consumer (build ids, new symbols, and the worker dir are referenced nowhere outside itself and the two incident-response dirs), and launch-10's EDITION.json has zero overlap — the F1–F4 fixes remain unintegrated.

## Honest limits

- Timestamps: file mtimes are local CDT (UTC-5); conversions to Z are stated inline. The audit's "05:41:36–05:46:12 bracket" is itself a local-time bracket; I used exact seconds where available.
- The exact quarantine time of the stray is NOT directly recorded anywhere; bounded only as between ~07:44 (worker-stray-root-hardening active) and 08:09 local (already renamed at this worker's first listing). This does not affect any verdict: the relevant boundary is the misrun at 05:42:47, and every post-misrun seh64 artifact is CLEAN by mechanism regardless of when the marker was removed.
- The misrun's exact argv is unrecoverable (`commands: []` — the fatal first command was never appended). The mis-joined lld-link form is known only from the receipt error string, matching the audit's inference; whether `--sdk`/`--llvm-bin` were also passed explicitly in that run is unknowable (the run died before reaching them).
- mtime-based classification assumes the worker dir was not modified after 06:30 local; dir mtimes and the ledger (06:33Z entry) support this, but mtimes are the only evidence — no journal exists.
- No re-execution: receipts are trusted as recorded bytes (I verified their internal path/hash structure and cross-references, not the underlying compilations). The three modulo-build-id spot-checks verify evidence-stream consistency between receipts, not the claims' technical truth.
- Binary outputs (.lib/.dll/.o/test-sdk-driver/ELF) inside out-* dirs are inventoried and hashed but were not functionally re-verified (out of scope; no new engine evidence created).
- The 2651-file manifest is a snapshot at reconciliation time (~08:10–08:40 local); any later writer to the worker dir would not be reflected. The worker's own bounds say writes were confined to its dir, and its last write was 06:30.
- `worker-seh64-family` receipts were read for the baseline cross-check; I did not audit that worker's dirs beyond the two host-normal.jsonl files used.
- Ledger addendum text is a PROPOSAL with a `{UTC}` placeholder; the ledger is controller-owned and was not modified.
- Beyond the seh64 scope: this worker did NOT re-audit the other marker-walk victims (walkguard-prep, r3-armed) — the audit already covers them, and the controller's quarantine unblocks them.
