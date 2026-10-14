typedef std::unique_lock<std::mutex> UL;
void f(Lua& lua){ UL g(m); luaL_error(L,"x"); }
