void f(Lua& lua, std::function<void()> cb){ std::lock_guard<std::mutex> g{m}; cb(); }
void h(Lua& lua){ f(lua, [&]{ lua.throw_error("x"); }); }
