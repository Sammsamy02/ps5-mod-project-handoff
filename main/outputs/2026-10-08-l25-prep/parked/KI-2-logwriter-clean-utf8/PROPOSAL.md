# KI-2 PROPOSAL (parked; labelled change; DO NOT IMPLEMENT from this file)

## Logwriter clean-UTF-8 — make UE4SS.log single-grammar (narrow UTF-8 only)

- Proposed: 2026-10-08, agent GZ (log-decode task item 4). PROPOSAL ONLY.
  Nothing applied, no tree edits, no builds, no console contact.
- Status: PARKED. Independent of (and complementary to) the already-landed
  atomicity fixes F1/F3 in `typescan-r4/candidate/include/DynamicOutput/
  FileDevice.hpp` (recursive write mutex; CE FIX-PROPOSAL) and the
  per-call-site F4 codec fix in LuaLibrary.cpp that the
  l26-20261010T043709Z build carries for `[Lua]` prints.
- Consumer-side mitigation ALREADY LANDED this wave:
  `harvest/decode_session_log.py` + decode-first wiring in
  `harvest/harvest.py` (`--raw` = legacy), `bind_launch.sh --watch`,
  `fast_cycle.sh watch` (agent GZ; see ../../LOG-REDECODE-VERDICTS.md).
  This proposal removes the producer-side debt so the mitigation can
  eventually retire.

## 1. What the writer emits today (grounded, with evidence)

Two byte grammars land in ONE file (`/data/plugins/UE4SS.log` via
`Output::NewFileDevice`, single fd, `O_WRONLY|O_APPEND`,
`PosixFile::write_string_to_file` -> `encode_utf8` -> `write_all`):

1. Narrow clean UTF-8 — format literals and native StringType args.
   Never contains 0x00.
2. Wide-doubled runs — every arg converted through
   `to_generic_string` -> `ensure_str` -> `to_charT_string` in a TU that
   resolves the VENDORED `deps/first/Helpers/include/Helpers/String.hpp`
   codecvt (`std::wstring_convert`): the PS5 SDK libc++ locale pairs
   2-byte-wchar do_in/do_out wrappers with 4-byte-unit helpers, so the
   converted StringType interleaves the original bytes with U+0000
   units. On disk: `T\0o\0w\0n`. Sources: staged-fix comment
   `port/first/include/Helpers/String.hpp:15-22`;
   `outputs/2026-10-08-l26-runtime/logwriter-proposal/FIX-PROPOSAL.md`
   section 2 item 3 (host-reproduced byte-for-byte by `race_repro.cpp`,
   harness `1-baseline-deployed` = 124 corrupted lines).
   Live proof: `[EU-VMT] arming refused: n\0o\0n\0-\0t\0e\0x\0t\0-\0e\0n\0t`
   at evidence/claude2-menu-session-20261010.log line 625; 4,058 NULs in
   that one log; 4058/4309/4277/... NULs across the retained corpus
   (LOG-REDECODE-VERDICTS.md section 2).
3. Race overlay — unlocked fd shared by the loader/drain thread and mod
   async workers: whole narrow messages land inside wide messages
   (mid-line switches, displaced tails, orphan `\0s\0t\0a` fragments;
   zzdiag-v2 session proof). F1/F3 (mutex) address this half and are
   already in the candidate tree.

Consequence (why this is a known issue, not a cosmetic one): plain
substring scans over the raw log silently miss every marker spanning a
wide run — the agent-GO row-3 audit trail shows the overclaim class;
9 retained verdicts changed when the tools were fixed to decode first.

## 2. The minimal change (labelled)

Label suggestion: `scope_gz_logwriter_clean_utf8` (one CHANGES.json row
per touched file; candidate copies under typescan-r4/candidate as usual).

GOAL: the log device emits ONE grammar — narrow UTF-8 — regardless of
which Helpers/String.hpp a TU resolves, and regardless of arg origin.

F4a (the core, one call site): `ue4ss/src/LuaLibrary.cpp`
`global_print` (~line 58, both the raw-string and nil/error branches):
replace `to_generic_string(raw_string)` with the explicit locale-free
codec already shipped and asserted-correct in-tree:
`Unicode::DecodeUtf8(std::string_view{raw_string})` (Helpers/Utf16Utf8.hpp;
2-byte-CharType static_assert). This is CE FIX-PROPOSAL F4; the
l26-20261010T043709Z build already carries it for `[Lua]` prints
(corpus proof: claude2 session has ZERO wide `[Lua]` runs while every
2026-10-08-era log has hundreds).

