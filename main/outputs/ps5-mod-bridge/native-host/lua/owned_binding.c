#include "owned_binding.h"
#include "lauxlib.h"
#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8 && sizeof(int32_t) == 4 && sizeof(uint32_t) == 4,
               "Int32 byte contract");
_Static_assert(LUA_VERSION_NUM == 504 && sizeof(lua_Integer) == 8 &&
               sizeof(lua_Number) == 8, "matching pinned LuaRaw ABI");
_Static_assert(LUA_MININTEGER <= INT32_MIN && LUA_MAXINTEGER >= INT32_MAX,
               "Lua must contain signed Int32 range");

typedef struct {
    lba_provider provider;
    lba_bool_provider boolean;
    lba_ref_provider reference;
    int has_boolean;
    int has_reference;
} binding_context;
typedef struct { lba_receiver receiver; lba_descriptor descriptor; } bound_method;

/* Clear while storage is still live, before any Lua error/allocation. No
 * callback may retain these addresses or unwind across the owning frame. */
static void clear_bytes(void *storage, size_t length) {
    volatile uint8_t *p = storage;
    while (length--) *p++ = 0;
}

static int valid_name(const uint8_t *bytes, size_t length) {
    size_t i = 0;
    if (!bytes || !length || length > LBA_NAME_CAP) return 0;
    while (i < length) {
        uint8_t first = bytes[i++];
        size_t continuation;
        if (first == 0) return 0;
        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) continuation = 1;
        else if (first >= 0xe0u && first <= 0xefu) continuation = 2;
        else if (first >= 0xf0u && first <= 0xf4u) continuation = 3;
        else return 0;
        if (continuation > length - i) return 0;
        if ((first == 0xe0u && bytes[i] < 0xa0u) ||
            (first == 0xedu && bytes[i] > 0x9fu) ||
            (first == 0xf0u && bytes[i] < 0x90u) ||
            (first == 0xf4u && bytes[i] > 0x8fu)) return 0;
        while (continuation--)
            if ((bytes[i++] & 0xc0u) != 0x80u) return 0;
    }
    return 1;
}

int lba_descriptor_valid(const lba_descriptor *d) {
    if (!d || d->version != LBA_DESCRIPTOR_VERSION ||
        d->frame_bytes < 8u || d->frame_bytes > LBA_FRAME_CAP ||
        d->input_count != 1u || d->return_count != 1u ||
        d->input_type != LBA_TYPE_INT32 || d->return_type != LBA_TYPE_INT32 ||
        d->encoding != LBA_ENCODING_LE_TWOS_COMPLEMENT || d->reserved != 0u ||
        d->member_id == 0u || d->generation == 0u) return 0;
    /* Bounded frame is established before subtraction/addition. */
    if (d->input_offset > d->frame_bytes - 4u ||
        d->return_offset > d->frame_bytes - 4u) return 0;
    return d->input_offset + 4u <= d->return_offset ||
           d->return_offset + 4u <= d->input_offset;
}

int lba_bool_descriptor_valid(const lba_bool_descriptor *d) {
    unsigned mask;
    if (!d || d->version != LBA_BOOL_DESCRIPTOR_VERSION ||
        !d->storage_bytes || d->storage_bytes > LBA_FRAME_CAP ||
        d->byte_offset >= d->storage_bytes || !d->property_id || !d->generation ||
        d->value_type != LBA_TYPE_BOOL || d->encoding != LBA_ENCODING_BOOL_MASK ||
        d->reserved || !d->permissions ||
        (d->permissions & ~(LBA_BOOL_READ | LBA_BOOL_WRITE))) return 0;
    if (d->field_mask == 255u && d->byte_mask == 1u) return 1;
    mask = d->field_mask;
    return mask && !(mask & (mask - 1u)) && d->byte_mask == d->field_mask;
}

int lba_ref_descriptor_valid(const lba_ref_descriptor *d) {
    return d && d->version == LBA_REF_DESCRIPTOR_VERSION &&
           d->permissions && !(d->permissions & ~(LBA_REF_READ | LBA_REF_WRITE)) &&
           d->property_id && d->generation &&
           d->value_type == LBA_TYPE_OBJECT_REFERENCE &&
           d->encoding == LBA_ENCODING_OWNED_IDENTITY && !d->reserved;
}

