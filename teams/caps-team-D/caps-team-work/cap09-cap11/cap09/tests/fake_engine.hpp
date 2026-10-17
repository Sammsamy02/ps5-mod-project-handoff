// fake_engine.hpp -- host-test double for RC::Platform::NativeMods::EngineServices.
// A tiny reflected-object model (names, classes, typed properties, functions)
// plus recorders for hook / key installs. Test-only: never part of a build.
#pragma once
#include <Platform/NativeModHostCore.hpp>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct FakeProp
{
    uint32_t type{};
    bool bitfield{};
    int32_t b{};
    uint8_t u8{};
    int32_t i32{};
    int64_t i64{};
    float f32{};
    double f64{};
    void* object{};
    std::string text;
};

struct FakeObject
{
    std::string name;
    std::string full_name;
    std::string class_name;
    std::vector<std::string> class_chain; // includes class_name
    std::map<std::string, FakeProp> props;
    bool valid{true};
};

class FakeEngine final : public RC::Platform::NativeMods::EngineServices
{
  public:
    RC::Platform::HookAdmission::Facts f{};
    std::vector<std::string> lines;
    std::vector<FakeObject*> objects;
    std::map<uint64_t, std::string> hooks; // record -> path
    std::map<uint64_t, std::pair<uint32_t, uint32_t>> keys;
    std::vector<uint64_t> removed_hooks;
    bool game_thread{true};
    bool echo{false};
    RC::Platform::NativeMods::Host* host{};

    void log(int32_t level, std::string_view mod, std::string_view text) override
    {
        std::string l = "[" + std::to_string(level) + "] " + std::string(mod) + ": " + std::string(text);
        while (!l.empty() && l.back() == '\n') l.pop_back();
        if (echo) std::printf("    log %s\n", l.c_str());
        lines.push_back(l);
    }
    auto facts() -> RC::Platform::HookAdmission::Facts override { return f; }

