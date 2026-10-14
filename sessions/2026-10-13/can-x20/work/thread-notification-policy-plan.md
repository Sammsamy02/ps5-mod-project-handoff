# Module-aware DisableThreadLibraryCalls

Stage `cg-thread-notification-policy-20261013-22`, copied from isolated stage 21. Shared candidate and unchanged DLLs remain untouched.

All five unchanged DLL samples statically import `DisableThreadLibraryCalls`, and all five have a PE TLS directory. Microsoft's documented behavior is failure for active static TLS. Add a module-handle policy registry to `WinApiShim`, populated by a loader adapter with TLS status and a notification-disable callback. The imported handler returns FALSE and sets `ERROR_INVALID_PARAMETER` for null/unregistered handles or active static TLS; for a registered non-TLS module it calls the adapter callback outside the shim mutex and returns TRUE only when notifications were actually disabled. On success preserve prior last error. The exact failure code is an emulation choice because the public function reference does not specify it.

Add a `GuestModuleLifecycle` switch that skips thread attach/detach callbacks after a successful disable, while retaining process attach/detach and wrong-state refusal. A failed process attach resets the switch. The adapter can wire the registry callback to this method after validating the mapped PE metadata; this stage itself does not change the loader's real-TLS refusal.

Write failing host checks first for invalid handles, TLS refusal, callback failure, non-TLS success, notification suppression, and process notifications. Run prior regressions and package exact-base patch/evidence.