static int disjoint(const void *a, size_t alen, const void *b, size_t blen) {
    uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
    if (!a || !b || alen > UINTPTR_MAX - aa || blen > UINTPTR_MAX - bb) return 0;
    return aa + alen <= bb || bb + blen <= aa;
}

int lba_bool_apply(const lba_bool_descriptor *d, uint8_t *storage, size_t length,
                   uint32_t operation, int value, int *result) {
    uint8_t byte;
    int boolean;
    /* Establish all bounds/permissions/aliases before touching any output. */
    if (!lba_bool_descriptor_valid(d) || length != d->storage_bytes ||
        (operation != LBA_BOOL_READ && operation != LBA_BOOL_WRITE) ||
        !(d->permissions & operation) ||
        (operation == LBA_BOOL_READ ? value != 0 : (value != 0 && value != 1)) ||
        !disjoint(d, sizeof(*d), storage, length) ||
        !disjoint(d, sizeof(*d), result, sizeof(*result)) ||
        !disjoint(storage, length, result, sizeof(*result))) return 0;
    byte = storage[d->byte_offset];
    boolean = (byte & d->field_mask) != 0u;
    if (operation == LBA_BOOL_WRITE) {
        /* Pinned UE4SS LuaUObject.cpp push_boolproperty, Operation::Set. */
        byte = (uint8_t)((byte & (uint8_t)~d->field_mask) |
                         (value ? d->byte_mask : 0u));
        storage[d->byte_offset] = byte;
        boolean = value;
    }
    *result = boolean;
    return 1;
}

static int fail(lua_State *L, const char *message) {
    lua_pushstring(L, message);
    return lua_error(L);
}

static binding_context *context(lua_State *L) {
    /* This upvalue is installed only by trusted lba_install. */
    return lua_touserdata(L, lua_upvalueindex(1));
}

static int owned_metatable(lua_State *L, int index, int which) {
    int equal;
    index = lua_absindex(L, index);
    if (lua_type(L, index) != LUA_TUSERDATA || !lua_getmetatable(L, index)) return 0;
    lua_getiuservalue(L, lua_upvalueindex(1), which);
    equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return equal;
}

static int owned_context(lua_State *L, int index) {
    int equal;
    lua_getiuservalue(L, index, 1);
    equal = lua_rawequal(L, -1, lua_upvalueindex(1));
    lua_pop(L, 1);
    return equal;
}

static lba_receiver null_receiver(const binding_context *c) {
    lba_receiver r = {c->provider.owner_id, 0, 0};
    return r;
}

static int null_identity(lba_receiver r) {
    return r.object_id == 0u && r.generation == 0u;
}

static int identity_valid(const binding_context *c, lba_receiver r) {
    return r.owner_id == c->provider.owner_id &&
           (null_identity(r) || (r.object_id != 0u && r.generation != 0u));
}

static int owned_receiver(lua_State *L, int index, lba_receiver *out) {
    binding_context *c = context(L);
    index = lua_absindex(L, index);
    if (!owned_metatable(L, index, 1) ||
        lua_rawlen(L, index) != sizeof(lba_receiver) || !owned_context(L, index))
        return 0;
    memcpy(out, lua_touserdata(L, index), sizeof(*out));
    return identity_valid(c, *out);
}

static int equal_receiver(lba_receiver a, lba_receiver b) {
    return a.owner_id == b.owner_id && a.object_id == b.object_id &&
           a.generation == b.generation;
}

static int copy_name(lua_State *L, int index, uint8_t *owned, size_t *length) {
    const char *bytes;
    if (lua_type(L, index) != LUA_TSTRING) return 0;
    bytes = lua_tolstring(L, index, length);
    if (!valid_name((const uint8_t *)bytes, *length)) return 0;
    memcpy(owned, bytes, *length);
    return 1;
}

