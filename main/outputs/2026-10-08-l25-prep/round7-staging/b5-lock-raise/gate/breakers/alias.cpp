#include <mutex>
using Guard = std::lock_guard<std::recursive_mutex>;
void f(Lua& lua){ Guard g{m}; lua.throw_error("x"); }
