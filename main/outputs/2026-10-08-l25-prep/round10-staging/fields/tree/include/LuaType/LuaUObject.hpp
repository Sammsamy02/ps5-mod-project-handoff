#pragma once

#define NOMINMAX

#include <format>
#include <functional>
#include <type_traits>

#include <Constructs/Generator.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <Common.hpp>
#include <LuaMadeSimple/LuaObject.hpp>
#include <LuaFNameBridge.hpp> // [LFB] ONE hand Lua-string -> engine-name converter (agent-EY)
#include <LuaType/LuaCustomProperty.hpp>
#pragma warning(disable : 4005)
#include <Unreal/FOutputDevice.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/TypeChecker.hpp> // [IK 02b] get_world_name for the GetWorld outer-chain fallback (AActor.cpp:38-52 shape)
#include <Unreal/VersionedContainer/Container.hpp>
#include <Unreal/World.hpp>
#pragma warning(default : 4005)

template <typename SupposedIntegralType>
concept IsConvertableToLuaInteger = std::is_integral_v<SupposedIntegralType>;

namespace RC::LuaType
{
    // =====================================================================================
    // [R9-PROP] property/member lookup cost + cache (PC-timing work, launch 7: ~13 us per member call)
    //   - per-step cycle counters for prepare_to_handle (read and reset by the [PERF] line)
    //   - FindProperty cache for NATIVE classes only (CLASS_Native: compiled engine/game classes are
    //     never unloaded, so a cached FField* can never point into a freed class). Blueprint classes
    //     keep the uncached search. Key: class pointer + member FName (comparison index + number).
    // =====================================================================================
    namespace r9prop
    {
        inline uint64_t calls{0}, hits{0}, t_bridge{0}, t_fname{0}, t_custom{0}, t_find{0}, t_handle{0};
        struct Slot
        {
            const void* cls;
            uint64_t name;
            Unreal::FField* field;
        };
        inline constexpr size_t kSlots = 8192;
        inline Slot table[kSlots]{};
        inline int spin{0};
        inline auto lock() -> void
        {
            while (__atomic_exchange_n(&spin, 1, __ATOMIC_ACQUIRE) != 0)
            {
                while (__atomic_load_n(&spin, __ATOMIC_RELAXED) != 0) { __builtin_ia32_pause(); }
            }
        }
        inline auto unlock() -> void { __atomic_store_n(&spin, 0, __ATOMIC_RELEASE); }
        inline auto key_of(Unreal::FName n) -> uint64_t
        {
            return (static_cast<uint64_t>(n.GetComparisonIndex().ToUnstableInt()) << 32) | static_cast<uint32_t>(n.GetNumber());
        }
        inline auto pos(const void* cls, uint64_t name) -> size_t
        {
            uint64_t h = reinterpret_cast<uintptr_t>(cls) ^ (name * 0x9E3779B97F4A7C15ULL);
            h ^= h >> 31;
            return static_cast<size_t>(h) & (kSlots - 1);
        }
        // nullptr + false = not cached
        inline auto find(const void* cls, uint64_t name, Unreal::FField*& out) -> bool
        {
            lock();
            const Slot& s = table[pos(cls, name)];
            const bool hit = s.cls == cls && s.name == name;
            if (hit) { out = s.field; }
            unlock();
            return hit;
        }
        inline auto store(const void* cls, uint64_t name, Unreal::FField* field) -> void
        {
            lock();
            table[pos(cls, name)] = Slot{cls, name, field};
            unlock();
        }
        // Native = the class's own class is exactly the engine 'Class' (Blueprint classes are
        // 'BlueprintGeneratedClass' etc.). Uses only ClassPrivate + NamePrivate, which this game's
        // profile proves (launch 8: UClass::ClassFlags is NOT in the layout map, so HasAnyClassFlags
        // threw on every property read).
        inline auto is_native_class(Unreal::UClass* cls) -> bool
        {
            static const Unreal::FName s_class_name{STR("Class"), Unreal::FNAME_Find};
            if (cls == nullptr || s_class_name == Unreal::NAME_None) { return false; }
            Unreal::UClass* meta = cls->GetClassPrivate();
            return meta != nullptr && meta->GetNamePrivate().Equals(s_class_name);
        }
        // True when objects of this class are themselves structs/classes/functions ('Struct' in the chain).
        inline auto is_struct_type(Unreal::UStruct* cls) -> bool
        {
            static const Unreal::FName s_struct_name{STR("Struct"), Unreal::FNAME_Find};
            if (s_struct_name == Unreal::NAME_None) { return true; } // unknown -> treat as struct (never cache)
            for (Unreal::UStruct* sc = cls; sc != nullptr; sc = sc->GetSuperStruct())
            {
                if (sc->GetNamePrivate().Equals(s_struct_name)) { return true; }
            }
            return false;
        }
        inline auto add(uint64_t& counter, uint64_t ticks) -> void { __atomic_fetch_add(&counter, ticks, __ATOMIC_RELAXED); }
        inline auto take(uint64_t& counter) -> uint64_t { return __atomic_exchange_n(&counter, uint64_t{0}, __ATOMIC_RELAXED); }
    } // namespace r9prop

    struct FLuaObjectDeleteListener : public Unreal::FUObjectDeleteListener
    {
        static FLuaObjectDeleteListener s_lua_object_delete_listener;

        void NotifyUObjectDeleted(const Unreal::UObjectBase* object, [[maybe_unused]] int32_t index) override;

        void OnUObjectArrayShutdown() override
        {
            Unreal::UObjectArray::RemoveUObjectDeleteListener(this);
        }
    };

    auto call_ufunction_from_lua(const LuaMadeSimple::Lua& lua) -> int;

    using Operation = LuaMadeSimple::Type::Operation;

