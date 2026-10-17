# Agent FK — seam-refusal logging upgrade (Claude round-3 Q2)

2026-10-10, console REST MODE, offline. Scope honored: **only**
`2026-10-08-l26-runtime/typescan-r4/candidate/src/NameTypes.cpp` edited
(+86/-7 lines in two regions; the other 669-line content untouched —
diff-verified against frozen snapshot `snapshot-20261010T042420Z`).
`NameTypes.hpp` untouched; LuaFNameBridge refusal sites left to FL;
no CHANGES.json edits (root syncs); no `--execute`; no console contact.

## What changed

`NotifyFNameSeamRefusedOnce` (the `[FN] seam refused` producer, the only
one in the tree — grep-verified) upgraded from once-per-process to:

1. **First 10 occurrences per refusal reason family.** Families are
   string-matched: `embedded-nul` and `embedded` share ONE family
   (deployed build spelled it `embedded`, candidate header
   `embedded-nul` — console lines stay comparable across builds);
   `overlong` its own; unknown/null reasons a shared third bucket so a
   NEW reason still logs its own first 10. Gate = per-family
   `fetch_add` on `std::atomic<uint32_t>`; races only risk a duplicate
   line, never a missed family (same discipline as the OOB dedupe).
2. **Call-site tag on every logged line**: `call-site-tag=0xNNN` = low
   12 bits of `__builtin_return_address(0)` computed inside the helper
   — i.e. the return address into the header's inline
   `construct_with_sized_string` audit emission. Each TU's emission has
   its own address, so tag + the build's `runtime.map` symbolization
   names the exact TU/function that fed the seam (this is the "len=4
   source identifiable" lever; it works on the very next console run).
3. **Hex units machinery**: TU-local (anonymous-namespace)
   `FormatSeamRefusalUnits(const CharType*, uint32, char(&)[96])`
   renders `units=` as colon-joined fixed-width hex
   (2*sizeof(CharType) digits/unit) of the first **min(len,8)** units.
   Crash-safety: reads never leave the caller-length-validated extent,
   a null view is never dereferenced, fixed 96-byte buffer (worst case
   8 units x 8 digits + 7 colons + NUL = 72), no allocation inside the
   noexcept path. Marker prefix and the entire existing line text
   (reason/len/at + parenthetical) are byte-identical; new fields are
   appended after the parenthetical, before the newline.

## New marker format

```
[FN] seam refused: reason='embedded-nul' len=4 at=1 (NAME_None produced; embedded NUL in a wide buffer is the stride-corruption signature) call-site-tag=0x1a2 units=n/a(3arg)
```

With a wired buffer view the units field renders e.g.
`units=0050:0000:006c:0000` (stride-corrupted 'P',0,'l',0) vs
`units=0050:006c:0061:0079` (clean wide 'Play') — the exact
double-conversion-vs-stride discriminator Claude asked for.

## Honest limitation — `units=n/a(3arg)` on this build

The only `[FN] seam refused` call sites are the TWO inside
`construct_with_sized_string`, which is inline in the frozen
`NameTypes.hpp` (`"overlong"` at :344, `"embedded-nul"` at :356) and
bind to the exported 3-arg symbol
`_ZN2RC6Unreal26NotifyFNameSeamRefusedOnceEPKcjj` (present in the
deployed runtime.map). Per this round's constraint (NameTypes.hpp
untouched; helpers TU-local), the signature is frozen — the 3-arg entry
cannot receive the buffer, so it renders `units=n/a(3arg)`. The
dump itself is implemented, live (called on every logged refusal with
the null view), and crash-safe. **Staged follow-up for the next
header-touching round (root/FM): pass `StrName`/`Owned` as a units
pointer at the two header audit sites (2-line change + 1 param).** The
call-site tag already identifies the source without it.

## Behavior, verified unchanged

NAME_None still produced by the caller's early return; helper still
`noexcept`, still swallows all formatting/logger failures; no new
reads, throws, or engine calls on the refusal path. Only the logging
gate/format changed.

## Verification

- **Prospero syntax check**: `prospero-clang++` (sdk-v0.43 wrapper)
  with the FULL flag set replayed from
  `2026-10-08-l26-build-variants/l26-A/pass1/objects/unreal-nametypes.command.json`,
  `-fsyntax-only` replacing the `-c/-o/-MD/-MF` tail; overlay
  `l26-A/headers.vfs.json` present and all 51 roots exist (no stale
  path to strip this time); env per FF lineage
  (`LLVM_CONFIG=<LLVM-21.1.8>/bin/llvm-config`, bin prepended to PATH —
  the wrapper otherwise resolves an empty bindir). **exit=0, 5
  warnings, ALL the inherited third-party set FE documented for this
  TU** (Atomic.hpp -Wswitch x2, UnrealType.hpp enum-conversion x2,
  FProperty -Wundefined-inline); zero diagnostics from NameTypes.cpp
  or the new code. Log: `/tmp/l26-fk/prospero-syntax.log`.
- **Logic smoke (host, /tmp/l26-fk/logic_smoke.cpp — verbatim helper
  port)**: 12 embedded refusals → exactly 10 logged; overlong logged
  independently; `embedded` spelling counted in the SAME family (13th
  of the family suppressed; fresh-state run logs its own first 10);
  `0050:0000:006c:0000` / `0050:006c:0061:0079` render correctly;
  len=16 caps at 8 units; null view → `n/a(3arg)`.
- **No other candidate files touched by FK** (LuaUObject.cpp/LuaMod.cpp
  mtimes are sibling FL/FM concurrent work, not mine).

## Artifact identity

- File: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/NameTypes.cpp` (755 lines)
- sha256: `d21483f3b774cec73dcdef42c1d2eabb14eb095f8a04e5bccb855e76ad85d3d9`
- Diff regions: `#include <cstring>` (+1); refusal-helper block
  (comment rewrite + TU-local family gate/units formatter + upgraded
  body), nothing else (snapshot diff, 111 diff lines total).

## Harvest notes

- Existing greps keep working: prefix `[FN] seam refused:` unchanged,
  reason/len/at fields unchanged.
- New greps: `call-site-tag=0x` (symbolize low-12-bits against the
  build's runtime map), `units=` (n/a until the header follow-up
  lands).
- The 10-per-family cap means the console session's repeated refusals
  surface up to 10 distinct call-site tags per family instead of 1
  line total.

— agent FK (ZCode), 2026-10-10
