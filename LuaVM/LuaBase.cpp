#include "pch.h"
#include "LuaBase.hpp"

#ifndef LUA_TNIL
#define LUA_TNONE		(-1)
#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
#endif

inline LuaType Native2Type(int Type)
{
    switch (Type) {
    case LUA_TNIL:           return LuaType::Nil;
    case LUA_TBOOLEAN:       return LuaType::Boolean;
    case LUA_TLIGHTUSERDATA: return LuaType::LightUserdata;
    case LUA_TNUMBER:        return LuaType::Number;
    case LUA_TSTRING:        return LuaType::String;
    case LUA_TTABLE:         return LuaType::Table;
    case LUA_TFUNCTION:      return LuaType::Function;
    case LUA_TUSERDATA:      return LuaType::Userdata;
    case LUA_TTHREAD:        return LuaType::Thread;
    default:                 return LuaType::None;
    }
}

#ifndef LUA_OK
#define LUA_OK		0
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
#define LUA_ERRERR	5
#endif // !LUA_OK


inline LuaStatus Native2Status(int Status)
{
    switch (Status) {
    case LUA_OK:          return LuaStatus::Ok;
    case LUA_YIELD:       return LuaStatus::Yield;
    case LUA_ERRRUN:      return LuaStatus::ErrorRun;
    case LUA_ERRSYNTAX:   return LuaStatus::ErrorSyntax;
    case LUA_ERRMEM:      return LuaStatus::ErrorMemory;
    }
    //assert(false && "Unknown Lua thread status");
    return LuaStatus::Error;
}