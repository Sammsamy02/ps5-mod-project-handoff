#ifdef _WIN32
#include <DynamicOutput/Output.hpp>
#include <Helpers/Casting.hpp>
#include <LuaLibrary.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <Windows.h>

namespace RC::LuaLibrary
{
    auto deref_to_int32(const LuaMadeSimple::Lua& lua) -> int
    {
        if (lua.get_stack_size() != 1 || !lua.is_integer())
        {
            Output::send(STR("[Fatal] Lua function 'DerefToInt32' must have only 1 parameter and it must be of type 'int'.\n"));
            lua.set_nil();
            return 1;
        }

        int32_t* int32_ptr = reinterpret_cast<int32_t*>(lua.get_integer());
        int32_t int32_val = Helper::Casting::offset_deref_safe<int32_t>(int32_ptr, 0, GetCurrentProcess());

        if (int32_val == 0)
        {
            Output::send(STR("[Fatal] Address passed to Lua function 'DerefToInt32' was not a valid pointer.\n"));
            lua.set_nil();
            return 1;
        }

        lua.set_integer(int32_val);
        return 1;
    }

}
#endif
