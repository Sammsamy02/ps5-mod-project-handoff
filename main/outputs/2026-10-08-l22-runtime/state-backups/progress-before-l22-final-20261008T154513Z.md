# Persistent PS5 progress — l21 local audit, 2026-10-08

Console-proven (saved PID184 launch only): runtime885f43c6 ran, pool/FName/current profile gates passed, then startup stopped before Lua on missing FMalloc::QuantizeSize. No menu/spawn proof. No new console contact in l21. Prior user report: no crash.

Mac-only verified: exact official UE5.6.1 headers fetched through existing source access; documented Shipping-default FExec order gives17 allocator candidates, all executable image targets, six independent semantic anchors agree. Both conditional source branches retained; actual compiler overrides unobserved. Two exact-header host layout probes passed and parent reproduction matched5 semantic artifacts. Original image measurement parent reproduction matched7 artifacts.

Complete linked lookup census:254 virtual literals +435 member literals,711 unique keys after profile/direct-reader union.76 existing profile facts,635 unresolved/unclassified; this is NOT635 menu requirements.11 focused offline extractor/collect-all controls pass. Runtime collect-all gate NOT implemented or built in this audit.

Targeted API dependency join:63 unique keys =47 profiled +14 absent +2 FText runtime discoveries. Four absent startup/menu allocator keys have new candidates (QuantizeSize/Malloc/Realloc/Free);10 others are conditional, not proven menu blockers. This join is not a closed dynamic dependency graph.

Remaining issues: scoped startup/menu requirement closure; promote validated allocator facts with provenance; selected SCO placeholder alignment8 versus official inline callback alignment16 (actual caller alignment unknown); nested/reflected value bounds and FString/FText/TArray ownership; complete generic reflection/API validation. Input/menu/spawn remain future console checks, not a circular condition requiring prior success before building. Spawn-only dependencies separately classified.

Version audit: banner3.0.1 hides selected revision1152 commits beyond stable3.0.1; later5.4/5.5/5.6 code exists. UTF8/ANSI pusher implementations lack two dispatch registrations; not established as this mod's current blocker.374 selected source hashes match.

Next: review the missing-facts packet and complete the local requirement/ABI/ownership work. User instruction: build only when all startup/menu facts are measured; otherwise tables only. No l21 PRX built, no upload/launch authorized by this audit. Original PC mod/profile/runtime unchanged.

Full report: outputs/2026-10-08-l21-audit/REVIEW-REPORT.md
Packet location/hash and final manifest verification: outputs/2026-10-08-l21-audit/PACKET-RECEIPT.json