    template <typename ObjectType, template <typename T> typename LuaObjectBase, typename ObjectName>
    class ObjectBase : public LuaObjectBase<ObjectType>
    {
      protected:
        using SelfType = ObjectBase<ObjectType, LuaObjectBase, ObjectName>;
        using Super = SelfType;
        using ClassName = ObjectName;

      public:
        ObjectBase(ObjectType* object) : LuaObjectBase<ObjectType>(ObjectName::ToString(), object)
        {
        }
        ObjectBase(ObjectType&& object) : LuaObjectBase<ObjectType>(ObjectName::ToString(), std::move(object))
        {
        }
        ObjectBase() = delete;
        virtual ~ObjectBase() = default;

      public:
        // Whether this object is a specific type or inherits from a specific type
        virtual auto derives_from_actor() -> bool
        {
            return false;
        }
        virtual auto derives_from_name() -> bool
        {
            return false;
        }
        virtual auto derives_from_string() -> bool
        {
            return false;
        }
        virtual auto derives_from_text() -> bool
        {
            return false;
        }
        virtual auto derives_from_weak_object_ptr() -> bool
        {
            return false;
        }
        virtual auto derives_from_mod() -> bool
        {
            return false;
        }
        virtual auto derives_from_array() -> bool
        {
            return false;
        }
        virtual auto derives_from_class() -> bool
        {
            return false;
        }
        virtual auto derives_from_enum() -> bool
        {
            return false;
        }
        virtual auto derives_from_function() -> bool
        {
            return false;
        }
        virtual auto derives_from_object() -> bool
        {
            return false;
        }
        virtual auto derives_from_script_struct() -> bool
        {
            return false;
        }
        virtual auto derives_from_world() -> bool
        {
            return false;
        }
        virtual auto derives_from_property() -> bool
        {
            return false;
        }
        virtual auto derives_from_property_class() -> bool
        {
            return false;
        }
        virtual auto derives_from_ufunction() -> bool
        {
            return false;
        }

      public:
        // Constructor for ObjectBase
        auto static construct(const LuaMadeSimple::Lua& lua, Unreal::UObject* unreal_object) -> const LuaMadeSimple::Lua::Table
        {
            SelfType lua_object{unreal_object};

            auto metatable_name = lua_object.get_object_name();

            LuaMadeSimple::Lua::Table table = lua.get_metatable(metatable_name);
            if (lua.is_nil(-1))
            {
                lua.discard_value(-1);
                LuaObjectBase<ObjectType>::construct(lua, lua_object);
                setup_member_functions<LuaMadeSimple::Type::IsFinal::Yes>(table);
                lua.new_metatable<SelfType>(metatable_name, lua_object.get_metamethods());
            }

            // Create object & surrender ownership to Lua
            lua.transfer_stack_object(std::move(lua_object), metatable_name, lua_object.get_metamethods());

            return table;
        }

        // Constructor for objects that inherit from ObjectBase
        auto static construct(const LuaMadeSimple::Lua& lua, LuaMadeSimple::Type::BaseObject& construct_to) -> const LuaMadeSimple::Lua::Table
        {
            const LuaMadeSimple::Lua::Table table = LuaObjectBase<ObjectType>::construct(lua, construct_to);

            // Setup functions that can be called on this object
            setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);

            // Creation & transferring the object & its ownership fully to Lua is the responsibility of the overriding object

            return table;
        }

        template <LuaMadeSimple::Type::IsFinal is_final>
        auto static setup_member_functions(const LuaMadeSimple::Lua::Table& table) -> void
        {
            // Overridden functions from 'Lua::RemoteObject'/'Lua::LocalObject'.
            table.add_pair("GetAddress", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();
                if constexpr (!LuaObjectBase<ObjectType>::IsLocal)
                {
                    lua.set_integer(reinterpret_cast<uintptr_t>(lua_object.get_remote_cpp_object()));
                }
                else
                {
                    lua.set_integer(reinterpret_cast<uintptr_t>(&lua_object.get_local_cpp_object()));
                }
                return 1;
            });

