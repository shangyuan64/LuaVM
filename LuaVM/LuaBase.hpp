#pragma once

struct lua_State;
typedef void* (*lua_Alloc) (void* ud, void* ptr, size_t osize, size_t nsize);
typedef const char* (*lua_Reader) (lua_State* L, void* ud, size_t* sz);

typedef ptrdiff_t lua_KContext;
typedef int (*lua_KFunction) (lua_State* L, int status, lua_KContext ctx);

enum class LuaType
{
    None = -1, Nil, Boolean,
    LightUserdata, Number,
    String, Table, Function,
    Userdata, Thread
};

LUAVM_API LuaType Native2Type(int Type);

enum class LuaStatus
{
    Ok = 0, Yield, ErrorRun,
    ErrorSyntax, ErrorMemory, Error
};

LUAVM_API LuaStatus Native2Status(int Status);


using LuaInt = __int64;
using LuaNum = double;
using LuaStr = const char*;
using LuaBol = bool;
using LuaUdt = void*;

using LuaCFunc = int (*)(lua_State*);

template<typename Class>
using LuaMethod = int (Class::*)(lua_State*);

template<typename Class>
using LuaMethodConst = int (Class::*)(lua_State*) const;

namespace LuaC
{
    using tgettop = int(*)(lua_State* L);
    using tsettop = void(*)(lua_State* L, int idx);
    using tcopy = void(*)(lua_State* L, int fromidx, int toidx);
    using tpushvalue = void(*)(lua_State* L, int idx);
    using trotate = void(*)(lua_State* L, int idx, int n);
    using tremove = void(*)(lua_State* L, int idx);
    using tinsert = void(*)(lua_State* L, int idx);
    using tcheckstack = int(*)(lua_State* L, int n);

    using tpushnil = void(*)(lua_State* L);
    using tpushboolean = void(*)(lua_State* L, int b);
    using tpushinteger = void(*)(lua_State* L, LuaInt n);
    using tpushnumber = void(*)(lua_State* L, double n);
    using tpushlstring = void(*)(lua_State* L, const char* s, size_t len);
    using tpushcclosure = void(*)(lua_State* L, LuaCFunc f, int n);
    using tpushlightuserdata = void(*)(lua_State* L, void* p);
    using tlua_pushexternalstring = const char* (*)(lua_State* L, const char* s, size_t len, lua_Alloc falloc, void* ud);

    using tlua_isinteger = int(*)(lua_State* L, int idx);
    using ttoboolean = int(*)(lua_State* L, int idx);
    using ttointeger = LuaInt(*)(lua_State* L, int idx);
    using ttointegerx = LuaInt(*)(lua_State* L, int idx, int* isnum);
    using ttonumber = double(*)(lua_State* L, int idx);
    using ttonumberx = double(*)(lua_State* L, int idx, int* isnum);
    using ttolstring = const char* (*)(lua_State* L, int idx, size_t* pLen);
    using tocfunction = LuaCFunc(*)(lua_State* L, int idx);
    using ttouserdata = void* (*)(lua_State* L, int idx);

    using tgettable = int(*)(lua_State* L, int idx);
    using tsettable = void(*)(lua_State* L, int idx);
    using tgetfield = int(*)(lua_State* L, int idx, const char* k);
    using tsetfield = void(*)(lua_State* L, int idx, const char* k);
    using trawget = int(*)(lua_State* L, int idx);
    using trawset = void(*)(lua_State* L, int idx);
    // 第三个参数存在历史遗留问题，所以使用替代方案
    //using trawgeti = void(*)(lua_State* L, int idx, LuaInt n);
    //using trawseti = void(*)(lua_State* L, int idx, LuaInt n);
    using tcreatetable = void(*)(lua_State* L, int narr, int nrec);
    using tnewuserdatauv = void* (*)(lua_State* L, size_t sz, int nuvalue);
    using tgetglobal = int(*)(lua_State* L, const char* name);
    using tsetglobal = void(*)(lua_State* L, const char* name);
    using tlua_getmetatable = int(*)(lua_State* L, int idx);
    using tlua_setmetatable = int(*)(lua_State* L, int idx);

    using tlua_load = int(*)(lua_State* L, lua_Reader reader, void* data, const char* chunkname, const char* mode);
    using tlua_call = void(*)(lua_State* L, int nargs, int nresults);
    using tlua_callk = void(*)(lua_State* L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k);
    using tlua_pcall = int(*)(lua_State* L, int nargs, int nresults, int errfunc);
    using tlua_pcallk = int(*)(lua_State* L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
    using tlua_error = int(*)(lua_State* L);
    using tlua_typeerror = int(*)(lua_State* L, int idx, const char* tname);
    using tlua_argumenterror = int(*)(lua_State* L, int arg, const char* extramsg);

    using tlua_newstate = lua_State * (*)(lua_Alloc f, void* ud, unsigned seed);
    using tlua_close = void(*)(lua_State* L);
    //using tlua_gc = int(*)(lua_State* L, int what, int data);
    using tlua_type = int(*)(lua_State* L, int idx);

    struct Info
    {
        int GlobalTableIndex; // 可选：优先使用get/setglobal
        int RegistryTableIndex;

        tgettop gettop;
        tsettop settop;
        tcopy copy;
        tpushvalue pushvalue;
        struct {
            trotate rotate; /*opt1*/

            /*opt2*/
            tremove remove;
            tinsert insert;
        };
        tcheckstack checkstack;

        tpushnil pushnil;
        tpushboolean pushboolean;
        tpushinteger pushinteger;
        tpushnumber pushnumber;
        tpushlstring pushlstring;
        tpushcclosure pushcclosure;
        tpushlightuserdata pushlightuserdata;
        tlua_pushexternalstring pushexternalstring; /*nullable*/

        tlua_isinteger isinteger; /*nullable*/
        ttoboolean toboolean;
        struct {
            ttointeger tointeger; /*opt1*/
            ttointegerx tointegerx; /*opt2*/
        };

        struct {
            ttonumber tonumber; /*opt1*/
            ttonumberx tonumberx; /*opt2*/
        };

        ttolstring tolstring;
        tocfunction tocfunction;
        ttouserdata touserdata;

        tgettable gettable;
        tsettable settable;
        tgetfield getfield;
        tsetfield setfield;
        trawget rawget;
        trawset rawset;
        //trawgeti rawgeti;
        //trawseti rawseti;
        tcreatetable createtable;
        tnewuserdatauv newuserdatauv;
        tgetglobal getglobal; /*nullable*/
        tsetglobal setglobal; /*nullable*/
        tlua_getmetatable getmetatable;
        tlua_setmetatable setmetatable;

        tlua_load load;
        // 优先使用k版本（如果有）
        struct {
            tlua_call call; /*opt1*/
            tlua_callk callk; /*opt2*/
        };

        struct {
            tlua_pcall pcall; /*opt1*/
            tlua_pcallk pcallk; /*opt2*/
        };

        tlua_error error;
        tlua_typeerror typeerror;
        tlua_argumenterror argerror;

        // Startup/Cleanup需要
        tlua_newstate newstate; /*nullable*/
        tlua_close close; /*nullable*/

        //tlua_gc gc;
        tlua_type type;

        void* _temp;
    };
}