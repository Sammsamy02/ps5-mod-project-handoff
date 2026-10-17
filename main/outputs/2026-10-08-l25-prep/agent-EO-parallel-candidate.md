# Agent EO — parallel diagnostic candidate (l27 no-bind control, A-shaped, + EC)

2026-10-09. READ-ONLY analysis; proposal patches only. Wrote exactly:
`outputs/2026-10-08-l26-runtime/parallel-candidate/{01,02,03,04,05}-*.patch|.md`
+ this report. No tree edits (EM is live-editing
typescan-r4/candidate/src/UnrealInitializer.cpp — verified untouched by me:
sha at read time ee6af9be…, mtime 09:58, unchanged after my session).
No console/network contact, no builds, no driver --execute.

## 0. What actually failed tonight (session forensics — inputs for EM/parent)

Tonight's build was 10b11e437f… (31,424,919 B, LAUNCH-READY-A receipt
15:35:36Z; = bind_launch --all: EI bind + EB marker + EC + variant C + Z15).
Session facts (/tmp/bind1.log, /tmp/l25-variant-A/eboot-monitor-…, klog
20261009-142135):

- eboot lived 15:36:21Z → 15:39:21Z. UE4SS init at 15:37:43:
  BuildProfile authenticated (image 3b6cfad4…, base 0x400000), pool
  admitted, slot 2 ctor bound + verified at 0x63D590.