F4b (the generalization — the actual KI): the remaining wide-doubled
runs in the newest build are NOT Lua prints; they are fmt ARGS from
engine-side senders (corpus proof: claude2 log wide-run contents are
paths, hashes, mangled names — 'PA', 'startup-', 'mem', 'N2RC6Unreal13...',
'3b6cfad47dd17fea...'). The minimal, allocation-light fix at the SINK
instead of chasing every `ensure_str` caller:

  `typescan-r4/candidate/src/File/FileType/PosixFile.cpp`,
  `PosixFile::write_string_to_file` (line ~293): before `encode_utf8`,
  collapse NUL-interleaved runs with an in-place pre-pass over the
  encoded buffer — drop every U+0000 code unit (the unit-doubling
  artifact), i.e. encode first, then strip 0x00 bytes from the
  to-be-written byte vector inside the F1/F3 device lock. Rationale:
    - encode_utf8 maps U+0000 units to single 0x00 bytes, so a
      post-encode 0x00 byte can ONLY be a doubling artifact (narrow
      text never emits one; see decoder grammar, corpus-verified);
    - zero allocation (in-place compaction), no engine calls, R3-safe;
    - single point of repair — every sender is fixed at once, including
      future call sites;
    - the alternative (include-order fix so port/first/include wins over
      deps/first in EVERY TU) is the CE FIX-PROPOSAL's recommendation and
      should be done in parallel, but it is not byte-provable per-TU
      without a full rebuild audit — the sink strip is the belt to that
      braces.

Label the strip so it is auditable: a one-shot startup line
`[ST] logwriter nul-strip active` emitted once from UnrealInitializer
next to the existing [ST] self-test family, so the console log itself
proves which grammar the device is emitting.

Explicitly OUT OF SCOPE here: message atomicity (already F1/F3),
the klog/diag-jsonl/aligned-entry sinks (they are different writers,
verified NUL-free in the corpus), and any LuaMod/LuaLibrary behavior
beyond F4a.

## 3. Console-verification plan (for the next build window; no console contact now)

Preconditions: regression gate GREEN (host-tests/run-regression.sh);
CHANGES.json rows + fake-sign + decrypt-OFF magic/sha verification per
the PS5 rules; ONE launch (no rapid-fire relaunches; clean game exit
before any second launch).

1. Build with F4a+F4b + the `[ST] logwriter nul-strip active` line.
2. Launch the standard menu-up sequence (vanilla boot -> loadlist mod
   set -> F7 menu open attempt).
3. Harvest with the decode-first tools AND keep the raw pull:
   - PRIMARY: `python3 harvest/decode_session_log.py UE4SS.log - --json`
     must report `nul_count == 0` (and `wide_pairs == 0`) for the whole
     session — the grammar-uniformity receipt;
   - the `[ST] logwriter nul-strip active` line present once;
   - marker parity: every marker family that appeared in the previous
     NUL-interleaved sessions still appears (esp. `[Lua]
     [TownfallItemSpawner] Loaded v1.0`, `[EU-VMT] arming refused:
     non-text-ent ...` — the refusal reason must now be contiguous
     ASCII in the RAW bytes);
   - no regression in the verdict set: `harvest.py --local-ue4ss
     <fresh-log> --skip-klog` decoded vs `--raw` must be IDENTICAL
     (when the file has zero NULs the two modes converge — that
     identity IS the acceptance signal);
   - race check: repeat the zzdiag-style long-line + F7-press session
     once; confirm no orphan leading-NUL fragments (decoder
     `orphan_nuls == 0`) and no mid-line `[l22]`-inside-`[Lua]`
     collisions (F1/F3 receipt; a nonzero orphan count means the mutex
     half needs attention, not the strip).
4. Rollback: the strip is a pure byte filter — revert = remove the
   pre-pass; the decode-first tooling keeps working either way (a
   NUL-free file decodes to itself, property-tested).

## 4. Risk register (honest)

- A genuine U+0000 in a logged payload would be silently dropped by the
  strip. Corpus + code audit: no sender logs NUL-containing text (Lua
  strings are NUL-terminated C strings; fmt args are paths/names/hashes;
  engine strings are UTF-16 text). Risk accepted and documented in the
  [ST] line's design note.
- The strip runs under the device lock -> one extra memmove-class pass
  per message; negligible vs the existing encode+write cost.
- If a future sender legitimately requires binary-safe logging, the
  right move is a separate device, not re-widening this one.
