# Guest TEB and TLS lifecycle: isolated Mac stage

**Result:** The isolated `cg-teb-lifecycle-20261013-11` stage adds a per-thread Windows x64 TEB prefix, a published static TLS pointer array, and a Win64 callback/entry notification dispatcher. A segment-switch interface must enter successfully before any guest call. The included production placeholder refuses entry, so this stage does **not** execute an unchanged DLL or lift the existing PE loader refusal.

## Implemented seam

- `GuestTebView` owns the thread's TLS pointer-array snapshot. It writes its own address at `TEB+0x30` and the array address at `TEB+0x58`, then refreshes the array after late module registration. It refuses use on a different host thread.
- `GuestModuleLifecycle` rejects a TEB from another TLS bank, invokes the retained image's validated TLS callbacks in table order with the Win64 three-argument bridge, then calls the image entry point for process and thread attach/detach notifications. It treats a false process-attach entry return as failure. The caller must validate mapped addresses and serialize module notifications.
- `GuestSegmentSwitch` isolates the unproved platform operation. `UnavailableSegmentSwitch` always refuses; a recording fake is used only by the host test. No PS5 GS-base code is claimed or enabled.

The [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) specifies the TLS callback signature and table order; [Microsoft's DllMain documentation](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain) specifies the reason values and process-attach failure signal. The x64 TEB offsets are consistent with [Wine's compatibility structures](https://github.com/wine-mirror/wine/blob/master/include/winternl.h) and the direct GS accesses observed in the five unchanged samples. Calling callbacks before the entry point follows the [ReactOS loader implementation](https://github.com/mirror/reactos/blob/master/reactos/dll/ntdll/ldr/ldrinit.c) as an offline implementation hypothesis; it is not proof of every Windows build or the console route.

## Verification

The new x86-64 host runner passed **12/12** checks using synthetic `ms_abi` callbacks and entry code. It checked TEB bytes and callback order, a separate worker TLS block, late array refresh, process/thread reason codes, wrong-thread and wrong-bank refusal, false process attach, and backend refusal without guest calls. The prior static TLS suite passed **19/19**. Five unchanged real DLLs again passed read-only TLS preparation with their original hashes unchanged; no callback or entry code from those DLLs ran. The [evidence ZIP](guest-teb-lifecycle-evidence-20261013.zip) contains all three runner logs and the patch replay check.

The [incremental patch](guest-teb-lifecycle-increment-20261013.patch) adds four files against the exact prior `cg-real-tls-evidence-20261013-10` stage. The [manifest](guest-teb-lifecycle-manifest-20261013.json) pins their hashes and records the base files as absent. `git apply --check`, application in a fresh scratch directory, and byte-for-byte comparison with the staged files passed. The shared candidate, Claude's work, kernel code, and original DLL binaries were untouched.

## Remaining gates

The TEB prefix supplies only the two measured offsets; ordinary Windows TEB/PEB and runtime behavior are not emulated. The PS5 guest GS-base route, loader wiring, callback-target validation at the invocation boundary, thread-event serialization and edge cases, MSVC CRT and UE4SS ABI, dynamic imports, and automatic cross-build game-target correspondence remain open. A synthetic host callback sees the published pointer through the fake backend; it does not prove that actual `GS:` instructions in an unchanged DLL read it. These remain offline engineering tasks where possible, with console behavior requiring a later target run. The project is **not yet a console-only wait**.
