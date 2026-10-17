# Guest TEB view and TLS lifecycle dispatcher plan

**Goal:** Extend static TLS preparation into a per-thread Windows x64 TEB prefix and callback/entry notification sequence, while refusing execution when no platform guest-segment backend is available.

**Stage:** `cg-teb-lifecycle-20261013-11`, isolated from Claude and the shared candidate.

**Design:** `GuestTebView` owns the current thread's `StaticTlsBank::ThreadArray` snapshot and a zero-initialized prefix. `refresh()` writes its own address at offset `0x30` and the snapshot array address at offset `0x58`; callers must refresh after late module registration and retain the view through guest calls. `GuestSegmentSwitch` is an interface with `enter(teb)` and `leave()`; production has no implementation until the PS5 route is proved. `GuestModuleLifecycle` uses `cap13_win64_call3` for callback table order and DLL entry notifications (process attach, thread attach/detach, process detach), but starts each event by refreshing/publishing the TEB. It never invokes guest code if `enter()` fails.

**Evidence basis and limit:** The five unchanged PE samples decode direct `GS:[0x58]` or `GS:[0x30]` reads. The x64 TEB pointer offsets are also documented in Wine's compatibility headers. Microsoft documents the callback three-argument shape and order within its table, plus DllMain reason codes. ReactOS loader source places TLS callbacks before entry notifications; use that as an implementation hypothesis, not proof of Windows or PS5 behavior.

**Tests:** Write failing host tests with synthetic `ms_abi` callbacks and DllMain, a recording segment-switch fake, two threads, a late module refresh, failed backend entry, callback order, and attach/detach reasons. Build on macOS x86-64 with warnings as errors. Package an exact-base patch and evidence. Do not enable the PE loader's real-TLS path until a guest GS backend, callback-target validation, and all relevant ABI gates are verified.
