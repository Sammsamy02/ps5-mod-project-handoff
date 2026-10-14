struct Hold { Hold(){ LuaMod::m_thread_actions_mutex.lock(); } ~Hold(){ LuaMod::m_thread_actions_mutex.unlock(); } };
void f(Lua& lua){ Hold h; lua.throw_error("x"); }

void ctl_ok(Lua& l){ std::lock_guard<std::mutex> g2{mm}; }
void ctl_raise(Lua& l){ l.throw_error("y"); }