static void push_receiver(lua_State *L, lba_receiver r) {
    lba_receiver *owned = lua_newuserdatauv(L, sizeof(*owned), 1);
    *owned = r;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setiuservalue(L, -2, 1);
    lua_getiuservalue(L, lua_upvalueindex(1), 1);
    lua_setmetatable(L, -2);
}

static int receiver_is_valid(lua_State *L) {
    binding_context *c = context(L);
    lba_receiver r;
    int status;
    if (lua_gettop(L) != 1 || !owned_receiver(L, 1, &r))
        return fail(L, "IsValid requires one owned receiver");
    if (null_identity(r)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    status = c->provider.validate_receiver(c->provider.owned_context, r);
    if (status != 0 && status != 1)
        return fail(L, "owned receiver validation failed");
    lua_pushboolean(L, status);
    return 1;
}

static int receiver_equal(lua_State *L) {
    lba_receiver a, b;
    int equal = lua_gettop(L) == 2 && owned_receiver(L, 1, &a) &&
                owned_receiver(L, 2, &b) && equal_receiver(a, b);
    /* Same numeric IDs in another instance are foreign; no provider callback
     * runs for equality, including null or stale values. */
    lua_pushboolean(L, equal);
    return 1;
}

static int lookup_receiver(lua_State *L) {
    binding_context *c = context(L);
    uint8_t name[LBA_NAME_CAP + 1u] = {0};
    size_t length = 0;
    lba_receiver r = {0, 0, 0};
    int resolved;
    if (lua_gettop(L) != 1 || !copy_name(L, 1, name, &length)) {
        clear_bytes(name, sizeof(name));
        return fail(L, "lookup requires one bounded UTF-8 string");
    }
    resolved = c->provider.resolve_receiver(c->provider.owned_context, name, length, &r);
    clear_bytes(name, sizeof(name));
    if (resolved == 0) { lua_pushnil(L); return 1; }
    if (resolved != 1 || r.owner_id != c->provider.owner_id ||
        r.object_id == 0u || r.generation == 0u ||
        c->provider.validate_receiver(c->provider.owned_context, r) != 1)
        return fail(L, "invalid owned receiver");
    push_receiver(L, r);
    return 1;
}

static int access_bool(lua_State *L, binding_context *c, lba_receiver r,
                       lba_bool_descriptor d, uint32_t operation, int value) {
    int result = 0, status, exact;
    if (!lba_bool_descriptor_valid(&d))
        return fail(L, "unsupported Boolean descriptor");
    if (!(d.permissions & operation)) return fail(L, "Boolean property permission denied");
    if (c->provider.validate_receiver(c->provider.owned_context, r) != 1)
        return fail(L, "stale receiver");
    /* One trusted transaction validates the CURRENT receiver/property and
     * applies mask+value to its own storage. No read-byte/write-byte roundtrip. */
    status = c->boolean.access(c->boolean.owned_context, r, d, operation, value, &result);
    exact = result == 0 || result == 1;
    value = result;
    clear_bytes(&result, sizeof(result));
    if (status != 0) return fail(L, "owned Boolean operation failed");
    if (!exact) return fail(L, "invalid provider Boolean result");
    if (operation == LBA_BOOL_WRITE) return 0;
    lua_pushboolean(L, value);
    return 1;
}

static int access_reference(lua_State *L, binding_context *c, lba_receiver r,
                            lba_ref_descriptor d, uint32_t operation,
                            lba_receiver value) {
    lba_receiver result = null_receiver(c), target;
    int status;
    if (!lba_ref_descriptor_valid(&d))
        return fail(L, "unsupported object-reference descriptor");
    if ((operation != LBA_REF_READ && operation != LBA_REF_WRITE) ||
        !(d.permissions & operation))
        return fail(L, "object-reference property permission denied");
    if (!identity_valid(c, r) || null_identity(r) ||
        c->provider.validate_receiver(c->provider.owned_context, r) != 1)
        return fail(L, "stale receiver");
    if (!identity_valid(c, value) ||
        (operation == LBA_REF_READ && !null_identity(value)))
        return fail(L, "invalid object-reference input identity");
    if (operation == LBA_REF_WRITE && !null_identity(value) &&
        c->provider.validate_receiver(c->provider.owned_context, value) != 1)
        return fail(L, "stale object-reference target");
    /* One transaction owns all current source/property/target checks and any
     * write. The adapter receives copied identities, never provider pointers. */
    status = c->reference.access(c->reference.owned_context, r, d, operation,
                                  value, &result);
    target = result;
    clear_bytes(&result, sizeof(result));
    if (status != 0) return fail(L, "owned object-reference operation failed");
    if (operation == LBA_REF_WRITE) return 0; /* Result is intentionally ignored. */
    if (!identity_valid(c, target))
        return fail(L, "invalid provider object-reference result");
    if (!null_identity(target) &&
        c->provider.validate_receiver(c->provider.owned_context, target) != 1)
        return fail(L, "stale provider object-reference result");
    push_receiver(L, target);
    return 1;
}

static int set_bound_member(lua_State *L) {
    binding_context *c = context(L);
    lba_receiver r, value_receiver;
    lba_bool_descriptor d;
    uint8_t name[LBA_NAME_CAP + 1u] = {0};
    size_t length = 0;
    int resolved, value;
    if (lua_gettop(L) != 3 || !owned_receiver(L, 1, &r))
        return fail(L, "foreign or invalid receiver");
    if (!copy_name(L, 2, name, &length)) {
        clear_bytes(name, sizeof(name));
        return fail(L, "member requires one bounded UTF-8 string");
    }
    if (length == sizeof("IsValid") - 1u &&
        memcmp(name, "IsValid", sizeof("IsValid") - 1u) == 0) {
        clear_bytes(name, sizeof(name));
        return fail(L, "IsValid is reserved and read-only");
    }
    if (null_identity(r)) {
        clear_bytes(name, sizeof(name));
        return fail(L, "cannot write a property on a null receiver");
    }
    /* Retain the Boolean-only wrapper's type refusal before provider calls. */
    if (!c->has_reference && c->has_boolean && lua_type(L, 3) != LUA_TBOOLEAN) {
        clear_bytes(name, sizeof(name));
        return fail(L, "property assignment requires Boolean type");
    }
    if (c->provider.validate_receiver(c->provider.owned_context, r) != 1) {
        clear_bytes(name, sizeof(name));
        return fail(L, "stale receiver");
    }
    if (c->has_reference) {
        lba_ref_descriptor property;
        memset(&property, 0, sizeof(property));
        resolved = c->reference.resolve(c->reference.owned_context, r, name,
                                         length, &property);
        if (resolved != 0) {
            clear_bytes(name, sizeof(name));
            if (resolved != 1)
                return fail(L, "object-reference property resolution failed");
            if (lua_type(L, 3) == LUA_TNIL) value_receiver = null_receiver(c);
            else if (!owned_receiver(L, 3, &value_receiver))
                return fail(L, "reference assignment requires owned receiver or nil");
            return access_reference(L, c, r, property, LBA_REF_WRITE,
                                     value_receiver);
        }
    }
    if (!c->has_boolean) {
        clear_bytes(name, sizeof(name));
        return fail(L, "property unavailable");
    }
    if (lua_type(L, 3) != LUA_TBOOLEAN) {
        clear_bytes(name, sizeof(name));
        return fail(L, "property assignment requires Boolean type");
    }
    value = lua_toboolean(L, 3);
    memset(&d, 0, sizeof(d));
    resolved = c->boolean.resolve(c->boolean.owned_context, r, name, length, &d);
    clear_bytes(name, sizeof(name));
    if (resolved != 1) return fail(L, "Boolean property unavailable");
    return access_bool(L, c, r, d, LBA_BOOL_WRITE, value);
}

static int resolve_bound_member(lua_State *L) {
    binding_context *c = context(L);
    uint8_t name[LBA_NAME_CAP + 1u] = {0};
    size_t length = 0;
    lba_receiver r;
    lba_descriptor d;
    bound_method *owned;
    int resolved;
    if (lua_gettop(L) != 2 || !owned_receiver(L, 1, &r))
        return fail(L, "foreign or invalid receiver");
    if (!copy_name(L, 2, name, &length)) {
        clear_bytes(name, sizeof(name));
        return fail(L, "member requires one bounded UTF-8 string");
    }
    if (length == sizeof("IsValid") - 1u &&
        memcmp(name, "IsValid", sizeof("IsValid") - 1u) == 0) {
        clear_bytes(name, sizeof(name));
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, receiver_is_valid, 1);
        return 1;
    }
    if (null_identity(r)) {
        clear_bytes(name, sizeof(name));
        push_receiver(L, null_receiver(c));
        return 1;
    }
    if (c->provider.validate_receiver(c->provider.owned_context, r) != 1) {
        clear_bytes(name, sizeof(name));
        return fail(L, "stale receiver");
    }
    if (c->has_reference) {
        lba_ref_descriptor property;
        memset(&property, 0, sizeof(property));
        resolved = c->reference.resolve(c->reference.owned_context, r, name,
                                         length, &property);
        if (resolved != 0) {
            clear_bytes(name, sizeof(name));
            if (resolved != 1)
                return fail(L, "object-reference property resolution failed");
            return access_reference(L, c, r, property, LBA_REF_READ,
                                     null_receiver(c));
        }
    }
    if (c->has_boolean) {
        lba_bool_descriptor property;
        memset(&property, 0, sizeof(property));
        resolved = c->boolean.resolve(c->boolean.owned_context, r, name, length, &property);
        if (resolved != 0) {
            clear_bytes(name, sizeof(name));
            if (resolved != 1) return fail(L, "Boolean property resolution failed");
            return access_bool(L, c, r, property, LBA_BOOL_READ, 0);
        }
    }
    memset(&d, 0, sizeof(d));
    resolved = c->provider.resolve_member(c->provider.owned_context, r, name, length, &d);
    clear_bytes(name, sizeof(name));
    if (resolved != 1) return fail(L, "member unavailable");
    if (!lba_descriptor_valid(&d)) return fail(L, "unsupported trusted descriptor");
    owned = lua_newuserdatauv(L, sizeof(*owned), 2);
    owned->receiver = r;
    owned->descriptor = d;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setiuservalue(L, -2, 1);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 2);
    lua_getiuservalue(L, lua_upvalueindex(1), 2);
    lua_setmetatable(L, -2);
    return 1;
}

