#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <cstdio>
int main(int n,char** a){if(n!=4)return 2;auto* L=luaL_newstate();if(!L)return 3;luaL_openlibs(L);lua_pushstring(L,a[2]);lua_setglobal(L,"ORIGINAL_MOD_PATH");lua_pushstring(L,a[3]);lua_setglobal(L,"CASE");int rc=luaL_dofile(L,a[1]);if(rc)std::fprintf(stderr,"%s\n",lua_tostring(L,-1));lua_close(L);return rc?1:0;}