            table.add_pair("IsValid", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();
                if constexpr (!LuaObjectBase<ObjectType>::IsLocal)
                {
                    if (lua_object.get_remote_cpp_object())
                    {
                        lua.set_bool(true);
                    }
                    else
                    {
                        lua.set_bool(false);
                    }
                }
                else
                {
                    lua.set_bool(true);
                }
                return 1;
            });

            // Add things that are intended to be overridden later here
            // These will then be set only if this is the final object (nothing overrides it later)
            if constexpr (is_final == LuaMadeSimple::Type::IsFinal::Yes)
            {
                table.add_pair("type", [](const LuaMadeSimple::Lua& lua) -> int {
                    lua.set_string("RemoteObjectBase");
                    return 1;
                });

                // If this is the final object then we also want to finalize creating the table
                // If not then it's the responsibility of the overriding object to call 'make_global()
                // table.make_global("RemoteObjectBase");
            }
        }
    };

    struct RC_UE4SS_API UE4SSBaseObjectName
    {
        constexpr static const char* ToString()
        {
            return "UE4SSBaseObject";
        }
    };

    template <typename DerivedType, typename ObjectName>
    using RemoteObjectBase = ObjectBase<DerivedType, LuaMadeSimple::Type::RemoteObject, ObjectName>;

    template <typename DerivedType, typename ObjectName>
    using LocalObjectBase = ObjectBase<DerivedType, LuaMadeSimple::Type::LocalObject, ObjectName>;

    using UE4SSBaseObject = ObjectBase<uint8_t, LuaMadeSimple::Type::RemoteObject, UE4SSBaseObjectName>;

    struct RC_UE4SS_API PusherParams
    {
        const Operation operation;
        const LuaMadeSimple::Lua& lua;
        Unreal::UObject* base;
        void* data;
        Unreal::FProperty* property;

        // Where in the Lua stack the data is at
        // Default is 1 because that's the default for LuaMadeSimple because that's how the system works
        int32_t stored_at_index = 1;

        // Whether to create a new Lua item (example: table) or use an existing one on the top of the stack
        bool create_new_if_get_non_trivial_local{true};

        auto get_operation() const -> const char*
        {
            switch (operation)
            {
                case Operation::Get:
                    return "Get";
                case Operation::GetNonTrivialLocal:
                    return "GetNonTrivialLocal";
                case Operation::Set:
                    return "Set";
                case Operation::GetParam:
                    return "GetParam";
            }
            return "UnknownOperation";
        }

        // https://stackoverflow.com/questions/59091462/from-c-how-can-i-print-the-contents-of-the-lua-stack/59097940#59097940
        auto get_stack_dump(const char* message = "") const -> std::string
        {
            auto lua_state = lua.get_lua_state();
            auto out_message = fmt::format("\n\nLUA Stack dump -> START------------------------------\n{}\n", message);
            int top = lua_gettop(lua_state);
            for (int i = 1; i <= top; i++)
            {
                out_message.append(fmt::format("{}\t{}\t", i, luaL_typename(lua_state, i)));
                switch (lua_type(lua_state, i))
                {
                case LUA_TNUMBER:
                    out_message.append(fmt::format("{}", lua_tonumber(lua_state, i)));
                    break;
                case LUA_TSTRING:
                    out_message.append(fmt::format("{}", lua_tostring(lua_state, i)));
                    break;
                case LUA_TBOOLEAN:
                    out_message.append(fmt::format("{}", (lua_toboolean(lua_state, i) ? "true" : "false")));
                    break;
                case LUA_TNIL:
                    out_message.append("nil");
                    break;
                case LUA_TFUNCTION:
                    out_message.append("function");
                    break;
                default:
                    out_message.append(fmt::format("{}", lua_topointer(lua_state, i)));
                    break;
                }
                out_message.append("\n");
            }
            out_message.append("\nLUA Stack dump -> END----------------------------\n\n");
            return out_message;
        }

        auto throw_error_internal_append_args(std::string&) const -> void
        {
        }
        template <typename K1, typename V1>
        auto throw_error_internal_append_args(std::string& error_message, K1&& key, V1&& value) const -> void
        {
            error_message.append(fmt::format("    {}: {}\n", key, value));
        }
        template <typename K1, typename V1, typename... Remaining>
        auto throw_error_internal_append_args(std::string& error_message, K1&& key, V1&& value, Remaining&&... remaining) const -> void
        {
            error_message.append(fmt::format("    {}: {}\n", key, value));
            throw_error_internal_append_args(error_message, remaining...);
        }
        // Helper to throw nicely formatted error messages in property pushers.
        // You can supply extra data by passing two extra args per data.
        // The first arg is the name of the data, it must be a string.
        // The second arg is anything that fmt::format can format.
        // Example: throw_error("push_objectproperty", "Value must be UObject or nil", "extra_data", 1234)
        // Output:
        // [push_objectproperty] Error:
        // Value must be UObject or nil
        //     Property: ObjectProperty Children./Script/Engine.Actor:Children
        //     extra_data: 1234
        template <typename... Args>
        auto throw_error(std::string_view handler_name, std::string_view format, Args&&... args) const -> void
        {
            std::string error_message{};
            error_message.append(fmt::format("[{}] Error:\n", handler_name));
            error_message.append(fmt::format(" {}\n", format));
            error_message.append(fmt::format("    Operation: {}\n", get_operation()));
            error_message.append(fmt::format("    Base: {}\n", static_cast<void*>(base)));
            error_message.append(fmt::format("    Data: {}\n", data));
            error_message.append(fmt::format("    Property ({}): {}\n", static_cast<void*>(property), property ? to_string(property->GetFullName()) : "null"));
            error_message.append(fmt::format("    StoredAtIndex: {}\n", stored_at_index));
            error_message.append(fmt::format("    CreateNewIfGetNonTrivialLocal: {}\n", create_new_if_get_non_trivial_local));
            throw_error_internal_append_args(error_message, args...);
            error_message.append(get_stack_dump());
            lua.throw_error(error_message);
        }
    };

    struct RC_UE4SS_API FunctionPusherParams
    {
        const LuaMadeSimple::Lua& lua;
        Unreal::UObject* base;
        Unreal::UFunction* function;
    };

    struct StaticState
    {
        using PropertyValuePusherCallable = std::function<void(const PusherParams&)>;
        static inline std::unordered_map<int32_t, PropertyValuePusherCallable> m_property_value_pushers;
    };

    RC_UE4SS_API auto auto_construct_object(const LuaMadeSimple::Lua&, Unreal::UObject*) -> void;

    // Helpers to construct an object that we cannot have access to in this header
    RC_UE4SS_API auto construct_fname(const LuaMadeSimple::Lua&) -> void;
    RC_UE4SS_API auto construct_uclass(const LuaMadeSimple::Lua&) -> void;
    RC_UE4SS_API auto construct_xproperty(const LuaMadeSimple::Lua&, Unreal::FProperty* property) -> void;

    auto convert_lua_table_to_struct(const LuaMadeSimple::Lua& lua,
                                     Unreal::UScriptStruct* script_struct,
                                     void* data,
                                     int table_index,
                                     Unreal::UObject* base = nullptr) -> void;
    auto convert_struct_to_lua_table(const LuaMadeSimple::Lua& lua,
                                     Unreal::UScriptStruct* script_struct,
                                     void* data,
                                     bool create_new_table = true,
                                     Unreal::UObject* base = nullptr) -> void;

    // Push to Lua -> START
    RC_UE4SS_API auto push_unhandledproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_objectproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_classproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_int8property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_int16property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_intproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_int64property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_byteproperty(const PusherParams&) -> void; // Unreal Engine calls uint8 a "byte"
    RC_UE4SS_API auto push_uint16property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_uint32property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_uint64property(const PusherParams&) -> void;
    RC_UE4SS_API auto push_structproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_arrayproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_setproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_mapproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_floatproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_doubleproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_boolproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_enumproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_weakobjectproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_nameproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_textproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_strproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_utf8strproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_ansistrproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_softobjectproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_interfaceproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_delegateproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_multicastdelegateproperty(const PusherParams&) -> void;
    RC_UE4SS_API auto push_multicastsparsedelegateproperty(const PusherParams&) -> void;

    RC_UE4SS_API auto push_functionproperty(const FunctionPusherParams&) -> void;
    // Push to Lua -> END

    auto handle_unreal_property_value(
            const Operation operation, const LuaMadeSimple::Lua&, Unreal::UObject* base, Unreal::FName property_name, Unreal::FField* field) -> void;

    auto is_a_implementation(const LuaMadeSimple::Lua& lua) -> int;
    auto uobject_equal_implementation(const LuaMadeSimple::Lua& lua) -> int;

    template <typename DerivedType, typename ObjectName>
    class UObjectBase;

    struct RC_UE4SS_API UObjectName
    {
        constexpr static const char* ToString()
        {
            return "UObject";
        }
    };
    using UObject = UObjectBase<Unreal::UObject, UObjectName>;

    auto add_to_global_unreal_objects_map(Unreal::UObject* object) -> void;
    auto is_object_in_global_unreal_object_map(Unreal::UObject* object) -> bool;

    template <typename DerivedType, typename ObjectName>
    class UObjectBase : public RemoteObjectBase<DerivedType, ObjectName>
    {
      protected:
        using Super = RemoteObjectBase<DerivedType, ObjectName>;
        using SelfType = Super;

      public:
        explicit UObjectBase(DerivedType* object) : Super(object)
        {
        }

      public:
        auto derives_from_object() -> bool override
        {
            return true;
        }

      public:
        UObjectBase() = delete;
        virtual ~UObjectBase() = default;

        // Constructor for UObject
        auto static construct(const LuaMadeSimple::Lua& lua, DerivedType* unreal_object) -> const LuaMadeSimple::Lua::Table
        {
            if (unreal_object != LuaMadeSimple::Type::special_invalid_ptr())
            {
                add_to_global_unreal_objects_map(unreal_object);
            }

            SelfType lua_object{unreal_object};

            auto metatable_name = ObjectName::ToString();

            LuaMadeSimple::Lua::Table table = lua.get_metatable(metatable_name);
            if (lua.is_nil(-1))
            {
                lua.discard_value(-1);
                Super::construct(lua, lua_object);
                setup_metamethods(lua_object);
                setup_member_functions<LuaMadeSimple::Type::IsFinal::Yes>(table);
                lua.new_metatable<SelfType>(metatable_name, lua_object.get_metamethods());
            }

            // Create object & surrender ownership to Lua
            lua.transfer_stack_object(std::move(lua_object), metatable_name, lua_object.get_metamethods());

            return table;
        }

        // Constructor for objects that inherit from UObject
        auto static construct(const LuaMadeSimple::Lua& lua, LuaMadeSimple::Type::BaseObject& construct_to) -> const LuaMadeSimple::Lua::Table
        {
            LuaMadeSimple::Lua::Table table = Super::construct(lua, construct_to);

            // Setup functions that can be called on this object
            setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);

            setup_metamethods(construct_to);

            // Creation & transferring the object & its ownership fully to Lua is the responsibility of the overriding object

            return table;
        }

      private:
        auto static setup_metamethods(LuaMadeSimple::Type::BaseObject& base_object) -> void
        {
            // Another object can override these metamethods by calling the 'create()' function again
            base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Index, [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Get, lua);
                return 1;
            });

            base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::NewIndex, [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Set, lua);
                return 0;
            });

            base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Call, [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();

                if (!lua_object.get_remote_cpp_object())
                {
                    lua.throw_error("Tried calling a member function but the UObject instance is nullptr\n");
                }
                return 0;
            });

            base_object.get_metamethods().create(LuaMadeSimple::Lua::MetaMethod::Equal, uobject_equal_implementation);
        }

      public:
        template <LuaMadeSimple::Type::IsFinal is_final>
        auto static setup_member_functions(const LuaMadeSimple::Lua::Table& table) -> void
        {
            Super::template setup_member_functions<LuaMadeSimple::Type::IsFinal::No>(table);

            // Add functions that are not intended to be overridden later here
            table.add_pair("GetFullName", [](const LuaMadeSimple::Lua& lua) -> int {
                // Get the userdata from the Lua stack
                // We're making an assumption here about the type
                // I know of no way to get around this assumption
                const auto& lua_object = lua.get_userdata<SelfType>();

                if (lua_object.get_remote_cpp_object())
                {
                    // [LFB] Claude item 4 (reverse direction): pool decoder
                    // (FName::ToString inside GetFullName) -> hand UTF-16 ->
                    // UTF-8 -> lua_pushlstring. No to_string/wide libc.
                    const StringType full_name = lua_object.get_remote_cpp_object()->GetFullName();
                    LuaFNameBridge::push_wide_to_lua(lua.get_lua_state(), full_name.data(), full_name.size());
                }
                else
                {
                    // We have a nullptr, lets return 'nil' for easy object verification in Lua
                    lua.set_nil();
                }

                return 1;
            });

#ifdef __PROSPERO__
            // [IA world-fix 2026-10-10, Claude round-8 item 4]: 'GetName' is
            // NOT a reflected UFunction anywhere in UObject's chain (round-4
            // console log: "[UF] miss w=[n:29D61] c=[n:1E6C3] v=4" — the
            // UWorld class chain walk visits 4 UFunctions, never 'GetName';
            // UObject's methods are native C++), and no native binding
            // existed (port OR upstream UE4SS), so the common mod idiom
            // World/Actor:GetName() fell into UFunction dispatch
            // (handle_unreal_property_value -> GetFunctionByNameInChain),
            // missed, and handed Lua the non-callable special_invalid_ptr
            // wrapper — "attempt to call a userdata value". That is exactly
            // why zzdiag's [MAP] tracker logged 0 lines while
            // FindFirstOf("World") itself worked ([FFO2] q='World' exact=2
            // first=0x18005194C0). Fix: native binding, same proven
            // rendering family as GetFullName directly above
            // (GetNamePrivate().ToString() -> pool decoder -> hand UTF-16 ->
            // UTF-8). Host builds keep upstream parity (no binding).
            table.add_pair("GetName", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();

                if (lua_object.get_remote_cpp_object())
                {
                    const StringType object_name = lua_object.get_remote_cpp_object()->GetName();
                    LuaFNameBridge::push_wide_to_lua(lua.get_lua_state(), object_name.data(), object_name.size());
                }
                else
                {
                    // Same nullptr contract as GetFullName above.
                    lua.set_nil();
                }

                return 1;
            });
