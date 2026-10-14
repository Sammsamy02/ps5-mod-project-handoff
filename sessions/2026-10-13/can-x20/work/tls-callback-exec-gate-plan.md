# Pre-registration executable TLS callback gate

Stage `cg-tls-callback-exec-gate-20261013-23`, copied from isolated stage 22. Shared candidate and unchanged DLL binaries remain untouched.

Change `register_mapped_static_tls` to require a caller-supplied executable-target predicate for every non-null TLS callback. It checks the relocated callback target after confirming it lies inside the mapped image, before creating a static TLS bank slot or writing AddressOfIndex. The caller derives executable ranges from validated PE section metadata. Reject a callback in a non-executable section with no bank slot, index write, or published callback vector. Update the synthetic registration test and read-only five-DLL preparation runner to supply this predicate. Keep the runner's independent post-check as defense in depth.

Add a failing test first for an in-image non-executable callback target. Run synthetic/static TLS, unchanged DLL read-only prep, and all later regressions. Package exact-base patch and evidence.
