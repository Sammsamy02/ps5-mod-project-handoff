/* Retained Boolean/Int32 regression, now independent of ignored VM sources. */
#include "vm_gate.h"
#include "../owned_binding.h"
#include "lauxlib.h"

static unsigned checks, failures, cases;
static void check(int condition, const char *label) {
    ++checks;
    if (!condition) {
        if (failures < 12u) printf("FAIL %s\n", label);
        ++failures;
    }
}

static lba_bool_descriptor good_bool(void) {
    lba_bool_descriptor d;
    memset(&d, 0, sizeof(d));
    d.version = LBA_BOOL_DESCRIPTOR_VERSION;
    d.storage_bytes = 8u; d.byte_offset = 3u;
    d.permissions = LBA_BOOL_READ | LBA_BOOL_WRITE;
    d.property_id = 301u; d.generation = 4u;
    d.value_type = LBA_TYPE_BOOL; d.encoding = LBA_ENCODING_BOOL_MASK;
    d.field_mask = d.byte_mask = 4u;
    return d;
}

static void pure_masks(void) {
    unsigned mask, byte, value;
    lba_bool_descriptor d = good_bool();
    ++cases;
    for (mask = 0u; mask < 9u; ++mask) {
        d.field_mask = mask == 8u ? 255u : (uint8_t)(1u << mask);
        d.byte_mask = mask == 8u ? 1u : d.field_mask;
        check(lba_bool_descriptor_valid(&d), "supported native/packed descriptor");
        for (byte = 0; byte < 256u; ++byte) for (value = 0; value < 2u; ++value) {
            uint8_t storage[8], expected[8];
            int result = -7;
            memset(storage, 0xa5, sizeof(storage)); storage[3] = (uint8_t)byte;
            memcpy(expected, storage, sizeof(storage));
            check(lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_READ, 0,
                                 &result) == 1 && result == ((byte & d.field_mask) != 0u) &&
                  memcmp(storage, expected, sizeof(storage)) == 0,
                  "all-byte Boolean read without mutation");
            expected[3] = (uint8_t)((byte & (unsigned)(uint8_t)~d.field_mask) |
                                   (value ? d.byte_mask : 0u));
            result = -7;
            check(lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_WRITE,
                                 (int)value, &result) == 1 && result == (int)value &&
                  memcmp(storage, expected, sizeof(storage)) == 0,
                  "all-byte false/true mask writes preserve surrounding bytes/bits");
        }
    }
}

static void invalid_descriptors(void) {
    lba_bool_descriptor d = good_bool(), bad;
    uint8_t storage[8], before[8];
    unsigned i;
    ++cases;
    for (i = 0; i < 14u; ++i) {
        int result = 77;
        bad = d;
        switch (i) {
        case 0: bad.version = 2u; break;
        case 1: bad.storage_bytes = 0u; break;
        case 2: bad.storage_bytes = 65u; break;
        case 3: bad.byte_offset = bad.storage_bytes; break;
        case 4: bad.permissions = 0u; break;
        case 5: bad.permissions = 4u; break;
        case 6: bad.property_id = 0u; break;
        case 7: bad.generation = 0u; break;
        case 8: bad.value_type = LBA_TYPE_INT32; break;
        case 9: bad.encoding = LBA_ENCODING_LE_TWOS_COMPLEMENT; break;
        case 10: bad.reserved = 1u; break;
        case 11: bad.field_mask = 0u; break;
        case 12: bad.field_mask = bad.byte_mask = 3u; break;
        default: bad.byte_mask = 8u; break;
        }
        memset(storage, 0x7b, sizeof(storage)); memcpy(before, storage, sizeof(storage));
        check(!lba_bool_descriptor_valid(&bad) &&
              !lba_bool_apply(&bad, storage, sizeof(storage), LBA_BOOL_WRITE, 1, &result) &&
              result == 77 && !memcmp(before, storage, sizeof(storage)),
              "invalid descriptor has no scalar/storage publication");
    }
    d.field_mask = 255u; d.byte_mask = 2u;
    check(!lba_bool_descriptor_valid(&d), "unproved native mask profile is unsupported");
}

