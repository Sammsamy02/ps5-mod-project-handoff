# Windows heap and thread error services

Continue the recorded next action after verified dependency DLL execution.
The previous goal turn was progress; the full DLL/UE4SS objective remains open.
Console/game execution is not assumed from local tests or object compilation.

The design agreed during delegation separates three owned components:

1. A native C heap owner with genuine allocation/release callbacks, 16-byte
   aligned results, checked arithmetic, real calloc initialization and realloc
   preservation. Pins and live allocations prevent backend teardown.
2. A native pthread adapter providing a stable Windows errno slot per registered
   native thread, separate from host errno. Entry happens before any Windows
   calls; lookup does not allocate. Explicit leave or thread exit releases the
   registration. Final domain cleanup requires quiescence and joined workers.
3. A typed Microsoft x64 bridge owning an explicit service session for each
   consumer module. A session retains the heap backend until consumer cleanup.
   Imported calls serialize through the native lock callbacks. Context changes
   require global quiescence; there is no temporary per-call global owner swap.

The bridge handles the four observed heap names and `_errno`; a combined binder
adds the existing fourteen stateless services only after a valid session's heap
binder returns unresolved. Unknown dependencies remain unresolved.

Focused evidence is native heap unit behavior, three actual native threads,
cross-ABI calls from an owned Windows DLL, and PS5 object compilation. The DLL
must exercise allocation, zeroing, resizing, injected native allocation failure,
old-buffer preservation, per-thread errno and lifetime refusal. Include one old
CRT import to prove binder composition. Independent reviews run in parallel.

Do not add Windows thread creation, Tls/Fls, PE TLS callbacks, exception support,
game mappings or a console dispatcher under this slice's name. The existing
stopped native actions are not retried. Retain source/receipts and publish only
the coherent private delta once focused integration succeeds.
