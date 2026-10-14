// NativeModEngineServices.inc -- runtime binding of the native mod host (#9).
//
// Included ONCE, by the candidate src/Mod/CppMod.cpp, inside #ifdef
// __PROSPERO__ (the build replaces existing units; it does not add new ones,
// the same pattern as VmtTickPumpCore.inc). Implements
// NativeMods::EngineServices with the runtime's own, already-shipped
// services only:
//   * reflection: StaticFindObject / FindFirstOf / FindAllOf, FProperty lookup
//     by name through the class chain, UObject::ProcessEvent with a parameter
//     frame built from the UFunction's own reflected parameters (the same
//     shape LuaUObject.cpp uses for Lua calls);
//   * hooks: UFunction::RegisterPreHook / RegisterPostHook, the per-UFunction
//     Func-pointer swap (ScriptHookThunks) used by Lua RegisterHook and the R3
//     carriers -- data-only, no code patch;
//   * keys: UE4SSProgram::register_keydown_event (pad mapping on PS5), with a
//     private custom_data tag (3) that Lua-mod cleanup (tag 1) never touches;
//   * game thread: NativeModsGameThreadFrame(), called from the existing
//     data-only pump drains (LuaMod.cpp diff).
// No address, offset, game name or code-patching primitive appears here.
#include <Mod/NativeModAbi.h>
#include <Platform/HookAdmission.hpp>
#include <Platform/NativeModHostCore.hpp>
#include <Platform/NativeModLoaderPolicy.hpp>