static void pure_preflight(void) {
    lba_bool_descriptor d = good_bool();
    uint8_t storage[8], before[8];
    int result = 99, overlapping = 12345, original;
    ++cases;
    memset(storage, 0x6c, sizeof(storage)); memcpy(before, storage, sizeof(storage));
    check(!lba_bool_apply(&d, storage, 7u, LBA_BOOL_WRITE, 1, &result) && result == 99 &&
          !memcmp(storage, before, sizeof(storage)), "exact bounded span required");
    check(!lba_bool_apply(&d, storage, sizeof(storage), 3u, 1, &result) && result == 99 &&
          !memcmp(storage, before, sizeof(storage)), "invalid operation refused");
    check(!lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_WRITE, 2, &result) &&
          result == 99 && !memcmp(storage, before, sizeof(storage)), "write value is exact Boolean");
    check(!lba_bool_apply(NULL, storage, sizeof(storage), LBA_BOOL_WRITE, 1, &result),
          "null descriptor refused");
    check(!lba_bool_apply(&d, NULL, sizeof(storage), LBA_BOOL_WRITE, 1, &result),
          "null storage refused");
    check(!lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_WRITE, 1, NULL),
          "null scalar refused");
    d.storage_bytes = sizeof(overlapping); d.byte_offset = 0;
    original = overlapping;
    check(!lba_bool_apply(&d, (uint8_t *)&overlapping, sizeof(overlapping),
                          LBA_BOOL_WRITE, 1, &overlapping) && overlapping == original,
          "result/storage alias untouched");
    {
        lba_bool_descriptor saved = d;
        check(!lba_bool_apply(&d, (uint8_t *)&d, d.storage_bytes,
                              LBA_BOOL_WRITE, 1, &result) &&
              !memcmp(&d, &saved, sizeof(d)), "descriptor/storage alias untouched");
        check(!lba_bool_apply(&d, storage, d.storage_bytes,
                              LBA_BOOL_WRITE, 1, (int *)&d) &&
              !memcmp(&d, &saved, sizeof(d)), "descriptor/result alias untouched");
    }
    d = good_bool(); d.permissions = LBA_BOOL_READ;
    check(!lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_WRITE, 1, &result) &&
          result == 99 && !memcmp(storage, before, sizeof(storage)), "read-only denies write");
    d.permissions = LBA_BOOL_WRITE;
    check(!lba_bool_apply(&d, storage, sizeof(storage), LBA_BOOL_READ, 0, &result) &&
          result == 99, "write-only denies read");
}

typedef struct {
    lba_receiver receiver;
    lba_bool_descriptor descriptor;
    uint8_t storage[LBA_FRAME_CAP];
    unsigned accesses, writes, methods;
    int denied, failed, stale_member, invalid_output, flip_neighbor;
} owned_provider;
typedef struct { lua_State *L; vm_context_t vm; owned_provider a, b; } suite;

