#pragma once
#include <LuaVM.hpp>
#ifndef LUAC_ENTRYEX
#define LUAC_ENTRYEX(Base, Key, Value) Base->##Key = Value
#endif // !LUAC_ENTRYEX

#ifndef LUAC_ENTRY
#define LUAC_ENTRY(Base, Key) LUAC_ENTRYEX(Base, Key, lua_##Key)
#endif // !LUAC_ENTRY

#define LUAC_55(Base) \
    /* 先清空结构体（所有指针置空）*/ \
    memset((Base), 0, sizeof(*(Base))); \
    /* 固定常量 */ \
    (Base)->RegistryTableIndex = (-(INT_MAX/2 + 1000)); \
    /* 基础栈操作 */ \
    LUAC_ENTRY((Base), gettop); \
    LUAC_ENTRY((Base), settop); \
    LUAC_ENTRY((Base), copy); \
    LUAC_ENTRY((Base), pushvalue); \
    LUAC_ENTRY((Base), rotate); \
    LUAC_ENTRY((Base), checkstack); \
    /* 压栈函数 */ \
    LUAC_ENTRY((Base), pushnil); \
    LUAC_ENTRY((Base), pushboolean); \
    LUAC_ENTRY((Base), pushinteger); \
    LUAC_ENTRY((Base), pushnumber); \
    LUAC_ENTRYEX((Base), _temp, lua_pushlstring); \
        using Tpushlstring = void(*)(lua_State* L, const char* s, size_t len); \
        static auto __pushlstring = (Tpushlstring)(Base)->_temp; \
        (Base)->pushlstring = [](lua_State* L, const char* s, size_t len) \
        { \
            __pushlstring(L, s, len); \
        };\
    LUAC_ENTRY((Base), pushcclosure); \
    LUAC_ENTRY((Base), pushlightuserdata); \
    LUAC_ENTRY((Base), pushexternalstring); \
    /* 取值函数 */ \
    LUAC_ENTRY((Base), iscfunction); \
    LUAC_ENTRY((Base), isinteger); \
    LUAC_ENTRY((Base), toboolean); \
    LUAC_ENTRY((Base), tointegerx); \
    LUAC_ENTRY((Base), tonumberx); \
    LUAC_ENTRY((Base), tolstring); \
    LUAC_ENTRY((Base), touserdata); \
    LUAC_ENTRY((Base), tothread); \
    LUAC_ENTRY((Base), tocfunction); \
    LUAC_ENTRY((Base), topointer); \
    /* 表操作 */ \
    LUAC_ENTRY((Base), gettable); \
    LUAC_ENTRY((Base), settable); \
    LUAC_ENTRY((Base), getfield); \
    LUAC_ENTRY((Base), setfield); \
    LUAC_ENTRY((Base), rawget); \
    LUAC_ENTRY((Base), rawset); \
    /*LUAC_ENTRY((Base), rawgeti);*/ \
    /*LUAC_ENTRY((Base), rawseti);*/ \
    LUAC_ENTRY((Base), createtable); \
    LUAC_ENTRY((Base), newuserdatauv); \
    LUAC_ENTRY((Base), getglobal); \
    LUAC_ENTRY((Base), setglobal); \
    LUAC_ENTRY((Base), getmetatable); \
    LUAC_ENTRY((Base), setmetatable); \
    /* 加载与执行 */ \
    LUAC_ENTRY((Base), load); \
    LUAC_ENTRY((Base), callk); \
    LUAC_ENTRY((Base), pcallk); \
    /* 错误处理（lauxlib 函数） */ \
    LUAC_ENTRY((Base), error); \
    auto lua_typeerror = &luaL_typeerror; \
    LUAC_ENTRY((Base), typeerror); \
    auto lua_argerror = &luaL_argerror; \
    LUAC_ENTRY((Base), argerror); \
    /* 状态机 */ \
    LUAC_ENTRY((Base), newstate); \
    LUAC_ENTRY((Base), close); \
    /* 类型信息 */ \
    LUAC_ENTRY((Base), type);
