void f(Lua& lua){ std::unique_lock<std::recursive_mutex> g; g = std::unique_lock<std::recursive_mutex>(LuaMod::m_thread_actions_mutex); lua.throw_error("x"); }

void ctl_ok(Lua& l){ std::lock_guard<std::mutex> g2{mm}; }
void ctl_raise(Lua& l){ l.throw_error("y"); }
