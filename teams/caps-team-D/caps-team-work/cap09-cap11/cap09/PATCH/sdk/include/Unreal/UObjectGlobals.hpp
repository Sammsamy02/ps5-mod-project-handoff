// Unreal/UObjectGlobals.hpp -- PS5 Mod Bridge native mod SDK (subset).
// FindFirstOf / FindAllOf / StaticFindObject / RegisterHook / UnregisterHook
// with UE4SS spellings. RegisterHook goes through the host's hook admission:
// on this platform it is a per-UFunction function-pointer swap (data-only).
#pragma once
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>

#include <utility>
#include <vector>

namespace RC::Unreal::UObjectGlobals
{
    inline auto FindFirstOf(StringViewType class_short_name) -> UObject*
    {
        auto* h = Sdk::host();
        mbm_object out{};
        const auto n = Sdk::to_utf8(class_short_name);
        if (!h || h->find_first_of(h->ctx, Sdk::token(), n.c_str(), &out) != MBM_OK) return nullptr;
        return static_cast<UObject*>(out);
    }

    inline auto FindAllOf(StringViewType class_short_name, std::vector<UObject*>& out_objects) -> void
    {
        auto* h = Sdk::host();
        if (!h) return;
        const auto n = Sdk::to_utf8(class_short_name);
        int32_t total = 0;
        if (h->find_all_of(h->ctx, Sdk::token(), n.c_str(), nullptr, 0, &total) != MBM_OK || total <= 0) return;
        std::vector<mbm_object> buf(static_cast<size_t>(total));
        if (h->find_all_of(h->ctx, Sdk::token(), n.c_str(), buf.data(), total, &total) != MBM_OK) return;
        for (int32_t i = 0; i < total && i < static_cast<int32_t>(buf.size()); ++i) out_objects.push_back(static_cast<UObject*>(buf[static_cast<size_t>(i)]));
    }

    template <typename ObjectType = UObject*>
    inline auto StaticFindObject(UClass* /*ObjectClass*/, UObject* /*InObjectPackage*/, StringViewType OrigInName, bool /*bExactClass*/ = false) -> ObjectType
    {
        auto* h = Sdk::host();
        mbm_object out{};
        const auto n = Sdk::to_utf8(OrigInName);
        if (!h || h->find_object(h->ctx, Sdk::token(), n.c_str(), &out) != MBM_OK) return nullptr;
        return static_cast<ObjectType>(static_cast<UObject*>(out));
    }

    namespace Detail
    {
        struct HookEntry
        {
            UnrealScriptFunctionCallable pre;
            UnrealScriptFunctionCallable post;
            void* custom{};
        };
        auto hook_store() -> Sdk::CallbackStore<HookEntry>&;

        inline auto trampoline(void* user, const mbm_hook_call* call, bool pre) -> mbm_status
        {
            auto* e = static_cast<HookEntry*>(user);
            return Sdk::contain([&] {
                UnrealScriptFunctionCallableContext ctx;
                ctx.Context = static_cast<UObject*>(call->context);
                ctx.Function = static_cast<UFunction*>(call->function);
                ctx.Params = call->params;
                ctx.RESULT_DECL = call->return_value;
                auto& fn = pre ? e->pre : e->post;
                if (fn) fn(ctx, e->custom);
            });
        }
        inline auto pre_tramp(void* user, const mbm_hook_call* call) -> mbm_status { return trampoline(user, call, true); }
        inline auto post_tramp(void* user, const mbm_hook_call* call) -> mbm_status { return trampoline(user, call, false); }
    } // namespace Detail

    // Returns {id, id} on success (one host id covers pre+post), {-1,-1} when
    // refused (the host log line says why).
    inline auto RegisterHook(StringViewType FunctionFullNameNoType,
                             const UnrealScriptFunctionCallable& PreCallback,
                             const UnrealScriptFunctionCallable& PostCallback,
                             void* CustomData) -> std::pair<int, int>
    {
        auto* h = Sdk::host();
        if (!h) return {-1, -1};
        auto* entry = Detail::hook_store().keep(Detail::HookEntry{PreCallback, PostCallback, CustomData});
        uint64_t id{};
        const auto n = Sdk::to_utf8(FunctionFullNameNoType);
        const auto s = h->register_ufunction_hook(h->ctx, Sdk::token(), n.c_str(), PreCallback ? &Detail::pre_tramp : nullptr,
                                                  PostCallback ? &Detail::post_tramp : nullptr, entry, &id);
        if (s != MBM_OK) return {-1, -1};
        return {static_cast<int>(id), static_cast<int>(id)};
    }

    inline auto UnregisterHook(StringViewType /*FunctionFullNameNoType*/, std::pair<int, int> Ids) -> void
    {
        auto* h = Sdk::host();
        if (h && Ids.first > 0) h->unregister(h->ctx, Sdk::token(), static_cast<uint64_t>(Ids.first));
    }
} // namespace RC::Unreal::UObjectGlobals
