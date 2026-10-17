void f(Lua& lua){ std::lock_guard<std::recursive_mutex> g{LuaMod::m_thread_actions_mutex}; try { work(); } catch (std::exception& e) { lua.throw_error(e.what()); } }

void ctl_ok(Lua& l){ std::lock_guard<std::mutex> g2{mm}; }
void ctl_raise(Lua& l){ l.throw_error("y"); }
