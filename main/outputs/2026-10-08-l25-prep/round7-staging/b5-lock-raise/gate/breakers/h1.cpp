struct Holder {
    std::unique_lock<std::recursive_mutex> g;
    Holder() : g(m) {}
};
void f(lua_State* L){ Holder h; luaL_error(L,"x"); }
