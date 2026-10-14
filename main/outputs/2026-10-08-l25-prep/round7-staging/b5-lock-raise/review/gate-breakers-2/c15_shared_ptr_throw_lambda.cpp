void f(Lua& lua){ std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex}; auto r = [&]{ lua.throw_error("x"); }; r(); }

void ctl_ok(Lua& l){ std::lock_guard<std::mutex> g2{mm}; }
void ctl_raise(Lua& l){ l.throw_error("y"); }
