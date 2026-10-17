/* Minimal behavioral RED/GREEN for owned Lua object references. */
#include "vm_gate.h"
#include "../owned_binding.h"
#include "lauxlib.h"

typedef struct {
    lba_receiver a, b, c;
    lba_receiver link;
    uint8_t bool_storage[8];
    unsigned bool_resolves, bool_accesses, bool_writes;
    unsigned ref_resolves, ref_accesses, ref_writes;
    unsigned method_calls, validity_calls;
    uint32_t link_permissions, child_permissions;
    uint64_t ref_generation;
    int fail_ref_access, change_generation_after_resolve;
    int bad_ref_descriptor, malformed_ref_output;
    int validation_override_enabled, validation_override;
} fixture;

typedef struct {
    lua_State *L;
    vm_context_t vm;
    fixture data;
} test_state;

static unsigned checks, failures;

static void check(int ok, const char *label) {
    ++checks;
    if (!ok) {
        if (failures < 16u) (void)printf("FAIL %s\n", label);
        ++failures;
    }
}

static int same(lba_receiver a, lba_receiver b) {
    return a.owner_id == b.owner_id && a.object_id == b.object_id &&
           a.generation == b.generation;
}

static int receiver_lookup(void *user, const uint8_t *name, size_t n,
                           lba_receiver *out) {
    fixture *f = (fixture *)user;
    if (n == 10u && memcmp(name, "/Fixture/A", n) == 0) *out = f->a;
    else if (n == 10u && memcmp(name, "/Fixture/B", n) == 0) *out = f->b;
    else if (n == 10u && memcmp(name, "/Fixture/C", n) == 0) *out = f->c;
    else return 0;
    return 1;
}

static int receiver_valid(void *user, lba_receiver value) {
    fixture *f = (fixture *)user;
    ++f->validity_calls;
    if (f->validation_override_enabled) return f->validation_override;
    return same(value, f->a) || same(value, f->b) || same(value, f->c);
}

static lba_descriptor method_descriptor(void) {
    lba_descriptor d;
    memset(&d, 0, sizeof(d));
    d.version = LBA_DESCRIPTOR_VERSION;
    d.frame_bytes = 8u;
    d.input_offset = 0u;
    d.return_offset = 4u;
    d.input_count = 1u;
    d.return_count = 1u;
    d.input_type = LBA_TYPE_INT32;
    d.return_type = LBA_TYPE_INT32;
    d.encoding = LBA_ENCODING_LE_TWOS_COMPLEMENT;
    d.member_id = 710u;
    d.generation = 9u;
    return d;
}

static int method_resolve(void *user, lba_receiver receiver,
                          const uint8_t *name, size_t n,
                          lba_descriptor *out) {
    if (!receiver_valid(user, receiver) || n != 4u ||
        memcmp(name, "Echo", n) != 0) return 0;
    *out = method_descriptor();
    return 1;
}

static int method_authorize(void *user, lba_receiver receiver,
                            lba_descriptor d) {
    return receiver_valid(user, receiver) && d.member_id == 710u &&
           d.generation == 9u;
}

static int method_invoke(void *user, lba_receiver receiver,
                         lba_descriptor d, uint8_t *frame, size_t n) {
    fixture *f = (fixture *)user;
    if (!method_authorize(user, receiver, d) || n != 8u) return -1;
    memcpy(frame + 4u, frame, 4u);
    ++f->method_calls;
    return 0;
}

static lba_bool_descriptor bool_descriptor(uint64_t property, uint32_t offset) {
    lba_bool_descriptor d;
    memset(&d, 0, sizeof(d));
    d.version = LBA_BOOL_DESCRIPTOR_VERSION;
    d.storage_bytes = sizeof(((fixture *)0)->bool_storage);
    d.byte_offset = offset;
    d.permissions = LBA_BOOL_READ | LBA_BOOL_WRITE;
    d.property_id = property;
    d.generation = 9u;
    d.value_type = LBA_TYPE_BOOL;
    d.encoding = LBA_ENCODING_BOOL_MASK;
    d.field_mask = 1u;
    d.byte_mask = 1u;
    return d;
}

