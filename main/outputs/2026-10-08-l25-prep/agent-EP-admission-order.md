# agent-EP — Phase 4c admission-order verification (bind1.log prologue refusal)

Date: 2026-10-08/09. Scope: READ-ONLY verification of the hypothesis that EI's
Phase 4c `rc::fnameabi::signature` call is mistyped (char16_t constant /
doubled sizeof) and refused pre-VirtualQuery. Sources: the exact l26-B compiled
sources (verified via `l26-B/ACTUAL-DEPENDENCIES.json`), the rc::fnameabi
implementation, and `/tmp/bind1.log` raw bytes.

## VERDICT (one line each)

1. **Constant-type hypothesis: REFUTED.** Both `rc::fnameprofile::kPrefix` and
   `rc::engtostring::kToStringInternalPrefix` are `inline constexpr uint8_t[]`
   of exactly 32 bytes; both call sites pass `sizeof()==32` as a **byte**
   count. No char16_t constant is passed to `signature()` anywhere.
2. **Actual root cause: CONFIRMED — owner-gate fail-closed after
   `copyin_cleanup.complete()`.** The refusal is `signature()`'s FIRST
   pre-query guard, `ops.known_fault()` → `bridge_copyin_range_faulted()`,
   which returns 1 ("faulted") unconditionally once the l26 early-release
   patch has disarmed the initializer owner gate. Phase 4c runs AFTER
   `complete()` (line 1317 → block at 1329); slot 2 ran BEFORE it. Slot 2's
   own success retires the gate Phase 4c then needs — that is the slot-2
   "interaction".
3. **Exact fix:** move `copyin_cleanup.complete();` (staged
   `UnrealInitializer.cpp:1317`, with its adjacent L25 note call at 1323-1325)
   to AFTER the Phase 4c block (after line 1448) — ideally after Phase 5/6's
   `cached_code_signature` checks, which fail closed on the same gate.
   Alternative (bridge-side, restores the documented l25 invariant): delete
   the early `g_copyin_initializer_owner.leave();` at
   `runtime_start_bridge.cpp:1138` (+ its event at :1137).
   **No constant retyping and no length change is needed or correct.**

## 1. signature() full control flow (early returns in order)

Implementation: `rc::fnameabi::signature` is defined inline in
`outputs/2026-10-07-l19-local-profile-r4/units/NativeFNameChecks.hpp`
(identical bytes in `2026-10-08-l26-package-B/lineage/NativeFNameChecks.hpp`;
this is the file the l26-B build compiled, per ACTUAL-DEPENDENCIES.json —
`FNameAbiProfile.hpp` only supplies the `rc::fnameprofile` constants).
Signature: `template<class Ops> Check signature(Ops&, pooladmission::Budget&,
uint64_t address, const uint8_t* expected, size_t size)`.

Early returns, in evaluation order, with exact reason literals:

| # | Guard | Reason literal | Fires before VirtualQuery? |
|---|-------|----------------|----------------------------|
| 1 | `!expected \|\| size==0 \|\| size>64 \|\| !valid_range(address,size)` | `"invalid constructor profile"` | YES |
| 2 | `budget.expired(ops.now_ms())` | `"constructor signature deadline expired"` | YES |
| 3 | `ops.known_fault(address,size)` | `"constructor prefix has recorded copyin fault"` | **YES** ← fired |
| 4 | `!ops.code_identity(address,size)` | `"constructor startup image identity/base unavailable or mismatched"` | YES (logs nothing itself) |
| 5 | `q=ops.query(address); ops.log_region(...)` | — | this IS the L14 `VirtualQuery needed=[...]` line |
| 6 | unmapped / not fully committed exec | `"constructor unmapped"` / `"constructor lacks full committed executable mapping"` | after query |
| 7 | XOM path (prot&4, !prot&1): `cached_code_signature` fail | `"constructor authenticated XOM prefix unavailable or mismatched"` (+ L18 `code-signature=...` line from `log_code_segment`) | after query |
| 8 | live-copyin path: probe/capture/mismatch | `"constructor prefix copyin fault"`, `"constructor fresh live prefix unavailable; no cached or disk-only fallback"`, `"constructor live prefix unavailable; no disk-only fallback"`, `"constructor live prologue mismatch"` | after query |

The literal producing the logged text is **line 17 of NativeFNameChecks.hpp**:
`"constructor prefix has recorded copyin fault"` (44 chars). It is checked
BEFORE `code_identity` and BEFORE `ops.query()` — so no L14 line for 0x63FA00
and no L18 `pin=` line ever appear. That matches bind1.log exactly: between
the slot-2 success block and the L27 refusal there is no
`[L14] VirtualQuery needed=[0x63FA00,...)` line.

### The "constructor prefix has" truncation is a console artifact, not a distinct literal

