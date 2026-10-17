# Reusable native lifecycle wrapper — staged source only

Design-only staged runtime component; no compile, link, execution or deployment. Independent runtime review is required before integration. `staged/lifecycle_adapter.cpp` wraps original source-based mod exports using the existing child lease API, rather than changing original mod files or supplying an invented host gateway.

## Real source candidates and smallest honest path

Only EventViewerMod and KismetDebuggerMod were found in the retained local cppmods tree. Neither is a no-GUI candidate: their original constructors register tabs, which the actual port refuses. All previously listed full-support requirements remain. Primary-source browsing also found game-specific mods needing global hooks or detours; none was admitted as a generic no-GUI third gameplay mod. No downloads were made in this continuation.

The upstream published MyAwesomeMod example in `source-dependencies/ue4ss/docs/guides/creating-a-c++-mod.md` is the smallest source example requiring only metadata, lifecycle, logging and optionally a reflected object lookup. It is a published tutorial example, not a third real gameplay mod and not a #9 compatibility pass. The existing project-authored `outputs/ps5-mod-bridge/native-cppmod-source/lifecycle_mod.cpp` is likewise only a diagnostic. Do not count either as a real mod result. [Official example](https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html), [official MIT template](https://github.com/UE4SS-RE/UE4SSCPPTemplate).

For an initial load prerequisite, reuse the existing lifecycle diagnostic and child packaging machinery, not new invented imports. For an unchanged published example/real mod, use this wrapper only after checking the original factory ownership contract. A different export ABI, custom entry ownership or factory that does not return a uniquely owned genuine CppUserModBase object requires a separate recipe; this wrapper does not guess.

## Exact existing interfaces

- Child lifecycle: `outputs/ps5-mod-bridge/native-cppmod-source/child_runtime.h`, matching child_runtime.cpp. Functions mb_cppmod_child_acquire/release return the existing status enum. Their calls are nonblocking and can return BUSY; serialize all lifecycle and module-stop activity. Under that precondition an acquired lease releases successfully; the wrapper follows existing lifecycle_mod.cpp's release behavior. No arbitrary worker/callback quiescence is inferred from the lease.
- Native GUI retirement: cap11/NATIVE-GUI-SEAM.md traces genuine raw mod-owner callbacks. A future host must revoke registrations and drain callbacks BEFORE original uninstall/derived destruction, with an explicit self-removal and exception policy. The staged wrapper does not implement this handshake; child leases alone cannot admit either real GUI mod.
- Loader: `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/Mod/CppMod.cpp:100-104` resolves start_mod/uninstall_mod; `:140-142` stores the factory result; `:166-175` clears ownership only after successful original teardown.
- Discovery: same source root `include/Platform/NativeCppModPath.hpp:21-23` selects native names. `src/Platform/CppModModule.cpp:60-69` refuses PE; `:75-78` uses native dlopen, and successful code references remain resident. A .dll rename is not translation.
- Genuine base: `src/Mod/CppUserModBase.cpp`; constructor, destructor, RTTI and STL/string/runtime dependencies must match the host. No duplicate shadow base class is permitted.
- Current runtime startup: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp:734-770` is one-shot/gated native startup; `:2205-2216` dispatches Unreal-ready lifecycle to actual CppMod objects. These files were only read, not changed.

Line references accompany SHA-256 receipts in LIFECYCLE-SOURCE-RECEIPT.json; re-check hashes before using the recipe because live source can advance.

## Derived compile/link plan, for a later build window

1. Keep original source read-only. Run the exact-once staging script below and compile its derived entrypoint TU in place of the original. This script-generated source is the sole supported recipe. Prohibit global `-Dstart_mod` and `-Duninstall_mod` defines: they would also rename identifiers in included UE4SS headers (including CppMod member declarations), risking ODR/import mismatches. The derived script changes only two checked export anchors in the original mod file.
2. Compile adapter against the SAME genuine CppUserModBase headers/compiler flags/runtime profile as the host and original derived mod. Add the existing native-cppmod-source include directory for child_runtime.h. Compile the existing child_runtime.cpp once for that child. No second copy of child init/fini or base implementation.
3. Link original mod objects + adapter + child runtime. Publish start_mod/uninstall_mod from the adapter; keep renamed original symbols child-internal where packaging permits. Keep any additional original exports intentionally required. Two originals defining start_mod cannot share a child without explicit per-mod renaming/ownership design.
4. Reuse the existing native-cppmod-contract writer path for exact host exports/import types, constructor/finalizer boundaries and unwind registration. It currently targets the project diagnostic: its plugin-source and export-list selection need explicit adaptation before it can package an arbitrary original mod. Do not run its defaults and call that the real mod artifact.
5. Resolve the actual imports from the real host closure; dynamic native module acceptance, export lookup against an injected host and target exception/unwind behavior remain unproved. Static relinking also does not automatically register a mod: the actual CppMod path requires a native module handle. Do not describe static composition as ready until registration is implemented.
6. Review the staged adapter independently, then build once in the owner's next allowed window. Before target use check symbol collisions, exact signatures, original custom CRT assumptions, import provenance, code retention and child/module entry ownership.

## Exact-once staging and Mac checks

From the repository root, dry-run either real mod with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap09/stage_lifecycle_adapter.py --mod KismetDebuggerMod
```

Add `--stage-dir outputs/2026-10-12-caps-prep-chatgpt/cap09/derived-kismet` to create a new isolated derived source tree. This optional write has not been run. Existing targets, source hash drift, wrapper hash drift, ambiguous export anchors and output locations outside cap09 are refused. Use only that renamed entrypoint. Global export-renaming compiler defines are prohibited. This script does not integrate, compile or apply source to the runtime.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap09/test_lifecycle_stage.py
```

Four Python tests cover reversible exact-name replacement, source drift, already-derived source and missing exports. The two real-mod dry-run receipts record exact inputs/outputs. These tests establish source transformation behavior only, not C++ ownership or execution.

## Required behavior verification before shipping

A future compiled test must execute these cases against the real child API: inactive child refuses factory; active factory success retains one lease; null result and constructor throw release the lease; successful original uninstall releases after returning; throwing uninstall keeps it; null uninstall does nothing; stop refuses while a lease remains. Test lifecycle serialization and verify no child finalization can overlap a running callback. No compiled test was run here, so these remain acceptance requirements.

Console prerequisite proof is one native load, genuine factory call, update/lifecycle callback, clean original uninstall and no crash. It proves a loader/lifecycle subset only. EventViewer capture and Kismet GUI/debugger functionality still need their own complete APIs. No kernel work is introduced by this wrapper.

Independent review: root identified and removed the compiler-macro alternative because it could rewrite included class declarations. The remaining lease branches were reviewed as coherent only under serialized lifecycle/stop calls and the genuine factory ownership preconditions. Compiled C++ behavior remains unproved.
