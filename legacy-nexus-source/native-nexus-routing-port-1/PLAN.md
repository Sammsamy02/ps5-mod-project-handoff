# Native metadata endpoint routing

Port the existing desktop client's six metadata routes into the native module. Use an optional owning URL and a whole-output C interface; reject invalid kind/context/domain/identity. Fixed HTTPS origins, validated ASCII domain segments and exact integer serialization avoid arbitrary URLs. This is request construction only, with no credentials, entitlement or network action.

1. Add minimal empty-result scaffolds and focused positive/negative C++/C behavior assertions; observe RED.
2. Implement routing using existing domain validation and signed-63-bit identity policy.
3. Run only metadata/interface checks, then compile/link the changed module for the pinned PS5 target. Keep the target unexecuted.
4. Record actual evidence and integrate the product sources. No review/sanitizer loop or console/game/system writer is involved.