#include <DynamicOutput/Output.hpp>
#include <Helpers/Utf16Utf8.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UnrealInitializer.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RC::Platform::NativeMods
{
    // Engine names (FName) compare case-insensitively: the display spelling is whichever spelling the game's name
    // table saw first (console-proven 2026-10-13: Townfall reflects CreateCanvasRenderTarget2D's "Width"/"Height"
    // as "width"/"height"). Compare the way the engine does: ASCII case folded, everything else exact.
    template <typename A, typename B>
    inline auto engine_name_eq(const A& a, const B& b) -> bool
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            auto x = static_cast<uint32_t>(a[i]), y = static_cast<uint32_t>(b[i]);
            if (x >= 'A' && x <= 'Z') x += 32;
            if (y >= 'A' && y <= 'Z') y += 32;
            if (x != y) return false;
        }
        return true;
    }

    namespace Runtime
    {
        inline std::atomic<bool> unreal_ready{false};
        inline std::atomic<bool> pump_seen{false};
        inline std::atomic<bool> vmt_seen{false};
        // Extension point for per-frame game-thread services built on #9 (the
        // #11 GUI host sets it). Called after the native mods' frame work.
        inline std::atomic<void (*)(float)> after_frame{nullptr};
    } // namespace Runtime

    class RuntimeEngineServices final : public EngineServices
    {
        using UObject = Unreal::UObject;
        using UFunction = Unreal::UFunction;
        using FProperty = Unreal::FProperty;

        struct HookRecord
        {
            UFunction* function{};
            Unreal::CallbackId pre{-1};
            Unreal::CallbackId post{-1};
        };
        std::mutex m_hooks_mutex;
        std::unordered_map<uint64_t, HookRecord> m_hooks;

        static auto wide(const char* utf8) -> StringType { return Unicode::DecodeUtf8(std::string_view{utf8 ? utf8 : ""}); }
        static auto utf8(const StringType& s) -> std::string { return Unicode::EncodeUtf8(s); }

        static auto put_string(const std::string& s, char* buf, int32_t cap, int32_t* len) -> mbm_status
        {
            *len = static_cast<int32_t>(s.size());
            if (cap <= *len) return MBM_E_TRUNCATED;
            std::memcpy(buf, s.c_str(), s.size() + 1);
            return MBM_OK;
        }

        static auto obj(mbm_object o) -> UObject* { return static_cast<UObject*>(o); }

        // O(1) liveness: the object's own GUObjectArray slot still holds it and
        // it is not unreachable. (UObject::IsReal walks every object: far too
        // slow per call.) Same risk class as upstream UE4SS C++ mods holding a
        // raw UObject*: a pointer to an already-freed object is read once.
        static auto valid(mbm_object o) -> bool
        {
            if (!o) return false;
            auto* u = static_cast<UObject*>(o);
            auto* item = Unreal::FUObjectArray::IndexToObject(u->GetInternalIndex());
            return item && item->GetUObject() == u && !item->IsUnreachable();
        }

        static auto find_property(UObject* o, const char* name) -> FProperty*
        {
            const auto w = wide(name);
            return o->GetPropertyByNameInChain(w.c_str());
        }

        enum class Kind : uint8_t { Unsupported, Bool, Byte, Int, Int64, Float, Double, Object, Name };

        static auto kind_of(FProperty* p) -> Kind
        {
            using namespace RC::Unreal;
            if (p->IsA<FBoolProperty>()) return Kind::Bool;
            if (p->IsA<FByteProperty>()) return Kind::Byte;
            if (p->IsA<FIntProperty>()) return Kind::Int;
            if (p->IsA<FInt64Property>()) return Kind::Int64;
            if (p->IsA<FFloatProperty>()) return Kind::Float;
            if (p->IsA<FDoubleProperty>()) return Kind::Double;
            if (p->IsA<FObjectProperty>()) return Kind::Object; // includes class / object-ptr subclasses
            if (p->IsA<FNameProperty>()) return Kind::Name;
            return Kind::Unsupported;
        }

        // Read one value at `where` (a property inside `container`).
        static auto read(FProperty* p, void* container, mbm_value* v) -> mbm_status
        {
            using namespace RC::Unreal;
            void* where = p->ContainerPtrToValuePtr<void>(container);
            switch (kind_of(p))
            {
            case Kind::Bool: {
                // Same bit access as the shipping Lua bool pusher (LuaUObject.cpp push_boolproperty).
                if (v->type != MBM_VT_BOOL) return MBM_E_TYPE;
                auto* bp = static_cast<FBoolProperty*>(p);
                const uint8_t* byte_value = static_cast<const uint8_t*>(where) + bp->GetByteOffset();
                v->v.b = (*byte_value & bp->GetFieldMask()) ? 1 : 0;
                return MBM_OK;
            }
            case Kind::Byte:
                if (v->type != MBM_VT_U8) return MBM_E_TYPE;
                v->v.u8 = *static_cast<uint8_t*>(where);
                return MBM_OK;
            case Kind::Int:
                if (v->type != MBM_VT_I32) return MBM_E_TYPE;
                std::memcpy(&v->v.i32, where, sizeof(int32_t));
                return MBM_OK;
            case Kind::Int64:
                if (v->type != MBM_VT_I64) return MBM_E_TYPE;
                std::memcpy(&v->v.i64, where, sizeof(int64_t));
                return MBM_OK;
            case Kind::Float:
                if (v->type == MBM_VT_F32) { std::memcpy(&v->v.f32, where, sizeof(float)); return MBM_OK; }
                if (v->type == MBM_VT_F64) { float f; std::memcpy(&f, where, sizeof f); v->v.f64 = f; return MBM_OK; }
                return MBM_E_TYPE;
            case Kind::Double:
                if (v->type == MBM_VT_F64) { std::memcpy(&v->v.f64, where, sizeof(double)); return MBM_OK; }
                if (v->type == MBM_VT_F32) { double d; std::memcpy(&d, where, sizeof d); v->v.f32 = static_cast<float>(d); return MBM_OK; }
                return MBM_E_TYPE;
            case Kind::Object:
                if (v->type != MBM_VT_OBJECT) return MBM_E_TYPE;
                std::memcpy(&v->v.object, where, sizeof(void*));
                return MBM_OK;
            case Kind::Name: {
                if (v->type != MBM_VT_NAME || !v->utf8_out || v->utf8_out_cap <= 0) return MBM_E_TYPE;
                const auto s = utf8(static_cast<FName*>(where)->ToString());
                v->utf8_out_len = static_cast<int32_t>(s.size());
                if (v->utf8_out_cap <= v->utf8_out_len) return MBM_E_TRUNCATED;
                std::memcpy(v->utf8_out, s.c_str(), s.size() + 1);
                return MBM_OK;
            }
            default:
                return MBM_E_TYPE;
            }
        }

        static auto write(FProperty* p, void* container, const mbm_value* v) -> mbm_status
        {
            using namespace RC::Unreal;
            void* where = p->ContainerPtrToValuePtr<void>(container);
            switch (kind_of(p))
            {
            case Kind::Bool: {
                if (v->type != MBM_VT_BOOL) return MBM_E_TYPE;
                auto* bp = static_cast<FBoolProperty*>(p);
                uint8_t* byte_value = static_cast<uint8_t*>(where) + bp->GetByteOffset();
                *byte_value = static_cast<uint8_t>((*byte_value & ~bp->GetFieldMask()) | (v->v.b ? bp->GetByteMask() : 0));
                return MBM_OK;
            }
            case Kind::Byte:
                if (v->type != MBM_VT_U8) return MBM_E_TYPE;
                *static_cast<uint8_t*>(where) = v->v.u8;
                return MBM_OK;
            case Kind::Int:
                if (v->type != MBM_VT_I32) return MBM_E_TYPE;
                std::memcpy(where, &v->v.i32, sizeof(int32_t));
                return MBM_OK;
            case Kind::Int64:
                if (v->type != MBM_VT_I64) return MBM_E_TYPE;
                std::memcpy(where, &v->v.i64, sizeof(int64_t));
                return MBM_OK;
            case Kind::Float: {
                if (v->type != MBM_VT_F32 && v->type != MBM_VT_F64) return MBM_E_TYPE;
                const float f = v->type == MBM_VT_F32 ? v->v.f32 : static_cast<float>(v->v.f64);
                std::memcpy(where, &f, sizeof f);
                return MBM_OK;
            }
            case Kind::Double: {
                if (v->type != MBM_VT_F32 && v->type != MBM_VT_F64) return MBM_E_TYPE;
                const double d = v->type == MBM_VT_F64 ? v->v.f64 : static_cast<double>(v->v.f32);
                std::memcpy(where, &d, sizeof d);
                return MBM_OK;
            }
            case Kind::Object:
                // ABI v1: only null or a live object; class compatibility is the
                // mod's responsibility exactly as with upstream raw writes.
                if (v->type != MBM_VT_OBJECT || (v->v.object && !valid(v->v.object))) return MBM_E_TYPE;
                std::memcpy(where, &v->v.object, sizeof(void*));
                return MBM_OK;
            default:
                return MBM_E_TYPE; // names / strings / structs are not writable in ABI v1
            }
        }

      public:
        void log(int32_t level, std::string_view mod_name, std::string_view text) override
        {
            if (text.rfind("[NMOD] ", 0) == 0) text.remove_prefix(7); // host-core lines carry the tag already
            std::string raw = std::string("[NMOD] ") + std::string(mod_name) + ": " + std::string(text);
            while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r')) raw.pop_back();
            const StringType line = Unicode::DecodeUtf8(raw);
            switch (level)
            {
            case MBM_LOG_WARNING: Output::send<LogLevel::Warning>(STR("{}\n"), line); break;
            case MBM_LOG_ERROR: Output::send<LogLevel::Error>(STR("{}\n"), line); break;
            case MBM_LOG_VERBOSE: Output::send<LogLevel::Verbose>(STR("{}\n"), line); break;
            default: Output::send(STR("{}\n"), line); break;
            }
        }

        auto facts() -> HookAdmission::Facts override
        {
            HookAdmission::Facts f{};
            f.ufunction_thunks_ready = Runtime::unreal_ready.load(std::memory_order_acquire);
            f.game_thread_pump_live = Runtime::pump_seen.load(std::memory_order_acquire);
            f.vmt_engine_tick_armed = Runtime::vmt_seen.load(std::memory_order_acquire);
            // Not observable from this unit in ABI v1; their families are not
            // deliverable to native mods anyway (reported, never registered).
            // [N2] So the probe prints object-delete route=REFUSED and
            // load-map live=0 even though R11's own Lua routes for these are
            // live: that is the ABI-v1 scope, not a failure (launch card says so).
            f.delete_listener_admitted = false;
            f.world_event_route_live = false;
            f.create_listener_two_sources = false;
            f.viewport_tick_slot_two_sources = false;
            f.gnatives_two_sources = false;
            return f;
        }

        auto find_object(const char* path, mbm_object* out) -> mbm_status override
        {
            if (!Runtime::unreal_ready) return MBM_E_UNAVAILABLE;
            auto* o = Unreal::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, wide(path));
            *out = o;
            return o ? MBM_OK : MBM_E_NOT_FOUND;
        }
        auto find_first_of(const char* cls, mbm_object* out) -> mbm_status override
        {
            if (!Runtime::unreal_ready) return MBM_E_UNAVAILABLE;
            const auto w = wide(cls);
            auto* o = Unreal::UObjectGlobals::FindFirstOf(StringViewType{w});
            *out = o;
            return o ? MBM_OK : MBM_E_NOT_FOUND;
        }
        auto construct_object(mbm_object cls, mbm_object outer, mbm_object* out) -> mbm_status override
        {
            *out = nullptr;
            if (!Runtime::unreal_ready ||
                !Unreal::UObjectGlobals::GlobalState::StaticConstructObjectInternal426.is_ready())
                return MBM_E_UNAVAILABLE;
            if (!Unreal::IsInGameThreadRaw()) return MBM_E_STATE;
            if (!valid(cls) || (outer && !valid(outer))) return MBM_E_INVALID;
            int32_t is_class{};
            const auto status = object_is_a(cls, "Class", &is_class);
            if (status != MBM_OK) return status;
            if (!is_class) return MBM_E_TYPE;
            // Native params are constructed by the port, never by reinterpreting
            // a foreign MSVC parameter object. This service admits defaults only.
            Unreal::FStaticConstructObjectParameters params{
                static_cast<Unreal::UClass*>(obj(cls)), obj(outer)};
            *out = Unreal::UObjectGlobals::StaticConstructObject(params);
            return *out ? MBM_OK : MBM_E_NOT_FOUND;
        }
        auto find_all_of(const char* cls, mbm_object* out, int32_t cap, int32_t* total) -> mbm_status override
        {
            if (!Runtime::unreal_ready) return MBM_E_UNAVAILABLE;
            std::vector<UObject*> all;
            const auto w = wide(cls);
            Unreal::UObjectGlobals::FindAllOf(StringViewType{w}, all);
            *total = static_cast<int32_t>(all.size());
            for (int32_t i = 0; i < cap && i < *total; ++i) out[i] = all[static_cast<size_t>(i)];
            return MBM_OK;
        }
        auto object_name(mbm_object o, bool full, char* buf, int32_t cap, int32_t* len) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            return put_string(utf8(full ? obj(o)->GetFullName() : obj(o)->GetName()), buf, cap, len);
        }
        auto object_class(mbm_object o, mbm_object* out) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            *out = obj(o)->GetClassPrivate();
            return *out ? MBM_OK : MBM_E_NOT_FOUND;
        }
        auto object_is_a(mbm_object o, const char* cls, int32_t* out) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            const auto w = wide(cls);
            *out = 0;
            for (Unreal::UStruct* s = obj(o)->GetClassPrivate(); s; s = s->GetSuperStruct())
            {
                if (engine_name_eq(s->GetName(), w))
                {
                    *out = 1;
                    break;
                }
            }
            return MBM_OK;
        }
        auto object_is_valid(mbm_object o, int32_t* out) -> mbm_status override
        {
            *out = valid(o) ? 1 : 0;
            return MBM_OK;
        }
        auto property_get(mbm_object o, const char* name, mbm_value* v) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            auto* p = find_property(obj(o), name);
            return p ? read(p, o, v) : MBM_E_NOT_FOUND;
        }
        auto property_set(mbm_object o, const char* name, const mbm_value* v) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            auto* p = find_property(obj(o), name);
            return p ? write(p, o, v) : MBM_E_NOT_FOUND;
        }
        auto property_address(mbm_object o, const char* name, uint32_t type, void** out) -> mbm_status override
        {
            if (!valid(o)) return MBM_E_INVALID;
            auto* p = find_property(obj(o), name);
            if (!p) return MBM_E_NOT_FOUND;
            const auto k = kind_of(p);
            const bool match = (k == Kind::Bool && type == MBM_VT_BOOL && static_cast<Unreal::FBoolProperty*>(p)->GetFieldMask() == 0xFF && static_cast<Unreal::FBoolProperty*>(p)->GetByteOffset() == 0) ||
                               (k == Kind::Byte && type == MBM_VT_U8) || (k == Kind::Int && type == MBM_VT_I32) || (k == Kind::Int64 && type == MBM_VT_I64) ||
                               (k == Kind::Float && type == MBM_VT_F32) || (k == Kind::Double && type == MBM_VT_F64) ||
                               (k == Kind::Object && type == MBM_VT_OBJECT);
            if (!match) return MBM_E_TYPE;
            *out = p->ContainerPtrToValuePtr<void>(static_cast<void*>(o));
            return MBM_OK;
        }

        auto call_function(mbm_object o, const char* fn_name, const mbm_value* args, int32_t argc, mbm_value* ret) -> mbm_status override
        {
            using namespace RC::Unreal;
            if (!valid(o)) return MBM_E_INVALID;
            const auto w = wide(fn_name);
            UFunction* fn = obj(o)->GetFunctionByNameInChain(w.c_str());
            if (!fn) return MBM_E_NOT_FOUND;
            const auto frame_size = static_cast<size_t>(fn->GetParmsSize());
            const uint16_t ret_offset = fn->GetReturnValueOffset();
            // 16-byte aligned, zeroed frame; only scalar / object parameters are
            // admitted, for which all-zero is a valid initial state.
            std::vector<std::max_align_t> storage((frame_size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t) + 1);
            auto* frame = reinterpret_cast<uint8_t*>(storage.data());
            std::memset(frame, 0, storage.size() * sizeof(std::max_align_t));
            int32_t next = 0;
            FProperty* ret_prop{};
            for (FProperty* p : TFieldRange<FProperty>(fn, EFieldIterationFlags::IncludeDeprecated))
            {
                if (!p->HasAnyPropertyFlags(CPF_Parm)) continue;
                const auto off = p->GetOffset_Internal();
                if (off < 0 || static_cast<size_t>(off) + static_cast<size_t>(p->GetSize()) > frame_size) return MBM_E_INVALID;
                if (ret_offset != 0xFFFF && off == ret_offset)
                {
                    ret_prop = p;
                    continue;
                }
                if (next >= argc) return MBM_E_INVALID; // too few arguments
                if (kind_of(p) == Kind::Unsupported || kind_of(p) == Kind::Name) return MBM_E_TYPE;
                if (auto s = write(p, frame, &args[next]); s != MBM_OK) return s;
                ++next;
            }
            if (next != argc) return MBM_E_INVALID; // too many arguments
            // [FIX N4] Decide on the return type BEFORE the call: a return value
            // this ABI cannot read (FString, TArray, struct, ...) would be
            // constructed in the frame by ProcessEvent and never destroyed.
            if (ret_prop && kind_of(ret_prop) == Kind::Unsupported) return MBM_E_TYPE;
            obj(o)->ProcessEvent(fn, frame);
            if (ret && ret_prop) return read(ret_prop, frame, ret);
            return MBM_OK;
        }

        auto install_ufunction_hook(const char* path, uint64_t record, bool pre, bool post) -> mbm_status override
        {
            using namespace RC::Unreal;
            if (!Runtime::unreal_ready) return MBM_E_UNAVAILABLE;
            auto* fn = UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, wide(path));
            if (!fn) return MBM_E_NOT_FOUND;
            if (!fn->GetFuncPtr()) return MBM_E_REFUSED; // nothing to swap
            HookRecord rec{fn};
            auto make = [record, fn](bool is_pre) {
                // [FIX N3] the ABI promises call->function = the hooked UFunction.
                return [record, is_pre, fn](UnrealScriptFunctionCallableContext& ctx, void*) {
                    mbm_hook_call call{ctx.Context, fn, ctx.TheStack.Locals(), ctx.RESULT_DECL};
                    runtime_host().dispatch_ufunction(record, is_pre, call);
                };
            };
            if (pre) rec.pre = fn->RegisterPreHook(make(true), nullptr);
            if (post) rec.post = fn->RegisterPostHook(make(false), nullptr);
            // Same validity test as the shipping R3 carrier arm (callback_id < 0).
            if ((pre && rec.pre < 0) || (post && rec.post < 0))
            {
                if (rec.pre >= 0) fn->UnregisterHook(rec.pre);
                if (rec.post >= 0) fn->UnregisterHook(rec.post);
                return MBM_E_REFUSED;
            }
            std::lock_guard g{m_hooks_mutex};
            m_hooks[record] = rec;
            return MBM_OK;
        }
        void remove_ufunction_hook(uint64_t record) override
        {
            HookRecord rec{};
            {
                std::lock_guard g{m_hooks_mutex};
                auto it = m_hooks.find(record);
                if (it == m_hooks.end()) return;
                rec = it->second;
                m_hooks.erase(it);
            }
            // The host record is already inactive: a failed removal is harmless.
            try
            {
                if (rec.pre >= 0) rec.function->UnregisterHook(rec.pre);
                if (rec.post >= 0) rec.function->UnregisterHook(rec.post);
            }
            catch (...)
            {
            }
        }
        auto install_key(uint32_t key, uint32_t mask, uint64_t record) -> mbm_status override
        {
            Input::Handler::ModifierKeyArray mods{};
            size_t n = 0;
            if (mask & MBM_KEYMOD_SHIFT) mods[n++] = Input::ModifierKey::SHIFT;
            if (mask & MBM_KEYMOD_CONTROL) mods[n++] = Input::ModifierKey::CONTROL;
            if (mask & MBM_KEYMOD_ALT) mods[n++] = Input::ModifierKey::ALT;
            constexpr uint8_t kNativeModKeyTag = 3; // 1 = Lua (erased on Lua reload), 2 = upstream C++ (custom_data2 freed)
            UE4SSProgram::get_program().register_keydown_event(static_cast<Input::Key>(key), mods,
                                                               [record] { runtime_host().dispatch_key(record); }, kNativeModKeyTag, nullptr);
            return MBM_OK;
        }
        auto on_game_thread() -> bool override { return Unreal::IsInGameThreadRaw(); }

        static auto runtime_host() -> Host&;
    };

    inline auto runtime_engine() -> RuntimeEngineServices&
    {
        static RuntimeEngineServices engine;
        return engine;
    }

    inline auto RuntimeEngineServices::runtime_host() -> Host&
    {
        static Host host{runtime_engine(), "modbridge-native-mods-abi1"};
        return host;
    }

    inline auto runtime_host() -> Host& { return RuntimeEngineServices::runtime_host(); }
} // namespace RC::Platform::NativeMods

