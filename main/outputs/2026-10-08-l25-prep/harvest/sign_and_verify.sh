#!/bin/bash
# sign_and_verify.sh — PS5 PRX/ELF fake-sign + local verification (Mac-local, no console)
#
# Usage:
#   ./sign_and_verify.sh <input.prx|input.elf> <output.self>
#
# Pipeline (permanent console rules, outputs/2026-10-06-recovery-fakesign/POSTMORTEM.md):
#   1. Input MUST be a plain unsigned ELF (magic 7f454c46). Refuses inputs that are
#      already signed (4f153d1d / 5414f5ee) to prevent double-signing, and refuses
#      non-ELF files entirely.
#   2. Fake-signs via the ps5-payload-sdk make_fself.py (default ptype=fake, zeroes
#      e_shnum). Documented invocation: python3 make_fself.py input.elf output.self
#   3. Verifies the output magic and records input/output sha256 + sizes to a
#      receipt JSON (<output>.sign-receipt.json). Exits nonzero on ANY mismatch.
#
# Magic table (POSTMORTEM.md rule 5):
#   4f153d1d = SELF/FSELF container  (what make_fself.py emits — required output)
#   5414f5ee = Sony-signed sprx      (accepted on console readback, never our output)
#   7f454c46 = plain ELF             (unsigned for console purposes — input only,
#                                     never a bootable top-level file on this setup)
#
# POST-UPLOAD READBACK VERIFICATION TEMPLATE (decrypt-OFF, REQUIRED before launch —
# the console ftpsrv decrypts SELF/SPRX on download by default and fakes LIST/SIZE):
#   # 1. Download with decrypt-off on the SAME connection (-Q runs before the transfer):
#   curl -s -Q "SITE SELF" -o $EV/readback.prx \
#     "ftp://PS5_HOST:PORT/data/plugins/<name>.prx"
#   #    (EV = outputs/2026-10-08-l25-prep/evidence/runtime/<label>/ -- DURABLE
#   #    since 2026-10-10, agent-HA evidence law: readbacks are receipts,
#   #    never /tmp scratch; legacy /tmp/readback.prx examples are superseded)
#   #    (server must reply "226 SELF transfer mode disabled"; never trust a download
#   #     made without -Q "SITE SELF" — 7f454c46 in such a download is the DECRYPTED
#   #     view, not the file on disk)
#   # 2. Magic must be 4f153d1d (fake-SELF) or 5414f5ee (Sony sprx):
#   xxd -p -l 4 $EV/readback.prx
#   # 3. Full sha256 must match the signed artifact recorded in <output>.sign-receipt.json:
#   shasum -a 256 $EV/readback.prx
#   # Any mismatch => STOP: do not launch; restore from the pre-upload decrypt-off backup.
#
# Exit codes: 0 = signed + verified; 1 = usage/tool/environment error;
#             2 = input rejected (already signed or not an ELF);
#             3 = output verification failed (bad magic / missing / hash unreadable).

set -euo pipefail

MAKE_FSELF="/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-sdk-build-check/ps5-payload-sdk/samples/install_app/make_fself.py"

MAGIC_ELF="7f454c46"
MAGIC_FSELF="4f153d1d"
MAGIC_SONY="5414f5ee"

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <input.prx|input.elf> <output.self>" >&2
  exit 1
fi

INPUT="$1"
OUTPUT="$2"

[ -r "$INPUT" ] || { echo "FAIL: input not readable: $INPUT" >&2; exit 1; }
[ -f "$MAKE_FSELF" ] || { echo "FAIL: make_fself.py not found: $MAKE_FSELF" >&2; exit 1; }

IN_MAGIC=$(xxd -p -l 4 "$INPUT")
if [ "$IN_MAGIC" = "$MAGIC_FSELF" ] || [ "$IN_MAGIC" = "$MAGIC_SONY" ]; then
  echo "FAIL: input $INPUT is ALREADY SIGNED (magic $IN_MAGIC)." >&2
  echo "      Sign the plain-ELF build output instead; never double-sign or re-upload" >&2
  echo "      a signed artifact through this pipeline." >&2
  exit 2