static int bool_resolve(void *user, lba_receiver receiver,
                        const uint8_t *name, size_t n,
                        lba_bool_descriptor *out) {
    fixture *f = (fixture *)user;
    ++f->bool_resolves;
    if (!same(receiver, f->a)) return 0;
    if (n == 4u && memcmp(name, "Flag", n) == 0) {
        *out = bool_descriptor(801u, 0u);
        return 1;
    }
    /* Deliberately collide with the reserved method name. The adapter must
     * reject IsValid assignment before asking either provider to resolve it. */
    if (n == 7u && memcmp(name, "IsValid", n) == 0) {
        *out = bool_descriptor(802u, 1u);
        return 1;
    }
    return 0;
}

static int bool_access(void *user, lba_receiver receiver,
                       lba_bool_descriptor d, uint32_t operation,
                       int value, int *result) {
    fixture *f = (fixture *)user;
    ++f->bool_accesses;
    if (!same(receiver, f->a) ||
        (d.property_id != 801u && d.property_id != 802u)) return -1;
    if (!lba_bool_apply(&d, f->bool_storage, sizeof(f->bool_storage),
                        operation, value, result)) return -1;
    if (operation == LBA_BOOL_WRITE) ++f->bool_writes;
    return 0;
}

static lba_ref_descriptor ref_descriptor(uint64_t property) {
    lba_ref_descriptor d;
    memset(&d, 0, sizeof(d));
    d.version = LBA_REF_DESCRIPTOR_VERSION;
    d.permissions = LBA_REF_READ | LBA_REF_WRITE;
    d.property_id = property;
    d.generation = 9u;
    d.value_type = LBA_TYPE_OBJECT_REFERENCE;
    d.encoding = LBA_ENCODING_OWNED_IDENTITY;
    return d;
}

static int ref_resolve(void *user, lba_receiver receiver,
                       const uint8_t *name, size_t n,
                       lba_ref_descriptor *out) {
    fixture *f = (fixture *)user;
    ++f->ref_resolves;
    if (same(receiver, f->a) && n == 4u && memcmp(name, "Link", n) == 0) {
        *out = ref_descriptor(901u);
        out->permissions = f->link_permissions;
    } else if (same(receiver, f->b) && n == 5u &&
               memcmp(name, "Child", n) == 0) {
        *out = ref_descriptor(902u);
        out->permissions = f->child_permissions;
    } else if (same(receiver, f->a) && n == 8u &&
               memcmp(name, "NullLink", n) == 0) {
        *out = ref_descriptor(903u);
    } else if (same(receiver, f->a) && n == 7u &&
               memcmp(name, "IsValid", n) == 0) {
        /* Deliberate collision: reserved IsValid must precede both providers. */
        *out = ref_descriptor(904u);
    } else return 0;
    out->generation = f->ref_generation;
    if (f->bad_ref_descriptor) out->version++;
    if (f->change_generation_after_resolve && out->property_id == 901u)
        ++f->ref_generation;
    return 1;
}