static int same_receiver(lba_receiver a, lba_receiver b) {
    return a.owner_id == b.owner_id && a.object_id == b.object_id && a.generation == b.generation;
}
static int receiver_lookup(void *user, const uint8_t *name, size_t n, lba_receiver *out) {
    owned_provider *p = user;
    if (n != 14u || memcmp(name, "/Fixture/Actor", n)) return 0;
    *out = p->receiver; return 1;
}
static int receiver_valid(void *user, lba_receiver r) {
    return same_receiver(r, ((owned_provider *)user)->receiver);
}
static lba_descriptor int_descriptor(void) {
    lba_descriptor d;
    memset(&d, 0, sizeof(d));
    d.version = LBA_DESCRIPTOR_VERSION; d.frame_bytes = 8u; d.return_offset = 4u;
    d.input_count = d.return_count = 1u; d.input_type = d.return_type = LBA_TYPE_INT32;
    d.encoding = LBA_ENCODING_LE_TWOS_COMPLEMENT; d.member_id = 900u; d.generation = 5u;
    return d;
}
static int method_resolve(void *user, lba_receiver r, const uint8_t *name,
                          size_t n, lba_descriptor *out) {
    if (!receiver_valid(user, r) || n != 4u || memcmp(name, "Echo", n)) return 0;
    *out = int_descriptor(); return 1;
}
static int method_authorize(void *user, lba_receiver r, lba_descriptor d) {
    return receiver_valid(user, r) && d.member_id == 900u && d.generation == 5u;
}
static int method_invoke(void *user, lba_receiver r, lba_descriptor d,
                         uint8_t *frame, size_t n) {
    owned_provider *p = user;
    if (!method_authorize(user, r, d) || n != 8u) return -1;
    ++p->methods; memcpy(frame + 4u, frame, 4u); return 0;
}
static int property_resolve(void *user, lba_receiver r, const uint8_t *name,
                            size_t n, lba_bool_descriptor *out) {
    owned_provider *p = user;
    if (!receiver_valid(user, r) || n != 4u || memcmp(name, "Flag", n)) return 0;
    *out = p->descriptor;
    if (p->stale_member) ++p->descriptor.generation; /* after descriptor copy */
    return 1;
}
static int property_access(void *user, lba_receiver r, lba_bool_descriptor d,
                           uint32_t operation, int value, int *result) {
    owned_provider *p = user;
    ++p->accesses;
    /* One synchronous provider transaction owns validation and CURRENT byte. */
    if (!receiver_valid(user, r) || memcmp(&d, &p->descriptor, sizeof(d)) ||
        p->denied || p->failed) return -1;
    if (p->invalid_output) { *result = 2; return 0; }
    if (operation == LBA_BOOL_WRITE && p->flip_neighbor) p->storage[d.byte_offset] ^= 0x40u;
    if (!lba_bool_apply(&d, p->storage, d.storage_bytes, operation, value, result)) return -1;
    if (operation == LBA_BOOL_WRITE) ++p->writes;
    return 0;
}
static void init_provider(owned_provider *p) {
    memset(p, 0, sizeof(*p));
    p->receiver.owner_id = 31u; p->receiver.object_id = 101u; p->receiver.generation = 3u;
    p->descriptor = good_bool(); memset(p->storage, 0x55, sizeof(p->storage));
    p->storage[3] = 0xb5u;
}
static lba_provider methods_for(owned_provider *p) {
    lba_provider result = {p->receiver.owner_id, p, receiver_lookup, receiver_valid,
                           method_resolve, method_authorize, method_invoke};
    return result;
}
static lba_bool_provider properties_for(owned_provider *p) {
    lba_bool_provider result = {p, property_resolve, property_access}; return result;
}
static int install(lua_State *L) {
    suite *s = lua_touserdata(L, lua_upvalueindex(1));
    lba_provider a = methods_for(&s->a), b = methods_for(&s->b);
    lba_bool_provider pa = properties_for(&s->a), pb = properties_for(&s->b);
    if (!lba_install_with_bool(L, &a, &pa, "StaticFindObject", 16u) ||
        !lba_install_with_bool(L, &b, &pb, "ForeignFindObject", 17u)) return luaL_error(L, "install failed");
    return 0;
}
static int start(suite *s) {
    int status;
    memset(s, 0, sizeof(*s)); init_provider(&s->a); init_provider(&s->b);
    s->L = new_limited_state(&s->vm); check(s->L != NULL, "bounded real Lua state");
    if (!s->L) return 0;
    lua_pushlightuserdata(s->L, s); lua_pushcclosure(s->L, install, 1);
    status = lua_pcall(s->L, 0, 0, 0); check(status == LUA_OK && !lua_gettop(s->L), "protected owning installation");
    if (status != LUA_OK) lua_settop(s->L, 0);
    return status == LUA_OK;
}
static void finish(suite *s) {
    if (!s->L) return;
    close_state(s->L, &s->vm); s->L = NULL;
    check(s->vm.live_bytes == 0u && s->vm.peak_bytes <= LUA_MEMORY_CAP, "Lua cleanup and allocator cap");
}
static int script(suite *s, const char *text) {
    if (!reset_budget(s->L, &s->vm, 500000u, UINT64_C(2000000000))) return LUA_ERRRUN;
    return run_text(s->L, text);
}
static void expect_bool(suite *s, const char *text, int expected, const char *label) {
    int result = -1;
    check(script(s, text) == LUA_OK && lua_gettop(s->L) == 0, label);
    check(global_boolean(s->L, "answer", &result) && result == expected, "exact Boolean publication");
    lua_settop(s->L, 0);
}
static void expect_error(suite *s, const char *text, const char *label) {
    unsigned writes = s->a.writes;
    uint8_t before[LBA_FRAME_CAP]; memcpy(before, s->a.storage, sizeof(before));
    check(script(s, text) == LUA_ERRRUN && lua_gettop(s->L) == 1, label);
    check(writes == s->a.writes && !memcmp(before, s->a.storage, sizeof(before)), "failure does not mutate provider storage");
    lua_settop(s->L, 0);
}

