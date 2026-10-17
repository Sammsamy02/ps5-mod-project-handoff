# Controller recovery candidate — local only

The exact selected native helper caches negative or retired results forever and
can return a different user's handle. Its detached worker also has no join owner.
The original script and console configuration remain untouched.

Implement within PS5PadNative.cpp without changing PadBackend or class layout:

1. Preserve startup deferral and borrowed-versus-owned close behavior.
2. Serialize the helper request tuple and ownership. Only one helper may run.
3. Join completed helpers before resetting their state; never restart a blocked call.
4. Retry completed negative results via the existing poll backoff. Discard completed
   results from a different user/type/index and invalidate retired borrowed results.
5. Retain the worker; expose a stop-admission/residence query which closes new helper
   starts and refuses unload while a worker is running. Do not integrate an unload
   claim before the actual module-stop entry uses it.
6. Reproduce original failures first; test the actual helper body with explicit host
   SDK seams, then compile the whole selected SDK source and compare imports.

Only local source candidate and checks are authorized here. Hardware semantics,
current PS5Pad.ini mapping, and native input/menu delivery remain unproved.
