# Generic title paging and read-only native source capture

The first validation title is open. Reuse format/engine dispatch without a title or mod-ID whitelist. This batch implements prerequisites for the PS5-only pipeline; it does not complete native Nexus, engine ports or game installation.

Design: ../specs/generic-title-inventory-paging-design.md. Source grounding: ../specs/ps5-title-metadata-primary-source-investigation.md. Follow outputs/ps5-mod-bridge/EXPERIMENTS.md: focused local checks, no console promotion in this batch.

- [x] Inventory owner: separate full validated immutable capture from v1 size-limited projection. Add bounded canonical v2 page/current accessors, generation cursors and single-capture concurrency. Resolve only against the full trusted cache. Add focused failing-first tests for large generic captures, full invalidation, wire boundaries and concurrency.
- [x] UI owner: adapt the retained canonical-page normalizer; bounded stream reads, request and deadline ledger, cross-page consistency, current confirmation and atomic target commit. Capability fallback preserves native unavailable preview. Add focused failing-first UI controls; no premature selectable prefix.
- [x] Root: add exact-byte authenticated routes, capability and safe constant errors. Protect refresh with existing exact Origin/session/CSRF plus 16-byte exact-empty-object body. Add focused HTTP controls and integrate all owners.
- [x] Native owner: add one-candidate read-only metadata capture under fixed metadata-cache source profiles. Production roots are fixed by enum; candidate is a generic validated segment. Traverse no symlinks, require regular file, capture full bounded bytes with aggregate read/operation budgets and before/after identity, match decoded title ID. Return owning metadata-only result with all game/build/install flags zero. A host-only injected fixture seam must not enable arbitrary production paths. Directory enumeration and installed/running completeness are out of scope; platform provider stays unavailable.
- [x] Verify relevant Python/UI/native source fixture and pinned SDK compile/link checks. Review concrete changed boundaries and update the public checkpoint. Root owns the final commit and guarded original fast-forward sync; active-goal progress records its resulting head and preservation receipt.

Agents own disjoint files; root owns host, integration docs, Makefile and Git. No account, FTP, console payload-store, game or system writes are part of this batch. Existing frozen artifact roots are preserved; create fresh evidence when assets/source change.

Verification: module11, HTTP8, UI23 and actual-host128-title browser fixture passed; existing host27, inventory8 and report5 passed. Source capture six groups/332assertions plus C ABI passed. Updated native UI and native source artifacts compile/link only; no console execution, provider enablement or installation.
