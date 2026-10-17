#!/usr/bin/env python3
"""grep_markers.py — PRX composition-marker grep (UTF-8 + UTF-16LE).

Why this exists (2026-10-09/10 seam round): the previous build's Lua
self-test lines were composed OUT of the PRX at link time and we only
discovered it on console. Claude's fix: before upload, grep the PRX for
the critical marker strings to prove they were composed in.

Encoding reality (verified against the real artifacts):
  - narrow C literals (char* / snprintf / fmt::format templates, Lua chunk
    bytes) land in the binary as UTF-8;
  - STR()/wide literals (UE4SS StringType = char16_t on Prospero) land as
    UTF-16LE — i.e. each ASCII char followed by a 0x00 byte, and non-ASCII
    chars as their UTF-16LE code units.
  So every marker must be searched in BOTH encodings; the fake-sign SELF
    container does not obscure string data (verified: identical counts in
    the bare 7f454c46 ELF and the signed 4f153d1d PRX).

Usage:
  grep_markers.py <prx-path> [--json] [--marker "TEXT"]...

  --json      machine-readable result (single JSON object on stdout)
  --marker    restrict the scan to the given marker text(s); otherwise the
              full built-in MARKERS list is scanned.

Exit codes: 0 = every CRITICAL marker found (non-critical may be missing);
            1 = at least one CRITICAL marker missing;
            2 = usage / file error.
"""

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

# ---------------------------------------------------------------------------
# Marker registry.
# critical=True  -> missing marker fails the gate (exit 1): these prove the
#                   Lua/FName seam fix actually composed in.
# critical=False -> informational: report, never gate.
# ---------------------------------------------------------------------------
MARKERS = [
    {
        "text": "[LUA-FN]",
        "critical": True,
        "what": "converter's log prefix (LuaFNameBridge first-20-uses line; narrow snprintf template)",
    },
    {
        "text": "[ST] lua-ffo",
        "critical": True,
        "what": "Lua-originated FindFirstOf self-test line prefix",
    },
    {
        "text": "[FN] pool-decoder verified",
        "critical": True,
        "what": "FName pool decoder startup verification",
    },
    {
        "text": "[DRY]",
        "critical": False,
        "what": "zzdiag menu dry-run lines",
    },
    {
        "text": "[FN] seam refused",
        "critical": False,
        "what": "embedded-NUL audit refusal lines",
    },
    {
        "text": "[FFO] query-fallback",
        "critical": False,
        "what": "FindFirstOf query-fallback decode evidence",
    },
    {
        "text": "vmt_engine_tick_drain",
        "critical": False,
        "what": "VMT pump's C-linkage symbol (section/dynstr witness)",
    },
    {
        "text": "[ST] fname",
        "critical": False,
        "what": "standard FName self-test lines",
    },
    {
        "text": "[Seeds][L27",
        "critical": False,
        "what": "any remaining L27 diagnostic lines",
    },
]

# Magic table (permanent console rules, POSTMORTEM.md rule 5).
MAGIC_SELF = "4f153d1d"  # fake-signed SELF container (our make_fself output)
MAGIC_SONY = "5414f5ee"  # Sony-signed sprx (readback-acceptable, never our output)
MAGIC_ELF = "7f454c46"  # plain ELF — decrypt view / pre-sign artifact


def scan(data: bytes, marker_text: str) -> dict:
    """Count occurrences of marker_text in UTF-8 and UTF-16LE.

    UTF-16LE of a narrow ASCII string is each char + 0x00; encoding via
    Python's utf-16-le codec handles non-ASCII markers correctly too.
    """
    utf8 = data.count(marker_text.encode("utf-8"))
    utf16 = data.count(marker_text.encode("utf-16-le"))
    if utf8 and utf16:
        where = "both"
    elif utf8:
        where = "UTF-8"
    elif utf16:
        where = "UTF-16LE"
    else:
        where = None
    return {"utf8": utf8, "utf16le": utf16, "where": where}


def main() -> int:
    ap = argparse.ArgumentParser(description="PRX composition-marker grep (UTF-8 + UTF-16LE)")
    ap.add_argument("prx", help="path to the built PRX/SELF")
    ap.add_argument("--json", action="store_true", help="emit JSON result")
    ap.add_argument("--marker", action="append", default=None,
                    help="scan only this marker text (repeatable); default: full registry")
    args = ap.parse_args()

    path = Path(args.prx)
    if not path.is_file():
        print(f"grep_markers: FAIL: not a readable file: {path}", file=sys.stderr)
        return 2
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    magic = data[:4].hex()

    if args.marker:
        registry = []
        for text in args.marker:
            hit = next((m for m in MARKERS if m["text"] == text), None)
            registry.append(hit if hit else {"text": text, "critical": False, "what": "(ad-hoc)"})
    else:
        registry = MARKERS

    results = []
    for m in registry:
        r = scan(data, m["text"])
        r.update({"text": m["text"], "critical": m["critical"], "what": m["what"],
                  "found": r["where"] is not None})
        results.append(r)

    missing_critical = [r for r in results if r["critical"] and not r["found"]]
    missing_other = [r for r in results if not r["critical"] and not r["found"]]

    if args.json:
        print(json.dumps({
            "prx": str(path.resolve()),
            "magic": magic,
            "size": len(data),
            "sha256": digest,
            "scanned_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "results": results,
            "missing_critical": [r["text"] for r in missing_critical],
            "missing_noncritical": [r["text"] for r in missing_other],
        }, indent=2))
    else:
        print(f"marker scan: {path}")
        print(f"  magic={magic}  size={len(data)}  sha256={digest}")
        for r in results:
            state = f"FOUND ({r['where']})" if r["found"] else "MISSING"
            tag = "CRITICAL" if r["critical"] else "info"
            counts = f"utf8={r['utf8']} utf16le={r['utf16le']}"
            print(f"  [{tag:8}] {state:16} {counts:24} {r['text']!r}  <- {r['what']}")
        if missing_critical:
            print("MISSING CRITICAL: " + ", ".join(repr(r["text"]) for r in missing_critical))
        else:
            print("all CRITICAL markers present")
        if missing_other:
            print("missing (non-critical): " + ", ".join(repr(r["text"]) for r in missing_other))

    return 1 if missing_critical else 0


if __name__ == "__main__":
    sys.exit(main())
