# Agent AO — Claude review 2 item 3 execution: container-audit gate extensions (G1–G3)

Date: 2026-10-08. Tree: `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/`
(LuaMod.cpp + UnrealInitializer.cpp only, + CHANGES.json). Plan source:
`outputs/2026-10-08-l25-prep/agent-AC-container-audit.md` §4.

## 0. Pin verification before editing (all matched)

| File | Pinned sha256 (prefix) | Verified pre-edit | Post-edit (new pin) |
|---|---|---|---|
| `candidate/src/LuaMod.cpp` | `1c0c103b04e4` | match | `cef8d02f74b66e2cdd944d176ae3c0d2ec79439e1573ae51fff7fd1a11fd8e98` |
| `candidate/src/UnrealInitializer.cpp` | `efdef2535fe7` | match | `cac95e4822709b790cd96718f7d1a3f43d7346fb899e155d81c48a0e675d268d` |
| `candidate/src/UObjectArray.cpp` | `4f4afddfd24e` | match, NOT touched | `4f4afddfd24e213faa2e80bc2ac3d41e23e634446d5f27f23c97ea5b7ac78b09` (unchanged) |

Integrity proof: reversing my 11 edit hunks reproduces byte-exact files hashing
`1c0c103b...` / `efdef253...` (the pre-edit pins), i.e. the diffs below are the
complete delta.

