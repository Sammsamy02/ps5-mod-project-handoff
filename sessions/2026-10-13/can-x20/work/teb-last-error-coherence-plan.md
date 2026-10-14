# Guest TEB and imported last-error coherence

Stage `cg-teb-last-error-coherence-20261013-19`, copied from isolated stage 18. Shared candidate and unchanged DLL binaries remain untouched.

Add a common per-thread last-error state helper shared by `WinApiShim` and the offline guest-TEB lifecycle. Use the existing x64 TEB prefix at offset `0x68` for its DWORD `LastErrorValue`. `GuestTebView::refresh` must copy the host-thread value into the prefix. While a lifecycle callback/entry is active, imported `GetLastError` reads the prefix, imported `SetLastError` writes both the slot and prefix, and a direct guest write to the prefix is copied back before leaving the guest segment. Nested bindings restore the prior TEB pointer. The production segment backend still refuses entry.

Add a failing lifecycle host test first: set a thread error, refresh TEB and inspect its field, simulate direct guest modification in a Win64 callback, observe that value via the shared import state in a later callback, then set a new value through the import state and verify it survives segment exit. Run prior host/fixture regressions and exact-base replay. State clearly that a Mac fake segment switch does not validate actual PS5 GS switching or arbitrary TEB layout compatibility.
