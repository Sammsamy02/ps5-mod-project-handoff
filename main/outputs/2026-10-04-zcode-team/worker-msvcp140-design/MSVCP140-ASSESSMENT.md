# MSVCP140-ASSESSMENT.md — the C++ standard-library import family, classified and gap-analyzed

worker-msvcp140-design, 2026-10-04. Assessment only; nothing implemented, nothing executed.

## 1. Evidence base (exact, frozen)

Source: frozen readobj dumps `.superpowers/native-dependency-spike/windows-image-original-inputs-1/final-1/{dwmapi.dll,UE4SS.dll}.llvm.txt`
(the two retained PE DLLs from `ue4ss-v1.zip`, per `worker-modreqs/MOD-CENSUS.md` §8a and `SERVICES-BACKLOG.md` §10).
Raw symbol extract retained at `ue4ss-msvcp140-symbols.txt` (this directory).

The family is actually **three** C++-runtime DLLs, not one:

| DLL | dwmapi.dll imports | UE4SS.dll imports | verified block size |
|---|---|---|---|
| MSVCP140.dll | 61 | 207 | readobj `Import { Name: MSVCP140.dll }` blocks: 61 and 207 `Symbol:` lines |
| MSVCP140_ATOMIC_WAIT.dll | 0 | 13 | lines 709–726 of UE4SS readobj |
| MSVCP140_CODECVT_IDS.dll | 0 | 1 | `?id@?$codecvt@_SDU_Mbstatet@@@std@@2V0locale@2@A` |
| **total family** | **61** | **221** | **282 import slots** |

