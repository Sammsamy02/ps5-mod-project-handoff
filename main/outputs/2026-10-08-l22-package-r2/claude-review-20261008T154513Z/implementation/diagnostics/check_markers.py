from pathlib import Path
import json,sys
O=Path(__file__).resolve().parent
m=(O/'src/LuaMod.cpp').read_text();u=(O/'src/LuaUObject.cpp').read_text();h=(O/'include/LuaType/LuaUObject.hpp').read_text()
checks={
 'Lua mod started only after successful main script':'Lua mod started' in m and 'load_and_execute_script(main_script_path)' in m,
 'RegisterKeyBind result/key/modifier marker':'RegisterKeyBind returned key=' in m and 'RegisterKeyBind modifier[' in m,
 'keybind callback entry marker':'keybind callback entry key=' in m,
 'queue marker':'ExecuteInGameThread queued' in m,
 'drain marker':'ExecuteInGameThread drained' in m,
 'widget construction marker':'widget/object constructed' in m,
 'viewport returned marker':'AddToViewport returned' in u,
 'viewport marker after ProcessEvent':('AddToViewport returned' in u and u.index('AddToViewport returned')>u.index('calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);')),
 'viewport identity observed from existing Lua member lookup':'record_member_lookup(member_name' in h and 'record_function_lookup(func' in u,
}
print('\n'.join(('PASS' if v else 'FAIL')+': '+k for k,v in checks.items()))
(O/'tests'/('markers-GREEN.json' if all(checks.values()) else 'markers-RED.json')).write_text(json.dumps(checks,indent=2)+'\n')
sys.exit(not all(checks.values()))