Site-number drift (P's phase tags): the audit's l24-unit lines map to this tree as
7995→8053 (G1) and 5007/5012/5017/5050/5056/5099/5105 → 5040/5045/5050/5083/5089/
5132/5138 (G2); UnrealInitializer 523/524 → 545/551 inside the `:503` cache block (G3).
Post-edit current lines: G1 at LuaMod.cpp:8076-8091; G2 at 5049/5056/5063/5098/5106/
5152/5160; G3 at UnrealInitializer.cpp:503-511 + 568-575.

## 1. G1 — gate the Lua delete-listener registration (DONE, mandatory)

`LuaMod::on_program_start`, first statement — wrapped in
`#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)` with a loud `#else` skip log
mirroring the UnrealInitializer.cpp:557/578 skip-line style (unsafe-long-term
stale-object-pointer consequence). Read of the function confirmed only the
registration is gated; the `r3_marshal_calibration_consumer()` call and everything
after runs in both variants. `s_lua_object_delete_listener` has exactly one
reference in this TU (the gated call), so variant B references it nowhere (nm-proven
below).

New skip marker (exact string):

```
[L25][DeleteListener] Lua-mod delete-listener registration SKIPPED (variant B; unsafe long-term: Lua-held stale object pointers never cleared and Lua onDelete callbacks never fire)
```

## 2. G2 — ToString-free identity at the 7 R3 consumer print sites (DONE, both variants)

Unconditional in A and B (the audit's rank-2 class is unproven on console for both;
"only the log text changes"). All 7 `carrier->GetFullName()` payloads replaced with
`carrier->GetFName().GetComparisonIndex().ToUnstableInt()` + `reinterpret_cast<uintptr_t>(carrier)`
— FName member copy + two integer reads, no engine calls, no allocation — in the
proven init-gate `[n: X]` convention (UnrealInitializer.cpp:1966+ style). Messages
keep their distinct per-site text; refusal semantics (incl. the no-FUNC_Native
refusal = audit :5017 semantics) unchanged — every refusal still `return false`s.

Exact new message templates (greppable, one per site, in execution order):

```
[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} has a null FuncPtr
[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} is a script function (Func == ProcessInternal)
[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} does not have FUNC_Native
[R3] marshal arm refused: RegisterPostHook returned an invalid callback id for carrier [n: {:X}] @0x{:016X}
[R3] FuncPtr-marshal armed on carrier [n: {:X}] @0x{:016X} (callback id {})
[R3] event carrier refused: RegisterPostHook returned an invalid callback id for carrier [n: {:X}] @0x{:016X}
[R3] FuncPtr-marshal event carrier armed: [n: {:X}] @0x{:016X} (callback id {})
```

Deliberately NOT touched (audit do-not-gate list binding): the three remaining
GetFullName sites outside the R3 consumer — `LuaMod.cpp:352` (script-hook name
derivation) and `:5439/:5457` (Lua `RegisterHook` API prints, mod-boundary) — plus
the precheck scans, `RegisterPostHook`/`SetFuncPtr`, `install_lua_mods`/`start_mod`,
main.lua. G4 not implemented (log hygiene only; avoided churn).

## 3. G3 — variant B forces the UObjectArray cache off regardless of ini (DONE)

The whole `if (UnrealConfig.bUseUObjectArrayCache)` block in
`UnrealInitializer::PostInitialize` (searcher pools + `AddUObjectCreateListener`
+ the already-item-2-gated class-delete registration) now sits inside
`#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)`. The variant-B `#else` logs one
line ONLY when the ini would have enabled the cache (today's 1947 B ini has it
false ⇒ silent, matching current observed behavior). The inner item-2
markers/nesting and the shutdown-listener gate below are untouched, so variant A
preprocesses to exactly the previous A behavior.

New suppression marker (exact string):

```
[L25][DeleteListener] UObjectArray cache block suppressed under variant B: searcher pools + class create/delete listeners skipped (ini had bUseUObjectArrayCache=true; forced off)
```

Note: with G3 the inner (item-2) class-delete `#else` skip line is unreachable in
both variants (outer condition is a superset); left in place to avoid touching
frozen item-2 instrumentation.

## 4. Diffs

`LuaMod.cpp` (pin `1c0c103b...` → `cef8d02f...`), 105-line unified diff:

```diff
--- /tmp/ao-preedit/LuaMod.cpp
+++ .../typescan-r4/candidate/src/LuaMod.cpp
@@ -5027,6 +5027,15 @@
     // RegisterHook native branch applies (LUA:4618-4620), so the R3 tier gets the
     // PLH-missing semantic validation for free.
     // =====================================================================================
+    // [L25][G2] (agent-AC container audit rank 2): the carrier identity in every R3 log
+    // below (precheck, drain-arm, event-carrier arm — 7 print sites) is the FName
+    // comparison index + pointer, NEVER UObject::GetFullName(). GetFullName() routes
+    // FName::ToString -> (PS5: fname_tostring unbound by design) engine
+    // ProcessEvent(Conv_NameToString) + a GMalloc-backed FString allocation — an
+    // unproven engine-call class on console (no run in this lineage has completed one).
+    // Applies to BOTH variants: data reads only, no engine calls, no allocation; the
+    // "[n: X]" style matches the proven init-gate log convention.
+    // =====================================================================================
     static auto r3_marshal_carrier_precheck(Unreal::UFunction* carrier) -> bool
@@ -5037,17 +5046,23 @@
         const auto current_func_ptr = carrier->GetFuncPtr();
         if (!current_func_ptr)
         {
-            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} has a null FuncPtr\n"), carrier->GetFullName());
+            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} has a null FuncPtr\n"),
+                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                                         reinterpret_cast<uintptr_t>(carrier));
             return false;
         }
         if (current_func_ptr == Unreal::UObject::ProcessInternalInternal.get_function_address())
         {
-            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} is a script function (Func == ProcessInternal)\n"), carrier->GetFullName());
+            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} is a script function (Func == ProcessInternal)\n"),
+                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                                         reinterpret_cast<uintptr_t>(carrier));
             return false;
         }
         if (!carrier->HasAnyFunctionFlags(Unreal::EFunctionFlags::FUNC_Native))
         {
-            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier {} does not have FUNC_Native\n"), carrier->GetFullName());
+            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: carrier [n: {:X}] @0x{:016X} does not have FUNC_Native\n"),
+                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                                         reinterpret_cast<uintptr_t>(carrier));
             return false;
         }
         return true;
@@ -5080,13 +5095,18 @@
         const int32_t callback_id = carrier->RegisterPostHook(&funcptr_marshal_hook, nullptr);
         if (callback_id < 0)
         {
-            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: RegisterPostHook returned an invalid callback id for {}\n"), carrier->GetFullName());
+            Output::send<LogLevel::Error>(STR("[R3] marshal arm refused: RegisterPostHook returned an invalid callback id for carrier [n: {:X}] @0x{:016X}\n"),
+                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                                         reinterpret_cast<uintptr_t>(carrier));
             return false;
         }
         m_funcptr_marshal_callback_id = callback_id;
         m_funcptr_marshal_carrier_id = carrier_id;
         m_funcptr_marshal_armed.store(true, std::memory_order_release);
-        Output::send(STR("[R3] FuncPtr-marshal armed on carrier {} (callback id {})\n"), carrier->GetFullName(), callback_id);
+        Output::send(STR("[R3] FuncPtr-marshal armed on carrier [n: {:X}] @0x{:016X} (callback id {})\n"),
+                     carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                     reinterpret_cast<uintptr_t>(carrier),
+                     callback_id);
         return true;
     }
@@ -5129,13 +5149,18 @@
         const int32_t callback_id = carrier->RegisterPostHook(&funcptr_marshal_hook, nullptr);
         if (callback_id < 0)
         {
-            Output::send<LogLevel::Error>(STR("[R3] event carrier refused: RegisterPostHook returned an invalid callback id for {}\n"), carrier->GetFullName());
+            Output::send<LogLevel::Error>(STR("[R3] event carrier refused: RegisterPostHook returned an invalid callback id for carrier [n: {:X}] @0x{:016X}\n"),
+                                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                                         reinterpret_cast<uintptr_t>(carrier));
             return false;
         }
         m_funcptr_marshal_event_carriers.push_back(carrier);
         if (m_funcptr_marshal_event_carriers.size() <= 8)
         {
-            Output::send(STR("[R3] FuncPtr-marshal event carrier armed: {} (callback id {})\n"), carrier->GetFullName(), callback_id);
+            Output::send(STR("[R3] FuncPtr-marshal event carrier armed: [n: {:X}] @0x{:016X} (callback id {})\n"),
+                         carrier->GetFName().GetComparisonIndex().ToUnstableInt(),
+                         reinterpret_cast<uintptr_t>(carrier),
+                         callback_id);
         }
         if (!is_funcptr_marshal_armed())
@@ -8050,7 +8075,17 @@
 
     auto LuaMod::on_program_start() -> void
     {
+#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)
         Unreal::UObjectArray::AddUObjectDeleteListener(&LuaType::FLuaObjectDeleteListener::s_lua_object_delete_listener);
+#else
+        // [L25][G1] (agent-AC container audit rank 1): this AddUObjectDeleteListener is the
+        // same 3-touch GMalloc TArray-append family as the gated shutdown-listener
+        // registration in UnrealInitializer (temp TArray Malloc + engine-array
+        // Append/Realloc + temp Free through the FMemory virtual wrappers). Variant B
+        // would hit the same rejected-Realloc crash here one call later, on the
+        // UE4SS-UpdateThread. Skip-log mirrors the UnrealInitializer skip-line style.
+        Output::send(STR("[L25][DeleteListener] Lua-mod delete-listener registration SKIPPED (variant B; unsafe long-term: Lua-held stale object pointers never cleared and Lua onDelete callbacks never fire)\n"));
+#endif
 #ifdef __PROSPERO__
```

`UnrealInitializer.cpp` (pin `efdef253...` → `cac95e48...`), 33-line unified diff:

```diff
--- /tmp/ao-preedit/UnrealInitializer.cpp
+++ .../typescan-r4/candidate/src/UnrealInitializer.cpp
@@ -500,6 +500,14 @@
             bFAssetDataAvailable = true;
         }
 
+#if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)
+        // [L25][G3] (agent-AC container audit X1/X2): this whole bUseUObjectArrayCache
+        // block also requires the variant-A gate. It registers the class create +
+        // class-delete listeners (AddUObjectCreateListener / AddUObjectDeleteListener on
+        // the engine GUObjectArray listener TArrays) — the same 3-touch GMalloc
+        // TArray-append family as the gated shutdown-listener registration below — so a
+        // settings flip must not be able to reintroduce that family ahead of the gated
+        // site during variant-B testing. Variant B forces the cache off regardless of ini.
         if (UnrealConfig.bUseUObjectArrayCache)
         {
             Output::send(STR("Initializing searcher pools\n"));
@@ -556,7 +564,15 @@
 #else
             Output::send(STR("[L25][DeleteListener] delete-listener registration SKIPPED (variant B; unsafe long-term: stale object pointers not cleared)\n"));
 #endif
+        }
+#else
+        // [L25][G3] variant-B side of the cache-block gate above: log only when the ini
+        // would have enabled the cache (today's 1947 B settings already have it false).
+        if (UnrealConfig.bUseUObjectArrayCache)
+        {
+            Output::send(STR("[L25][DeleteListener] UObjectArray cache block suppressed under variant B: searcher pools + class create/delete listeners skipped (ini had bUseUObjectArrayCache=true; forced off)\n"));
         }
+#endif
 
 #if !defined(MODBRIDGE_L25_SKIP_DELETE_LISTENER)
```

## 5. Compile matrix (the /tmp/l25-combined-smoke recipe) — ALL GREEN

Host = `/usr/bin/clang++` via `host-argvs-obj.json` (Z13 stub layer; the LuaMod host
shim was regenerated from the post-edit tree file with the same single Z13
apple-libc++ ifstream line change, header notes updated). Prospero = real
`prospero-clang++` with the real receipt argv + l25 `-I` inserts (ad4_prospero.py
pattern). Variant B = `-DMODBRIDGE_L25_SKIP_DELETE_LISTENER`. Script:
`/tmp/l25-combined-smoke/ao_matrix.py`; logs `logs/ao-*`; objects `obj/ao-*`.

| Unit | Host -fsyntax-only A | Host -fsyntax-only B | Host -c A | Host -c B | Prospero -fsyntax-only A | Prospero -fsyntax-only B |
|---|---|---|---|---|---|---|
| LuaMod.cpp | exit 0 | exit 0 | exit 0 (`obj/ao-luamod-variantA.o`) | exit 0 (`obj/ao-luamod-variantB.o`) | exit 0 | exit 0 |
| UnrealInitializer.cpp | exit 0 | exit 0 | exit 0 (`obj/ao-unreal-unrealinitializer-variantA.o`) | exit 0 (`obj/ao-unreal-unrealinitializer-variantB.o`) | exit 0 | exit 0 |

Warnings: identical sets A vs B per file (LuaMod 25, UnrealInitializer 6), all
inherited (deprecated metadata-less hooks, `operator unsigned int` implicit
conversion at LuaMod :293/:402/:457/:488/:7984/:8026 — NOT my lines, which call
`ToUnstableInt()` explicitly; asmjit pragma; -Wswitch; etc.). Zero new warnings.

### Compiled-out proofs

G1 (the audit's "the B-build of LuaMod.cpp must show the 7995 registration compiled
OUT") — `nm` on the host objects:

- variant A `ao-luamod-variantA.o`:
  `U __ZN2RC6Unreal13FUObjectArray24AddUObjectDeleteListenerEPNS0_22FUObjectDeleteListenerE`
  `U __ZN2RC7LuaType24FLuaObjectDeleteListener28s_lua_object_delete_listenerE`
- variant B `ao-luamod-variantB.o`: **0 references** to either symbol.

G3 — preprocessor proof (`clang++ -E`, UnrealInitializer.cpp):
variant A: create-listener call=1, class-delete call=1, shutdown-delete call=1,
suppression-log=0, "Initializing searcher pools"=1;
variant B: create=0, class-delete=0, shutdown-delete=0, suppression-log=1,
searcher-pools=0.

## 6. CHANGES.json

`outputs/2026-10-08-l25-runtime/typescan-r4/CHANGES.json`: `candidate_sha256`
recomputed in BOTH the `changes[]` and `files[]` rows for the two touched files
(LuaMod `1c0c103b...`→`cef8d02f...`, UnrealInitializer `efdef253...`→`cac95e48...`),
and a dated `review_fixes[]` entry appended citing agent-AC G1–G3 (entry #6;
JSON re-validated). UObjectArray.cpp row untouched (`4f4afddf...`).

## 7. Not done / honest boundaries

No host suites re-run, no twin/packaged build, no target link, no console contact,
no engine run. Variant B is still a diagnostic build: Lua `onDelete` never fires and
Lua-held stale object pointers are never cleared (stated in the skip log). Per the
audit's flag-to-Claude: main.lua runs before "[l22] Lua mod started" and its own
engine API usage is not gated by any runtime switch.