static void lua_positive(void) {
    suite s;
    ++cases; if (!start(&s)) { finish(&s); return; }
    expect_bool(&s, "o=StaticFindObject('/Fixture/Actor'); answer=o.Flag", 1, "property __index");
    expect_bool(&s, "o.Flag=false; answer=o.Flag", 0, "property __newindex false");
    check(s.a.storage[3] == 0xb1u && s.a.writes == 1u, "packed false preserves adjacent flags");
    expect_bool(&s, "o.Flag=true; answer=o.Flag", 1, "property __newindex true");
    check(s.a.storage[3] == 0xb5u && s.a.writes == 2u, "packed true preserves adjacent flags");
    s.a.flip_neighbor = 1;
    expect_bool(&s, "o.Flag=false; answer=o.Flag", 0, "single transaction uses current neighbor byte");
    check(s.a.storage[3] == 0xf1u, "provider-side concurrent neighbor is preserved");
    s.a.flip_neighbor = 0; s.a.descriptor.field_mask = 255u; s.a.descriptor.byte_mask = 1u;
    expect_bool(&s, "o.Flag=true; answer=o.Flag", 1, "native Boolean true");
    check(s.a.storage[3] == 1u, "native true uses declared byte mask");
    expect_bool(&s, "o.Flag=false; answer=o.Flag", 0, "native Boolean false");
    check(s.a.storage[3] == 0u, "native false clears field");
    check(script(&s, "answer=o:Echo(-123)") == LUA_OK, "old Int32 method coexists");
    { lua_Integer result = 0; check(global_integer(s.L, "answer", &result) && result == -123 && s.a.methods == 1u, "old method identity/result retained"); }
    finish(&s);
}
static void lua_types_permissions(void) {
    suite s; const char *values[] = {"1", "0.0", "'false'", "nil", "{}", "function() end", "o"};
    size_t i; char text[160];
    ++cases; if (!start(&s)) { finish(&s); return; }
    check(script(&s, "o=StaticFindObject('/Fixture/Actor')") == LUA_OK, "capture owned receiver");
    for (i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        unsigned calls = s.a.accesses;
        snprintf(text, sizeof(text), "o.Flag=%s", values[i]);
        expect_error(&s, text, "non-Boolean assignment refused");
        check(calls == s.a.accesses, "type failure before provider access");
    }
    s.a.descriptor.permissions = LBA_BOOL_READ;
    expect_error(&s, "o.Flag=false", "read-only property write refused");
    expect_bool(&s, "answer=o.Flag", 1, "read-only property readable");
    s.a.descriptor.permissions = LBA_BOOL_WRITE;
    expect_error(&s, "answer=o.Flag", "write-only property read refused");
    check(script(&s, "o.Flag=false") == LUA_OK && s.a.writes == 1u, "write-only setter works");
    expect_error(&s, "o.Unknown=true", "unknown setter does not add local userdata field");
    expect_error(&s, "o.Echo=true", "method cannot be replaced with Boolean property");
    finish(&s);
}
static void lua_stale_failure(void) {
    suite s;
    ++cases; if (!start(&s)) { finish(&s); return; }
    check(script(&s, "o=StaticFindObject('/Fixture/Actor')") == LUA_OK, "capture generation");
    ++s.a.receiver.generation;
    expect_error(&s, "o.Flag=false", "stale receiver refused");
    check(s.a.accesses == 0u, "stale receiver never accesses storage");
    --s.a.receiver.generation; s.a.stale_member = 1;
    expect_error(&s, "o.Flag=false", "member changes between resolve and transaction");
    check(s.a.writes == 0u && s.a.accesses == 1u, "transaction rejects changed member");
    s.a.stale_member = 0; s.a.denied = 1;
    expect_error(&s, "o.Flag=false", "provider authorization denied");
    s.a.denied = 0; s.a.failed = 1;
    expect_error(&s, "answer=o.Flag", "controlled provider read failure");
    s.a.failed = 0; s.a.invalid_output = 1;
    expect_error(&s, "answer=o.Flag", "invalid provider Boolean output refused");
    s.a.invalid_output = 0; s.a.descriptor.version = 99u;
    { unsigned calls = s.a.accesses;
      expect_error(&s, "o.Flag=false", "invalid descriptor refused before provider access");
      check(calls == s.a.accesses, "invalid descriptor has no access"); }
    s.a.descriptor = good_bool();
    expect_bool(&s, "o.Flag=false; answer=o.Flag", 0, "state recovers after controlled failures");
    finish(&s);
}
static void lua_ownership_names(void) {
    suite s; int status;
    ++cases; if (!start(&s)) { finish(&s); return; }
    check(script(&s, "a=StaticFindObject('/Fixture/Actor'); b=ForeignFindObject('/Fixture/Actor')") == LUA_OK, "same IDs in separate owning instances");
    lua_getglobal(s.L, "a"); lua_getmetatable(s.L, -1); lua_getfield(s.L, -1, "__newindex");
    lua_getglobal(s.L, "b"); lua_pushliteral(s.L, "Flag"); lua_pushboolean(s.L, 0);
    status = lua_pcall(s.L, 3, 0, 0);
    check(status == LUA_ERRRUN && !s.a.accesses && !s.b.accesses, "foreign userdata rejected despite identical numeric IDs");
    lua_settop(s.L, 0);
    expect_error(&s, "a[1]=true", "non-string property name refused");
    expect_error(&s, "a['F\\0lag']=true", "embedded NUL name refused");
    expect_error(&s, "a['\\255']=true", "invalid UTF-8 property name refused");
    expect_error(&s, "a.__metatable=true", "locked userdata cannot receive arbitrary fields");
    expect_bool(&s, "a.Flag=false; answer=b.Flag", 1, "independent instance storage remains unchanged");
    finish(&s);
}

