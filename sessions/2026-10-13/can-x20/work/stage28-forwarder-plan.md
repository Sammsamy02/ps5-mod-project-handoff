# Stage 28: bounded mapped PE forwarders

Base: isolated stage 27. Work only in `cg-forwarded-module-exports-20261013-28`.

1. Add failing host tests that mutate copies of the synthetic fixture to forward named and ordinal exports to attached providers. Keep the original fixture unchanged. Check missing target, malformed text, and cycles.
2. Read forwarder strings only within the export data directory. Resolve through the existing shim registry with a bounded per-thread lookup chain. Accept a verified callable target; refuse absent, malformed, unattached, or cyclic chains.
3. Run focused and full offline regressions; package an exact incremental patch and update handoff. No console or gameplay claim.
