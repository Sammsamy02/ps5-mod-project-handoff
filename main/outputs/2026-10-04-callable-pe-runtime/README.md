# Callable Windows DLL progress

The new reusable components progressed beyond data preparation: a freshly built
Windows AMD64 DLL executed in the local x86_64 Mac runner through the production
PE module owner and six real Microsoft-ABI CRT services. The test returned
`DLL->native:PortBridge`, correctly used a relocated data pointer, returned
`103.5` from a six-argument integer/floating-point export, and unloaded after its
call leases were released. No downloaded DLL or console code was executed.

Sources and reproducible commands:

- `outputs/ps5-mod-bridge/windows-services/`: actual `memcpy`, `memmove`, `memset`,
  `memcmp`, `strlen`, `strcmp`, and a binder for the exact documented import pairs.
  Nine focused groups include real ABI calls and all Microsoft nonvolatile
  integer/XMM register checks.
- `outputs/ps5-mod-bridge/windows-execution/`: portable module ownership, strict
  runtime-feature admission, actual-address relocation, per-page protections,
  callable export leases and unload/cleanup lifetime. A review-found repeated
  cleanup failure originally lost the first error; that is fixed and tested.
- `python3 outputs/ps5-mod-bridge/windows-execution/verify_execution.py` rebuilds
  and executes only the owned fixture, including its real imported functions.

The same service source emits a PS5 AMD64 object through unedited LLVM IR with
six explicit `win64cc` definitions. This is necessary because the PS5 C frontend
rejects the `ms_abi` attribute. The alternative frontend's data layout and 16
type/field-layout facts match the native frontend; the native backend preserves
the required convention. The adapter separately compiles directly through the
PS5 C frontend. Neither object has been executed on the console.

The current DLL profile has no entry point, TLS, exception machinery or threads.
Those features, additional Windows services, dependency loading, C++ interfaces
and a PS5 memory-operation caller remain outside this verified execution result.
The six-function binder is an implemented subset, not Windows/CRT equivalence.
The existing native UE4SS C++ mod loader's Windows-file rejection is unchanged.

The engine investigation found no new justified source change: runtime construction
already consumes retained context, but its genuine game-resident context provider,
matching Unreal layouts and actual game-thread hook delivery are still missing.
Metadata records alone cannot provide live residency and hook operations. No
matching Townfall 5.6 facts were invented from the Linux 5.1 example. Prior stopped
native probing and placement routes were not repeated or rerouted.

All production logic added here is generic format/API handling; the test fixture
does not encode a game or Nexus mod. Automatic installation across supported
handlers is the design; this result establishes neither arbitrary Windows DLL
compatibility nor UE4SS Item Spawner success. The attached receipts distinguish
host execution, target compilation and unperformed console work.
