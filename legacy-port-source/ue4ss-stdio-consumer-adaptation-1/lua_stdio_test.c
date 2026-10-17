#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int run(lua_State *L, const char *s) {
    if (luaL_dostring(L, s) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    lua_State *L = luaL_newstate();
    if (!L) return 3;
    luaL_openlibs(L);
    lua_pushstring(L, argv[1]);
    lua_setglobal(L, "testdir");
    const char *files[] = {"plain.lua", "bom.lua", "shebang.lua", "bom-shebang.lua"};
    for (size_t i=0;i<sizeof(files)/sizeof(files[0]);++i) {
        char p[4096];
        snprintf(p,sizeof(p),"%s/%s",argv[1],files[i]);
        if (luaL_loadfilex(L,p,"t") != LUA_OK || lua_pcall(L,0,1,0) != LUA_OK ||
            lua_tointeger(L,-1) != 42) {
            fprintf(stderr,"loadfile fixture %s: %s\n",files[i],lua_tostring(L,-1));
            return 1;
        }
        lua_pop(L,1);
    }
    if (!run(L,
        "local f=assert(io.open(testdir..'/empty','rb')); assert(f:read('l')==nil);"
        "assert(f:read(0)==nil); assert(f:read('a')==''); assert(f:close());"
        "local f=assert(io.open(testdir..'/lines','rb'));"
        "assert(f:read('l')=='alpha'); assert(f:read('L')=='beta\\n');"
        "assert(f:read('l')==''); assert(f:read('l')=='last'); assert(f:read('l')==nil);"
        "assert(f:seek('set',0)==0); assert(f:read('l')=='alpha'); assert(f:close());"
        "local f=assert(io.open(testdir..'/numbers','rb'));"
        "assert(f:read('n')==12.5); assert(f:read('n')==-7); assert(f:read('n')==255);"
        "assert(f:read('n')==nil); assert(f:read('l')=='X'); assert(f:close());"
        "local f=assert(io.open(testdir..'/bytes','rb'));"
        "assert(f:read(1)==string.char(255)); assert(f:read(1)==string.char(0));"
        "assert(f:read(1)==string.char(128)); assert(f:read(1)==nil); assert(f:close());"
        "local f=assert(io.open(testdir..'/write-only','wb'));"
        "local v,err,code=f:read('l'); assert(v==nil and type(err)=='string' and type(code)=='number');"
        "assert(f:close());"
        "local v,err=loadfile(testdir..'/missing.lua'); assert(v==nil and type(err)=='string');"
        "local v,err=loadfile(testdir..'/incomplete-bom.lua'); assert(v==nil and type(err)=='string');"
        "local f=assert(io.open(testdir..'/long-line','rb'));"
        "assert(#f:read('l')==17000); assert(f:read('l')=='tail'); assert(f:close());"
    )) return 1;
    lua_close(L);
    puts("PASS actual Lua loadfile and io: BOM/shebang, EOF/clearerr, numeric lookahead/ungetc, bytes, long lines, read/open errors");
    return 0;
}