Census cross-check: both counts match SERVICES-BACKLOG §10 exactly ("61 symbols dwmapi.dll; 207 + 13 + 1 on UE4SS.dll").
Two census corrections found by direct read:
- **No `std::_Execute_once` symbol exists in either image** (grep count 0/0). The census phrase "std::_Execute_once-class interops" is not supported by the frozen read; drop it from planning.
- UE4SS does **not** import MSVCP140_1.dll (only dwmapi's VCRUNTIME140_1 `__CxxFrameHandler4`, which is the SEH64 family, not this one).

Census weight: dwmapi has 130 unimplemented import slots, of which 61 (47%) are MSVCP140. UE4SS has 620 unimplemented
slots (634 − 14 name-matched per `RETAINED-IMPORT-COVERAGE.json`), of which 221 (36%) are this family. It is the single
largest unimplemented library block in both images.

## 2. Classification: essential vs optional

### 2a. dwmapi.dll — the 61 slots, exact assignment

**Essential (DLL startup / every stream use) — 53 slots:**

- *Streambuf backbone (34):* `?egptr ?gbump ?setg ?epptr ?_Gndec ?_Gninc ?_Gnavail ?pbump ?_Pninc ?_Pnavail ?_Init(x2 overloads) ?xsgetn ?xsputn` on `basic_streambuf<char>`, `?showmanyc ?snextc ?sgetc ?sbumpc ?getloc`, ctor/dtor pair, `?pptr ?eback ?gptr`, `??Bios_base` (ios_base bool conversion), `??0/??1 basic_ios`, `?setstate ?rdbuf ?widen`, `??0/??1 basic_istream`, `?_Ipfx`, `?_Fiopen` (file open for ifstream/ofstream). These are the explicit-instantiation members MSVC emits for `basic_streambuf<char>`/`basic_ios<char>`; every read/write/open of a `std::` stream calls into them. The `_Fiopen` import plus `codecvt` facets strongly indicate the proxy opens config files via `std::ifstream` — a plausibly real startup path, not dead template weight.
- *Locale/facet bootstrap (19):* `?_Incref ?_Decref` (facet refcount), `??0/??1 facet`, `?_Getgloballocale`, `?_Init@locale`, `?_Id_cnt@id`, `?id@codecvt<char>`, `?id@numpunct<char>`, `?_Getcat@codecvt`, `?in ?out ?unshift ?always_noconv` on `codecvt<char,char,_Mbstatet>`, `??0/??1 _Locinfo`, `?_Getcvt ?_Getfalse ?_Gettrue`. Any `basic_ios` construction (including the `<iostream>`-style static-init guard objects MSVC puts in every /MD C++ DLL) touches locale::_Init, the id counter and the facet pair; `codecvt<char,char>::in/out` is the identity conversion every narrow stream runs on each read/write.

**Optional (error paths only) — 8 slots:**
`?_Xbad_alloc ?_Xlength_error ?_Xout_of_range` (throw helpers, failure paths only), `?_Syserror_map ?_Winerror_map` (error-message tables for `std::system_error`), `??0/??1 _Lockit` (compile-time-toggled internal lock — on release builds the ctor is a no-op integer but the symbols are still imported), `?c_str@$_Yarn` (internal char-string holder, used by `_Locinfo`/error construction).

### 2b. UE4SS.dll — the 207 + 13 + 1 slots, grouped

| group | slots | essential? |
|---|---|---|
| iostream hierarchy `basic_{streambuf,ios,istream,ostream,iostream}` char + wchar_t incl. `?_Fiopen` (narrow+wide), `_xsgetn/_xsputn/_underflow/_uflow/_seekoff/_seekpos/_sync/_setbuf` | ~95 | essential — same rationale as dwmapi, wider instantiations |
| locale/facet/codecvt/ctype/numpunct/time_put/`_Locinfo`/lconv, `_Strcoll/_Strxfrm/_Wcscoll/_Wcsxfrm` | ~58 | core subset essential (~40: locale init, facet refcount, `codecvt<char>`, `codecvt<wchar_t>`, `codecvt<char16_t>` id, ctype, `_Lockit`, `_Getcat`); collation (`_Strcoll` family) and `time_put` optional |
| thread support `_Mtx_{init_in_situ,lock,unlock}`, `_Cnd_{init_in_situ,wait,broadcast,register_at_thread_exit,do_broadcast_at_thread_exit}`, `_Thrd_{id,join}`, `_Xtime_get_ticks` | 12 | essential for UE4SS itself (heavily threaded; any `std::mutex`/`std::condition_variable`/`std::thread` use) |
| `Concurrency::details` — `_TaskEventLogger` x6, `task_continuation_context` ctor, `_ContextCallback` x3, `_ReportUnobservedException`, `GetCurrentThreadId`, `_Schedule_chore`, `_Release_chore`, `ReportUnhandledError` | 15 | optional — only with `concurrency::task`/PPL usage; the 6 logger slots are pure ETW no-ops |
| `__ExceptionPtr{Create,Destroy,Copy,Assign,Rethrow,ToBool,CurrentException}` | 7 | optional — `std::exception_ptr`, i.e. `std::async`/futures only |
| `_X*` throwers: `_Xbad_alloc _Xlength_error _Xout_of_range _Xruntime_error _Xinvalid_argument _Xregex_error _Xbad_function_call _Throw_Cpp_error _XGetLastError` | 9 | optional (error paths) but load-bearing when hit — every failed `vector::at`, bad `std::function` call, or allocation failure funnels here |
| `_Syserror_map _Winerror_map`, `?uncaught_exceptions`, `setw/setprecision/_setf` manipulators, `?c_str ??4 $_Yarn`, `_Lockit` pair, `_Id_cnt` | ~10 | optional |
| MSVCP140_ATOMIC_WAIT.dll: `__std_atomic_wait_direct __std_atomic_notify_{all,one}_direct` (6) + `__std_tzdb_*` time-zone/leap-second DB (7) | 13 | atomic-wait 6: essential where C++20 `atomic::wait` used, and trivially forwardable; tzdb 7: optional feature (C++20 chrono zones) |
| CODECVT_IDS.dll | 1 | data-only companion (facet-id storage); rides the codecvt work |

Practical essential surface: **dwmapi 53/61; UE4SS roughly 150/221** (streams + locale core + thread support + atomic-wait). Everything else is error-path, PPL, exception_ptr, or tzdb.

## 3. What the essential ones actually do

- `basic_streambuf<char/wchar_t>` slot block: get/put area pointer management (`eback/gptr/egptr/setg/gbump`, `pbase/pptr/epptr/pbump`), availability counts (`_Gnavail/_Pnavail`), the six virtual hooks (`underflow/uflow/overflow-adjacent xsgetn/xsputn/showmanyc/sync/seekoff/seekpos/setbuf`) and ctor/dtor that lay out the object. This is the vtable backbone every stream read/write dispatches through.
- `basic_ios` ctor/dtor/`setstate/rdbuf/widen`, `??Bios_base`, `_Ipfx`: stream state word + tie/fill management, error-flag setting, `operator bool`, and the input-prefix (skipws) preamble of every `>>`.
- `_Fiopen@std`: converts a filename+mode into a UCRT `FILE*` (`_iobuf`) for the `filebuf` — the only file-open entry in the family; it bottoms out in `CreateFileW`/`_wfsopen` territory (file-I/O and UCRT stdio families).
- Locale bootstrap: `locale::_Init/_Getgloballocale/_New_Locimp/_Addfac` maintain the process-global `_Locimp` facet table; `facet::_Incref/_Decref` refcount facets; `id::_Id_cnt` hands out monotonically increasing facet ids; `_Getcat` returns the default-facet pointer per type; `_Locinfo` ctor/dtor wrap UCRT `setlocale`/`localeconv`/`__crtState` queries (`_Getcvt/_Getcoll/_Getlconv/_Getfalse/_Gettrue`) — i.e. the family depends on the UCRT locale imports UE4SS already carries (`api-ms-win-crt-locale-l1-1-0.dll`, readobj line 963).
- `codecvt<char,char,_Mbstatet>::in/out/unshift/always_noconv`: the identity narrow→narrow conversion each filebuf applies; `codecvt<wchar_t,char>`/`codecvt<char16_t>` do real UTF-16/UTF-8 conversion.
- `_Mtx_*`/`_Cnd_*`: MSVC's `std::mutex`/`std::condition_variable` runtime — `_Mtx_lock/unlock` on an SRW-backed mutex, `_Cnd_wait/broadcast` on `SleepConditionVariableSRW`/`WakeAllConditionVariable`, plus the at-thread-exit broadcast registry.
- `_Thrd_id/_Thrd_join`, `_Xtime_get_ticks`: thread ids, join, and the monotonic-clock tick query used by timed waits (clock family already provides QPC).
- `__std_atomic_wait_direct/notify_*`: thin direct forwarders to `WaitOnAddress`/`WakeByAddress{All,Single}` — the one slice SERVICES-BACKLOG already marks implemented.

## 4. Coverage by existing families

- **Heap family (mb_win_heap/heap_bridge — implemented, verified): NO MSVCP140 overlap, and one correction.** Neither DLL imports `operator new`/`operator delete`/`operator new[]` at all (grep for `??2@/??3@/??_U@`: zero hits in both readobj files). C++ allocation in these images is statically linked and bottoms out in UCRT `malloc/free/calloc/realloc` (already implemented by the heap bridge) plus `_callnewh` — which **is** imported by both DLLs and **is not** implemented. `_callnewh` is a ~10-line UCRT-heap-family gap (invoke the registered new-handler, return its verdict), not MSVCP140 work. The heap family serves this project as the *allocator underneath* any MSVCP140 shim we write.
- **CRT startup family (worker-crt-startup-design `crt_startup.c`: `_initterm/_initterm_e/_seh_filter_dll/_cexit`): prerequisite, not overlap.** MSVCP140-dependent static initializers (iostream guard objects, global locale) run over the same `_initterm` machinery — so the startup family must land first, but it covers none of the MSVCP140 *state* (locale::_Init table, facet refcounts, id counter, at-thread-exit registry). Those are new.
- **SEH64 family (worker-seh64-design: design-only today, no implementation yet): enables, does not absorb.** The throw machinery (`_CxxThrowException`, `__CxxFrameHandler4`) the `_X*` helpers call into is VCRUNTIME140 imports belonging to SEH64. Once SEH64 lands, each `_X*` shim is a small function that constructs a `std::exception`-derived object and throws it — but the object layout/type-info must be catchable by the mod's `catch (const std::exception&)`, which is genuinely new ABI-sensitive work *on top of* SEH64, not inside it.
- **Synchronization family (critical/SRW/CV/address-wait implemented and verified; mutex family in progress): absorbs the thread-support block.** `_Mtx_lock/unlock` ≈ existing SRW exclusive acquire/release; `_Cnd_wait/broadcast` ≈ `SleepConditionVariableSRW`/`WakeAllConditionVariable`; `_Cnd_register_at_thread_exit/_Cnd_do_broadcast_at_thread_exit` need one thread-exit hook extension on `mb_win_threads`. Binding work over verified primitives.
- **ATOMIC_WAIT: 6 of 13 slots near-free** (direct forwarders to implemented `WaitOnAddress`/`WakeByAddress*`). The 7 tzdb slots are optional feature stubs.

Net: of the 282 slots, the *semantics* of roughly 30 (atomic-wait forwarders, `_Mtx`/`_Cnd` core, `_Thrd` glue) are already-built territory; the remaining ~250 slots' semantics are genuinely new, though the majority (~125 iostream slots) share one implementation core.

## 5. Gap list — genuinely NEW work

1. **iostream/locale compatibility core (the real work, ~125–150 slots):** ABI-exact `basic_streambuf`/`basic_ios` object layouts (vtables, buffer-pointer arrangement, locale pointer) for char and wchar_t, plus a minimal locale engine: global `_Locimp`, facet refcounting, `_Id_cnt`, `_Lockit`, "C"-locale defaults for `ctype<char>`, `codecvt<char,char>` (identity), `codecvt<wchar_t,char>`, `numpunct<char>`, and the `codecvt<char16_t>` id that CODECVT_IDS.dll backs. Wrong layout = silent memory corruption in caller code; this must be built against the exact MSVC x64 layout, not approximated.
2. **`_Fiopen` (2 slots):** depends on the P1 file-I/O family (`CreateFileW`…) and UCRT stdio (`_iobuf` FILE*) — sequenced after both.
3. **Thread-support bindings (12 slots):** new binding module over verified SRW/CV + threads + clock, plus the at-thread-exit hook.
4. **`_X*` throwers (12 slots across both DLLs):** start as abort stubs (honest crash on error path), upgraded to real catchable C++ throws once SEH64 implementation exists.
5. **Error maps + `uncaught_exceptions` + `_Yarn` + manipulators (~12 slots):** trivial static tables and small functions once the core exists.
6. **PPL/exception_ptr/tzdb (~29 slots):** stub policy first (loggers as no-ops; `_Schedule_chore` returns failure so `concurrency::task` degrades cleanly; tzdb returns "not available"); real chore scheduling only if a mod actually exercises PPL.
7. **`_callnewh` (1 slot, both DLLs):** misattributed by everyone to C++; belongs to the UCRT heap slice. Cheapest real gap in the whole census.

Explicitly deferred/optional: collation (`_Strcoll/_Wcscoll/_Strxfrm/_Wcsxfrm`), `time_put`, regex-error thrower details, full tzdb.

## 6. Recommendation

**Do not open a long-lived standalone "MSVCP140 owner" at full breadth; open one bounded owner for the iostream/locale core + facade, and hand the edges to the families that already own their semantics.**

- **Absorb now (cheap, existing owners):** `_callnewh` → UCRT heap slice; `_Mtx/_Cnd/_Thrd` + atomic-wait forwarders → sync/threads owner as a bounded addendum; `_X*` stubs → SEH64 owner once that family implements throw.
- **One new bounded owner (the only genuinely new core):** MSVCP140 stream/locale compatibility core delivering the dwmapi 61-slot block first (narrow-only, no thread support — dwmapi imports none), then the UE4SS char+wide additions. dwmapi-first is attractive: it is 61 slots, no `_Mtx`, and its own startup is blocked anyway on file-I/O + CRT startup, so the core can be proven on the smaller surface.
- **Sequencing:** P4 confirmed, but for a structural reason stronger than size — the family sits *downstream* of three families still ahead of it (CRT startup P1, file-I/O P1, SEH64 P3, plus UCRT stdio/locale). Nothing in the MSVCP140 block can be end-to-end verified until those land. Meanwhile the loader's current refuse-unknown-requirements behavior is correct: a real MSVC mod DLL fails cleanly at load with a named unmet requirement, not a crash.
- **Honest priority statement:** after the already-implemented 14+5 slots, this family is the largest remaining import block in *both* retained DLLs (61% of dwmapi's unimplemented slots when combined with the CRT block it depends on; 36% of UE4SS's), so it is real, census-observed work — but it converts to "essential at startup" only through the `_initterm`-run static initializers and `_Fiopen` config reads of the proxy, and through UE4SS's threading. Build the minimal essential surface (~53 + ~150 slots), stub the optional 29 honestly, and re-assess after SEH64 lands whether the `_X*` upgrade or PPL chore scheduling is justified by any observed mod code path.
