# Windows DLL worker runtime

The opt-in owned DLL candidate now creates a real native worker from an internal, non-exported Windows PE function. Actual PE instructions pass a 64-bit parameter, exercise separate thread LastError, observe WAIT_TIMEOUT/STILL_ACTIVE, wait for completion, retrieve DWORD 0x81234567, repeat the wait, and close the handle. The original consumer/helper calls still return 86/93.

The runtime acquires an independent executable code lease before starting the worker and retains it through join, including early handle closure and cleanup failures. Imported calls preserve the Microsoft x64 ABI; the PS5 objects are produced using the SDK C frontend and unchanged Win64 LLVM IR. DllMain creation admission is suspended even during nested LoadLibrary. A failed leave stays owned and retries on the original thread without repeating either worker export.

Evidence: 62 commands passed for the fresh composed SDK candidate, plus 16 rejected-attach controls; the worker component has real pthread/Win64 and sanitizer checks, and distinct actual-PE lifetime tests cover closed handles and retained release failures. Only the existing PE /align link warnings remain. Nonworker entry/clock PE fixture bytes match the frozen clock profile; the default no-entry fixture sources are unchanged. The earlier clock regression receipt predates gated cleanup edits; its matching fixture bytes do not establish a fresh current-source runtime result.

The native candidate is 220,968 bytes, SHA-256 03a6cd2d38d5e638a21ebe319df8cb987e7747e2f79f006ddf78ba0de65ffc82. It has not been uploaded or run on PS5. TEST_THIS_DLL remains the f72faea5 startup/clock candidate awaiting a reported run. The earlier db1c40e6 direct DLL test is proven on PS5.

This slice does not establish UE4SS engine/game-thread calls, arbitrary Windows DLL compatibility, MSVC Unreal object layouts, static PE TLS, SEH, thread loader notifications or asynchronous DllMain work. Only default-stack, zero-flag threads and owned thread handles are provided. No console game files were modified and no stopped target action was retried.

Build locally from the worktree:

```sh
python3 outputs/ps5-mod-bridge/windows-candidate/build.py --entry --clock --workers --report-path /data/modbridge-owned-dll-candidate.jsonl
```

Receipt copies in this directory distinguish actual host PE execution from native object/ELF compilation and target execution.
