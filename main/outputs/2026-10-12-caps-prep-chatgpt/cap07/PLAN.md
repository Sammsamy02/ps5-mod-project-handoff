# Container selection — Mac-only plan

User intent: prepare #7 immediately without touching Claude's runtime or delaying its build.

1. Use the existing admitted v8 TOC parser, read-only; discover package ownership from ExportBundleData chunk IDs rather than a chunk filename.
2. Test renamed containers, ambiguous ownership, multi-container mods, missing matches, unsupported inputs, determinism and input preservation before implementing.
3. Emit candidate file identities and derived sidecar names only. Refuse ambiguity, empty scans, and unsupported or multi-container composition. An incomplete capture never proves global ownership.
4. Replay discovery on the five retained base TOCs plus the original Leon mod. This is Mac-only candidate selection, never a runtime-admission receipt.
5. Hand back corrected vtable geometry, pin-evidence gaps, an observe-first data-only mount plan and the user's test order. No runtime patch is applied or made build-ready from unverified pins.
