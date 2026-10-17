// Unreal/UObject.hpp -- PS5 Mod Bridge native mod SDK (source-compatible subset).
//
// RC::Unreal::UObject* values are engine object pointers handed out by the
// host. The SDK class has NO data members and NO virtuals: every member
// function passes `this` back to the host through the C ABI and never reads
// the object's memory itself. That keeps the mod independent of the engine's
// object layout (the host resolves everything by reflection at run time).
//
// Supported (same names as UE4SS): GetFullName, GetName, GetClassPrivate,
// IsA<T>/IsA(UClass*), IsValid-style checks via UObjectGlobals,
// GetValuePtrByPropertyName(InChain)<T> for non-bitfield scalars/objects,
// plus SDK helpers Get/Set/Call. Anything else does not compile (by design:
// a missing API is a compile error, never a runtime surprise).
#pragma once
#include <mbm/Sdk.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

namespace RC::Unreal
{
    class UObject;
    class UClass;
    class UFunction;
    class AActor;

    namespace Detail
    {
        template <typename T> struct ValueType { static constexpr uint32_t value = MBM_VT_NONE; };
        template <> struct ValueType<bool> { static constexpr uint32_t value = MBM_VT_BOOL; };
        template <> struct ValueType<uint8_t> { static constexpr uint32_t value = MBM_VT_U8; };
        template <> struct ValueType<int32_t> { static constexpr uint32_t value = MBM_VT_I32; };
        template <> struct ValueType<int64_t> { static constexpr uint32_t value = MBM_VT_I64; };
        template <> struct ValueType<float> { static constexpr uint32_t value = MBM_VT_F32; };
        template <> struct ValueType<double> { static constexpr uint32_t value = MBM_VT_F64; };
        template <typename T> struct ValueType<T*> { static constexpr uint32_t value = std::is_base_of_v<UObject, T> ? MBM_VT_OBJECT : MBM_VT_NONE; };

        inline auto make(uint32_t type) -> mbm_value
        {
            mbm_value v{};
            v.type = type;
            return v;
        }
    } // namespace Detail

    // A value passed to / returned from UObject::Call.
    struct Value
    {
        mbm_value raw{};
        std::string utf8; // backing store for NAME/STRING
        static auto Bool(bool b) -> Value { Value v; v.raw.type = MBM_VT_BOOL; v.raw.v.b = b ? 1 : 0; return v; }
        static auto Int(int32_t i) -> Value { Value v; v.raw.type = MBM_VT_I32; v.raw.v.i32 = i; return v; }
        static auto Int64(int64_t i) -> Value { Value v; v.raw.type = MBM_VT_I64; v.raw.v.i64 = i; return v; }
        static auto Float(float f) -> Value { Value v; v.raw.type = MBM_VT_F32; v.raw.v.f32 = f; return v; }
        static auto Double(double d) -> Value { Value v; v.raw.type = MBM_VT_F64; v.raw.v.f64 = d; return v; }
        static auto Object(UObject* o) -> Value { Value v; v.raw.type = MBM_VT_OBJECT; v.raw.v.object = o; return v; }
        static auto Name(StringViewType s) -> Value { Value v; v.raw.type = MBM_VT_NAME; v.utf8 = Sdk::to_utf8(s); return v; }
        static auto String(StringViewType s) -> Value { Value v; v.raw.type = MBM_VT_STRING; v.utf8 = Sdk::to_utf8(s); return v; }
    };

    class UObject
    {
      public:
        UObject() = delete; // engine-owned; never constructed by a mod
        UObject(const UObject&) = delete;

        auto raw() const -> mbm_object { return const_cast<void*>(static_cast<const void*>(this)); }

        auto GetFullName() const -> StringType { return name(true); }
        auto GetName() const -> StringType { return name(false); }

        auto GetClassPrivate() const -> UClass*
        {
            mbm_object out{};
            auto* h = Sdk::host();
            if (!h || h->object_class(h->ctx, Sdk::token(), raw(), &out) != MBM_OK) return nullptr;
            return static_cast<UClass*>(out);
        }

        // IsA by class short name (e.g. STR("PlayerController")).
        auto IsA(StringViewType class_short_name) const -> bool
        {
            int32_t yes = 0;
            auto* h = Sdk::host();
            const auto n = Sdk::to_utf8(class_short_name);
            return h && h->object_is_a(h->ctx, Sdk::token(), raw(), n.c_str(), &yes) == MBM_OK && yes != 0;
        }

