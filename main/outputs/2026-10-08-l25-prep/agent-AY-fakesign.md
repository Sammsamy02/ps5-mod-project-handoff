# agent-AY — PRX fake-sign pipeline proven end-to-end (l25 prep, Mac-local dry run)

Date: 2026-10-08. Working dir: /tmp/l25-fakesign/. No console/network access used.

## 1. Signer tool verified

- REAL path in this worktree (confirmed present, executable):
  `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-sdk-build-check/ps5-payload-sdk/samples/install_app/make_fself.py`
  (sha256 d76a78f4bdf214d2f6a7508797dcede7d680c8b3d4ca0a4fe0a4d1d436cc9275)
- AGENTS.md's path (`outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/.../make_fself.py`)
  does NOT resolve on this Mac (confirmed ENOENT). A `find` across `.superpowers` located
  exactly ONE copy — the path above. No alternates.
- Documented invocation (POSTMORTEM.md rule 3): `python3 make_fself.py input.elf output.self`
  (default ptype=fake; also zeroes e_shnum). Matches the tool's argparse
  (`--paid/--ptype/--app-version/--fw-version/--auth-info`, defaults used).

## 2. Dry-run evidence on a real l24 PRX copy

Input selection note: the TOP-LEVEL packaged PRX
(`outputs/2026-10-08-l24-package/pass-1/modbridge_supplier_composition.prx`, 31,331,783 B,
sha256 2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4, and the review
zip's `artifact/` copy — same size) is ALREADY fake-signed (magic 4f153d1d). The plain-ELF
pre-signing form lives at `pass-1/bare-original/modbridge_supplier_composition.prx`
(and `bare-sdk/`, different content, same role). The bare-original copy was used as signer
input — this is exactly what l25 must feed the signer.

| | input (bare copy) | output (.self) |
|---|---|---|
| file | /tmp/l25-fakesign/l24-bare-unsigned.prx | /tmp/l25-fakesign/l24-bare-unsigned.self |
| magic @0 | 7f454c46 (plain ELF — unsigned) | 4f153d1d (SELF/FSELF — this tool's emitted magic) |
| size | 31,315,144 B | 31,331,783 B (delta +16,639 B container overhead) |
| sha256 | 1aba1af9216457078c2e7eebc99529cf56951efa1805a036c3f58f61ff5e50ff | 0ba7170e9c9188b317933c05d292866ae263dea66a4411eac59552fc8918d2f7 |

- Determinism: re-signing the same input produced a byte-identical .self (same sha256,
  `cmp` clean). The pipeline is reproducible.
- Output size exactly equals the shipped l24 signed artifact size (31,331,783 B); hash
  differs because the shipped 2e712a8b artifact was signed from the selected pass build's
  bare form (bare-original and bare-sdk differ in content). Expected, not a defect: the
  pipeline is deterministic per-input, and l25 must sign ITS OWN bare build.
- Magic table (POSTMORTEM.md rule 5, confirmed by test):
  `4f153d1d` = SELF/FSELF container (what make_fself.py emits; required output);
  `5414f5ee` = Sony-signed sprx (readback-acceptable, never our output);
  `7f454c46` = plain ELF (unsigned for console purposes; valid input only).

Decrypt-hazard detection is testable offline: the unsigned input reads 7f454c46 while both
the tool output and the shipped artifact read 4f153d1d — i.e., a 4-byte check
(`xxd -p -l 4`) distinguishes signed from unsigned/plains before any upload. A 7f454c46
download WITHOUT `-Q "SITE SELF"` is the console's DECRYPTED view, not the disk file.

## 3. sign_and_verify.sh (harvested)

- Paths: /tmp/l25-fakesign/sign_and_verify.sh (working copy) and
  /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/sign_and_verify.sh (deliverable, executable).
- Behavior (all tested):
  - usage: `sign_and_verify.sh <input.prx|input.elf> <output.self>`
  - refuses inputs already signed (4f153d1d/5414f5ee) — tested against the l24 pass-1
    packaged PRX: exit 2, no output written;
  - refuses non-ELF inputs — tested with a shell script: exit 2;
  - runs make_fself.py (path pinned inside), requires output magic 4f153d1d (exit 3 on
    mismatch), writes receipt JSON `<output>.sign-receipt.json` with input/output magic,
    size, size-delta, both sha256, tool sha256, magic table, and the post-upload
    decrypt-OFF readback command template;
  - happy path tested on the l24 bare copy: exit 0, receipt valid JSON
    (harvested as l24-dryrun-sign-receipt.json), output sha256 identical to the manual
    dry run above.

## 4. Expected per-variant flow (one page) — package-A prx example

Per variant (A/B/... each with its OWN bare plain-ELF build output):

1. BUILD: produce the variant's bare PRX/ELF (plain 7f454c46; e.g. l25 package pass
   bare-original form). Never feed the top-level packaged .prx to the signer — in l24
   that file was already signed.
2. SIGN + VERIFY LOCAL (one line):
   `bash outputs/2026-10-08-l25-prep/harvest/sign_and_verify.sh <variant>-bare.prx <variant>.self`
   — script refuses signed/non-ELF input, fake-signs, checks 4f153d1d, emits
   `<variant>.self.sign-receipt.json`. Record the receipt's output sha256; this is the
   ground truth for upload verification. Any nonzero exit = stop, do not upload.
3. FRESH DECRYPT-OFF BACKUP of the live console file FIRST (upload condition): with
   `-Q "SITE SELF"` on the same connection, download + sha256 the current
   /data/plugins/modbridge_supplier_composition.prx before overwriting. Preserve rollback.
4. UPLOAD the .self (never a plain-ELF build output, never a decrypt-on download) to
   /data/plugins/ only, under the standing upload conditions (gate receipts, only-PRX
   changes, configs untouched).
5. DECRYPT-OFF READBACK VERIFY (template also embedded in the script comments and every
   receipt):
   `curl -s -Q "SITE SELF" -o /tmp/readback.prx "ftp://PS5_HOST:PORT/data/plugins/modbridge_supplier_composition.prx"`
   then `xxd -p -l 4 /tmp/readback.prx` MUST show 4f153d1d (or 5414f5ee) and
   `shasum -a 256 /tmp/readback.prx` MUST equal the receipt's output sha256.
   Mismatch or 7f454c46 in a -Q download => STOP, restore from step-3 backup, no launch.
6. LAUNCH: ONE test launch on a fresh boot, launched by the user (no rapid-fire retries —
   failed launches leave kstuff paused and all games fail auth until a clean exit/reboot).
7. HARVEST: UE4SS.log / runtime-status / supplier-diag jsonl / klog to NEW files.

Determinism note: identical bare input always yields identical .self bytes, so a failed
readback can be retried against the SAME receipt hash without ambiguity; a differing hash
means the wrong input variant was signed or the upload corrupted — re-run step 2 from the
correct bare build, never re-sign a signed artifact.
