#ifndef PATTERNSLEUTH_MAPPED_CORE_H
#define PATTERNSLEUTH_MAPPED_CORE_H
#include "PsScanAbi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* IDs:1 EngineVersion,2 genuine GUObjectArray patterns,3 FNameCtorWchar,
 * 4 FNameToString,5 GMalloc,6 StaticConstructObjectInternal,7 UGameEngineTick.
 * Status:0 unique,1 arguments,2 descriptor/format/extents,3 no match or absent
 * root metadata,4 ambiguous resolution,5 malformed/unsupported metadata/operand.
 * Caller retains VALID descriptor/array storage and genuinely owned immutable
 * segment bytes throughout the synchronous call. Output must be valid aligned
 * writable storage NON-ALIASING with descriptor, array and ALL supplied bytes.
 * Read lease/protection/lifetime stability is a caller obligation; no arbitrary
 * numeric address authorizes a slice, mapping, registration or access. Runtime
 * address must exactly equal the supplied bytes pointer. Output clears on every
 * failure when its pointer satisfies the valid caller contract.
 * Returned address is an operand-derived resolver candidate. Caller must
 * validate it against supplied leased spans and the output permission role;
 * the leaf does not certify ownership, engine identity, callable ABI or setters.
 * Existing serialized-file endpoints and file-only candidates are separate. */
typedef struct PsMappedCoreResult { uint64_t address;uint16_t major;uint16_t minor;uint32_t reserved; } PsMappedCoreResult;
int ps_resolve_mapped_core_v1(const PsMappedImageViewV1*,uint32_t,PsMappedCoreResult*);
#ifdef __cplusplus
}
#endif
#endif
