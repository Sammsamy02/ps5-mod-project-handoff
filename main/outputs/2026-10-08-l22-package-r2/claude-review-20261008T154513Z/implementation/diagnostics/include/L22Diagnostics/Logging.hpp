#pragma once

#include <atomic>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>

namespace RC::L22Diagnostics
{
    // Diagnostics must never replace the original return value or exception.
    // No game call is made here; formatting/logger failures are observational.
    template <typename Callable>
    inline void observe(Callable&& callback) noexcept
    {
        try { callback(); } catch (...) {}
    }

    template <typename Message>
    inline void error(const char* context, const Message& message) noexcept
    {
        observe([&]() {
            Output::send<LogLevel::Error>(STR("[l22] {}: {}\n"), ensure_str(context), ensure_str(message));
        });
    }

    template <typename Lua, typename Message>
    inline void throw_lua_error(const Lua& lua, const Message& message, const char* context)
    {
        error(context, message);
        lua.throw_error(message);
    }

    // Reuse the member name/index already read by Lua property lookup. Do not
    // introduce FName construction, string conversion, or a game call to label progress.
    inline std::atomic<int> AddToViewportNameIndex{-1};
    inline std::atomic<void*> AddToViewportFunction{nullptr};

    template <typename Name>
    inline void record_member_lookup(const Name& name, int index) noexcept
    {
        if (name == STR("AddToViewport")) AddToViewportNameIndex.store(index, std::memory_order_relaxed);
    }

    inline void record_function_lookup(void* function, int index) noexcept
    {
        if (index == AddToViewportNameIndex.load(std::memory_order_relaxed))
            AddToViewportFunction.store(function, std::memory_order_relaxed);
    }

    inline bool is_add_to_viewport(void* function) noexcept
    {
        return function && function == AddToViewportFunction.load(std::memory_order_relaxed);
    }
}
