# Stage 46 owned guest worker entry

Base: exact isolated DLL stage 42. This stage supplies the lifecycle
primitive a future reviewed CreateThread route must use; it does not publish
CreateThread or claim Windows thread handles. Preserve the default segment
refusal until a console GS round trip passes.

Expose a bounded executable-address check on an attached GuestPeModule and a
Win64 one-argument worker-start invocation behind that module's guest
segment/TEB and last-error binding. A batch-level runner, called on the
new host worker thread, validates the start address against an attached
module, creates that thread's GuestTebView/static TLS, sends thread attach
to all modules in dependency order, invokes the start, and sends thread
detach in reverse order. On attach or entry failure, detach every module
already notified; if cleanup fails, return a distinct dirty result.

Test on a real host pthread using the unchanged synthetic PE fixture and
mock segment switch, plus invalid-start and backend-refusal cases. Run the
existing 15 host regressions, fixture, PS5 target build, exact-base replay.
The Windows CreateThread/WaitForSingleObject/CloseHandle API and console GS
remain later work.