static int ref_access(void *user, lba_receiver receiver,
                      lba_ref_descriptor d, uint32_t operation,
                      lba_receiver value, lba_receiver *result) {
    fixture *f = (fixture *)user;
    ++f->ref_accesses;
    if (f->fail_ref_access || d.version != LBA_REF_DESCRIPTOR_VERSION ||
        d.generation != f->ref_generation) return -1;
    if (operation == LBA_REF_READ) {
        if (!same(value, (lba_receiver){31u, 0u, 0u})) return -1;
        if (!(d.permissions & LBA_REF_READ)) return -1;
        if (same(receiver, f->a) && d.property_id == 901u) *result = f->link;
        else if (same(receiver, f->b) && d.property_id == 902u) *result = f->c;
        else if (same(receiver, f->a) && d.property_id == 903u)
            *result = (lba_receiver){31u, 0u, 0u};
        else return -1;
        if (f->malformed_ref_output == 1)
            *result = (lba_receiver){32u, 104u, 6u};
        else if (f->malformed_ref_output == 2)
            *result = (lba_receiver){31u, 0u, 6u};
        else if (f->malformed_ref_output == 3)
            *result = (lba_receiver){31u, 999u, 88u};
        return 0;
    }
    if (operation == LBA_REF_WRITE && !(d.permissions & LBA_REF_WRITE)) return -1;
    if (operation == LBA_REF_WRITE && same(receiver, f->a) &&
        d.property_id == 901u && (same(value, f->b) || same(value, f->c) ||
                                  same(value, (lba_receiver){31u, 0u, 0u}))) {
        f->link = value;
        ++f->ref_writes;
        if (result != NULL) *result = (lba_receiver){31u, 0u, 0u};
        return 0;
    }
    return -1;
}

static int install(lua_State *L) {
    test_state *state = (test_state *)lua_touserdata(L, lua_upvalueindex(1));
    fixture *f = &state->data;
    lba_provider provider = {31u, f, receiver_lookup, receiver_valid,
                             method_resolve, method_authorize, method_invoke};
    lba_bool_provider bools = {f, bool_resolve, bool_access};
    lba_ref_provider refs = {f, ref_resolve, ref_access};
    if (!lba_install_with_refs(L, &provider, &bools, &refs,
                               "StaticFindObject", 16u))
        return luaL_error(L, "fixture install failed");
    /* A second userdata instance deliberately has identical numeric IDs. */
    if (!lba_install_with_refs(L, &provider, &bools, &refs,
                               "ForeignFindObject", 17u))
        return luaL_error(L, "fixture install failed");
    return 0;
}

static int start(test_state *state) {
    int status;
    memset(state, 0, sizeof(*state));
    state->data.a = (lba_receiver){31u, 101u, 3u};
    state->data.b = (lba_receiver){31u, 102u, 4u};
    state->data.c = (lba_receiver){31u, 103u, 5u};
    state->data.link = state->data.b;
    state->data.link_permissions = LBA_REF_READ | LBA_REF_WRITE;
    state->data.child_permissions = LBA_REF_READ;
    state->data.ref_generation = 9u;
    state->data.bool_storage[0] = 1u;
    state->L = new_limited_state(&state->vm);
    check(state->L != NULL, "bounded real Lua state created");
    if (state->L == NULL) return 0;
    lua_pushlightuserdata(state->L, state);
    lua_pushcclosure(state->L, install, 1);
    status = lua_pcall(state->L, 0, 0, 0);
    check(status == LUA_OK && lua_gettop(state->L) == 0,
          "protected owning reference installation");
    if (status != LUA_OK) lua_settop(state->L, 0);
    return status == LUA_OK;
}

static int run(test_state *state, const char *source) {
    if (!reset_budget(state->L, &state->vm, 300000u, UINT64_C(1500000000)))
        return LUA_ERRRUN;
    return run_text(state->L, source);
}

static int global_bool(lua_State *L, const char *name, int expected) {
    int ok;
    lua_getglobal(L, name);
    ok = lua_isboolean(L, -1) && lua_toboolean(L, -1) == expected;
    lua_pop(L, 1);
    return ok;
}

static int read_global_integer(lua_State *L, const char *name, lua_Integer expected) {
    int is_number = 0;
    lua_Integer value;
    lua_getglobal(L, name);
    value = lua_tointegerx(L, -1, &is_number);
    lua_pop(L, 1);
    return is_number && value == expected;
}

static void run_boolean(test_state *state, const char *source,
                        int expected, const char *label) {
    int status = run(state, source);
    check(status == LUA_OK && lua_gettop(state->L) == 0, label);
    check(status == LUA_OK && global_bool(state->L, "answer", expected),
          "expected exact Lua Boolean result");
    lua_settop(state->L, 0);
}