static void installation_preflight(void) {
    suite s;
    lba_provider p;
    lba_bool_provider b;
    int top;
    ++cases; if (!start(&s)) { finish(&s); return; }
    p = methods_for(&s.a); b = properties_for(&s.a); top = lua_gettop(s.L);
    b.access = NULL;
    check(!lba_install_with_bool(s.L, &p, &b, "Invalid", 7u) && lua_gettop(s.L) == top,
          "missing property access callback is rejected before allocation");
    b = properties_for(&s.a); b.resolve = NULL;
    check(!lba_install_with_bool(s.L, &p, &b, "Invalid", 7u) && lua_gettop(s.L) == top,
          "missing property resolve callback is rejected before allocation");
    check(lba_install(s.L, &p, "LegacyFindObject", 16u) && lua_gettop(s.L) == top,
          "original Int32-only installation remains stack preserving");
    check(script(&s, "answer=LegacyFindObject('/Fixture/Actor'):Echo(17)") == LUA_OK,
          "original API runs without optional property provider");
    { lua_Integer answer = 0;
      check(global_integer(s.L, "answer", &answer) && answer == 17,
            "legacy bound method returns original scalar"); }
    finish(&s);
}

int main(void) {
    pure_masks(); invalid_descriptors(); pure_preflight();
    lua_positive(); lua_types_permissions(); lua_stale_failure(); lua_ownership_names();
    installation_preflight();
    printf("%s cases=%u checks=%u failures=%u; no engine/mod execution\n",
           failures ? "FAIL" : "PASS", cases, checks, failures);
    return failures ? 1 : 0;
}