        // Typed pointer into the object's reflected property (searched through
        // the class chain). nullptr when missing, wrong type, or a bitfield bool.
        template <typename T>
        auto GetValuePtrByPropertyNameInChain(StringViewType property_name) -> T*
        {
            static_assert(Detail::ValueType<T>::value != MBM_VT_NONE || std::is_pointer_v<T>,
                          "unsupported property type for the PS5 native mod SDK (use Get/Set or file a gap)");
            void* out{};
            auto* h = Sdk::host();
            const auto n = Sdk::to_utf8(property_name);
            const uint32_t type = std::is_pointer_v<T> ? static_cast<uint32_t>(MBM_VT_OBJECT) : Detail::ValueType<T>::value;
            if (!h || h->property_address(h->ctx, Sdk::token(), raw(), n.c_str(), type, &out) != MBM_OK) return nullptr;
            return static_cast<T*>(out);
        }
        template <typename T>
        auto GetValuePtrByPropertyName(StringViewType property_name) -> T*
        {
            return GetValuePtrByPropertyNameInChain<T>(property_name);
        }

        // Copying accessors (also handle bitfield bools). std::nullopt on failure.
        template <typename T>
        auto Get(StringViewType property_name) const -> std::optional<T>
        {
            constexpr uint32_t type = Detail::ValueType<T>::value;
            static_assert(type != MBM_VT_NONE, "unsupported type");
            mbm_value v = Detail::make(type);
            auto* h = Sdk::host();
            const auto n = Sdk::to_utf8(property_name);
            if (!h || h->property_get(h->ctx, Sdk::token(), raw(), n.c_str(), &v) != MBM_OK) return std::nullopt;
            if constexpr (std::is_same_v<T, bool>) return v.v.b != 0;
            else if constexpr (std::is_same_v<T, uint8_t>) return v.v.u8;
            else if constexpr (std::is_same_v<T, int32_t>) return v.v.i32;
            else if constexpr (std::is_same_v<T, int64_t>) return v.v.i64;
            else if constexpr (std::is_same_v<T, float>) return v.v.f32;
            else if constexpr (std::is_same_v<T, double>) return v.v.f64;
            else return static_cast<T>(v.v.object);
        }

        template <typename T>
        auto Set(StringViewType property_name, T value) -> bool
        {
            constexpr uint32_t type = Detail::ValueType<T>::value;
            static_assert(type != MBM_VT_NONE, "unsupported type");
            mbm_value v = Detail::make(type);
            if constexpr (std::is_same_v<T, bool>) v.v.b = value ? 1 : 0;
            else if constexpr (std::is_same_v<T, uint8_t>) v.v.u8 = value;
            else if constexpr (std::is_same_v<T, int32_t>) v.v.i32 = value;
            else if constexpr (std::is_same_v<T, int64_t>) v.v.i64 = value;
            else if constexpr (std::is_same_v<T, float>) v.v.f32 = value;
            else if constexpr (std::is_same_v<T, double>) v.v.f64 = value;
            else v.v.object = value;
            auto* h = Sdk::host();
            const auto n = Sdk::to_utf8(property_name);
            return h && h->property_set(h->ctx, Sdk::token(), raw(), n.c_str(), &v) == MBM_OK;
        }

        // Call a reflected UFunction on this object (game thread only).
        // Returns false if the host refused (wrong thread, unsupported params).
        auto Call(StringViewType function_name, std::vector<Value> args = {}, Value* ret = nullptr) -> bool
        {
            std::vector<mbm_value> raw_args;
            raw_args.reserve(args.size());
            for (auto& a : args)
            {
                a.raw.utf8_in = a.utf8.empty() ? nullptr : a.utf8.c_str();
                raw_args.push_back(a.raw);
            }
            mbm_value r{};
            if (ret) r = ret->raw;
            std::string ret_buf;
            if (ret && (r.type == MBM_VT_NAME || r.type == MBM_VT_STRING))
            {
                ret_buf.assign(512, '\0');
                r.utf8_out = ret_buf.data();
                r.utf8_out_cap = static_cast<int32_t>(ret_buf.size());
            }
            auto* h = Sdk::host();
            const auto n = Sdk::to_utf8(function_name);
            const auto s = h ? h->call_function(h->ctx, Sdk::token(), raw(), n.c_str(), raw_args.data(), static_cast<int32_t>(raw_args.size()), ret ? &r : nullptr)
                             : MBM_E_UNAVAILABLE;
            if (s != MBM_OK) return false;
            if (ret)
            {
                ret->raw = r;
                if (r.utf8_out) { ret->utf8.assign(r.utf8_out, static_cast<size_t>(std::max(0, std::min(r.utf8_out_len, r.utf8_out_cap - 1)))); ret->raw.utf8_out = nullptr; }
            }
            return true;
        }

      private:
        auto name(bool full) const -> StringType
        {
            auto* h = Sdk::host();
            if (!h) return {};
            const auto s = Sdk::read_string([&](char* buf, int32_t cap, int32_t* len) {
                return h->object_name(h->ctx, Sdk::token(), raw(), full ? 1 : 0, buf, cap, len);
            });
            return Sdk::to_wide(s);
        }
    };

    class UStruct : public UObject {};
    class UClass : public UStruct {};
    class UFunction : public UStruct {};
    class AActor : public UObject {};
} // namespace RC::Unreal
