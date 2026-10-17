void f(lua_State* L){ std::lock_guard<std::mutex> g{m}; lua_gettable(L,1); lua_arith(L, LUA_OPADD); lua_concat(L,2);}
