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
    using tpushexternalstring = const char* (*)(lua_State* L, const char* s, size_t len, lua_Alloc falloc, void* ud);

    using tiscfunction = int(*)(lua_State* L, int idx);
    using tisinteger = int(*)(lua_State* L, int idx);
    using ttoboolean = int(*)(lua_State* L, int idx);
    using ttonumber = double(*)(lua_State* L, int idx);
    using ttonumberx = double(*)(lua_State* L, int idx, int* isnum);
    using ttointeger = LuaInt(*)(lua_State* L, int idx);
    using ttointegerx = LuaInt(*)(lua_State* L, int idx, int* isnum);
    using ttolstring = const char* (*)(lua_State* L, int idx, size_t* pLen);
    using ttouserdata = void* (*)(lua_State* L, int idx);
    using ttothread = lua_State * (*)(lua_State* L, int idx);
    using ttocfunction = LuaCFunc(*)(lua_State* L, int idx);
    using ttopointer = const char*(*)(lua_State* L, int idx);

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
    using tgetmetatable = int(*)(lua_State* L, int idx);
    using tsetmetatable = int(*)(lua_State* L, int idx);

    using tload = int(*)(lua_State* L, lua_Reader reader, void* data, const char* chunkname, const char* mode);
    using tcall = void(*)(lua_State* L, int nargs, int nresults);
    using tcallk = void(*)(lua_State* L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k);
    using tpcall = int(*)(lua_State* L, int nargs, int nresults, int errfunc);
    using tpcallk = int(*)(lua_State* L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
    using terror = int(*)(lua_State* L);
    using ttypeerror = int(*)(lua_State* L, int idx, const char* tname);
    using targumenterror = int(*)(lua_State* L, int arg, const char* extramsg);

    using tnewstate = lua_State * (*)(lua_Alloc f, void* ud, unsigned seed);
    using tclose = void(*)(lua_State* L);
    //using tlua_gc = int(*)(lua_State* L, int what, int data);
    using ttype = int(*)(lua_State* L, int idx);

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
        tpushexternalstring pushexternalstring; /*nullable*/

        tiscfunction iscfunction;
        tisinteger isinteger; /*nullable*/
        ttoboolean toboolean;

        struct {
            ttonumber tonumber; /*opt1*/
            ttonumberx tonumberx; /*opt2*/
        };

        struct {
            ttointeger tointeger; /*opt1*/
            ttointegerx tointegerx; /*opt2*/
        };

        ttolstring tolstring;
        ttouserdata touserdata;
        ttothread tothread;
        ttocfunction tocfunction;
        ttopointer topointer;

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
        tgetmetatable getmetatable;
        tsetmetatable setmetatable;

        tload load;
        // 优先使用k版本（如果有）
        struct {
            tcall call; /*opt1*/
            tcallk callk; /*opt2*/
        };

        struct {
            tpcall pcall; /*opt1*/
            tpcallk pcallk; /*opt2*/
        };

        terror error;
        ttypeerror typeerror;
        targumenterror argerror;

        // Startup/Cleanup需要
        tnewstate newstate; /*nullable*/
        tclose close; /*nullable*/

        //tlua_gc gc;
        ttype type;

        void* _temp;
    };
}