Raw-byte analysis of `/tmp/bind1.log` (mixed ASCII/UTF-16LE console capture):
the refusal line is ASCII up to `prologue signature: `, then proper UTF-16LE
for exactly 22 code units `c\0o\0n\0s\0...` spelling "constructor prefix has",
then ASCII `; legacy Kismet route...`. Every narrow-string format argument in
this capture is rendered as UTF-16 truncated to exactly HALF its char count:
Git SHA `e3ba1016...68c4` (40→20), image SHA `3b6cfad4...e374` (64→32),
`authenticated-disk-XOM-signature`→`authenticated-di` (32→16),
`fname_ctor_wchar`→`fname_ct` (16→8), `verified-and-bound`→`verified-`
(18→9), and 44→22 here. (A widened buffer written with a byte-count length.)
Conclusion: the runtime reason string was the FULL 44-char literal; only the
console rendering halves it. Do not grep for "constructor prefix has" as a
separate string — it does not exist in any source.

## 2. Call-site byte-for-byte comparison (both in the exact compiled file)

Compiled initializer: `outputs/2026-10-08-l26-build-variants/staging-copyin-probe/UnrealInitializer.cpp`
(listed in l26-B ACTUAL-DEPENDENCIES.json).

- **Slot 2** (lines 1283-1284): `rc::fnameabi::signature(admission_ops,
  admission_budget, ctor_runtime, rc::fnameprofile::kPrefix,
  sizeof(rc::fnameprofile::kPrefix));`
  - `rc::fnameprofile::kPrefix` — `FNameAbiProfile.hpp:5`:
    `inline constexpr uint8_t kPrefix[] = {0x55,0x48,0x89,0xe5,...}` —
    **uint8_t[32]**, sizeof = **32 bytes**.
  - `ctor_runtime = base + kFNameCtorWchar.label(0x23D590) = 0x63D590`.
- **Phase 4c / EI** (lines 1380-1381): `rc::fnameabi::signature(admission_ops,
  admission_budget, tostring_runtime, rc::engtostring::kToStringInternalPrefix,
  sizeof(rc::engtostring::kToStringInternalPrefix));`
  - `rc::engtostring::kToStringInternalPrefix` — `typescan-r4/include/
    EngineToStringProfile.hpp:89-95`: `inline constexpr uint8_t
    kToStringInternalPrefix[] = {0x55,0x48,0x89,0xE5,...}` — **uint8_t[32]**
    with `static_assert(sizeof(kToStringInternalPrefix) == 32)`.
  - `tostring_runtime = base + 0x23FA00 = 0x63FA00`.

The arguments are type-identical at both sites: `(L13AdmissionOps&,
pooladmission::Budget&, uint64_t, const uint8_t*, size_t=32)`. The ONLY
differences are the address (0x63D590 vs 0x63FA00, 8 KiB apart in the same
exec PT_LOAD) and the 32 expected bytes. The only `char16_t` constants in
`rc::engtostring` are `kProbeExpect`/`kProbeId`-adjacent behavioral-probe
constants, never passed to `signature()`. **The u""-typed / doubled-sizeof
theory is false; a compile-time `static_assert` already forbids it.**

## 3. Length semantics and any pre-VirtualQuery bound

- `size` is a **byte count**, not an element count: guard 1 compares `size>64`
  directly; the live-compare loop reads `bytes[i]!=expected[i]` per byte;
  `cached_code_signature(address,expected,length)` forwards to
  `bridge_code_cache_signature(...,length)` → `g_code_cache.prefix(...)`,
  all byte-wise.
- There IS a length bound that fires pre-VirtualQuery: `size>64` (or 0) →
  `"invalid constructor profile"`. Both sites pass 32, so it does not fire.
  VirtualQuery (`ops.query`, guard 5) is skipped ONLY by guards 1-4 — never
  by length alone beyond that first check.

## 4. Why known_fault(0x63FA00,32) returned true with an empty ledger

`L13AdmissionOps::known_fault` (staged UnrealInitializer.cpp:1039-1042) calls
`bridge_copyin_range_faulted(address,length)`. Host implementation
(`2026-10-08-l26-package-B/lineage/lifecycle/runtime_start_bridge.cpp:1198-1202`;
byte-identical to the copyin-probe candidate bridge):

```cpp
extern "C" int bridge_copyin_range_faulted(uint64_t addr, uint64_t len)
{
    if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no ledger read
    return g_copyin_faults.overlaps(addr, len) ? 1 : 0;
}
```

The fault ledger itself was provably empty this run (log L25 line:
`oracle-probes=0 used-vq=1 used-copyin=0`; `FaultLedger::record` is only
called on a copyin EFAULT, CopyinSink.hpp ledger semantics confirmed —
`overlaps` also fail-closes when `saturated` or `!valid_range`, neither
applies to (0x63FA00,32)). The only path that returns 1 here is the owner
gate being closed.

The gate was closed by the **l26 gating patch** inside the retirement block of
`bridge_copyin_initializer_complete()` (runtime_start_bridge.cpp:1124-1138):

> "[L26 gating proposal] Admission is complete and the bank is retired: no
> legitimate copyin consumer remains for this launch ... Disarm the owner gate
> NOW instead of holding it armed until shutdown: probe_until and every gate
> sibling then fail closed (-1 / 0 / faulted) for the rest of the session."
> `::diagnostic_event("copyin_probe_gate_l26_early_release", 1u);`
> `g_copyin_initializer_owner.leave();`

