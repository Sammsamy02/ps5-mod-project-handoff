# agent-ER: pure in-port FName pool decoder (L27-bind superseded)

Date: 2026-10-09 (local). Claude directive 2, verbatim spec: "ComparisonIndex
-> block/offset -> entry header (len + wide flag) -> chars, plus '_' +
(Number-1) when Number > 0. No engine call, no admission gate, any thread."

Tree (consumption law respected — only these files touched):
`outputs/2026-10-08-l26-runtime/typescan-r4/` — candidate/src/NameTypes.cpp,
candidate/include/Unreal/NameTypes.hpp, candidate/src/UnrealInitializer.cpp,
include/EngineToStringProfile.hpp (comment), tests/run.py,
tests/actual_er_pooldecoder.cpp (new), results/er-pooldecoder-verify/** (new),
CHANGES.json.

## The decoder and its read path

`candidate/src/NameTypes.cpp` — new `namespace FNamePoolDecoder` (lines
173-517 after the review additions, verbatim-region sha-pinned in
`results/er-pooldecoder-verify/er-region-manifest.json`):

- Read path (admission-ops-independent): byte-wise volatile loads
  `load_u16/load_u32/load_u64` (NameTypes.cpp:212-232) — the same
  discipline as SeedArming's `read_u64` (UnrealInitializer.cpp:815) and
  the guard's proven live re-reads of CurrentBlock/CurrentByteOffset at
  pool+0x8/+0xC (NativePropertyPointerGuard.inl:146-166, agent O). The
  pool address itself is single-shot-published by SeedArming only AFTER
  the admission round verified the pool shape
  (UnrealInitializer.cpp:1235, right beside the guard's own publication at
  :1226); every per-call access is bounded by the LIVE words, so no read
  can leave the verified extents. rc::fnameabi::roundtrip's session ops
  are init-only by design and are NOT used post-admission.
- Decode core `decode()` (NameTypes.cpp:261; the review additions
  re-flowed the line numbers cited below): block = id>>16,
  offset = (id&0xFFFF)*2 (stride 2); Blocks[i] u64 at pool+0x10+i*8
  (validated live/non-null/8-aligned, pointer-overflow checked — the
  roundtrip discipline); entry u16 header: wide = header&1,
  units = header>>6 (refused at 0 or >1024 = kMaxUnits); open-block bound
  vs live CurrentByteOffset (rc_seed::check_entry_bounds semantics);
  chars: wide = UTF-16 units 1:1, ANSI widened 1:1 (the engine's own
  in-place widen; L15's <=0x7f narrow rule was proof-grade, not decode
  semantics); suffix '_'+(Number-1) appended only when Number>0. Pure:
  returns false on every refusal, never throws.
- First-use dual-check `ensure_verified()` (NameTypes.cpp:346-397):
  lock-free CAS state machine decodes rc::engtostring::kProbeId (0x3) and
  requires exactly "ByteProperty"; success logs
  '[FN] pool-decoder verified (ByteProperty)' once; failure logs
  '[FN] pool-decoder refused: ByteProperty probe did not decode; legacy
  route retains FName::ToString' and refuses the decoder permanently for
  the process. Racers during the first verify take the legacy route for
  that call (proven by the race suite).
- Routing `ToStringInternalWrapper` (NameTypes.cpp:444-473):
  decoder-first on ALL paths; any refusal (unpublished / dual-check
  refused / per-call shape anomaly with a once-per-process
  '[FN] pool-decoder anomaly' notice) falls to the legacy Scan/Kismet
  routes for THAT call. The Kismet fallback is retained as fallback only
  — it no longer runs when the decoder is live.
- Header surface (NameTypes.hpp:230-247):
  `FNamePoolDecoder::PublishVerifiedPoolAddress(uintptr_t) noexcept`
  (single-shot, first-verified-wins) + `Ready() noexcept`. All internals
  are TU-local.

## What happened to the L27 bind code (deleted, publication removed)

- `FName::ToStringNative` — declaration, definition, and every consumer
  (ToStringInternalWrapper_UsingEngine, _UsingEngine_Legacy, the
  anomaly-notifier) are DELETED from NameTypes.hpp/cpp. No code references
  remain (only comments documenting the deletion).
- UnrealInitializer.cpp: the entire Phase 4c bind block (~145 lines:
  CodeCallOnly class check, budget check, live-pool-id pin,
  rc::fnameabi::signature prologue gate, behavioral engine probe, the
  Claude-review runtime dual-check, and the
  `FName::ToStringNative.assign_address` publication) is DELETED,
  replaced by the short-circuit log
  '[FN] tostring: in-port pool decoder (L27-bind superseded)'
  (UnrealInitializer.cpp:1287-1313). Slot-2 admission's own
  rc::fnameabi::roundtrip L15 verification (UnrealInitializer.cpp:1259) is
  untouched — it still gates the ctor bind. The "Unbound by design"
  summary line was updated to name the decoder.
- include/EngineToStringProfile.hpp is RETAINED with a supersede note:
  kToStringInternalLabel/kToStringInternalPrefix still document
  provenance; kProbeId/kProbeExpect/kProbeExpectUnits are LIVE inputs —
  they pin the decoder's first-use dual-check (consumed in NameTypes.cpp).

## Evidence

Compile (results/er-pooldecoder-verify/compile_verify.py — agent-EI's
proven recipe: canonical l25-A argvs, source-swapped, DA host profile
`-D__PROSPERO__ -D__SCE__ -fshort-wchar` + stubs + dual vfs overlay; REAL
prospero-clang++ cross-compile):

- unreal-nametypes / unreal-unrealinitializer / unreal-uobjectglobals:
  host -fsyntax-only rc=0, host full -c rc=0, prospero-clang++
  -fsyntax-only rc=0. Warnings inherited only (memory-order switch,
  FProperty inline, asmjit deprecation) — none from the new code.
  Logs: /tmp/l26-er-pooldecoder/compile/*.log.

Fixtures (all plain + ASan/UBSan, -fno-sanitize-recover):

1. Unit suite tests/actual_er_pooldecoder.cpp (the VERBATIM decoder region
   compiled against a real mmap'd byte-layout mock pool; three suites as
   separate processes — happy / refuse / race; 6/6 runs pass,
   results/er-pooldecoder-verify/HOST-TESTS.json):
   - known ids: 0 -> "None", 0x3 -> "ByteProperty" (console-parity
     layout: None occupies bytes 0-5, ByteProperty starts at byte 6);
   - Number suffix: Number=1 -> "ByteProperty_0", 3 -> "_2",
     1000000000 -> "_999999999", Number=0 -> no suffix;
   - wide (UTF-16) entry with non-ASCII unit decodes 1:1; ANSI byte
     >= 0x80 widens 1:1; second block (id>>16 == 1) decodes; live-word
     corruption (CurrentBlock >= 8192) refuses -> legacy, restore ->
     decode again (words re-read every call);
   - refusal shapes: block > CurrentBlock, open-block overrun past
     CurrentByteOffset, len==0, len>1024, null block, misaligned block —
     each falls to the legacy route with exactly one anomaly notice;
   - dual-check: verified log exactly once; corrupted probe entry
     (refuse suite) -> loud permanent refusal, Ready() false, legacy
     route keeps ToString;
   - any-thread: 8 threads x 2000 decodes all correct; race suite: 8
     threads racing the first use -> exactly one verify log, zero wrong
     decodes, decoder live after first use.

2. DA harness with the decoder live
   (results/er-pooldecoder-verify/da-rerun/, ASan/UBSan build; 9/9
   scenarios pass): DA1-DA6 — agent-DA's verbatim verified-query
   expectations now running through the REAL decoder as the ToString
   engine (the mock pool is published as a real byte-layout pool with
   live-refreshed words). ER1 — decoder live + the Kismet empty-string
   failure mode ARMED: the verified fallback resolves (fast path on
   matching ids, string fallback on wrong ids), objects found, EB
   marker self-documents — the console repair with NO engine bind.
   ER2 — probe entry corrupted before first use: dual-check refuses
   loudly, the decoder stays off, the legacy empty route reproduces the
   console symptom — proving the decoder is the component that repairs
   the walk.

## CHANGES.json sync

- candidate_sha256 recomputed for candidate/src/NameTypes.cpp,
  candidate/include/Unreal/NameTypes.hpp, candidate/src/UnrealInitializer.cpp
  and candidate/src/UObjectGlobals.cpp (both `files` and `changes` rows) and
  the added-header hash for include/EngineToStringProfile.hpp.
- New scope key `scope_er_pool_decoder` (full change description + the
  Claude-review additions below).
- The two UObjectArray.hpp/cpp row mismatches disclosed earlier (a parallel
  agent's in-flight edits) have since converged: as of the final sync every
  `files`/`changes` row and added-header hash matches the live files
  (verified programmatically, zero mismatches). tests/actual_scan.cpp was
  modified mid-session by that parallel agent and remains theirs.

## Residuals / honest limits

- No console contact, no upload, no build beyond the three touched TUs;
  the decoder's runtime behavior on the real console pool is fixture-
  proven only (the L15 console evidence already proved this exact decode
  of ByteProperty from pool words on every prior run).
- The property guard's own verified-pool publication is unchanged and
  still bounds FName indices before its suffix test; it now coexists with
  the decoder publication (two single-shot publications of the same
  admission-verified address).
- Per consumption law, frozen EI artifacts (results/ei-tostring-verify/)
  were left untouched; the EI1/EI2 scenarios live on as ER1/ER2 in the ER
  da-rerun copy only.

---

# Claude-review additions (same session, all incorporated)

Claude approved the decoder with four additions; all are implemented,
recompiled (host + prospero ALL PASS) and re-fixture'd (all suites pass).

## (1) Bounds-check hardening — Claude item 4

decode() (candidate/src/NameTypes.cpp:261, region now lines 173-517) now
refuses EVERY index against the LIVE words BEFORE any entry read:

- live CurrentBlock/CurrentByteOffset re-read every call (unchanged);
- corrupt words (CurrentBlock >= 8192) refuse everything before any read;
- block >= FNameMaxBlocks or block > live CurrentBlock -> OOB refusal
  before the Blocks[] slot is even read;
- offset pre-check with the minimal need (header, 2 bytes) against the
  OPEN block's live CurrentByteOffset cursor (last block) or the CLOSED
  block's own extent kBlockCapacityBytes = 0x20000
  (NameTypes.cpp:197, rc_seed::kFNameBlockCapacityBytes parity) — before
  the block pointer is dereferenced;
- a full-entry extent re-check once the header length is known (still
  before any character byte is read).

On ANY out-of-range index decode returns {ok=false, oob=true} — empty,
no read — and try_tostring emits the LOUD per-id marker:

  [FN] pool-decoder OOB id=0x{:X} block={} offset={}: refused before any
  entry read (live CurrentBlock/CurrentByteOffset bound); legacy route
  used for that call

(notify_oob, NameTypes.cpp:447; bounded 16-id dedupe so a poisoned id in
a hot loop cannot flood the log while a second distinct poisoned id still
surfaces). Shape refusals (len==0, null/misaligned block — the
unreachable-units defensive check kept) retain the once-per-process
'[FN] pool-decoder anomaly' notice (NameTypes.cpp:420).

Fixture proof (tests/actual_er_pooldecoder.cpp, happy suite): a PROT_NONE
page is mmap'd and installed as the block pointer; poisoned ids — block 2
(>> live CurrentBlock 1) at offsets 0 and 8, and an open-block overrun
with Blocks[1] swapped to the guard page — are all refused WITH the exact
markers ('[FN] pool-decoder OOB id=0x20000 block=2 offset=0:' and
'id=0x20004 block=2 offset=8:' asserted byte-exact) and the process
survives ASan: a missing bound would dereference the PROT_NONE page and
crash, so survival IS the no-read proof. Discovered + documented while
hardening: the u16 entry header's Len field is 10 bits, so units >
kMaxUnits (1024) is unreachable defensive code; the fixture now proves
the max ENCODABLE len (1023) is refused by the full-entry extent re-check
instead of the old (vacuous) len>1024 case.

## (2) Decoded query strings in the marker — Claude item 2

LogFindXOfQueryFallback (candidate/src/UObjectGlobals.cpp:672-698) now
renders the first 96 chars of the DECODED string (was 24) — with the
decoder owning ToString, ts IS the round-trip's real output:

  [FFO] query-fallback [n:{:X} num:{} ts:'<decoded, up to 96 chars>']

Interpretation matrix (documented at the marker, UObjectGlobals.cpp:646-659):
- marker NEVER fires on a launch -> the round-trip passed everywhere:
  the indices were always right and ToString WAS THE ONLY BUG (Claude's
  suspected resolution of the N1 theory);
- ts:'<real other name>' -> the constructed index really was wrong
  (genuine N1); the string fallback did its job;
- ts:'' -> the decoder was refused/unpublished and the legacy Kismet
  route lied (the '[FN] pool-decoder ...' markers name which and why).

ER3_FASTPATH_ROUNDTRIP_PASSES (new da-rerun scenario) proves the first
case as a fixture: correct constructed index + the Kismet empty failure
mode ARMED -> decoder round-trip passes -> the FAST path only, ZERO
fallback markers. da-rerun is now 10/10 scenarios (DA1-DA6 verbatim,
ER1, ER2, ER3), ASan/UBSan build.

## (3) EC's StaticFindObject fix ships in this build — Claude item 1

Verified present in the same candidate and compatible with the decoder:

- SplitLongNameIntoPathParts (UObjectGlobals.cpp:283-311): '/'+ '.'/':'
  splitter, package part keeps its slashes ("/Script/UMG" +
  "Default__WidgetBlueprintLibrary" for the menu path shape);
- dot-less GetPackageNameFromLongName degradation (:356-372) — the v7
  "GetPackageNameFromLongName: Name wasn't long." throw is gone;
- the long-name arm (:379-418): every part resolves FNAME_Find-only
  against the live pool, a missing part refuses LOUDLY naming the part,
  and the outer chain matches through
  StaticFindObject_InternalNoToStringFromNames (:208-255) whose compare
  is index-based (FName::Equals on ComparisonIndex) — no ToString on the
  path, so nothing changes for the decoder, and indices the ctor returns
  are now verifiable via the decoder elsewhere;
- EC's own fixture tests/actual_staticfindobject_path_fix.cpp re-run on
  the CURRENT tree: ALL PASS in plain and ASan/UBSan
  (results/HOST-TESTS-L26-STATICFINDOBJECT-PATH.json is EC's frozen
  record; the re-run binaries were /tmp, tree untouched).
- The menu path's remaining dependencies (class()/construct() full paths,
  UEHelpers GetEngine/DefaultObject) route through the same
  StaticFindObject/GetGlobalObject long-name machinery this fix repairs.

## (4) Marker strings for the harvest (exact text)

Production markers emitted by this change set (all LogLevel::Error except
where noted):

- `[FN] pool-decoder verified (ByteProperty)` — dual-check passed (once
  per process, Default level).
- `[FN] pool-decoder refused: ByteProperty probe did not decode; legacy
  route retains FName::ToString` — dual-check failed (once, permanent).
- `[FN] pool-decoder OOB id=0x{:X} block={} offset={}: refused before any
  entry read (live CurrentBlock/CurrentByteOffset bound); legacy route
  used for that call` — NEW, per-id (16-id dedupe).
- `[FN] pool-decoder anomaly: entry refused for id 0x{:X}; legacy route
  used for that call` — shape refusal (once per process).
- `[FN] tostring: in-port pool decoder (L27-bind superseded)` — the
  Phase 4c replacement notice (Default level).
- `[FFO] query-fallback [n:{:X} num:{} ts:'{}']` — ts now the decoded
  string (96-char bound), newline-sanitized.
- `[FFO] add-fallback [n:{:X}]` — unchanged (CP layer).
- `[StaticFindObject] long-name part '<part>' not in FName pool
  (FNAME_Find miss); refusing to scan` — unchanged (EC layer, thrown).

Rerun evidence after the additions: compile_verify.py ALL PASS (host
syntax + full -c + real prospero-clang++ on NameTypes/UnrealInitializer/
UObjectGlobals, warnings inherited only); unit suites happy/refuse/race x
plain+ASan/UBSan 6/6; da-rerun 10/10; EC fixture 2/2; CHANGES.json rows
re-synced (zero mismatches across files/changes/added_header_hashes).
