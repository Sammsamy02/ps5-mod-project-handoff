# Agent AD — Claude review item 4 (Mac-only): pre-Append allocator logging + growth refusal

Date: 2026-10-08. Scope: exactly one source file edited,
`outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp`
(the hardened variant-A callee `AddUObjectDeleteListener` + its file-local
helpers), plus its `typescan-r4/CHANGES.json` rows. `UnrealInitializer.cpp`
NOT edited (hash proof below) — the variant-B `#if` switch region is
untouched by definition and by bytes. No builds beyond the single-TU compile
verification below; no console contact; no host suites re-run.

Pre-edit state verified: `candidate/src/UObjectArray.cpp` sha256
`8f45e6ce03fb066dee130c57ded022d555ace23b41860403975cae8a03c45c9c`
(Z19's post-E1-fix pin, confirmed on disk before editing). The parallel
`agent-AB-gmalloc-audit.md` was NOT present in this directory at edit time
(checked twice, before starting and before writing the guarded reads), so per
the assignment the guarded-read fallback was used with an in-code
PENDING-ALIGNMENT comment naming AB's path; if AB later publishes exact
expressions, reconcile the two read helpers without touching refusal
semantics.

## 1. What was added

All inside `RC::Unreal` in `candidate/src/UObjectArray.cpp`; new line
numbers refer to the edited file.

### 1a. Extended pre-Append log line (:610-619)

The existing `header pre:` dump keeps its greppable prefix and gains:

- `growth_required={}` — `(pre_num >= pre_max) || pre_data == nullptr`
  (Claude item 4's "Num==Max or Data==null" definition);
- `GMallocVar={}` — `&GMalloc`, this TU's definition of the variable
  (`extern FMalloc** GMalloc;`, FMemory.hpp:68; defined in FMemory.cpp,
  bound by the seed admission at UnrealInitializer.cpp:1110);
- `GMalloc={}` — the variable's call-time value (the engine `FMalloc*`
  slot address the seed bound);
- `GMallocInst={}` — `*variable` read through the guarded reader;
- `vptr={}` — `*(void**)instance`, the first word of the FMalloc instance —
  the exact word the Append/Realloc virtual dispatch (FMemory.cpp
  IMPLEMENT_UNREAL_VIRTUAL_WRAPPER Realloc) consumes. This is the direct
  observation point for Claude's fault hypothesis (a).

Full marker string:
`[L25][DeleteListener] header pre: Data={} Num={} Max={} growth_required={} GMallocVar={} GMalloc={} GMallocInst={} vptr={}`

### 1b. Guarded reads — the tree's safe-reader idiom (:457-536)

- `l25_read_word(const void*)` (:487) — byte-wise machine-word load through
  `const volatile unsigned char*`, mirroring the seed-slot `read_u64` in
  UnrealInitializer.cpp:793-801 (this TU cannot reach that static helper, so
  the idiom is replicated, cited, and kept file-local). NOT a fault guard;
  callers screen first.
- `l25_pointer_word_plausible(uintptr_t)` (:476) — non-null,
  `alignof(void*)`-aligned, `<= L25CanonicalUserMax` (x86-64 4-level
  canonical user bound `(1<<47)-1`, :474), which also implies the 8-byte
  read cannot wrap.
- `l25_snapshot_gmalloc()` (:518) — reads the variable address, the variable
  value, then `*variable`, then `*(void**)instance`, screening EVERY stage
  before dereferencing it; unbound/null stages stay 0 and are skipped, so
  the snapshot never dereferences an unscreened pointer. Strictly no worse
  than the status quo, which already raw-derefs `*GMalloc` at the growth
  gate; and the slot+instance were V1/V2-verified at seed bind time
  (UnrealInitializer.cpp:1097-1109), so these reads revisit proven pages.

### 1c. Post-Append success-path trio (:678-691)

One new line after the existing `header post:` dump + `l25_dl_post` event,
only on the success path (inside the try, after Append returns):
`[L25][DeleteListener] GMalloc post: GMallocVar={} GMalloc={} GMallocInst={} vptr={}`
— a changed variable value, changed instance, or changed/cleared vptr across
the Append is direct evidence for/against the GMalloc-virtual-call fault
hypothesis.

### 1d. New refusal code 6 — `ImplausibleDataForGrowth` (:446, check :643-660)

Claude's rule, verbatim: "If growth is required and the header isn't
verified sane, refuse cleanly instead of calling Realloc."

Integration shape (ADDITIVE — codes 1-5 and every existing check keep their
order and meanings):

- The unconditional sane-header gate (refusal 3) is unchanged and still runs
  first for every Add.
- Inside the existing live-buffer growth branch (`growth_needed &&
  pre_max > 0`), AFTER the existing GMalloc-bound refusal (5) and BEFORE the
  RISK line + Append, the new `l25_data_heap_plausible(pre_data, pre_num)`
  screen (:558-566) requires the engine-owned Data buffer to be non-null,
  pointer-aligned, inside the canonical user range, and not wrapping over
  the live Num elements. Failure refuses cleanly with
  `L25RefusalCode::ImplausibleDataForGrowth = 6`, logs
  `REFUSED (6) : growth required (...) but Data=... is not heap-plausible
  (...); refusing instead of calling Realloc; skipping registration and
  continuing init`, emits `l25_dl_refused` = 6, and returns — Append/Realloc
  is NOT called.
- Interpretation (documented in-code, the one judgment call for the
  coordinator): the strengthened screen applies to growth WITH a live
  buffer. After the sane gate, `Data == nullptr` is reachable only as the
  canonical empty header `Num==Max==0`; that Append is a fresh allocation
  handing no engine-owned storage to Realloc, and refusing it would void
  variant A's purpose (first registration would always refuse) and delete
  O's fresh-alloc harvest marker. The fresh path therefore proceeds per
  existing logic, as the task's "keep the existing sanity gate
  unconditional; the new rule ADDS the growth+plausibility refusal"
  directs.
- `growth_needed` is now the alias of the richer `growth_required` flag
  (:636); on every header that reaches the branch the two definitions are
  provably equal (sane ⇒ null-Data ⇒ Num==Max==0 ⇒ num>=max), so branch
  semantics are byte-for-byte preserved.
- Readability itself remains unprovable from this TU (no VirtualQuery
  bridge on this unit's include path — the same admission O recorded for
  `l25_listener_header_sane`); "verified sane" here means the arithmetic
  screen passed, and the pre/post markers discriminate the rest.

### 1e. Prospero diagnostic_events (:619, :691; refusal reuses :658)

Two new tags in the `l25_dl_*` family through the existing extern-C
`l25_diag` → `diagnostic_event(tag, value)` pattern (declared
UObjectArray.cpp:16, Prospero-guarded, defined in-link by diagnostic.c — no
new import, no new undefined symbol):

- `l25_dl_gmalloc_pre` — value = low 32 bits of the call-time FMalloc
  instance pointer (0 when unbound);
- `l25_dl_gmalloc_post` — same encoding, post-Append success path;
- refusals reuse `l25_dl_refused` with the new value 6.

## 2. Variant B untouched — proof

`candidate/src/UnrealInitializer.cpp` was NOT modified: sha256 after this
assignment is still `efdef2535fe702695bc5ba2ae3de273002bf047254e74458ce8ebfb56d1c8b4c`
(equal to the pinned value in both CHANGES.json rows). The
`#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` switch regions
(:546, :561) are byte-identical to agent O's product. The switch was
compile-verified BOTH ways (section 4).

## 3. Diff

Unified diff (pre-edit reconstructed by reverse-applying the five edits and
verified to hash exactly to the Z19 pin `8f45e6ce...`, so this diff IS the
complete set of changes since then; 186 lines):

```diff
--- a/outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp
+++ b/outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp
@@ -440,6 +440,10 @@
             InsaneHeader = 3,
             DuplicateListener = 4,
             GMallocUnboundForGrowth = 5,
+            // Claude review item 4 (2026-10-08 relay): growth is required but the
+            // engine-owned Data buffer failed the heap-plausibility screen, so
+            // Append/Realloc is not called. Codes 1-5 keep their meanings.
+            ImplausibleDataForGrowth = 6,
         };
 
         auto l25_listener_header_sane(const void* data, int32_t num, int32_t max) -> bool
@@ -451,6 +455,100 @@
             const auto data_address = reinterpret_cast<uintptr_t>(data);
             if (data_address % alignof(void*) != 0) { return false; }
             return data_address <= static_cast<uintptr_t>(-1) - static_cast<uintptr_t>(max) * sizeof(void*);
+        }
+
+        // =================================================================================
+        // Claude review item 4 (2026-10-08 relay) — pre-Append allocator logging +
+        // growth refusal scaffolding (agent AD).
+        //
+        // PENDING ALIGNMENT: the parallel agent-AB GMalloc audit
+        // (outputs/2026-10-08-l25-prep/agent-AB-gmalloc-audit.md) was NOT on disk
+        // when this was written; the guarded reads below follow the tree's existing
+        // safe-reader idiom (byte-wise reads through const volatile unsigned char*,
+        // exactly the seed-slot read_u64 in UnrealInitializer.cpp:793-801) behind an
+        // arithmetic plausibility screen. If AB publishes stricter expressions,
+        // reconcile here without touching the refusal semantics.
+        //
+        // x86-64 4-level paging canonical user-address bound (PS5 target included):
+        // anything at/above 2^47 is a kernel address, not engine heap.
+        constexpr uintptr_t L25CanonicalUserMax = (uintptr_t{1} << 47) - 1;
+
+        // Word-read plausibility screen: non-null, pointer-aligned, inside the
+        // canonical user range (which also implies the 8-byte read cannot wrap).
+        auto l25_pointer_word_plausible(uintptr_t address) -> bool
+        {
+            return address != 0
+                && address % alignof(void*) == 0
+                && address <= L25CanonicalUserMax;
+        }
+
+        // Tree safe-reader idiom: byte-wise load of one machine word through a
+        // const volatile pointer so the compiler cannot widen, cache, or elide the
+        // access. NOT a fault guard — callers screen the address first (the same
+        // doctrine l25_listener_header_sane states: this TU cannot prove
+        // page readability; the screen is arithmetic and the markers discriminate
+        // the rest). GMalloc's slot was V1/V2-verified at seed bind time
+        // (UnrealInitializer.cpp:1097-1109), so these reads revisit proven pages.
+        auto l25_read_word(const void* address) -> uintptr_t
+        {
+            const auto* base = static_cast<const volatile unsigned char*>(address);
+            uintptr_t value{};
+            for (size_t i = 0; i < sizeof(uintptr_t); ++i)
+            {
+                value |= static_cast<uintptr_t>(base[i]) << (8 * i);
+            }
+            return value;
+        }
+
+        // One guarded allocator snapshot: the GMalloc VARIABLE address (this
+        // TU's definition — reloc/ODR sanity), its call-time value (the engine
+        // FMalloc* slot the seed bound), the instance read through *variable,
+        // and the instance vptr read through *(void**)instance (the exact word
+        // the Append/Realloc virtual dispatch would consume — Claude's fault
+        // hypothesis (a)). Unbound/null stages read as 0 and are skipped, so the
+        // snapshot itself never dereferences an unscreened pointer.
+        struct L25GMallocSnapshot
+        {
+            const void* variable_address{};
+            uintptr_t variable_value{};
+            uintptr_t instance{};
+            uintptr_t vptr{};
+        };
+
+        auto l25_snapshot_gmalloc() -> L25GMallocSnapshot
+        {
+            L25GMallocSnapshot snap;
+            snap.variable_address = static_cast<const void*>(&GMalloc);
+            snap.variable_value = reinterpret_cast<uintptr_t>(GMalloc);
+            if (l25_pointer_word_plausible(snap.variable_value))
+            {
+                snap.instance = l25_read_word(reinterpret_cast<const void*>(snap.variable_value)); // *variable
+                if (l25_pointer_word_plausible(snap.instance))
+                {
+                    snap.vptr = l25_read_word(reinterpret_cast<const void*>(snap.instance)); // *(void**)instance
+                }
+            }
+            return snap;
+        }
+
+        // Claude review item 4 refusal screen: when Append must GROW the array,
+        // the engine-owned Data buffer must additionally be heap-plausible —
+        // non-null (growth with a live allocation), pointer-aligned, inside the
+        // canonical user range, and not wrapping over the live Num elements.
+        // Applied only on the live-buffer (realloc) growth path: after the
+        // unconditional sane-header gate, Data==nullptr is reachable solely as
+        // the canonical empty header Num==Max==0, whose Append is a fresh
+        // allocation with no engine-owned storage handed to Realloc — that path
+        // keeps O's fresh-alloc marker and proceeds. Readability itself stays
+        // unprovable from this TU (same admission as l25_listener_header_sane);
+        // refusing here means the arithmetic screen failed, i.e. the header was
+        // NOT verified sane enough to hand to Realloc.
+        auto l25_data_heap_plausible(const void* data, int32_t num) -> bool
+        {
+            if (data == nullptr) { return false; }
+            const auto address = reinterpret_cast<uintptr_t>(data);
+            return l25_pointer_word_plausible(address)
+                && address <= static_cast<uintptr_t>(-1) - static_cast<uintptr_t>(num) * sizeof(void*);
         }
 
         auto l25_diag(const char* tag, uint32_t value) -> void
@@ -505,9 +603,20 @@
             const void* pre_data = DeleteListeners.GetData();
             const int32_t pre_num = DeleteListeners.Num();
             const int32_t pre_max = DeleteListeners.Max();
-            Output::send(STR("[L25][DeleteListener] header pre: Data={} Num={} Max={}\n"),
-                         const_cast<void*>(pre_data), pre_num, pre_max);
+            // Claude review item 4 (2026-10-08 relay): growth-required flag and the
+            // guarded GMalloc variable/instance/vptr snapshot join the pre-Append dump,
+            // so the launch harvest sees the exact allocator state the Append/Realloc
+            // virtual dispatch would consume BEFORE the call is made.
+            const bool growth_required = (pre_num >= pre_max) || pre_data == nullptr;
+            const L25GMallocSnapshot pre_gmalloc = l25_snapshot_gmalloc();
+            Output::send(STR("[L25][DeleteListener] header pre: Data={} Num={} Max={} growth_required={} GMallocVar={} GMalloc={} GMallocInst={} vptr={}\n"),
+                         const_cast<void*>(pre_data), pre_num, pre_max, growth_required ? 1 : 0,
+                         const_cast<void*>(pre_gmalloc.variable_address),
+                         reinterpret_cast<void*>(pre_gmalloc.variable_value),
+                         reinterpret_cast<void*>(pre_gmalloc.instance),
+                         reinterpret_cast<void*>(pre_gmalloc.vptr));
             l25_diag("l25_dl_pre", static_cast<uint32_t>(pre_num));
+            l25_diag("l25_dl_gmalloc_pre", static_cast<uint32_t>(pre_gmalloc.instance & 0xFFFFFFFFu));
             if (!l25_listener_header_sane(pre_data, pre_num, pre_max))
             {
                 Output::send(STR("[L25][DeleteListener] REFUSED ({}) : insane UObjectDeleteListeners header (Data={} Num={} Max={}); skipping registration and continuing init\n"),
@@ -524,7 +633,7 @@
                 l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::DuplicateListener));
                 return;
             }
-            const bool growth_needed = pre_num >= pre_max;
+            const bool growth_needed = growth_required;
             if (growth_needed && pre_max > 0)
             {
                 if (GMalloc == nullptr || *GMalloc == nullptr)
@@ -532,6 +641,21 @@
                     Output::send(STR("[L25][DeleteListener] REFUSED ({}) : growth required (Num==Max={}) but GMalloc is unbound; skipping registration and continuing init\n"),
                                  static_cast<uint32_t>(L25RefusalCode::GMallocUnboundForGrowth), pre_max);
                     l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::GMallocUnboundForGrowth));
+                    return;
+                }
+                // Claude review item 4 refusal rule: "If growth is required and the
+                // header isn't verified sane, refuse cleanly instead of calling
+                // Realloc." On this path the sane gate has passed AND a live
+                // engine-owned buffer exists (Data!=nullptr is guaranteed here: null
+                // Data only survives the gate as Num==Max==0, excluded by pre_max>0),
+                // so the strengthened check verifies that buffer is heap-plausible
+                // before Append can hand it to Realloc. Failures refuse with the new
+                // code 6; Append/Realloc is NOT called.
+                if (!l25_data_heap_plausible(pre_data, pre_num))
+                {
+                    Output::send(STR("[L25][DeleteListener] REFUSED ({}) : growth required (Num==Max={}) but Data={} is not heap-plausible (non-null/aligned/canonical arithmetic screen); refusing instead of calling Realloc; skipping registration and continuing init\n"),
+                                 static_cast<uint32_t>(L25RefusalCode::ImplausibleDataForGrowth), pre_max, const_cast<void*>(pre_data));
+                    l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::ImplausibleDataForGrowth));
                     return;
                 }
                 // Residual documented race: Append reallocates the engine-owned buffer
@@ -554,6 +678,17 @@
             Output::send(STR("[L25][DeleteListener] header post: Data={} Num={} Max={} (path={})\n"),
                          const_cast<void*>(post_data), post_num, post_max, ensure_str(path));
             l25_diag("l25_dl_post", static_cast<uint32_t>(post_num));
+            // Claude review item 4: the same GMalloc variable/instance/vptr trio
+            // re-read in the success path — a changed variable value, changed
+            // instance, or changed/cleared vptr across the Append is direct
+            // evidence for/against the GMalloc-virtual-call fault hypothesis.
+            const L25GMallocSnapshot post_gmalloc = l25_snapshot_gmalloc();
+            Output::send(STR("[L25][DeleteListener] GMalloc post: GMallocVar={} GMalloc={} GMallocInst={} vptr={}\n"),
+                         const_cast<void*>(post_gmalloc.variable_address),
+                         reinterpret_cast<void*>(post_gmalloc.variable_value),
+                         reinterpret_cast<void*>(post_gmalloc.instance),
+                         reinterpret_cast<void*>(post_gmalloc.vptr));
+            l25_diag("l25_dl_gmalloc_post", static_cast<uint32_t>(post_gmalloc.instance & 0xFFFFFFFFu));
             if (post_num != pre_num + 1)
             {
                 Output::send(STR("[L25][DeleteListener] ANOMALY: Num advanced by {} (expected 1); concurrent engine-side writer suspected (pre Num={} post Num={})\n"),
```

## 4. Compile proof (actual results; all logs under /tmp/l25-combined-smoke/logs/)

Z13's reusable stub recipe, Z19's method: source pointed at the REAL tree
file (the harness JSON's Z13 shim was not used as evidence); Z13/Z19 logs
and objects untouched; one-off runners `ad4_verify.py` / `ad4_prospero.py`
in /tmp/l25-combined-smoke/.

| run | compiler | result | log |
|---|---|---|---|
| unreal-uobjectarray, edited TU | host /usr/bin/clang++ (Apple clang 17.0.0 arm64), stub recipe, exact adapted argv | `-fsyntax-only` exit 0 | `ad4-host-syntax-unreal-uobjectarray.log` |
| unreal-uobjectarray, edited TU | host, same argv | full `-c` codegen exit 0 | `ad4-host-obj-unreal-uobjectarray.log` (obj/`ad4-unreal-uobjectarray.o`) |
| unreal-unrealinitializer, variant A (default) | host, stub recipe | `-fsyntax-only` exit 0 | `ad4-host-syntax-unreal-initializer-variantA.log` |
| unreal-unrealinitializer, variant B (`-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`) | host, stub recipe | `-fsyntax-only` exit 0 | `ad4-host-syntax-unreal-initializer-variantB.log` |
| unreal-uobjectarray, edited TU | real `prospero-clang++` (receipt argv + l25 -I inserts only, no stubs) | `-fsyntax-only` exit 0; only the two pre-existing inherited `-Wswitch` warnings (port Atomic.hpp:241,262) | `prospero-syntax-ad4-unreal-uobjectarray.log` |
| unreal-unrealinitializer, variant A | prospero | `-fsyntax-only` exit 0 (known port-header warnings only) | `prospero-syntax-ad4-unreal-initializer-variantA.log` |
| unreal-unrealinitializer, variant B | prospero | `-fsyntax-only` exit 0 (same known warnings) | `prospero-syntax-ad4-unreal-initializer-variantB.log` |

Known inherited-only caveat, unchanged from Z19: the prospero run still
resolves `<Unreal/UObjectArray.hpp>` through the l22 vfs overlay to the OLD
port-tree header (the E3 routing gap owned by the parallel agent); this
change is body-level in the .cpp and header-variant-independent. No host
suites re-run, no target link, no packaging, no console contact.

## 5. CHANGES.json update

`outputs/2026-10-08-l25-runtime/typescan-r4/CHANGES.json` (new sha256
`9ec2f0bd16134768eceac79ca61451bb698d8234a2d19ca43b11f16898e69c56`):

- `candidate_sha256` for `candidate/src/UObjectArray.cpp` recomputed in BOTH
  rows that carry it (`changes[]` and `files[]`; legacy `changes` kept in
  sync with `files` per the established convention):
  - before: `8f45e6ce03fb066dee130c57ded022d555ace23b41860403975cae8a03c45c9c`
  - after: `4f4afddfd24e213faa2e80bc2ac3d41e23e634446d5f27f23c97ea5b7ac78b09`
- One dated `review_fixes` entry appended (2026-10-08, Claude review item 4
  via agent AD) describing sections 1a-1e and the verification above. ASCII
  escaping style of the existing entries preserved; no other rows altered.
- Post-edit full sweep re-run: all 56 hash pins across the 18 rows + 4
  added-header hashes match disk; no cross-part duplicate-original conflicts
  introduced (no rows added or removed).

## 6. Marker strings for the launch harvest (additions to O's grep list)

UE4SS.log lines (prefix `[L25][DeleteListener]`):
- `header pre: Data=` — now carries ` growth_required= GMallocVar= GMalloc=
  GMallocInst= vptr=` after Max
- `GMalloc post: GMallocVar=` (success path only)
- `REFUSED (6)` — growth required but Data failed the heap-plausibility
  screen (message contains `refusing instead of calling Realloc`)

Diagnostic-event tags: `l25_dl_gmalloc_pre`, `l25_dl_gmalloc_post` (value =
low 32 bits of call-time FMalloc instance; 0 = unbound);
`l25_dl_refused` now also carries value 6.

Reading the harvest: pre `growth_required=1` + `vptr=0x0` with GMalloc
non-null = instance died/changed between bind and Add; pre vs post `vptr`
differs = Append itself mutated the allocator chain; `REFUSED (6)` = the
engine-owned buffer was not verifiably sane to grow (Claude item 4 refusal,
no Realloc attempted); `header pre:` without `GMalloc post:` after
`growth_required=1` = the fault is inside Append/Realloc with full allocator
state now recorded beforehand.

## 7. Honest limits

- `l25_read_word` guards against compiler-elided/widened reads and
  unscreened derefs, not against page faults; arithmetic plausibility is
  the tree's stated ceiling from this TU (no VirtualQuery bridge on its
  include path). The GMalloc slot/instance pages were V1/V2-verified at
  seed bind, which is the only reason a second-level live read is
  defensible at all.
- The `L25CanonicalUserMax = (1<<47)-1` bound is the x86-64 4-level paging
  canonical user bound (correct for the PS5 target and the host smoke); it
  is a screen, not proof of heap membership.
- Agent-AB alignment remains PENDING (its audit file was absent); the
  in-code comment marks the reconciliation point.
- The fresh-allocation interpretation of Claude's refusal rule (proceed on
  Num==Max==0) is documented as this agent's judgment call for coordinator
  review — the literal alternate reading would refuse every first
  registration and reduce variant A to variant B behavior.
- Nothing beyond single-TU compile verification was run; the parent owns
  the closure rebuild, host suites, twin build, and any console proof.