"every gate sibling" includes `bridge_copyin_range_faulted` AND
`bridge_code_cache_context` (code_identity) AND `bridge_code_cache_signature`
AND `bridge_write_copyin_capture` — all return fail-closed once
`complete()` has run.

Timeline in bind1.log (all 10:37:43):

1. Slot-2 `signature(0x63D590, kPrefix, 32)` runs BEFORE `complete()` → gate
   armed → ledger empty → passes; `[L14] VirtualQuery needed=[0x63D590,0x63D5B0)
   region=[0x400000,0x787C000) protection=0x4` then `[L18] code-signature=
   authenticated-di pin=fname_ct ...` → "signature matched", slot 2 bound.
2. `copyin_cleanup.complete()` (staged line 1317) → bridge retires the sink
   bank AND (l26) releases the owner gate. The L25 console note printed right
   after (lines 918-923) still asserts the l25-era invariant "initializer
   owner gate remains armed ... until shutdown" — that text is now stale and
   documents the very assumption the l26 patch broke.
3. Phase 4c (line 1329+) → `signature(0x63FA00, kToStringInternalPrefix, 32)`
   → guards 1-2 pass (32 ≤ 64; 2721 ms of the 60000 ms budget used, log line
   561) → guard 3 `known_fault` → gate closed → return 1 → refused with
   `"constructor prefix has recorded copyin fault"` (console-halved to
   "constructor prefix has"). No L14, no L18 for 0x63FA00 — exactly as
   observed.

Note the blast radius: Phase 5 (slot 4 SCO, staged lines 1458-1465: `code_identity`
+ `cached_code_signature`) and Phase 6 (variant-C listener admission) also run
after `complete()` and hit the same fail-closed gate siblings; bind1.log shows
no slot-4 bind line before the capture ends. The l26 memo's premise "no
post-admission caller exists across the composed runtime" was falsified the
moment agent-EI's Phase 4c (and Phase 5/6 gates) landed after `complete()`.

## 5. The exact fix

The requested "retype the constant / pass the byte length" fix does not apply —
both constants are already `uint8_t[32]` and both lengths are already 32 bytes
(static_assert-enforced). The defect is ordering/gating, and it is minimal to
fix:

- **Primary (initializer, one-statement move):** in
  `staging-copyin-probe/UnrealInitializer.cpp` (and the typescan-r4 candidate
  twin), move `copyin_cleanup.complete();` from line 1317 — together with the
  `CopyinProbeLiveness::note_admission_complete(...)` call at 1323-1325 — to
  after the last gate-sibling consumer in `consume_required_slot_seeds`
  (minimally after the Phase 4c block's close at line 1448; correctly after
  Phase 5's `cached_code_signature` check and Phase 6's listener admission,
  since `bridge_code_cache_signature` fail-closes on the same gate).
- **Alternative (bridge, restores documented l25 invariant):** delete
  `::diagnostic_event("copyin_probe_gate_l26_early_release", 1u);` +
  `g_copyin_initializer_owner.leave();` (runtime_start_bridge.cpp:1137-1138)
  so the gate stays armed until shutdown ("armed gate, retired bank") — the
  exact behavior UnrealInitializer.cpp:879-881/913-915 still documents and
  Phase 4c/5/6 were built against. Choose this if the l26 early-release
  intent (stop post-admission kernel-write dispatch) must be re-derived
  rather than assumed.

Either fix re-runs Phase 4c's full slot-2-grade ladder (VQ → authenticated
disk-prefix match → ByteProperty probe → pool-decoder dual-check) inside the
armed window; the constants and the gate logic are otherwise sound.

## Evidence file references

- Log: `/tmp/bind1.log` (raw, mixed ASCII/UTF-16LE; refusal at byte offset
  ~111,485; halving artifact verified across 6+ narrow args).
- Compiled initializer: `.../2026-10-08-l26-build-variants/staging-copyin-probe/UnrealInitializer.cpp`
  (slot-2 signature 1283-1284; complete() 1317; Phase 4c 1329-1448; Phase 5
  1449+; L25 note text 903-924; known_fault 1039-1042).
- rc::fnameabi: `.../2026-10-07-l19-local-profile-r4/units/NativeFNameChecks.hpp`
  (signature 12-47; the fired literal at line 17).
- Constants: `.../units/FNameAbiProfile.hpp:5` (kPrefix uint8_t[32]);
  `.../2026-10-08-l26-runtime/typescan-r4/include/EngineToStringProfile.hpp:89-95`
  (kToStringInternalPrefix uint8_t[32], static_assert).
- Bridge: `.../2026-10-08-l26-package-B/lineage/lifecycle/runtime_start_bridge.cpp`
  (bridge_copyin_range_faulted 1198-1202; l26 early release 1124-1138;
  code-cache siblings 1268-1296) and `.../lineage/CopyinSink.hpp:22-46`
  (FaultLedger).
- Build provenance: `.../2026-10-08-l26-build-variants/l26-B/ACTUAL-DEPENDENCIES.json`
  (pins the exact header/cpp files above).