static void expect_script_error(test_state *state, const char *source,
                                const char *label) {
    int status = run(state, source);
    check(status == LUA_ERRRUN && lua_gettop(state->L) == 1, label);
    lua_settop(state->L, 0);
}

static void expect_reference_error(test_state *state, const char *source,
                                   const char *label) {
    fixture *f = &state->data;
    lba_receiver before = f->link;
    unsigned writes = f->ref_writes;
    expect_script_error(state, source, label);
    check(same(before, f->link) && writes == f->ref_writes,
          "failed reference access leaves provider target unchanged");
}

static void references(test_state *state) {
    fixture *f = &state->data;
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "c=StaticFindObject('/Fixture/C'); answer=(a.Link.Child==c)",
        1, "ref chain: Link.Child reaches owned C identity");
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "c=StaticFindObject('/Fixture/C'); a.Link=c; answer=(a.Link==c)",
        1, "ref assignment/equality: same-instance target accepted");
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); n=a.NullLink; "
        "answer=(n:IsValid()==false and n.Child:IsValid()==false)",
        1, "null IsValid/chain: null remains invalid without dereference");

    /* Equality uses instance-local owned identity and does not call providers. */
    {
        unsigned rr = f->ref_resolves, ra = f->ref_accesses;
        run_boolean(state,
            "a=StaticFindObject('/Fixture/A'); "
            "b=StaticFindObject('/Fixture/A'); answer=(a==b)", 1,
            "same-instance identity equality");
        check(f->ref_resolves == rr && f->ref_accesses == ra,
              "identity equality performs no property callbacks");
    }
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "b=ForeignFindObject('/Fixture/A'); "
        "answer=(a==b)", 0,
        "equal numeric IDs from separate instances compare unequal");
    check(run(state, "oldB=StaticFindObject('/Fixture/B')") == LUA_OK,
          "capture original generation for equality control");
    f->b.generation++;
    check(run(state, "newB=StaticFindObject('/Fixture/B')") == LUA_OK,
          "capture another generation for the same object ID");
    f->b.generation--;
    run_boolean(state, "answer=(oldB==newB)", 0,
                "same object ID with different generation compares unequal");

    /* IsValid follows provider liveness transitions and rejects bad statuses. */
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); answer=(a:IsValid()==true)", 1,
        "IsValid reports current identity");
    expect_script_error(state, "answer=a:IsValid(1)",
                        "IsValid rejects extra arguments");
    f->a.generation++;
    run_boolean(state, "answer=(a:IsValid()==false)", 1,
                "IsValid reports stale identity false");
    f->a.generation--;
    run_boolean(state, "answer=(a:IsValid()==true)", 1,
                "IsValid recovers when generation becomes current");
    f->validation_override_enabled = 1;
    f->validation_override = 2;
    expect_script_error(state, "answer=a:IsValid()",
                        "IsValid rejects provider statuses other than exact zero or one");
    f->validation_override_enabled = 0;

    /* Null checks and chaining never ask the provider to validate or resolve. */
    check(run(state,
        "a=StaticFindObject('/Fixture/A'); n=a.NullLink") == LUA_OK,
        "capture null reference userdata");
    {
        unsigned valid = f->validity_calls, resolves = f->ref_resolves;
        unsigned accesses = f->ref_accesses;
        run_boolean(state,
            "answer=(n:IsValid()==false and n.Child:IsValid()==false)", 1,
            "null IsValid and chained null validity");
        check(f->validity_calls == valid && f->ref_resolves == resolves &&
              f->ref_accesses == accesses,
              "null validity and chain do not call provider");
    }
    {
        unsigned resolves = f->ref_resolves, bool_resolves = f->bool_resolves;
        unsigned accesses = f->ref_accesses, bool_accesses = f->bool_accesses;
        expect_script_error(state,
            "n.Child=StaticFindObject('/Fixture/C')",
            "null reference write is rejected");
        check(f->ref_resolves == resolves && f->bool_resolves == bool_resolves &&
              f->ref_accesses == accesses && f->bool_accesses == bool_accesses,
              "null write is rejected before property provider callbacks");
    }
    {
        unsigned resolves = f->ref_resolves, bool_resolves = f->bool_resolves;
        unsigned accesses = f->ref_accesses, bool_accesses = f->bool_accesses;
        expect_script_error(state, "answer=n['\\255']",
                            "invalid UTF-8 property name on null is rejected");
        check(f->ref_resolves == resolves && f->bool_resolves == bool_resolves &&
              f->ref_accesses == accesses && f->bool_accesses == bool_accesses,
              "invalid null-chain name reaches no provider");
    }

    /* A nil write is the one accepted null representation. */
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); a.Link=nil; n=a.Link; "
        "answer=(n:IsValid()==false)", 1,
        "Lua nil writes canonical null reference");
    check(same(f->link, (lba_receiver){31u, 0u, 0u}) && f->ref_writes == 2u,
          "nil is published as exact canonical null");
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "b=StaticFindObject('/Fixture/B'); a.Link=b; answer=(a.Link==b)", 1,
        "live owned identity can replace a null reference");
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); n=a.NullLink; a.Link=n; "
        "answer=(a.Link:IsValid()==false)", 1,
        "same-instance null userdata assignment is accepted");

    /* Wrong Lua types and foreign-instance references never reach WRITE. */
    expect_reference_error(state, "a.Link=7", "integer reference assignment refused");
    expect_reference_error(state, "a.Link='B'", "string reference assignment refused");
    expect_reference_error(state,
        "a.Link=ForeignFindObject('/Fixture/B')",
        "foreign userdata reference assignment refused");
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "own=StaticFindObject('/Fixture/B'); "
        "foreign=ForeignFindObject('/Fixture/B'); answer=(own==foreign)", 0,
        "foreign duplicate identity cannot compare equal");

    /* Same-instance nulls compare equal; foreign nulls remain unequal. */
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); a.Link=nil; "
        "n=a.Link; m=a.NullLink; answer=(n==m)", 1,
        "same-instance canonical null equality");
    run_boolean(state,
        "n=StaticFindObject('/Fixture/A').NullLink; "
        "m=ForeignFindObject('/Fixture/A').NullLink; answer=(n==m)", 0,
        "foreign canonical null equality refused");

    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); "
        "b=StaticFindObject('/Fixture/B'); a.Link=b; answer=(a.Link==b)", 1,
        "restore live target before permission controls");

    /* Permission, current generation and one-transaction failure controls. */
    f->link_permissions = LBA_REF_READ;
    expect_reference_error(state, "a.Link=StaticFindObject('/Fixture/C')",
                           "read-only reference write refused");
    run_boolean(state, "answer=(a.Link==StaticFindObject('/Fixture/B'))", 1,
                "read-only reference remains readable");
    f->link_permissions = LBA_REF_WRITE;
    expect_script_error(state, "answer=a.Link",
                        "write-only reference read refused");
    f->link_permissions = LBA_REF_READ | LBA_REF_WRITE;

    {
        unsigned accesses = f->ref_accesses;
        f->bad_ref_descriptor = 1;
        expect_reference_error(state, "a.Link=StaticFindObject('/Fixture/C')",
                               "malformed reference descriptor refused");
        check(f->ref_accesses == accesses,
              "malformed descriptor refused before provider transaction");
        f->bad_ref_descriptor = 0;
    }
    {
        lba_receiver before = f->link;
        f->change_generation_after_resolve = 1;
        expect_reference_error(state, "a.Link=StaticFindObject('/Fixture/C')",
                               "property generation change between resolve and access refused");
        f->change_generation_after_resolve = 0;
        f->ref_generation = 9u;
        check(same(f->link, before), "generation race leaves target unchanged");
    }
    {
        lba_receiver before = f->link;
        check(run(state, "staleB=StaticFindObject('/Fixture/B')") == LUA_OK,
              "capture reference target before invalidation");
        f->b.generation++;
        expect_reference_error(state, "a.Link=staleB",
                               "stale saved target identity refused");
        f->b.generation--;
        check(same(f->link, before), "stale target refusal preserves stored identity");
    }
    f->fail_ref_access = 1;
    expect_reference_error(state, "a.Link=StaticFindObject('/Fixture/C')",
                           "provider transaction failure refuses write");
    f->fail_ref_access = 0;
    {
        unsigned accesses = f->ref_accesses, resolves = f->ref_resolves;
        f->malformed_ref_output = 1;
        expect_script_error(state, "answer=a.Link",
                            "foreign-owner reference result refused");
        check(f->ref_accesses == accesses + 1u && f->ref_resolves == resolves + 1u,
              "malformed live result reaches one read transaction only");
        f->malformed_ref_output = 2;
        expect_script_error(state, "answer=a.Link",
                            "mixed-null reference result refused");
        f->malformed_ref_output = 3;
        expect_script_error(state, "answer=a.Link",
                            "unknown current target reference result refused");
        f->malformed_ref_output = 0;
    }

    /* Staleness is checked before any property lookup or transaction. */
    {
        unsigned resolves = f->ref_resolves, bool_resolves = f->bool_resolves;
        unsigned accesses = f->ref_accesses, bool_accesses = f->bool_accesses;
        f->a.generation++;
        expect_script_error(state, "answer=a.Link",
                            "stale receiver reference read refused");
        check(f->ref_resolves == resolves && f->bool_resolves == bool_resolves &&
              f->ref_accesses == accesses && f->bool_accesses == bool_accesses,
              "stale receiver refused before either property provider");
        f->a.generation--;
    }
}