    auto by_ptr(void* p) -> FakeObject*
    {
        for (auto* o : objects)
            if (o == p) return o;
        return nullptr;
    }
    auto find_object(const char* path, mbm_object* out) -> mbm_status override
    {
        for (auto* o : objects)
            if (o->full_name == path || o->name == path ||
                (o->full_name.find(' ') != std::string::npos && o->full_name.substr(o->full_name.find(' ') + 1) == path))
            {
                *out = o;
                return MBM_OK;
            }
        return MBM_E_NOT_FOUND;
    }
    auto find_first_of(const char* cls, mbm_object* out) -> mbm_status override
    {
        for (auto* o : objects)
            if (o->class_name == cls)
            {
                *out = o;
                return MBM_OK;
            }
        return MBM_E_NOT_FOUND;
    }
    auto find_all_of(const char* cls, mbm_object* out, int32_t cap, int32_t* total) -> mbm_status override
    {
        int32_t n = 0;
        for (auto* o : objects)
            if (o->class_name == cls)
            {
                if (n < cap) out[n] = o;
                ++n;
            }
        *total = n;
        return MBM_OK;
    }
    auto object_name(mbm_object obj, bool full, char* buf, int32_t cap, int32_t* len) -> mbm_status override
    {
        auto* o = by_ptr(obj);
        if (!o) return MBM_E_NOT_FOUND;
        const std::string& s = full ? o->full_name : o->name;
        *len = static_cast<int32_t>(s.size());
        if (cap <= *len) return MBM_E_TRUNCATED;
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return MBM_OK;
    }
    auto object_class(mbm_object obj, mbm_object* out) -> mbm_status override
    {
        auto* o = by_ptr(obj);
        if (!o) return MBM_E_NOT_FOUND;
        for (auto* c : objects)
            if (c->name == o->class_name)
            {
                *out = c;
                return MBM_OK;
            }
        return MBM_E_NOT_FOUND;
    }
    auto object_is_a(mbm_object obj, const char* cls, int32_t* out) -> mbm_status override
    {
        auto* o = by_ptr(obj);
        if (!o) return MBM_E_NOT_FOUND;
        *out = 0;
        for (auto& c : o->class_chain)
            if (c == cls) *out = 1;
        return MBM_OK;
    }
    auto object_is_valid(mbm_object obj, int32_t* out) -> mbm_status override
    {
        auto* o = by_ptr(obj);
        *out = (o && o->valid) ? 1 : 0;
        return MBM_OK;
    }
    auto prop(mbm_object obj, const char* name) -> FakeProp*
    {
        auto* o = by_ptr(obj);
        if (!o) return nullptr;
        auto it = o->props.find(name);
        return it == o->props.end() ? nullptr : &it->second;
    }
    auto property_get(mbm_object obj, const char* name, mbm_value* v) -> mbm_status override
    {
        auto* p = prop(obj, name);
        if (!p) return MBM_E_NOT_FOUND;
        if (p->type != v->type) return MBM_E_TYPE;
        switch (p->type)
        {
        case MBM_VT_BOOL: v->v.b = p->b; break;
        case MBM_VT_U8: v->v.u8 = p->u8; break;
        case MBM_VT_I32: v->v.i32 = p->i32; break;
        case MBM_VT_I64: v->v.i64 = p->i64; break;
        case MBM_VT_F32: v->v.f32 = p->f32; break;
        case MBM_VT_F64: v->v.f64 = p->f64; break;
        case MBM_VT_OBJECT: v->v.object = p->object; break;
        default: return MBM_E_TYPE;
        }
        return MBM_OK;
    }
    auto property_set(mbm_object obj, const char* name, const mbm_value* v) -> mbm_status override
    {
        auto* p = prop(obj, name);
        if (!p) return MBM_E_NOT_FOUND;
        if (p->type != v->type) return MBM_E_TYPE;
        switch (p->type)
        {
        case MBM_VT_BOOL: p->b = v->v.b; break;
        case MBM_VT_U8: p->u8 = v->v.u8; break;
        case MBM_VT_I32: p->i32 = v->v.i32; break;
        case MBM_VT_I64: p->i64 = v->v.i64; break;
        case MBM_VT_F32: p->f32 = v->v.f32; break;
        case MBM_VT_F64: p->f64 = v->v.f64; break;
        case MBM_VT_OBJECT: p->object = v->v.object; break;
        default: return MBM_E_TYPE;
        }
        return MBM_OK;
    }
    auto property_address(mbm_object obj, const char* name, uint32_t type, void** out) -> mbm_status override
    {
        auto* p = prop(obj, name);
        if (!p) return MBM_E_NOT_FOUND;
        if (p->type != type || p->bitfield) return MBM_E_TYPE;
        switch (p->type)
        {
        case MBM_VT_BOOL: *out = &p->b; break; // test model only
        case MBM_VT_U8: *out = &p->u8; break;
        case MBM_VT_I32: *out = &p->i32; break;
        case MBM_VT_I64: *out = &p->i64; break;
        case MBM_VT_F32: *out = &p->f32; break;
        case MBM_VT_F64: *out = &p->f64; break;
        case MBM_VT_OBJECT: *out = &p->object; break;
        default: return MBM_E_TYPE;
        }
        return MBM_OK;
    }
    std::vector<std::string> calls;
    auto call_function(mbm_object obj, const char* fn, const mbm_value*, int32_t argc, mbm_value*) -> mbm_status override
    {
        auto* o = by_ptr(obj);
        if (!o) return MBM_E_NOT_FOUND;
        calls.push_back(o->name + "." + fn + "/" + std::to_string(argc));
        return MBM_OK;
    }
    bool refuse_hook_install{};
    auto install_ufunction_hook(const char* path, uint64_t record, bool, bool) -> mbm_status override
    {
        if (refuse_hook_install) return MBM_E_NOT_FOUND;
        hooks[record] = path;
        return MBM_OK;
    }
    void remove_ufunction_hook(uint64_t record) override { removed_hooks.push_back(record); }
    auto install_key(uint32_t key, uint32_t mods, uint64_t record) -> mbm_status override
    {
        keys[record] = {key, mods};
        return MBM_OK;
    }
    auto on_game_thread() -> bool override { return game_thread; }

    // test helpers: fire engine-side events the way the runtime would
    void fire_hook(const std::string& path, mbm_object ctx, void* params)
    {
        std::vector<uint64_t> recs;
        for (auto& [r, p] : hooks)
            if (p == path) recs.push_back(r);
        for (auto r : recs)
        {
            mbm_hook_call c{ctx, nullptr, params, nullptr};
            host->dispatch_ufunction(r, true, c);
            host->dispatch_ufunction(r, false, c);
        }
    }
    void press(uint32_t key)
    {
        std::vector<uint64_t> recs;
        for (auto& [r, km] : keys)
            if (km.first == key) recs.push_back(r);
        for (auto r : recs) host->dispatch_key(r);
    }
    auto count(const std::string& needle) const -> int
    {
        int n = 0;
        for (auto& l : lines)
            if (l.find(needle) != std::string::npos) ++n;
        return n;
    }
};

// ---- minimal check framework: counts checks, fails on zero ----
struct Checks
{
    int run{}, failed{};
    void ok(bool cond, const char* what)
    {
        ++run;
        if (!cond)
        {
            ++failed;
            std::printf("  FAIL %s\n", what);
        }
        else
            std::printf("  pass %s\n", what);
    }
    void ok(bool cond, const std::string& what) { ok(cond, what.c_str()); }
    auto finish(const char* suite) -> int
    {
        std::printf("[%s] checks=%d failed=%d\n", suite, run, failed);
        if (run == 0)
        {
            std::printf("[%s] FAIL: zero checks ran\n", suite);
            return 2;
        }
        return failed ? 1 : 0;
    }
};
