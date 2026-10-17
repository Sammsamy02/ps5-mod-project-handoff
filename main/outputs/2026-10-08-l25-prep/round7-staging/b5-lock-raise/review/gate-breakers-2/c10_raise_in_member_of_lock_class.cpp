struct S { std::recursive_mutex m; void f(Lua& lua){ m.lock(); lua.throw_error("x"); m.unlock(); } };

void ctl_ok(Lua& l){ std::lock_guard<std::mutex> g2{mm}; }
void ctl_raise(Lua& l){ l.throw_error("y"); }
