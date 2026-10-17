#ifndef MODBRIDGE_LUA_OWNED_BINDING_H
#define MODBRIDGE_LUA_OWNED_BINDING_H

#include <stddef.h>
#include <stdint.h>
#include "lua.h"

#define LBA_DESCRIPTOR_VERSION 1u
#define LBA_FRAME_CAP 64u
#define LBA_NAME_CAP 127u
#define LBA_TYPE_INT32 1u
#define LBA_ENCODING_LE_TWOS_COMPLEMENT 1u

/* These are owned provider identifiers, never addresses or engine handles. */
typedef struct {
    uint64_t owner_id;
    uint64_t object_id;
    uint64_t generation;
} lba_receiver;

typedef struct {
    uint32_t version;
    uint32_t frame_bytes;
    uint32_t input_offset;
    uint32_t return_offset;
    uint16_t input_count;
    uint16_t return_count;
    uint16_t input_type;
    uint16_t return_type;
    uint16_t encoding;
    uint16_t reserved;
    uint64_t member_id;
    uint64_t generation;
} lba_descriptor;

/* All callbacks are trusted, synchronous, bounded, non-reentrant C services.
 * They must not call Lua, longjmp, yield, spawn work, retain span/name pointers,
 * or access engine/process memory. Only invoke receives a writable owned span.
 * Its frame is live only during that call; the adapter clears it before any
 * subsequent Lua API call. Provider data must outlive lua_close. A future
 * engine provider requires a separate target ABI/lifetime/thread design.
 * Return exactly 1 for resolution/validation/authorization success; invoke
 * returns 0 for success, nonzero for a controlled failure. */
typedef struct {
    uint64_t owner_id;
    void *owned_context;
    int (*resolve_receiver)(void *, const uint8_t *, size_t, lba_receiver *);
    int (*validate_receiver)(void *, lba_receiver);
    int (*resolve_member)(void *, lba_receiver, const uint8_t *, size_t,
                          lba_descriptor *);
    int (*authorize)(void *, lba_receiver, lba_descriptor);
    int (*invoke)(void *, lba_receiver, lba_descriptor, uint8_t *, size_t);
} lba_provider;

/* Trusted installation must be inside a Lua protected call (allocation can
 * raise LUA_ERRMEM). Copies provider metadata into Lua-owned userdata, opens
 * no libraries and preserves the caller's stack. One instance owns its own
 * locked metatables. Same IDs from another instance are still foreign.
 * The lookup accepts exactly one UTF-8 string. Unresolved lookup returns nil.
 * A bound method accepts (integer), or (its exact captured userdata, integer).
 * Numeric strings, floating-point numbers and out-of-range integers fail. */
int lba_install(lua_State *, const lba_provider *, const char *, size_t);
int lba_descriptor_valid(const lba_descriptor *);

#define LBA_BOOL_DESCRIPTOR_VERSION 1u
#define LBA_TYPE_BOOL 2u
#define LBA_ENCODING_BOOL_MASK 2u
#define LBA_BOOL_READ 1u
#define LBA_BOOL_WRITE 2u

typedef struct {
    uint32_t version;
    uint32_t storage_bytes;
    uint32_t byte_offset;
    uint32_t permissions;
    uint64_t property_id;
    uint64_t generation;
    uint16_t value_type;
    uint16_t encoding;
    uint8_t field_mask;
    uint8_t byte_mask;
    uint16_t reserved;
} lba_bool_descriptor;

/* Optional property extension; copies into the existing owning instance.
 * resolve: 1 found, 0 not a property (method lookup may follow), other failure.
 * access: one trusted bounded transaction, no Lua/longjmp/yield/retained spans
 * or engine/process pointers. Revalidate current receiver/property generation
 * and permission inside that transaction. READ returns exact 0/1; WRITE
 * receives exact 0/1 and applies the mask to CURRENT owned storage, without
 * an adapter read-then-overwrite. Failure must leave provider storage unchanged.
 * Return 0 on success, nonzero on failure. Context outlives lua_close. This is
 * not a PS5 reflection/game-thread/GC provider. */
typedef struct {
    void *owned_context;
    int (*resolve)(void *, lba_receiver, const uint8_t *, size_t,
                   lba_bool_descriptor *);
    int (*access)(void *, lba_receiver, lba_bool_descriptor, uint32_t, int, int *);
} lba_bool_provider;