#endif

            table.add_pair("GetFName", [](const LuaMadeSimple::Lua& lua) -> int {
                construct_fname(lua);
                return 1;
            });

            table.add_pair("GetClass", [](const LuaMadeSimple::Lua& lua) -> int {
                construct_uclass(lua);
                return 1;
            });

            table.add_pair("GetOuter", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();

                UObject::construct(lua, lua_object.get_remote_cpp_object()->GetOuterPrivate());

                return 1;
            });

            table.add_pair("IsAnyClass", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();

                lua.set_bool(lua_object.get_remote_cpp_object()->template IsA<Unreal::UClass>());

                return 1;
            });

            table.add_pair("Reflection", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();

                auto reflection_table = lua.prepare_new_table();
                reflection_table.set_has_userdata(false);
                reflection_table.add_key("ReflectedObject");
                SelfType::construct(lua, lua_object.get_remote_cpp_object());
                reflection_table.fuse_pair();

                reflection_table.add_pair("GetProperty", [](const LuaMadeSimple::Lua& lua) -> int {
                    // Stack
                    // 1 (-2): ReflectionTable
                    // 2 (-1): Param #1 (PropertyName)

                    if (!lua.is_string(2))
                    {
                        lua.throw_error("Function 'GetProperty' requires a string as the first parameter");
                    }
                    // [LFB] ONE hand converter for the reflection property
                    // name (was ensure_str(lua.get_string(2))); FNAME_Find
                    // semantics unchanged.
                    const LuaFNameBridge::Bridge property_key{lua.get_lua_state(), 2};
                    const Unreal::FName property_name = property_key.to_fname(Unreal::FNAME_Find);

                    auto reflection_table = lua.get_table();
                    const auto& reflected_object = reflection_table.get_userdata_field<SelfType>("ReflectedObject").get_remote_cpp_object();

                    if (!reflected_object)
                    {
                        construct_xproperty(lua, nullptr);
                        return 1;
                    }

                    auto* obj_as_struct = Unreal::Cast<Unreal::UStruct>(reflected_object);
                    if (!obj_as_struct)
                    {
                        obj_as_struct = reflected_object->GetClassPrivate();
                    }
                    auto* property = obj_as_struct->FindProperty(property_name);

                    construct_xproperty(lua, property);
                    return 1;
                });
                return 1;
            });

            table.add_pair("GetPropertyValue", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Get, lua);
                return 1;
            });

            table.add_pair("SetPropertyValue", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Set, lua);
                return 1;
            });

            table.add_pair("IsClass", [](const LuaMadeSimple::Lua& lua) -> int {
                // TODO: Refactor this function into "IsA", but leave the original "IsClass" for compatibility reasons
                //       Creating an "IsA" function will require that we expose some identifier of every class to Lua
                //       We could use strings or FNames, those could be calculated on the Lua side and then
                //       we wouldn't need to preemptively expose anything
                const auto& lua_object = lua.get_userdata<SelfType>();
                lua.set_bool(lua_object.get_remote_cpp_object()->template IsA<Unreal::UClass>());
                return 1;
            });

            table.add_pair("GetWorld", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();
                const auto object = lua_object.get_remote_cpp_object();
#ifdef __PROSPERO__
                // [IK world-fix 02b, Claude round-8 item 4; IF audit §1.2/§4.4(b)]:
                // UEHelpers.GetWorld rides this hardcoded member (UEHelpers.lua:96-107
                // -> PlayerController:GetWorld() / GameInstance:GetWorld()). The staged
                // PS5 layout pair (profile/units/CompiledNativeLayoutDeclarations.hpp,
                // selection_source=compiled-audited-townfall-l25) stages NO [UObject]
                // GetWorld vtable entry, so the native virtual wrapper
                // (unreal/src/UObject.cpp:316-319) THROWS "Virtual UObject::GetWorld
                // is unavailable" — 8 of 24 UEHelpers exports transitively dead (IF
                // census). Instead of throwing, fall back to the in-tree outer-chain
                // walk with EXACT AActor::GetWorld semantics (unreal/src/AActor.cpp:38-52):
                // first OuterPrivate behind the same RF_ClassDefaultObject /
                // RF_BeginDestroyed / IsUnreachable gate, then walk OuterPrivate until
                // the outer's class FName == TypeChecker::get_world_name(); return
                // that outer as UWorld*, else nullptr. Host builds (#else) keep the
                // upstream member byte-identical.
                Unreal::UWorld* world{};
                if (Unreal::UObject::VTableLayoutMap.contains(STR("GetWorld")))
                {
                    world = object ? object->GetWorld() : nullptr;
                }
                else if (object)
                {
                    Unreal::UObject* outer = object->GetOuterPrivate();
                    if (outer && !object->HasAnyFlags(Unreal::RF_ClassDefaultObject) && !outer->HasAnyFlags(Unreal::RF_BeginDestroyed) && !outer->GetObjectItem()->IsUnreachable())
                    {
                        while (outer)
                        {
                            if (outer->GetClassPrivate()->GetNamePrivate() == Unreal::TypeChecker::get_world_name())
                            {
                                world = static_cast<Unreal::UWorld*>(outer);
                                break;
                            }
                            outer = outer->GetOuterPrivate();
                        }
                    }
                }
                auto_construct_object(lua, world);
#else
                auto_construct_object(lua, object ? object->GetWorld() : nullptr);
#endif
                return 1;
            });

            table.add_pair("CallFunction", [](const LuaMadeSimple::Lua& lua) -> int {
                return call_ufunction_from_lua(lua);
            });

            table.add_pair("IsA", [](const LuaMadeSimple::Lua& lua) -> int {
                return is_a_implementation(lua);
            });

            table.add_pair("HasAllFlags", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'UObject.HasAllFlags'.
Overloads:
#1: HasAllFlags(EObjectFlags ObjectFlags))"};

                const auto& lua_object = lua.get_userdata<SelfType>();
                auto* object = lua_object.get_remote_cpp_object();

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                Unreal::EObjectFlags object_flags = static_cast<Unreal::EObjectFlags>(lua.get_integer());
                if (!object)
                {
                    lua.set_bool(false);
                    return 1;
                }
                lua.set_bool(object->HasAllFlags(object_flags));
                return 1;
            });

            table.add_pair("HasAnyFlags", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'UObject.HasAnyFlags'.
Overloads:
#1: HasAnyFlags(EObjectFlags ObjectFlags))"};

                const auto& lua_object = lua.get_userdata<SelfType>();
                auto* object = lua_object.get_remote_cpp_object();

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                Unreal::EObjectFlags object_flags = static_cast<Unreal::EObjectFlags>(lua.get_integer());
                if (!object)
                {
                    lua.set_bool(false);
                    return 1;
                }
                lua.set_bool(object->HasAnyFlags(object_flags));
                return 1;
            });

            table.add_pair("HasAnyInternalFlags", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'UObject.HasAnyInternalFlags'.
Overloads:
#1: HasAnyInternalFlags(EInternalObjectFlags InternalObjectFlags))"};

                const auto& lua_object = lua.get_userdata<SelfType>();
                auto* object = lua_object.get_remote_cpp_object();

                if (!lua.is_integer())
                {
                    lua.throw_error(error_overload_not_found);
                }

                Unreal::EInternalObjectFlags object_internal_flags = static_cast<Unreal::EInternalObjectFlags>(lua.get_integer());
                if (!object)
                {
                    lua.set_bool(false);
                    return 1;
                }
                lua.set_bool(object->HasAnyInternalFlags(object_internal_flags));
                return 1;
            });

            table.add_pair("ProcessConsoleExec", [](const LuaMadeSimple::Lua& lua) -> int {
                std::string error_overload_not_found{R"(
No overload found for function 'UObject.ProcessConsoleExec'.
Overloads:
#1: ProcessConsoleExec(string Cmd, nil Reserved, UObject Executor))"};

                const auto& lua_object = lua.get_userdata<SelfType>();

                if (!lua.is_string())
                {
                    lua.throw_error(error_overload_not_found);
                }
#if defined(__PROSPERO__)
                // [LFB-M] agent-FW cleanup round (2026-10-10; FL inventory §3,
                // was the unrouted [FM]/root item): the console command is a
                // Lua string -> engine wide-text forward conversion and now
                // goes through the ONE hand converter. The stack-state Bridge
                // replicates get_string() EXACTLY (lua_tolstring + lua_remove
                // at idx 1), so the sequential parsing below is unchanged.
                // Logs the fstring-fwd family (first-10); a refused input
                // yields the empty command after the loud first-10 '[FN] seam
                // refused:' bridge-forward notice — never ensure_str's silent
                // wide-libc conversion.
                const LuaFNameBridge::Bridge cmd_bridge{lua.get_lua_state()};
                LuaFNameBridge::log_family_marker(LuaFNameBridge::Family::FStringFwd, cmd_bridge.utf8(), cmd_bridge.input_len(), cmd_bridge.ok());
#else
                auto cmd = ensure_str(lua.get_string());
#endif

                if (lua.get_stack_size() < 2)
                {
                    lua.throw_error(error_overload_not_found);
                }
                lua.discard_value();

                if (!lua.is_userdata())
                {
                    lua.throw_error(error_overload_not_found);
                }
                auto executor = lua.get_userdata<LuaType::UObject>();

                auto ar = Unreal::FOutputDevice{};
                auto return_value =
                        lua_object.get_remote_cpp_object()->ProcessConsoleExec(
#if defined(__PROSPERO__)
                                FromCharTypePtr<Unreal::TCHAR>(cmd_bridge.units()), ar, executor.get_remote_cpp_object());
#else
                                FromCharTypePtr<Unreal::TCHAR>(cmd.c_str()), ar, executor.get_remote_cpp_object());
#endif

                lua.set_bool(return_value);
                return 1;
            });

            table.add_pair("IsValid", [](const LuaMadeSimple::Lua& lua) -> int {
                const auto& lua_object = lua.get_userdata<SelfType>();
                if (lua_object.get_remote_cpp_object() && lua_object.get_remote_cpp_object() != LuaMadeSimple::Type::special_invalid_ptr() &&
                    is_object_in_global_unreal_object_map(lua_object.get_remote_cpp_object()) && !lua_object.get_remote_cpp_object()->IsUnreachable())
                {
                    lua.set_bool(true);
                }
                else
                {
                    lua.set_bool(false);
                }
                return 1;
            });

            // Add things that are intended to be overridden later here
            // These will then be set only if this is the final object (nothing overrides it later)
            if constexpr (is_final == LuaMadeSimple::Type::IsFinal::Yes)
            {
                table.add_pair("type", [](const LuaMadeSimple::Lua& lua) -> int {
                    lua.set_string("UObject");
                    return 1;
                });

                // If this is the final object then we also want to finalize creating the table
                // If not then it's the responsibility of the overriding object to call 'make_global()
                // table.make_global("UObject");
            }
        }

      private:
        auto static prepare_to_handle(const Operation operation, const LuaMadeSimple::Lua& lua) -> void
        {
            auto& lua_object = lua.get_userdata<SelfType>();
            auto* object = lua_object.get_remote_cpp_object();

            // [LFB] Claude item 2 (__index/__newindex member-name lookup,
            // properties AND UFunctions): the ONE hand converter owns the
            // member-name string (lua_tolstring + hand UTF-8->UTF-16), then
            // the slot-2 native ctor DIRECT (was ensure_str_const +
            // FName(StringType, FNAME_Find)). The key sits at index 1 after
            // get_userdata consumed the receiver. The custom-property-list
            // lookup still needs the engine StringType; bridge.string()
            // fills it from the SAME hand-converted units ((ptr,count),
            // stride-correct — not a std::wstring conversion).
            const uint64_t r9_t0 = __builtin_readcyclecounter();
            r9prop::add(r9prop::calls, 1);
            const LuaFNameBridge::Bridge member{lua.get_lua_state()};
            const uint64_t r9_t1 = __builtin_readcyclecounter();
            r9prop::add(r9prop::t_bridge, r9_t1 - r9_t0);

            // If nullptr then we assume the UObject wasn't found so lets return an invalid UObject to Lua
            // This allows the safe chaining of "__index" as long as the Lua script checks ":IsValid()" before using the object
            if (!object)
            {
                // If the operation is not "Get" then this isn't "__index" and we want to do nothing in this case
                switch (operation)
                {
                case Operation::Get:
                case Operation::GetParam:
                    // Construct an empty object to allow for safe chaining with a validity check at the end
                    SelfType::construct(lua, static_cast<DerivedType*>(nullptr));
                    break;
                case Operation::Set:
                    Output::send(STR("[Lua][Error] Tried setting member variable '{}' but UObject instance is nullptr\n"), member.view());
                    break;
                default:
                    Output::send(STR("[Lua][Error] The UObject instance is nullptr & operation type was invalid\n"));
                    break;
                }

                return;
            }

            // l26 postmortem revert (root): FNAME_Add for MEMBER names
            // mutated the FName pool from inside the save-load GC storm
            // (thousands of listener-driven Lua member accesses; calls=13970
            // in the crash log) — GATE-1-family crash. Member-name lookup
            // returns to upstream FNAME_Find on ALL platforms; the loud
            // NAME_None gate ([UF] nofn [n:0]) below reports misses instead
            // of inserting. FindXOf keeps its separate Find-first fix
            // (UObjectGlobals.cpp, rare call sites, no GC-storm volume).
            // The bridge keeps that law: FNAME_Find, slot-2 ctor direct.
            Unreal::FName property_name = member.to_fname(Unreal::FNAME_Find);
            const uint64_t r9_t2 = __builtin_readcyclecounter();
            r9prop::add(r9prop::t_fname, r9_t2 - r9_t1);
            Unreal::FField* field = LuaCustomProperty::StaticStorage::property_list.find_or_nullptr(object, member.string());
            const uint64_t r9_t3 = __builtin_readcyclecounter();
            r9prop::add(r9prop::t_custom, r9_t3 - r9_t2);

            if (!field)
            {
                Unreal::UStruct* obj_as_struct = nullptr;
                bool r9_cacheable = false;
                if constexpr (std::is_base_of_v<Unreal::UStruct, DerivedType>)
                {
                    // Avoid runtime Cast/IsA here; this path is hit in __index for hot code paths.
                    obj_as_struct = static_cast<Unreal::UStruct*>(object);
                }
                if (!obj_as_struct)
                {
                    Unreal::UClass* r9_class = object->GetClassPrivate();
                    obj_as_struct = r9_class;
                    r9_cacheable = r9_class != nullptr && property_name != Unreal::NAME_None && r9prop::is_native_class(r9_class);
                }
                const uint64_t r9_name = r9prop::key_of(property_name);
                if (r9_cacheable && r9prop::find(obj_as_struct, r9_name, field))
                {
                    r9prop::add(r9prop::hits, 1);
                }
                else
                {
                    field = obj_as_struct->FindProperty(property_name);
                    if (r9_cacheable) { r9prop::store(obj_as_struct, r9_name, field); }
                }
            }
            const uint64_t r9_t4 = __builtin_readcyclecounter();
            r9prop::add(r9prop::t_find, r9_t4 - r9_t3);

            handle_unreal_property_value(operation, lua, object, property_name, field);
            r9prop::add(r9prop::t_handle, __builtin_readcyclecounter() - r9_t4);
        }
    };

    template <typename ParamType>
    class LocalUnrealParam : public LuaMadeSimple::Type::LocalObject<ParamType>
    {
      private:
        Unreal::FProperty* m_property{};
        Unreal::UObject* m_base{};

      public:
        LocalUnrealParam(ParamType object, Unreal::UObject* base, Unreal::FProperty* property)
            : LuaMadeSimple::Type::LocalObject<ParamType>("LocalParam", std::move(object)), m_property(property), m_base(base)
        {
        }

        // Constructor for local param
        auto static construct(const LuaMadeSimple::Lua& lua, ParamType& object, Unreal::UObject* base, Unreal::FProperty* property) -> const LuaMadeSimple::Lua::Table
        {
            LuaType::LocalUnrealParam lua_object{object, base, property};

            auto metatable_name = "LocalUnrealParam";

            LuaMadeSimple::Lua::Table table = lua.get_metatable(metatable_name);
            if (lua.is_nil(-1))
            {
                lua.discard_value(-1);
                lua.prepare_new_table();
                setup_metamethods(lua_object);
                setup_member_functions(table);
                lua.new_metatable<LuaType::LocalUnrealParam<ParamType>>(metatable_name, lua_object.get_metamethods());
            }

            // Create object & surrender ownership to Lua
            lua.transfer_stack_object(std::move(lua_object), metatable_name, lua_object.get_metamethods());

            return table;
        }

      private:
        auto static setup_metamethods(LuaMadeSimple::Type::BaseObject& base_object) -> void
        {
        }

        auto static setup_member_functions(LuaMadeSimple::Lua::Table& table) -> void
        {
            table.add_pair("Get", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Get, lua);
                return 1;
            });

            table.add_pair("get", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Get, lua);
                return 1;
            });

            table.add_pair("Set", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Set, lua);
                return 0;
            });

            table.add_pair("set", [](const LuaMadeSimple::Lua& lua) -> int {
                prepare_to_handle(Operation::Set, lua);
                return 0;
            });

            // This class is not overridable so this is always the final class and because of that we always want to set the type & make it global
            table.add_pair("type", [](const LuaMadeSimple::Lua& lua) -> int {
                lua.set_string("LocalUnrealParam");
                return 1;
            });

            // If this is the final object then we also want to finalize creating the table
            // If not then it's the responsibility of the overriding object to call 'make_global()'
            // table.make_global("LocalUnrealParam");
        }

        auto static prepare_to_handle(const Operation operation, const LuaMadeSimple::Lua& lua) -> void
        {
            auto& lua_object = lua.get_userdata<LuaType::LocalUnrealParam<ParamType>>();

            Unreal::FName property_type = lua_object.m_property->GetClass().GetFName();
            int32_t type_name_comparison_index = property_type.GetComparisonIndex();

            if (StaticState::m_property_value_pushers.contains(type_name_comparison_index))
            {
                const PusherParams pusher_params{.operation = operation,
                                                 .lua = lua,
                                                 .base = lua_object.m_base,
                                                 .data = lua_object.get_local_cpp_object().get_data_ptr(),
                                                 .property = lua_object.m_property};
                StaticState::m_property_value_pushers[type_name_comparison_index](pusher_params);
            }
            else
            {
                // We can either throw an error and kill the execution
                /**/
                StringType property_type_name = property_type.ToString();
                lua.throw_error(fmt::format(
                        "[LocalUnrealParam::prepare_to_handle] Tried accessing unreal property without a registered handler. Property type '{}' not supported.",
                        to_string(property_type_name)));
                //*/

                // Or we can treat unhandled property types as some sort of generic type
                // For example, we might want to pass the unhandled property value to some sort of low-level Lua API
                // Then the Lua script can deal with figuring out how to handle the type
                /*
                push_unhandledproperty(lua, base);
                //*/
            }
        }
    };

    class RemoteUnrealParam : public LuaMadeSimple::Type::RemoteObject<void>
    {
      private:
        Unreal::FProperty* m_property{};
        Unreal::UObject* m_base{};
        Unreal::FName m_type{};

      public:
        explicit RemoteUnrealParam(void* param_ptr, Unreal::UObject* base, Unreal::FProperty* param_property_type_name, const Unreal::FName type);
        explicit RemoteUnrealParam(void* data_ptr, const Unreal::FName type);

        // Constructor for param
        auto static construct(const LuaMadeSimple::Lua&, void* param_ptr, Unreal::UObject* base, Unreal::FProperty* property) -> const LuaMadeSimple::Lua::Table;
        auto static construct(const LuaMadeSimple::Lua&, void* data_ptr, const Unreal::FName type) -> const LuaMadeSimple::Lua::Table;

      private:
        auto static setup_metamethods(BaseObject&) -> void;
        auto static setup_member_functions(LuaMadeSimple::Lua::Table& table) -> void;
        auto static prepare_to_handle(const Operation, const LuaMadeSimple::Lua&) -> void;
    };

    template <IsConvertableToLuaInteger IntegerType>
    auto push_integer(const PusherParams& params) -> void
    {
        IntegerType* integer_ptr = static_cast<IntegerType*>(params.data);
        if (!integer_ptr)
        {
            params.throw_error("push_integer", "data pointer is nullptr");
        }

        switch (params.operation)
        {
        case Operation::GetNonTrivialLocal:
        case Operation::Get:
            params.lua.set_integer(*integer_ptr);
            return;
        case Operation::Set:
            *integer_ptr = static_cast<IntegerType>(params.lua.get_integer(params.stored_at_index));
            return;
        case Operation::GetParam:
            RemoteUnrealParam::construct(params.lua, integer_ptr, params.base, params.property);
            return;
        default:
            params.throw_error("push_integer", "Unhandled Operation");
            break;
        }

        params.throw_error("push_integer", "Operation not supported");
    }
} // namespace RC::LuaType