static void write_int32_le(uint8_t *out, int32_t value) {
    uint32_t bits = (uint32_t)value;
    unsigned i;
    for (i = 0; i < 4; ++i) out[i] = (uint8_t)(bits >> (8u*i));
}

static int32_t read_int32_le(const uint8_t *in) {
    uint32_t bits = (uint32_t)in[0] | ((uint32_t)in[1] << 8u) |
                    ((uint32_t)in[2] << 16u) | ((uint32_t)in[3] << 24u);
    /* Avoid implementation-defined unsigned-to-signed narrowing. */
    int64_t value = bits <= INT32_MAX ? (int64_t)bits :
                    (int64_t)bits - INT64_C(4294967296);
    return (int32_t)value;
}

static int call_bound_member(lua_State *L) {
    binding_context *c = context(L);
    bound_method owned;
    lba_receiver captured;
    lua_Integer input;
    int top = lua_gettop(L), argument, captured_ok, receiver_equal, invoke_status;
    int32_t result;
    uint8_t frame[LBA_FRAME_CAP];
    if (!owned_metatable(L, 1, 2) || lua_rawlen(L, 1) != sizeof(owned) ||
        !owned_context(L, 1)) return fail(L, "foreign or invalid bound method");
    if (top != 2 && top != 3) return fail(L, "bound method requires one Int32 argument");
    memcpy(&owned, lua_touserdata(L, 1), sizeof(owned));
    lua_getiuservalue(L, 1, 2);
    captured_ok = owned_receiver(L, -1, &captured);
    receiver_equal = top != 3 || lua_rawequal(L, 2, -1);
    lua_pop(L, 1);
    if (!captured_ok || !receiver_equal || !equal_receiver(captured, owned.receiver))
        return fail(L, "receiver differs from captured owned receiver");
    argument = top == 3 ? 3 : 2;
    if (!lua_isinteger(L, argument)) return fail(L, "argument requires integer type");
    input = lua_tointeger(L, argument);
    if (input < INT32_MIN || input > INT32_MAX) return fail(L, "argument outside Int32 range");
    if (!lba_descriptor_valid(&owned.descriptor))
        return fail(L, "unsupported trusted descriptor");
    if (c->provider.validate_receiver(c->provider.owned_context, captured) != 1)
        return fail(L, "stale receiver");
    if (c->provider.authorize(c->provider.owned_context, captured, owned.descriptor) != 1)
        return fail(L, "invocation denied or stale member");
    memset(frame, 0, sizeof(frame));
    write_int32_le(frame + owned.descriptor.input_offset, (int32_t)input);
    /* No Lua API, Lua pointer or long-lived span occurs in the callback. */
    invoke_status = c->provider.invoke(c->provider.owned_context, captured,
        owned.descriptor, frame, owned.descriptor.frame_bytes);
    result = invoke_status == 0 ? read_int32_le(frame + owned.descriptor.return_offset) : 0;
    clear_bytes(frame, sizeof(frame));
    if (invoke_status != 0) return fail(L, "owned invocation failed");
    lua_pushinteger(L, (lua_Integer)result);
    return 1;
}