static void reserved_name(test_state *state) {
    fixture *f = &state->data;
    unsigned rr = f->ref_resolves, ra = f->ref_accesses;
    unsigned br = f->bool_resolves, ba = f->bool_accesses;
    int status = run(state,
        "a=StaticFindObject('/Fixture/A'); a.IsValid=true");
    check(status == LUA_ERRRUN && lua_gettop(state->L) == 1,
          "IsValid reserved name: assignment rejected");
    check(f->ref_resolves == rr && f->ref_accesses == ra &&
          f->bool_resolves == br && f->bool_accesses == ba,
          "IsValid reserved name: rejection precedes both property providers");
    lua_settop(state->L, 0);
}

static void existing_api(test_state *state) {
    fixture *f = &state->data;
    run_boolean(state,
        "a=StaticFindObject('/Fixture/A'); answer=a.Flag", 1,
        "legacy Bool property reads beside reference extension");
    check(run(state,
        "a=StaticFindObject('/Fixture/A'); answer=a:Echo(-321)") == LUA_OK &&
        lua_gettop(state->L) == 0 && read_global_integer(state->L, "answer", -321) &&
        f->method_calls == 1u,
        "legacy Int32 method survives reference extension");
    lua_settop(state->L, 0);
    run_boolean(state, "a.Flag=false; answer=(a.Flag==false)", 1,
                "legacy Bool property writes beside reference extension");
    check((state->data.bool_storage[0] & 1u) == 0u,
          "legacy Bool write updates its provider storage");
}

static void finish(test_state *state) {
    if (state->L == NULL) return;
    close_state(state->L, &state->vm);
    state->L = NULL;
    check(state->vm.live_bytes == 0u && state->vm.peak_bytes <= LUA_MEMORY_CAP,
          "Lua close releases bounded allocator storage");
}

int main(void) {
    test_state state;
    if (start(&state)) {
        references(&state);
        reserved_name(&state);
        existing_api(&state);
    }
    finish(&state);
    (void)printf("%s checks=%u failures=%u; no engine/mod execution\n",
                 failures ? "FAIL" : "PASS", checks, failures);
    return failures == 0u ? 0 : 1;
}
