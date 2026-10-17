# Windows DLL preparation progress

This slice adds a generic C11 PE32+ AMD64 image-preparation implementation at
`outputs/ps5-mod-bridge/windows-image/`. It places sections in caller-owned data,
zero-fills virtual storage, applies DIR64 relocations, exposes imports/exports,
and writes import bindings supplied by the caller. It does not execute input
code, provide Windows services, translate C++ object layouts, or install a mod.

The optional original-input test uses the unchanged `dwmapi.dll` and
`ue4ss/UE4SS.dll` from the retained UE4SS ZIP. It compares entire prepared images
and import/export identities against LLVM's independent PE records at the
preferred base and two rebased addresses. A fourth trial checks import writes
using opaque integers; those integers are byte-test controls, never callable
Windows implementations. No downloaded DLL is loaded as a host library.

| Original input | Imports | DIR64 relocations | Nonzero export slots |
| --- | ---: | ---: | ---: |
| dwmapi.dll | 135 | 117 | 143 |
| UE4SS.dll | 634 | 14,516 | 4,256 |

The full UE4SS DLL also declares TLS, exception data, load configuration and an
entry point. Those are remaining runtime requirements, not implemented services.
The current native C++ mod loader continues to reject Windows PE files.

An independent code review found overlapping IAT destinations and duplicate or
overlapping relocation destinations could corrupt preparation. Focused regression
checks accompany the correction; the final receipt records the reviewed source
and the fresh runs. Developer test commands live in the component README.

The parallel game-binding review did not justify a native AddDelegate ABI change:
the retained Linux example explicitly targets Unreal 5.1, while Townfall's saved
resolver evidence reports 5.6. A by-value API declaration alone does not establish
whether the callee expects registers or an indirect C++ argument. Existing SDK
file/mapping/errno and pthread paths already use the PS5-specific definitions.
No game-binding source was changed on the strength of those uncertain assumptions.

Next required work remains the genuine native game-context supplier, matching
Unreal layouts and game-thread execution for Item Spawner; separately, Windows
service implementations, calling-convention/C++ interoperability and DLL lifetime
support are needed before any binary DLL can run. This slice does not change the
historical console evidence, frozen PRX or pending playtest state.

This work involved local development only: no console contact, game/system writes,
production-account requests or input DLL execution. Original DLL bytes and build
binaries are excluded from the source publication.