namespace RC::Platform
{
    // Called by the runtime's data-only game-thread pumps (LuaMod.cpp):
    // source 0 = R3 FuncPtr-marshal carrier drain, 1 = copied-vtable Tick drain.
    // Must never throw into engine code.
    void NativeModsGameThreadFrame(int source) noexcept
    {
        try
        {
            using namespace NativeMods;
            static std::chrono::steady_clock::time_point last{};
            const auto now = std::chrono::steady_clock::now();
            const float dt = last.time_since_epoch().count() == 0 ? 0.0f : std::chrono::duration<float>(now - last).count();
            last = now;
            Runtime::pump_seen.store(true, std::memory_order_release);
            if (source == 1) Runtime::vmt_seen.store(true, std::memory_order_release);
            const auto pump = source == 1 ? PumpSource::VmtEngineTick : PumpSource::GameThreadPump;
            runtime_host().game_thread_frame(dt, pump);
            // [FIX F5] the per-frame service (#11 GUI render) runs at most once per
            // engine frame, not once per pump call (FrameGate, NativeModHostCore.hpp).
            if (auto* after = Runtime::after_frame.load(std::memory_order_acquire))
            {
                static FrameGate gate;
                float frame_dt = 0.0f;
                if (gate.due(now, pump, &frame_dt)) after(frame_dt);
            }
        }
        catch (...)
        {
        }
    }
} // namespace RC::Platform
