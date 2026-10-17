// KbmEngineKeysRuntime.inc -- runtime binding of the engine-keys input (keyboard and mouse, capability K2, Team I2).
//
// Included ONCE, by candidate/src/UE4SSProgram.cpp, inside #if defined(__PROSPERO__) && defined(HAS_INPUT) (the build
// replaces existing units and adds none: the same pattern as VmtTickPumpCore.inc and NativeModEngineServices.inc).
// Defines the three entry points:
//   RC::Platform::KbmComposeInput(input)  UE4SSProgram::validate_native_services, constructor thread, before
//                                         Handler::init: input = Merged{ the PS5Pad.ini source when the composition
//                                         made one, engine keys }. Any failure keeps `input` exactly as it was.
//   RC::Platform::KbmGameThreadFrame()    the game-thread drains in LuaMod.cpp: the R3 FuncPtr-marshal drain (the pump
//                                         that runs on PS5) and the VMT Tick drain (when it is armed), once per pump
//                                         call; the sampler samples at most every 8 ms, so it is once per frame.
//   RC::Platform::KbmRetire()             UE4SSProgram teardown, right after Handler::shutdown (which closed the
//                                         engine-keys channel): handler shutdown -> pump retirement -> unload.
//
// The engine is reached ONLY through what the runtime already ships and uses for Lua / C++ mods: the object array
// liveness check, UObjectGlobals::FindAllOf, reflection by name (GetFunctionByNameInChain, GetPropertyByNameInChain,
// TFieldRange over a UFunction / UScriptStruct, FProperty flags / offsets / sizes, FBoolProperty byte offset / mask,
// FStructProperty struct), the find-only FName constructor (FNAME_Find) and UObject::ProcessEvent (the call path Lua
// uses). No import, no address, no offset, no game or mod name, no hook, no detour.
#include <Platform/KbmEngineKeys.hpp>

#include <DynamicOutput/Output.hpp>
#include <Helpers/Utf16Utf8.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

namespace RC::Platform::Kbm
{
    class UnrealKeyReflection final : public KeyReflection
    {
        using UObject = Unreal::UObject;
        using UFunction = Unreal::UFunction;
        using UStruct = Unreal::UStruct;
        using FProperty = Unreal::FProperty;
        using FName = Unreal::FName;
        static_assert(std::is_trivially_copyable_v<FName>, "FName is copied into the parameter frame as bytes");

        static constexpr int k_max_fields = 64;      // bound on any reflected property walk
        static constexpr int k_max_class_depth = 64; // bound on any super-class walk