/* Installation is protected and stack-preserving like lba_install.
 * A NULL extension is the original Int32-only API. __newindex never creates
 * local userdata fields and accepts only an exact Lua Boolean. */
int lba_install_with_bool(lua_State *, const lba_provider *,
                          const lba_bool_provider *, const char *, size_t);
int lba_bool_descriptor_valid(const lba_bool_descriptor *);

/* Pure byte-storage primitive for the provider's SINGLE transaction.
 * Caller provides exactly descriptor.storage_bytes, <=64 bytes. Descriptor,
 * storage and scalar result are disjoint valid objects. On ANY preflight
 * rejection they are untouched. No pointers retained, no allocation.
 * READ needs read permission; WRITE needs write permission and exact value.
 * Success returns 1 and writes exact Boolean result; failure returns 0. */
int lba_bool_apply(const lba_bool_descriptor *, uint8_t *, size_t,
                   uint32_t operation, int value, int *result);

#define LBA_REF_DESCRIPTOR_VERSION 1u
#define LBA_TYPE_OBJECT_REFERENCE 3u
#define LBA_ENCODING_OWNED_IDENTITY 3u
#define LBA_REF_READ 1u
#define LBA_REF_WRITE 2u

typedef struct {
    uint32_t version;
    uint32_t permissions;
    uint64_t property_id;
    uint64_t generation;
    uint16_t value_type;
    uint16_t encoding;
    uint32_t reserved;
} lba_ref_descriptor;

/* Optional typed object-reference extension. Identities are owned provider
 * identifiers, NEVER engine handles or addresses. Canonical null is exactly
 * {provider.owner_id, 0, 0}; live identity has that owner and nonzero object ID
 * and generation. Mixed null/live fields and foreign owners are invalid.
 * resolve returns 1 found, 0 absent, other failure. access returns 0 success,
 * other failure. READ receives canonical null and returns a current identity
 * or canonical null. WRITE receives exact same-instance userdata identity or
 * canonical null (including a Lua nil assignment); its result is ignored.
 * Each access is ONE trusted bounded synchronous transaction: revalidate the
 * current receiver/property/target identity, permissions and generations;
 * failure leaves owned storage unchanged. No Lua/longjmp/yield/reentrant work,
 * retained spans/results or engine/process memory. Context outlives lua_close.
 * Provider validate_receiver status is exactly 1 current, 0 invalid/stale,
 * other failure. No callback is made for canonical-null validity/chaining.
 */
typedef struct {
    void *owned_context;
    int (*resolve)(void *, lba_receiver, const uint8_t *, size_t,
                   lba_ref_descriptor *);
    int (*access)(void *, lba_receiver, lba_ref_descriptor, uint32_t,
                  lba_receiver, lba_receiver *);
} lba_ref_provider;

/* Protected, stack-preserving installation like the retained wrappers above.
 * Copies metadata and owns locked instance-specific userdata/metatables; only
 * external provider contexts remain caller-owned. Either extension may be NULL.
 * Existing unresolved top-level lookup still returns nil. Ref reads returning
 * canonical null produce THIS instance's invalid userdata, not Lua nil.
 * __index precedence: reserved IsValid, null chain, reference, Boolean, method.
 * __newindex checks reference before Boolean; reference assignment accepts
 * only THIS instance's userdata or Lua nil. No arbitrary local fields exist.
 * Reserved IsValid assignment fails after bounded name validation and before
 * either property resolver is called, even if a provider exposes that name.
 * IsValid takes exactly one owned receiver and is available on stale values:
 * current status 1 -> true, status 0 or null -> false, other status -> error.
 * Null property reads produce another null; null writes/stale access error.
 * Equality compares same-instance owner/object/generation without callbacks;
 * same-instance nulls are equal and foreign instances always compare unequal.
 * This owned component supplies no discovery/reflection/game-thread/GC runtime.
 */
int lba_install_with_refs(lua_State *, const lba_provider *,
                          const lba_bool_provider *, const lba_ref_provider *,
                          const char *, size_t);
int lba_ref_descriptor_valid(const lba_ref_descriptor *);

#endif
