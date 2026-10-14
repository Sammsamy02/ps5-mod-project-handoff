# l18 authorized local plan

Status: local implementation and packaging authorized; console contact, upload,
payload dispatch, configuration changes and launches are not authorized.

The latest user request supersedes the logging-only proposal. Combine frozen l17
descriptor/thread cleanup with authenticated image-backed code-signature checking.
Preserve frozen l16/l17, the original PC release, captures and all shared work.

1. Audit the selected runtime code-access graph, including platform guards and
   actual startup/menu reachability. Report the audit before full composition.
2. Prepare a fixed sparse signature cache during module_start, before worker
   construction. Hash the entire configured decrypted ELF and collect prefix bytes
   from that same opened stream. Validate ELF/PT_LOAD geometry, identity, actual
   observed/resolved base and every known function pin. Close the image descriptor.
3. Preserve current live verification for readable code. Positive, committed,
   executable, non-readable VirtualQuery coverage may select the authenticated
   disk signature. Unmapped, malformed, partial, writable-XOM, prior faults and
   readable-copyin failures remain refusals; disk evidence never admits heap data.
   Log query bounds/protection, segment evidence and verification source.
4. Use one original 60-second monotonic admission deadline starting at image
   preparation. Keep the separate existing five-second sink-acquisition policy.
   Neither policy preempts a running OS call or mandatory cleanup.
5. Verify actual initializer/bridge lifecycle, signature selection, file parsing,
   error paths and ownership with focused host tests and native TU compiles. Retain
   l17 cleanup, once-only close, initializer-thread checks and joined-stop ordering.
6. Freeze source hashes; run an isolated full composition twice. Verify determinism,
   actual imports/provider declarations, relocations, fake SELF wrapper, SDK,
   packaging gates and a complete manifest. No old pipeline output is overwritten.
7. Report exact artifact and receipts as Mac-only, with downstream XOM/hook risks.
   Await separate explicit upload approval. Any approved later test is one manual
   user launch with a fresh decrypt-off rollback/readback and NEW klog capture.

Non-goals for this build: controller changes, modified mod files, new hooks,
kernel services, game executable writes, shadow scanner activation or menu/spawn
claims. The optional StaticConstructObject profile pin is cached without activating
its staged check or inventing ABI validation.