static void make_metatable(lua_State *L, int context_index, int uservalue,
                          lua_CFunction function, const char *operation,
                          const char *label) {
    lua_newtable(L);
    lua_pushvalue(L, context_index);
    lua_pushcclosure(L, function, 1);
    lua_setfield(L, -2, operation);
    lua_pushstring(L, label);
    lua_setfield(L, -2, "__metatable");
    lua_setiuservalue(L, context_index, uservalue);
}

int lba_install_with_refs(lua_State *L, const lba_provider *provider,
                          const lba_bool_provider *boolean,
                          const lba_ref_provider *reference,
                          const char *name, size_t length) {
    int context_index;
    binding_context *owned;
    if (!L || !provider || !provider->owner_id ||
        !provider->resolve_receiver || !provider->validate_receiver ||
        !provider->resolve_member || !provider->authorize || !provider->invoke ||
        (boolean && (!boolean->resolve || !boolean->access)) ||
        (reference && (!reference->resolve || !reference->access)) ||
        !valid_name((const uint8_t *)name, length)) return 0;
    luaL_checkversion(L);
    owned = lua_newuserdatauv(L, sizeof(*owned), 2);
    memset(owned, 0, sizeof(*owned));
    owned->provider = *provider;
    if (boolean) { owned->boolean = *boolean; owned->has_boolean = 1; }
    if (reference) { owned->reference = *reference; owned->has_reference = 1; }
    context_index = lua_absindex(L, -1);
    make_metatable(L, context_index, 1, resolve_bound_member, "__index", "owned receiver");
    lua_getiuservalue(L, context_index, 1);
    lua_pushvalue(L, context_index);
    lua_pushcclosure(L, set_bound_member, 1);
    lua_setfield(L, -2, "__newindex");
    lua_pushvalue(L, context_index);
    lua_pushcclosure(L, receiver_equal, 1);
    lua_setfield(L, -2, "__eq");
    lua_pop(L, 1);
    make_metatable(L, context_index, 2, call_bound_member, "__call", "owned Int32 method");
    lua_pushglobaltable(L);
    lua_pushlstring(L, name, length);
    lua_pushvalue(L, context_index);
    lua_pushcclosure(L, lookup_receiver, 1);
    lua_rawset(L, -3);
    lua_pop(L, 2); /* Globals table and owned context; closure anchors context. */
    return 1;
}

int lba_install_with_bool(lua_State *L, const lba_provider *provider,
                          const lba_bool_provider *boolean,
                          const char *name, size_t length) {
    return lba_install_with_refs(L, provider, boolean, NULL, name, length);
}

int lba_install(lua_State *L, const lba_provider *provider,
                const char *name, size_t length) {
    return lba_install_with_bool(L, provider, NULL, name, length);
}
