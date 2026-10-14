# Captured script class identities (Mac-only)

Three exact Leon ScriptImport class references now resolve from captured global ScriptObjects metadata:

| Selected export | Parsed script class outer-chain path |
|---|---|
| MI_Simon_BodywarmerBeanieTag_Deluxe | /Script/Engine/MaterialInstanceConstant |
| M_Invisible | /Script/Engine/Material |
| SK_Simon_HeadCap_Deluxe_optimized | /Script/Engine/SkeletalMesh |

Evidence: leon-script-class-resolution.json. The existing original-mod identity receipt is hash-bound, and the original ScriptObjects UCAS/TOC are bound by saved profile and capture receipt. Exact chunk lookup, stored block geometry and BLAKE3-20 all pass: 3,412,987 decoded bytes, 53 stored blocks, 48,810 names, 52,042 object records. Five captured UCAS bytes lie outside the declared chunk. Every small input's SHA256 is rechecked after replay; unchanged=true. No payload copy is written.

A dedicated exact v8 flags=0, one-chunk, non-indexed profile adapts existing TOC helpers; generic overlay admission remains unchanged. Header reserved bytes must be zero, with exact EOF and metadata checks. Current base manifest hash is preserved as context; profile/capture association is a retained observation, not proof the installed game remains identical today.

Reuses retained zen-script-import-resolver-1/zen_script_objects.py. Its README cites public retoc commit d034ade1ae8117d4786eaf6b0418d4cf48474d7f, but original script_objects.rs is absent locally, so upstream format attribution was not independently reverified. Decoder, README, profile, capture and TOC/source hashes are all included in the receipt. Retained decoder semantics are inspected and exercised by fixtures; an additional exact-null/complete-chain guard refuses ambiguous outers.

Four focused tests pass: exact identity/outer chain; missing index remains unresolved; cyclic outer refuses; empty selection refuses. tdd-red.txt records missing-feature failures and tdd-green.txt the final scoped suite. No base export/class equivalence, import compatibility, live class availability or native shader usability is promoted. compatible=false and native_shader_proven=false remain explicit.
