#include <LuaLibrary.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <cstdint>
#include <limits>

namespace RC::LuaLibrary
{
    auto deref_to_int32(const LuaMadeSimple::Lua& lua) -> int
    {
        if (lua.get_stack_size() != 1 || !lua.is_integer())
        {
            lua.set_nil();
            return 1;
        }

        const int64_t argument = lua.get_integer();
        constexpr auto width = sizeof(int32_t);
        if (argument <= 0 || static_cast<uint64_t>(argument) >
            static_cast<uint64_t>(std::numeric_limits<uintptr_t>::max()) - (width - 1))
        {
            lua.set_nil();
            return 1;
        }

        int32_t value{};
        bool complete = false;
        try
        {
            // This scope authorizes only our destination. The argument grants
            // no permission: its source must already have an owned registration.
            PLH::OwnedMemory::ScopedRegion destination(&value, width, PLH::W);
            size_t count = 0;
            const PLH::MemAccessor accessor;
            complete = accessor.safe_mem_read(static_cast<uint64_t>(argument),
                reinterpret_cast<uintptr_t>(&value), width, count) && count == width;
        }
        catch (...)
        {
            // Registration/resource failure is an unreadable request.
            complete = false;
        }

        // Lua pushes may longjmp on allocation failure. Revoke the stack
        // registration before touching the Lua return stack.
        if (complete) lua.set_integer(value);
        else lua.set_nil();
        return 1;
    }
}
