struct Holder {
    Holder() { m.lock(); }
    ~Holder() { m.unlock(); }
};
void f(lua_State* L){ Holder h; luaL_error(L,"x"); }