        // Engine key names are ASCII; the port's character type is wider.
        static auto wide(const char* ascii) -> StringType
        {
            StringType w;
            for (const char* c = ascii; c && *c; ++c) w.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*c)));
            return w;
        }
        // Engine names compare ASCII case-insensitively (the display spelling is whichever spelling the name table
        // saw first; console-proven 2026-10-13 for "Width"/"width"), everything else exact.
        static auto name_eq(const StringType& a, const char* b) -> bool
        {
            const size_t n = std::strlen(b);
            if (a.size() != n) return false;
            for (size_t i = 0; i < n; ++i)
            {
                auto x = static_cast<uint32_t>(a[i]), y = static_cast<uint32_t>(static_cast<unsigned char>(b[i]));
                if (x >= 'A' && x <= 'Z') x += 32;
                if (y >= 'A' && y <= 'Z') y += 32;
                if (x != y) return false;
            }
            return true;
        }
        // O(1) liveness, the same test the #9 / #11 bindings use: the object's own object-array slot still holds it
        // and it is not unreachable.
        static auto valid(UObject* o) -> bool
        {
            if (!o) return false;
            auto* item = Unreal::FUObjectArray::IndexToObject(o->GetInternalIndex());
            return item && item->GetUObject() == o && !item->IsUnreachable();
        }
        static auto class_chain_has(UObject* o, const char* class_name) -> bool
        {
            int depth = 0;
            for (UStruct* s = o->GetClassPrivate(); s && depth < k_max_class_depth; s = s->GetSuperStruct(), ++depth)
                if (name_eq(s->GetName(), class_name)) return true;
            return false;
        }
        static auto kind_of(FProperty* p) -> Kind
        {
            if (p->IsA<Unreal::FBoolProperty>()) return Kind::Bool;
            if (p->IsA<Unreal::FStructProperty>()) return Kind::Struct;
            if (p->IsA<Unreal::FNameProperty>()) return Kind::Name;
            return Kind::Other;
        }
        static auto field_of(FProperty* p) -> Field
        {
            Field f{};
            f.kind = kind_of(p);
            f.parm = p->HasAnyPropertyFlags(Unreal::CPF_Parm);
            f.ret = p->HasAnyPropertyFlags(Unreal::CPF_ReturnParm);
            f.out = p->HasAnyPropertyFlags(Unreal::CPF_OutParm);
            f.ref = p->HasAnyPropertyFlags(Unreal::CPF_ReferenceParm);
            f.offset = p->GetOffset_Internal();
            f.size = p->GetSize();
            f.array_dim = p->GetArrayDim();
            if (f.kind == Kind::Struct)
            {
                Unreal::UScriptStruct* st = static_cast<Unreal::FStructProperty*>(p)->GetStruct(); // TObjectPtr -> raw (as LuaUObject.cpp)
                f.struct_type = st;
            }
            if (f.kind == Kind::Bool)
            {
                auto* bp = static_cast<Unreal::FBoolProperty*>(p);
                f.bool_byte_offset = bp->GetByteOffset();
                f.bool_field_mask = bp->GetFieldMask();
            }
            return f;
        }

      public:
        auto find_local_controller() -> Handle override
        {
            try
            {
                std::vector<UObject*> controllers{};
                Unreal::UObjectGlobals::FindAllOf(STR("PlayerController"), controllers);
                for (UObject* pc : controllers)
                {
                    if (!valid(pc)) continue;
                    // APlayerController::Player (reflected UPROPERTY): a LocalPlayer for the local player's controller.
                    FProperty* p = pc->GetPropertyByNameInChain(STR("Player"));
                    if (!p || !p->IsA<Unreal::FObjectProperty>() || static_cast<size_t>(p->GetSize()) != sizeof(UObject*)) continue;
                    UObject* player{};
                    std::memcpy(&player, p->ContainerPtrToValuePtr<void>(static_cast<void*>(pc)), sizeof player);
                    if (!valid(player)) continue;
                    if (class_chain_has(player, "LocalPlayer")) return pc;
                }
            }
            catch (...)
            {
            }
            return nullptr; // none yet (the reader retries at most twice a second)
        }
        auto object_index(Handle object) -> int32_t override { return static_cast<UObject*>(object)->GetInternalIndex(); }
        // Reads only the object array, never the (possibly freed) object itself.
        auto is_live_at(Handle object, int32_t index) -> bool override
        {
            if (!object || index < 0) return false;
            auto* item = Unreal::FUObjectArray::IndexToObject(index); // bounds-checked
            return item && item->GetUObject() == static_cast<UObject*>(object) && !item->IsUnreachable();
        }
        auto class_of(Handle object) -> Handle override { return static_cast<UObject*>(object)->GetClassPrivate(); }
        auto find_function(Handle object, const char* name) -> Handle override
        {
            const StringType w = wide(name);
            return static_cast<UObject*>(object)->GetFunctionByNameInChain(w.c_str());
        }
        auto function_layout(Handle function, int32_t& parms_size, int32_t& return_offset, std::vector<Field>& params) -> bool override
        {
            auto* fn = static_cast<UFunction*>(function);
            parms_size = static_cast<int32_t>(fn->GetParmsSize());
            const uint16_t ret = fn->GetReturnValueOffset();
            return_offset = ret == 0xFFFF ? -1 : static_cast<int32_t>(ret);
            int seen = 0;
            for (FProperty* p : Unreal::TFieldRange<FProperty>(fn, Unreal::EFieldIterationFlags::IncludeDeprecated))
            {
                if (++seen > k_max_fields) return false;
                if (!p->HasAnyPropertyFlags(Unreal::CPF_Parm)) continue;
                params.push_back(field_of(p));
            }
            return true;
        }
        auto struct_is_named(Handle script_struct, const char* name) -> bool override
        {
            return name_eq(static_cast<Unreal::UScriptStruct*>(script_struct)->GetName(), name);
        }
        auto struct_members(Handle script_struct, std::vector<Field>& members) -> bool override
        {
            auto* st = static_cast<Unreal::UScriptStruct*>(script_struct);
            int seen = 0;
            for (FProperty* p : Unreal::TFieldRange<FProperty>(st, Unreal::EFieldIterationFlags::IncludeDeprecated))
            {
                if (++seen > k_max_fields) return false;
                Field f = field_of(p);
                f.name_is_keyname = name_eq(p->GetName(), "KeyName");
                members.push_back(f);
            }
            return true;
        }
        auto name_size() -> size_t override { return sizeof(FName); }
        auto find_name(const char* ascii, uint8_t* out, size_t cap) -> bool override
        {
            if (!ascii || !*ascii || cap < sizeof(FName)) return false;
            const StringType w = wide(ascii);
            const FName name(StringViewType{w}, Unreal::FNAME_Find); // find-only: never adds to the name table
            if (name.IsNone()) return false;
            std::memcpy(out, &name, sizeof name);
            return true;
        }
        auto name_matches_function(Handle function, const uint8_t* name_bytes) -> bool override
        {
            FName probe{};
            std::memcpy(&probe, name_bytes, sizeof probe);
            return !probe.IsNone() && probe.Equals(static_cast<UFunction*>(function)->GetNamePrivate());
        }
        void call(Handle object, Handle function, void* frame) override
        {
            static_cast<UObject*>(object)->ProcessEvent(static_cast<UFunction*>(function), frame);
        }
        auto now_ms() -> uint64_t override
        {
            return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    };

    namespace Runtime
    {
        // Created once by KbmComposeInput and never freed: a game-thread pump call may still be running while the
        // program tears down, so the sampler stays valid until the module is unloaded (process lifetime).
        inline std::atomic<EngineKeysRuntime*> g_runtime{nullptr};

        inline auto steady_ns() -> uint64_t
        {
            return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }
        inline void log_line(const char* line)
        {
            try
            {
                Output::send(STR("{}\n"), Unicode::DecodeUtf8(std::string_view{line}));
            }
            catch (...)
            {
            }
        }
        inline auto make_reader() -> std::unique_ptr<KeyStateReader>
        {
            return std::make_unique<EngineKeyReader>(std::make_unique<UnrealKeyReflection>());
        }
    } // namespace Runtime
} // namespace RC::Platform::Kbm

namespace RC::Platform
{
    void KbmComposeInput(std::shared_ptr<Input::PlatformInputSource>& input) noexcept
    {
        try
        {
            using namespace Kbm;
            if (Runtime::g_runtime.load(std::memory_order_acquire)) return; // once per process
            auto runtime = std::make_unique<EngineKeysRuntime>(&Runtime::make_reader, SamplerConfig{}, &Runtime::steady_ns, &Runtime::log_line);
            if (!runtime->compose(input)) return; // input unchanged (pad only, as before this change)
            Runtime::g_runtime.store(runtime.release(), std::memory_order_release);
        }
        catch (...)
        {
            // keep the supplied source exactly as it was
        }
    }

    void KbmGameThreadFrame() noexcept
    {
        if (auto* runtime = Kbm::Runtime::g_runtime.load(std::memory_order_acquire)) runtime->frame();
    }

    void KbmRetire() noexcept
    {
        if (auto* runtime = Kbm::Runtime::g_runtime.load(std::memory_order_acquire)) runtime->retire();
    }
} // namespace RC::Platform