fi
if [ "$IN_MAGIC" != "$MAGIC_ELF" ]; then
  echo "FAIL: input $INPUT is not an ELF (magic $IN_MAGIC); expected plain $MAGIC_ELF." >&2
  exit 2
fi

IN_SHA=$(shasum -a 256 "$INPUT" | awk '{print $1}')
IN_SIZE=$(stat -f %z "$INPUT")
echo "input : $INPUT  magic=$IN_MAGIC  size=$IN_SIZE  sha256=$IN_SHA"

python3 "$MAKE_FSELF" "$INPUT" "$OUTPUT"

OUT_SHA=$(shasum -a 256 "$OUTPUT" | awk '{print $1}')
OUT_SIZE=$(stat -f %z "$OUTPUT")
OUT_MAGIC=$(xxd -p -l 4 "$OUTPUT")
echo "output: $OUTPUT  magic=$OUT_MAGIC  size=$OUT_SIZE  sha256=$OUT_SHA"

if [ "$OUT_MAGIC" != "$MAGIC_FSELF" ]; then
  echo "FAIL: output magic $OUT_MAGIC != required $MAGIC_FSELF (fake-SELF)." >&2
  exit 3
fi

RECEIPT="${OUTPUT}.sign-receipt.json"
python3 - "$INPUT" "$OUTPUT" "$IN_MAGIC" "$OUT_MAGIC" "$IN_SHA" "$OUT_SHA" \
          "$IN_SIZE" "$OUT_SIZE" "$MAKE_FSELF" "$RECEIPT" <<'PYEOF'
import json, sys, datetime, os, subprocess
(i, o, im, om, ish, osh, isz, osz, tool, receipt) = sys.argv[1:11]
tool_sha = subprocess.run(["shasum","-a","256",tool],capture_output=True,text=True).stdout.split()[0]
json.dump({
  "script": "sign_and_verify.sh",
  "utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
  "tool": {"path": tool, "sha256": tool_sha, "invocation": "python3 make_fself.py INPUT OUTPUT (default ptype=fake)"},
  "input":  {"path": os.path.abspath(i), "magic": im, "size_bytes": int(isz), "sha256": ish},
  "output": {"path": os.path.abspath(o), "magic": om, "size_bytes": int(osz), "sha256": osh,
             "size_delta_bytes": int(osz) - int(isz)},
  "magic_table": {"4f153d1d": "SELF/FSELF container (this tool's output)",
                  "5414f5ee": "Sony-signed sprx (readback-acceptable, never our output)",
                  "7f454c46": "plain ELF (unsigned; valid input only, never bootable on console)"},
  "post_upload_readback_required": {
    "download": "curl -s -Q \"SITE SELF\" -o $EV/readback.prx ftp://PS5_HOST:PORT/data/plugins/<name>.prx  # $EV = outputs/2026-10-08-l25-prep/evidence/runtime/<label>/ (durable since 2026-10-10)",
    "magic_check": "xxd -p -l 4 $EV/readback.prx  # expect 4f153d1d or 5414f5ee",
    "hash_check": "shasum -a 256 $EV/readback.prx  # must equal output.sha256 above"
  },
  "verified": {"output_magic_is_fself": om == "4f153d1d", "input_was_plain_elf": im == "7f454c46"}
}, open(receipt, "w"), indent=2)
PYEOF

echo "receipt: $RECEIPT"
echo "OK: $INPUT -> $OUTPUT signed and verified (magic $OUT_MAGIC, sha256 $OUT_SHA)"
echo "NEXT: upload, then decrypt-OFF readback must show magic 4f153d1d and sha256 $OUT_SHA"
exit 0
