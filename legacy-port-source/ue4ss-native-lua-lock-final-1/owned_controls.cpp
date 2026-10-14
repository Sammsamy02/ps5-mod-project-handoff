#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
static lua_State* create() {auto*L=luaL_newstate();assert(L);return L;}
static void value(lua_State*L){assert(luaL_loadstring(L,"return 6 * 7")==LUA_OK);assert(lua_pcall(L,0,1,0)==LUA_OK);assert(lua_tointeger(L,-1)==42);lua_pop(L,1);}
struct Allocator {size_t calls{},fail_at{},live{};};
static void* allocation(void*ud,void*ptr,size_t,size_t size){auto&a=*static_cast<Allocator*>(ud);if(!size){if(ptr)--a.live;free(ptr);return nullptr;}if(++a.calls>=a.fail_at)return nullptr;auto*p=realloc(ptr,size);if(p&&!ptr)++a.live;return p;}
int main(){
 // Retained baseline host-3 blocks at the following real sequence: one
 // thread finishes lua_close, another enters luaL_newstate's LuaLock.
 auto*L=create();value(L);std::thread close([&]{lua_close(L);});close.join();
 std::thread next([]{auto*N=create();value(N);lua_close(N);});next.join();
 // Independent state remains usable while another is closed. Coroutine
 // creation shares initialization; reset/GC and close(child) use real API.
 auto*A=create();auto*B=create();auto*C=lua_newthread(A);int child_ref=luaL_ref(A,LUA_REGISTRYINDEX);value(C);assert(lua_resetthread(C)==LUA_OK);luaL_unref(A,LUA_REGISTRYINDEX,child_ref);lua_gc(A,LUA_GCCOLLECT);C=lua_newthread(A);std::thread close_child([&]{lua_close(C);});close_child.join();value(B);lua_close(B);
 // Initial allocation or partially constructed state failures must not
 // create a close-entry unlock that never had a matching acquisition.
 for(size_t fail_at:{size_t(1),size_t(2),size_t(3)}){Allocator a{};a.fail_at=fail_at;auto*F=lua_newstate(allocation,&a);assert(!F&&a.live==0);std::thread usable([]{auto*N=create();value(N);lua_close(N);});usable.join();}
 // Final releases exactly its own close entry, preserving a genuine outer
 // recursive acquisition. It does not force-unlock the global mutex.
 A=create();B=create();LuaLock(B);lua_close(A);std::promise<void>Entered,Acquired;auto entered=Entered.get_future(),acquired=Acquired.get_future();
 std::thread waiter([&]{Entered.set_value();LuaLock(B);Acquired.set_value();LuaUnlock(B);});entered.wait();assert(acquired.wait_for(std::chrono::milliseconds(40))==std::future_status::timeout);LuaUnlock(B);assert(acquired.wait_for(std::chrono::seconds(2))==std::future_status::ready);waiter.join();value(B);lua_close(B);
 printf("PASS actual LuaRaw cross-thread close/reopen Lua42, overlapping states, coroutine create/reset/GC/close, partial allocator failures, exact recursive entry balance\n");
}
