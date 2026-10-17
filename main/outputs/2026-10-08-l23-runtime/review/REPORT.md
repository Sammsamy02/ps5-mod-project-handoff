# Independent Track A integration review

Reviewed l23 profile, type-scan and thread overlays, selected l22 source/dependency receipts, and root's adapted l23 build driver. Read-only apart from these review notes and separately authorized typescan-r2 corrections. Passed suites were not repeated during review; r2's changed sources received focused new regression checks and required target recompilation.

## Actionable findings and resolution

- **P1, resolved in typescan-r2:** accepted TypeChecker read `field->GetNextFFieldUnsafe()` before validating the current field. An unreadable current/next field could reach a raw dereference first. R2 uses the same native read backend through ReadNext and checks both current and next node before iteration. Missing Next is still collected separately; pointer refusal is not swallowed.
- **Build blocker, resolved in typescan-r2:** the by-value Array Inner getter broke two selected CustomProperty.cpp assignments. The earlier type-scan audit missed direct Array assignments. Full selected-source search finds only these two writes. R2 preserves construction via a TU-local bounded writer into the fresh runtime-owned unique_ptr allocation; no unsafe getter reference is reinstated.

## Profile and skip behavior

`required_members` and `required_vtables` are byte-for-byte structurally equal between l22/l23 profile JSON (69 + 24 = 93 facts). Only evidence metadata differs. Compiled native declarations retain FArrayProperty::Inner=0x78 and contain no FMapProperty/FSetProperty member declarations. Joined preflight marks Inner Required and the three absent Map/Set keys Conditional/unpinned. The code's missing-member diagnostics use exactly `FMapProperty::KeyProp`, `FMapProperty::ValueProp`, `FSetProperty::ElementProp`, matching the collector and preflight spellings. Key and Value nested attempts are independent. No new offsets or Ansi/Utf8 pusher registrations were introduced.

## Header/closure review

`HEADER-ALIAS-AUDIT.json` lists every selected dependency alias for six changed container generated wrappers and the three profile headers. Every observed same-name dependency matches the expected l22 original digest; there are no unexpected differing-content aliases in this snapshot. Map/Set/Array return-type declarations affect their full include closure, not just TypeChecker/property implementation objects.

The current l23 driver correctly bases both passes on l22-build-r3, maps changed source names exactly, rewrites inherited VFS external targets to current candidates, merges redirects into a single overlay, removes inherited overlay flags, places candidate include directories first, invalidates every dependency using a changed alias, and rejects unapproved new dependencies. It preserves all 374 units/391 ordered link inputs and requires full raw undefined-symbol equality. Parent will verify its emitted dependencies and final package receipts. `vfs-order/RESULT.json` records a small new compiler experiment confirming that layered overlay order can affect redirected header contents; the merged current driver avoids relying on that subtle ordering.

New getter guards call existing VQ/fault/image mechanisms and request no new OS import. The thread probe's scePthreadGetthreadid import is already used by selected runtime sources; final raw/package import equality remains the build gate. Program revokes pointer guards after event-loop join, mod cleanup and input shutdown, retaining guards through engine-facing cleanup.

No further actionable issue found within this review scope after r2 correction. This does not replace complete parent compilation, binary/package checks, target execution or independent official-header layout evidence. Track A deliberately retains all 93 prior facts and promotes no blocked container measurement.
