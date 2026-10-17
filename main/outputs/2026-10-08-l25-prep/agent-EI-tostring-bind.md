# Agent EI — l27 engine FName::ToStringInternal bind (THE decisive fix)

Date: 2026-10-08, late night. IMPLEMENTED, harness-proven, cross-compiled,
rows synced. Written: the staged candidate edits (UnrealInitializer.cpp,
NameTypes.cpp/.hpp, UObjectGlobals.cpp), the new profile header, CHANGES
rows/scope key, results/ei-tostring-verify/**, and this report. Scratch in
/tmp/l26-ei-tostring/** only. No console contact, no staging, no rebuild.

## 0. Address verification first (the brief's slide confusion, resolved)

The brief said "ToStringInternal @0x63FA00 (file-image; runtime 0xA3FA00
with the +0x400000 slide — VERIFY against prior AA/CJ anchors". **The brief
was double-slid.** Verified against CJ's table AND re-derived from the
pinned eboot bytes (sha re-verified 3b6cfad4…3622fc; exec PT_LOAD file
0x4000 ↔ vaddr 0):

- **ToStringInternal @ IMAGE 0x23FA00 → RUNTIME 0x63FA00** (label + 0x400000
  slide, exactly like slot 2: ctor image 0x23D590 → runtime 0x63D590, and
  the FNameAbiProfile kPrefix re-verified byte-exact at file 0x241590).
- File offset of the prologue: **0x23FA00 + 0x4000 = 0x243A00**.
- "0x63FA00 file-image / 0xA3FA00 runtime" would be the image vaddr 0x63FA00
  — inside LOAD0 text still, but NOT this function. The admission harness
  proves that exact wrong slide FAILS CLOSED (image-identity refusal, see
  §4 scenario 3).

Function identity re-derived end-to-end with capstone 5.0.7 (not just
trusted from CJ): prologue `55 48 89 e5` + pushes; `mov r15d,[rdi]` (RDI =
this, 8-byte {ComparisonIndex,Number}); `mov r14,rsi` (RSI = Out);
`lea r13d,[rdx-1]` (EDX = OutSize, clamp = OutSize-1); pool access
`lea rcx,[rip+0x912647b]` → **image 0x9365EC0 = the 49-xref FNamePool
global**; `Blocks[id>>16]` via pool+0x10; header `len=header>>6`,
`wide=header&1`; `cmp r12d,r13d; cmovb r15d,r12d` = truncate; ANSI copy +
in-place widen dance; tail @0x23FBD4: `mov ecx,[rbx+4]` (Number),
`sete/setbe` short-circuit, `dec ecx` (internal→external) + snprintf-family
`_%i` append, own NUL at Out[len], `mov eax,r15d` return. **Zero allocator
involvement, no sret** (contrast slot 2's sret ctor — EB's discipline
applied and documented in the profile header receipt).

## 1. What landed (consumption law: typescan-r4 candidate + include)

1. **NEW `typescan-r4/include/EngineToStringProfile.hpp`**
   (sha256 309a355597006450…, registered in added_headers + hashes):
   label `kToStringInternalLabel = 0x23FA00`, the 32-byte
   `kToStringInternalPrefix` (slot-2 kPrefix size discipline) byte-extracted
   from the pinned image at 0x243A00 and re-verified before embedding, probe
   constants (`kProbeId 0x3`, `u"ByteProperty"`, 12 units),
   `kOutUnits = 1024+11 = 1035` (engine NAME_SIZE+11), image-sha mirror.
   Constants only — no switch tokens (EE profile-header doctrine).
2. **`candidate/src/UnrealInitializer.cpp`** (`ee6af9be6b68c6cc…`):
   - Phase 4c of `consume_required_slot_seeds` (after the slot-2 bind,
     before copyin release so the fresh-copyin signature route stays
     available): **CodeCallOnly class** check (classify_seed_address) →
     shared-budget → **live-pool-id pin** (words.byte_property_id must equal
     0x3; title drift refuses) → **`rc::fnameabi::signature`** 32-byte
     prologue gate (the SAME gate slot 2 uses) → **behavioral probe**
     `ToStringInternal(FName{0x3, 0})` must yield exactly "ByteProperty"
     (len 12, engine-written NUL) → **Claude-review runtime dual-check**: the
     engine output must ALSO equal the PORT'S OWN POOL DECODER
     (`rc::fnameabi::roundtrip` over the admitted pool words — the same
     init-probe decode that verified ByteProperty at slot-2 admission).
   - **NON-TERMINAL fail-closed refusal** (the bind is optional; slots
     0/2/3/4 stay terminal): nothing is published on ANY failure — loud
     `[Seeds][L27] ToString bind refused: …` / `[FN] tostring-bind disabled
     (mismatch/empty)` (the coordinator's distinct disable marker) — and the
     legacy Kismet route keeps FName::ToString. try/catch around the whole
     phase (exception ⇒ refusal, init continues).
   - Publication: `FName::ToStringNative.assign_address(0x63FA00)`, logs
     `[Seeds][L27] FName ToStringInternal signature matched: …` +
     `[FN] tostring-bind active: engine ToStringInternal @0x63FA00 owns
     FName::ToString (ByteProperty probe len 12 + pool-decoder dual-check
     ok; …)` — the harvest marker naming the LIVE route.
   - The "Unbound by design" seeds log now documents: the fname_tostring
     SCAN route stays unbound; the seeds Phase 4c owns it when admitted.
3. **`candidate/include/Unreal/NameTypes.hpp`** (`711fdbf204a3fa4b…`): new
   `static Function<uint32_t(const FName*, CharType*, uint32_t)>
   FName::ToStringNative` (documented; null on host/refusal).
4. **`candidate/src/NameTypes.cpp`** (`c937c9edb6bb0016…`):
   `ToStringInternalWrapper` on `__PROSPERO__` routes through
   `ToStringInternalWrapper_UsingEngine` when the bind is ready: caller
   buffer kOutUnits, engine ABI call, per-call fail-safe guard (impossible
   length or missing NUL ⇒ once-per-process `[FN] tostring-bind anomaly`
   notice + THAT call falls back to the legacy route — never crash), else
   `StringType{buffer, buffer+length}`. Host byte-identical behavior.
   static_assert sizeof(CharType)==2 in the Prospero block.
5. **`candidate/src/UObjectGlobals.cpp`** (`8bb3b251693a9cfc…`):
   EB fname-rootfix **hunk 1 applied** (patch(1) --fuzz=0 clean): the
   `[FFO] query-fallback [n:{:X} num:{} ts:'{24-char tail}']` enriched
   marker. One engine-compile fixup EB's host-check missed:
   `for (char& c : rendered)` → `for (auto& c : rendered)` (on Prospero
   ensure_str returns the WIDE StringType; char& cannot bind to wchar_t —
   found by the real prospero-clang++). EB hunk 2 (proof-lock comment on
   the pinned l18 lineage NativeFNameAdapter.hpp) NOT applied: it edits a
   BASE-pinned historical header outside this tree's ownership (would drift
   the l25-A dependency pins); the receipt lives in EB's DECISION-MEMO and
   is mirrored in my profile header's ABI notes.

Everything routes through FName::ToString, so this one bind repairs: DA's
round-trip verify, DA's string-scan fallback compare, GetFullName-dependent
paths, and the Lua FName→string marshaling (CI's ambers).

## 2. Fail-safe contract (Claude review, implemented exactly)

- Admission-time: prologue verified against the authenticated image BEFORE
  any call (the probe is the first execution, mirroring slot 2's accepted
  discipline).
- Startup runtime dual-check: engine(ByteProperty) == "ByteProperty" AND ==
  pool decoder output; divergence or empty DISABLES the bind loudly with
  the distinct `[FN] tostring-bind disabled (mismatch/empty)` marker
  (harvest sees which route is live: `active` vs `disabled/refused`).
- Per-call anomaly guard in NameTypes.cpp (impossible length / missing NUL)
  → legacy route for that call, once-per-process notice.
- Never terminal for the seed table; never a call through an unverified
  address (publication is the last statement of the admitted path).

## 3. Verification (all logs in results/ei-tostring-verify/)

1. **Admission harness (EE variant-C pattern, mmap'd module mock)** —
   `admission_main.cpp` compiles the **byte-verbatim extracted Phase 4c
   block** (`phase4c.inc`, marker-extracted from the live candidate) with
   the REAL lineage admission chain (rc::fnameabi::signature + roundtrip,
   SeedValidation.hpp, PoolAdmissionEngine.hpp) and a mock module: real
   16K-page W→X thunk at base+0x23FA00 implementing the byte-verified
   semantics, real-layout mock FNamePool at base+0x9365EC0 (None@0→id0,
   ByteProperty@6→id3, EName table at +0x14040), and the AUTHENTICATED
   disk-cache comparison reading the pinned eboot itself at 0x243A00.
   **39/39 PASS, plain AND ASan/UBSan**, covering: profile bytes ==
   pinned image (check (a)); HAPPY bind + active marker; round-trip
   fixtures id0="None", id3="ByteProperty", id10="PlayerController",
   number 3→"_2", number 1→"_0" (dec-ecx external semantics), max-legal
   name 1023 + widest suffix == exactly OutSize-1 (the NAME_SIZE+11
   guarantee, no truncation, engine NUL), garbage id → empty + no crash;
   WRONG-PREFIX (pin drift) non-terminal refusal; WRONG-SLIDE (the brief's
   0xA3FA00 family: +0x400000 base slide refused by image identity — and
   classify alone provably cannot catch it); classify boundaries (gap →
   Refused); DUAL-EMPTY (the console empty-ToString bug) → disabled marker,
   nothing published; DUAL-DIVERGENCE (engine output ≠ pool decoder) →
   disabled; ID-DRIFT (live ByteProperty id 7 ≠ pinned 3) → refused;
   BUDGET expiry → refused. Every refusal proves init continued.
2. **DA harness re-run with a REAL ToString** — `/tmp/l26-ei-tostring/
   da-rerun` (DA's fixture copied, never the original touched), region
   pins refreshed for the EB-enriched + layer-3-drifted candidate,
   FName::ToString routed like production (EngineBound = real byte-layout
   pool decode + external '_N' suffix; KismetReal; KismetEmpty = the
   in-world failure). **9/9 PASS ASan/UBSan**: DA1/DA2/DA2B/DA3/DA4/DA5/
   DA6 green (DA2B's byte-exact marker now validates the EB-enriched
   format, ts:'WrongNameEntry' tail included); **EI1_BIND_ACTIVE_KISMET_
   EMPTY**: with the engine bind live under the wrong-id N1 state + a
   lying Kismet route, the verified fallback discriminates and finds all
   objects (the console repair); **EI2_BIND_REFUSED_KISMET_EMPTY**: bind
   refused + empty Kismet reproduces the exact console symptom (markers
   fire, ts:'' names the liar, nothing found) — the A/B proof that the
   BIND is what repairs the walk.
3. **Host compile**: NameTypes.cpp and UObjectGlobals.cpp FULL `-c`
   codegen rc=0 via DA's proven 219-arg host recipe (logs staged).
   UnrealInitializer.cpp host-c is not reachable with that recipe's
   include universe (staging/thread headers); its host coverage is the
   byte-verbatim Phase 4c compile in the admission harness.
4. **Prospero cross-compile (the authority)**: all three touched TUs FULL
   `-c` through the REAL prospero-clang++ (canonical l25-A unit argvs +
   the l26 include injection build_runtime uses): **rc=0, 0 errors**
   (objects 625,648 / 2,198,264 / 2,231,416 B); every warning the
   documented inherited set (Atomic.hpp -Wswitch, UnrealType enum-bitwise/
   undefined-inline, asmjit) — none from this change. Two real errors
   were found and fixed BY this check (my fmt wide-char `const char*`
   class-name arg → ensure_str; EB's `char&` loop → `auto&`).
5. **Suites**: profile/tests/run_tests.py PASS (preflight10/INI/digest/
   join15/prior88); typescan-r4 test_custom PASS; test_scan FAILS
   PRE-EXISTING (actual_scan.cpp references NativePropertyGuard refusal
   APIs absent from its shim — the same concurrent-agent failure DA
   documented; zero references to this change, verified by grep).
6. **CHANGES.json synced**: candidate_sha256 refreshed in BOTH changes[]
   and files[] for all four files (UnrealInitializer ee6af9be…, NameTypes
   .cpp c937c9ed…, NameTypes.hpp 711fdbf2…, UObjectGlobals 8bb3b251…);
   EngineToStringProfile.hpp added to added_headers + hashes; new scope
   key scope_l27_engine_tostring_bind; originals/copy rows verified on
   disk. Remaining row drift is OTHER owners' (UObjectArray.hpp/.cpp —
   EE's files, refreshed by their owner at build time per NEXT-BUILD).

## 4. Notes for the rebuild owner

- The touched units for the variant define-scope: unreal-nametypes now
  includes EngineToStringProfile.hpp (typescan-r4/include is already a
  declared include dir; the header rides added_headers like EE's). No new
  OS imports; no switch tokens in the header.
- build_variants --plan will still report the pre-existing parallel drifts
  (UObjectArray rows; the Q .base expectation) — unchanged by this work.
- Console acceptance (authorized launch only), in order:
  `[Seeds][L27] FName ToStringInternal signature matched: source=… 32 bytes
  at 0x63FA00` then `[FN] tostring-bind active: …` ⇒ the engine route owns
  FName::ToString; any `[FFO] query-fallback … ts:'…'` marker now carries a
  REAL tail; `[FN] tostring-bind disabled (mismatch/empty)` or
  `[Seeds][L27] ToString bind refused: …` ⇒ admission/dual-check refused —
  capture the logged line, do NOT retry (fail-safe kept the Kismet route).
- Ready to rebuild: **YES** for this change (all three TUs prospero-clean,
  rows valid, harnesses green).

## Provenance

Inputs: agent-CJ (ToString family + verdict), agent-EB (ABI discipline,
DECISION-MEMO, fname-rootfix.diff), agent-DA (verified-query fix + harness),
agent-EE (profile-header + fixture doctrine, NEXT-BUILD), agent-AA anchors,
the pinned eboot, lineage headers (NativeFNameChecks/PoolAdmissionEngine/
SeedValidation/FNamePoolChecks), the live candidate tree. Binary work via
capstone 5.0.7 in /tmp/l26-ei-tostring; no writes outside the candidate/
include/CHANGES/results scope above and /tmp.

— agent EI (ZCode), 2026-10-08.
