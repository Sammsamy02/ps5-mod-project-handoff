void f(Lua& lua){ auto g = std::lock_guard(m); lua.throw_error("x"); }