- **Phase 4c REFUSED**: `[Seeds][L27] ToString bind refused: prologue
  signature: <wide-leak>; legacy Kismet route retains FName::ToString`.
  The reason is the NativeFNameChecks.hpp `signature()` gate's
  line-17 pre-check — de-widened it is uniquely
  **"constructor prefix has recorded copyin fault"**: `ops.known_fault`
  → `bridge_copyin_range_faulted(0x63FA00,32)` returned TRUE, exiting
  BEFORE code_identity/query (confirmed: no `[Seeds][L18] code-signature`
  and no `[Seeds][L14] VirtualQuery needed=[0x63FA00…` line exists for the
  ToString label — only slot 2's pair is present). The refusal is
  non-terminal by design; nothing published.
- **The paradox for EM**: the fault ledger (`g_copyin_faults`,
  runtime_start_bridge.cpp, sha 94f81a17 matches the MANIFEST pin — not a
  composition drift) records ONLY on EFAULT inside a copyin probe
  (line 1386), yet the L25 state line in the SAME log reports
  `oracle-probes=0 used-copyin=0` and the only other caller (io_ctrl_D,
  0x81D2A40) is not startup-run. So known_fault was TRUE for
  [0x63FA00,0x63FA20) with zero recorded probes this session.
  `l13_sink::FaultLedger::overlaps()` (CopyinSink.hpp:34-40) also returns
  true when `saturated` — and `record()` saturates on an INVALID range —
  a candidate mechanism EM should check first. Secondary string bug in
  the same line: `ensure_str(ts_signature.reason)` (narrow `const char*`
  → wide) rendered as a truncated interleaved-NUL leak — the refusal line
  itself demonstrates the narrow-arg defect class EI fixed at two sibling
  sites.
- **The bigger fact**: init then SILENTLY ABORTED. After
  `[Seeds][L15] FName binding total … outcome=verified-…` (scope exited
  normally, not the `refused-or-exception` default) there is ZERO further
  init logging — no Phase 5/6, no variant-C admission line, no
  `[L25][DeleteListener]` registration, no `[l25]` ladder, no
  `[l22] Lua mod started` — straight to `Stopping mod 'TownfallItemSpawner'
  for uninstall`. No [FFO] markers (mods never ran). This is a REGRESSION
  vs 5ea2c845 (which init'd fully, registered, ran mods, clean). Cause
  sits between the L15 scope exit and the next logged step; Phase 4c's
  residual presence, variant C Phase 6 (also NEW in this build), and the
  Z15 gated bridge are the suspects. THIS is why a no-bind control is the
  right parallel candidate — it is a single-delta test of suspect #1.

## 1. Candidate design (task 1)

Token **MODBRIDGE_L27_NO_TOSTRING_BIND** (NBTOKEN). Patch 01 wraps EI's
ENTIRE Phase 4c block (current lines 1232-1350, EM's fix-inside included
when it lands) in `#if defined(NBTOKEN) / #else / #endif`:

- NBTOKEN arm = comment only: ToStringNative stays null → NameTypes.cpp's
  `ToStringInternalWrapper` routes FName::ToString through the legacy
  Kismet path exactly like every pre-L27 build. NO `[Seeds][L27]` /
  `[FN] tostring-bind` logging exists in the arm (control must be
  indistinguishable from a pre-L27 build at this layer).
- Slots 0/2/3/4, Phase 5/6, registration untouched → **A-shaped**
  (CTOKEN on). Anchors are CONTEXT-based (slot-2 "bound at" message /
  Phase 5 header), so EM's in-block edits cannot break the rebase.

Driver (build_variants.py, VARIANTS read in full): patch 02 adds an
**env-gated third variant** (`L26_BUILD_NB=1`) instead of editing the
production A/B tuple — EM's fixed build keeps the identical pipeline.
Minimal mechanics: NBTOKEN constant; NB_VARIANT dict
(`define:['-D'+CTOKEN,'-D'+NBTOKEN]` — list-valued, ~4-line
build_variant change to splice a list at the argv site + safety
override); `targets_map[NBTOKEN]` with define_units =
union(NBTOKEN units, CTOKEN units) — WITHOUT the union UObjectArray.cpp
would miss CTOKEN and compile the KNOWN-BROKEN hand-rolled registration
(not A-shaped); `G4-nbtoken-present` execute-time hard fail (no silent
NB==A no-op); control gates **G11** (NB runtime ≠ A) and **G12**
(per-unit object deltas NB-vs-A == exactly `['unreal-unrealinitializer']`
— the single-delta guarantee). A/B G9/G10 pair receipt untouched.
Patch 03 mirrors the packaging entry (`L26_PACK_NB=1`,
l26-package-NB, FINAL-RECEIPT-VARIANT-NB). Apply/build procedure = 05.

## 2. EC independence from the ToString bind (task 2) — VERIFIED, with one benign coupling

Read the staged `typescan-r4/candidate/src/UObjectGlobals.cpp`
(8bb3b251…). EC's long-name arm (lines 389-422) resolution chain:

1. `SplitLongNameIntoPathParts` (:283-311) — pure in-port string split,
   zero engine calls.
2. `FName PartName{Part, FNAME_Find}` (:396) — the SLOT-2 bound ctor in
   Find mode. Depends on the slot-2 bind (0x63D590, admitted + verified
   tonight), NOT on the ToString bind.
3. Loud refusal on `IsNone()` (:397-406) — no ToString.
4. `StaticFindObject_InternalNoToStringFromNames` (:208-255) — outer-chain
   match via `NamePart.Equals(PathObject->GetNamePrivate())`, FName
   index-only; the comment at :223-224 states the design point explicitly.
   NO ToString.
5. Exact-class gate (:411) — `Class == PathFound->GetClassPrivate()`
   pointer compare. NO ToString.

The ToString touchpoints that remain, and how they degrade bind-absent:

- **`CacheGlobalObject(PathFound)` (:413)** — key derivation calls
  `Object->GetFullName()` (ClassListener.cpp:114-127) → FName::ToString.
  With the legacy Kismet route EMPTY in-world (the documented console
  state — empty, not throw; EI2 harness + ts:'' evidence) the cache key
  degenerates to an empty/garbage string. Benign: map ops cannot throw
  here, the RETURNED object is unaffected, and GetGlobalObject(name)
  lookups by real names simply keep missing (no false-positive key can
  equal a non-empty query). **Resolution itself does not depend on the
  bind.**
- **The legacy scan fallback** (clean miss falls through, :427-531): its
  package gate `GetOutermost()->GetNamePrivate().Equals(PackageName)`
  (:479) uses FNAME_Add-constructed PackageName — dead on console per
  GATE-1 numbered-duplicates, so the scan filters everything out BEFORE
  the `GetFullName()` compare at :506 is ever reached (EC's own comment
  at :419-421). The scan degrades to "returns nullptr", not throw.

**What the NB candidate therefore PROVES about EC**: the SPLITTER fix +
FNAME_Find-only resolution arm, end-to-end, independent of the ToString
bind — i.e. the S-path repair (S1e/S2e/S3e/U2e throw class). It does NOT
exercise anything string-compare-shaped in EC's path (there is none), and
it does NOT prove DA's FindXOf string fallback (that one genuinely needs
the bind; in NB it engages with `ts:''` — corroboration, not repair).
**One honest caveat**: the walk's index-only `Equals` retains N1 exposure
(constructed Find index vs stored index — the same unverified link as
FindXOf). Under N1 the arm degrades to a quiet clean-miss (nullptr), not
the loud refusal — the interpretation table (04) separates these
outcomes explicitly.

## 3. Console signature (task 3)

Full table in `parallel-candidate/04-console-signature-interpretation.md`.
Fixed signatures: ALL `[Seeds][L27]`/`[FN] tostring-bind` lines ABSENT
(the control itself); slot-2 bound PRESENT; variant-C admission line or
loud refusal PRESENT. Primary discriminator = how far init gets:
completes → Phase 4c's residual presence caused tonight's silent abort
(EM's fix is the cure); aborts identically → Phase 4c exonerated, hunt
moves to variant C Phase 6 / Z15 bridge / composition; aborts earlier →
tree drift, reconcile before anything. In a healthy boot, in-world:
`[FFO] query-fallback [n:X num:Y ts:'']` PRESENT (empty tail = the
Kismet liar named in-binary), `db=n`/FindXOf invalid EXPECTED (no bind),
U1 works; S-probes: v = EC works without the bind; quiet n = N1-or-absent
(cross-check [FFO]); loud `long-name part 'X' not in FName pool` = part
not interned (new evidence); the OLD `GetPackageNameFromLongName: Name
wasn't long.` throw = EC not in the binary (stop, reconcile).

## 4. Verdict (task 4) — build NOW, launch SECOND

**Worth the build slot: YES, unconditionally.** (a) Claude directive 4 is
exactly this: tonight's session regressed (silent init abort) and if the
EM-fixed build repeats the abort, the NB control is the immediate
single-delta discriminator — having it pre-built saves the 18-minute
critical-path gap at the moment of failure. (b) Its semantics are
INDEPENDENT of EM's fix content (the arm wraps EM's code out; only tree
STABILITY is needed), so it parallelizes perfectly with EM. (c) Offline
cost only (~18 min, no console contact); env-gated so EM's build pipeline
is untouched.

**Not worth the next LAUNCH slot: correct as ordered.** EM's fixed-bind
build gets the next launch — it is the decisive fix candidate. NB launches
only on the branch where (i) EM's build still refuses/aborts (then NB is
already built and its outcome A/B cleanly splits Phase-4c vs variant-C/Z15
suspects), or (ii) EM slips long enough that an authorized console window
would otherwise idle. If EM's build binds AND boots, NB's control value
collapses to the S-probe/EC proof — nice-to-have, not worth a slot.

## Provenance

Inputs: /tmp/bind1.log (+_clean/_stripped), /tmp/l25-variant-A/{klog-20261009-142135.log,
klog-20261009-153535.log, eboot-monitor-20261009T153536Z.log, LAUNCH-READY
receipt dir), agent-EI/EJ/EC/DA/EB reports, NEXT-BUILD.md, STATE-L26-WAVE.md,
live read-only trees: typescan-r4 candidate (UnrealInitializer.cpp,
UObjectGlobals.cpp, ClassListener.cpp, include/EngineToStringProfile.hpp,
Helpers/String.hpp), copyin-probe candidate bridge + MANIFEST (sha
94f81a17 verified), l26-package-A lineage (NativeFNameChecks.hpp,
CopyinSink.hpp), build_variants.py (549b54dd) + l26_packaging_driver.py
(68146c3f) read in full. Root-cause depth on the known_fault paradox stops
at mechanism candidates (ledger saturation / unlogged recorder) — that
attribution belongs to EM.

— agent EO (ZCode), 2026-10-09.
