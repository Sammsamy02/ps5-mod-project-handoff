# Windows service and dependency loader progress

This source slice extends the reusable DLL runtime with four observed byte-search
imports and a dependency owner over `mb_pe_module`. No game names, installation
paths or per-mod rewrite recipes occur in the production code.

The search imports are `VCRUNTIME140.dll!memchr`, `strchr`, `strrchr` and `strstr`.
Their implementations return addresses in the original input buffers and follow
byte conversion, terminator, first/last match and empty-needle semantics. The
service binder now implements fourteen exact library/function pairs. Unknown
pairs remain unresolved. All Windows-callable functions preserve the Microsoft
x64 ABI through the existing typed LLVM IR build route.

## DLL dependency ownership

`pe_dependencies.h` exposes `mb_pe_link` and a caller-declared registry of loaded
provider modules. During consumer preparation, imports from registered libraries
resolve through the provider's executable export API by name or ordinal. Each
resolved provider import holds an export lease. Registered libraries never fall
back to unrelated system implementations when an export is unavailable.

The consumer owns those leases through attach, execution, detach and cleanup.
Closing a consumer with active exports returns BUSY. Failed memory release keeps
its provider references for the next cleanup attempt. Only an EMPTY consumer can
retire its provider leases. A retirement failure retains wrapper ownership for a
later retry. This prevents unloading a helper DLL while another DLL still calls
it, including during initialization or shutdown.

This is import and lifetime management for already-loaded modules. It does not
discover files, load prerequisites from disk, resolve cycles/forwarders/data
exports, or implement Windows `LoadLibrary` and `GetProcAddress`. Caller-owned
objects and pin storage remain stable and externally serialized. The existing
PE owner's TLS/exception/page-protection/entry restrictions still apply.

## Evidence scope and remaining work

The owned consumer calls the provider through its real imported function and
returns the expected value `89`. Busy close, failed deallocation, immediate
failed-load rollback and deferred rollback all preserve or release provider
pins at the required time. Six focused owner groups pass normally and under
ASan/UBSan. The same owner compiles to an 8,224-byte PS5 object. Review found and
fixed one argument-alias/error-reporting issue; final receipts cover the fix.

The search fixture actually calls all four new services and checks returned
pointer offsets `[1, 0, 3, 1, 5, 0]` in original host buffers, along with guarded
bounds. Nineteen service groups pass, covering all fourteen current functions.
The PS5 service object is 5,888 bytes with no undefined imports.

Verification receipts accompany this report. The actual executable tests use
owned Windows DLLs compiled from the included C sources in a local x86_64 Mac
process. PS5 target objects provide build evidence only; this slice makes no
console execution claim and performs no console contact or game/system writes.

`RETAINED-IMPORT-COVERAGE.json` compares the exact current service bindings with
retained original DLL metadata. Five of 135 import slots in `dwmapi.dll` and
fourteen of 634 in the Windows `UE4SS.dll` match current services. These counts
are not compatibility percentages. Import families, TLS, exceptions, module
lookup and other image requirements are independent unfinished work. Neither
original DLL was executed by this comparison.

The native UE4SS source port is a separate path from running the Windows DLL.
It still requires a genuine native game-context provider, validated matching
Unreal layouts and actual engine/game-thread calls before Item Spawner can be
tested. No host fixture supplies that missing game evidence.

The next major Windows-service family is process-owned allocation and thread
state: the retained images require `malloc`, `calloc`, `free`, `realloc`, heap
APIs, TLS and last-error storage. Their ownership and platform behavior must be
implemented; substituting success returns is not a compatibility layer.
