#include "pch.h"
#include "LuaStack.hpp"
#include <cmath>

int LuaStack::GetTop() const
{
    return info->gettop(State);
}

void LuaStack::SetTop(int Index)
{
    info->settop(State, Index);
}

void LuaStack::Popup(int Count)
{
    return SetTop(-Count - 1);
}

void LuaStack::Assign(int FromIndex, int ToIndex)
{
    info->copy(State, FromIndex, ToIndex);
}

void LuaStack::Remove(int Index)
{
    if (info->rotate) {
        info->rotate(State, Index, -1);
        Popup(); return;
    }

    info->remove(State, Index);
}

void LuaStack::CopyToTop(int Index)
{
    info->pushvalue(State, Index);
}

void LuaStack::MoveTopTo(int Index)
{
    if (info->insert) {
        return info->insert(State, Index);
    }
    info->rotate(State, Index, 1);
}

void LuaStack::Replace(int Index)
{
    Assign(-1, Index);
    Popup();
}

int LuaStack::SureStackSpace(int Count)
{
    return info->checkstack(State, Count);
}

void LuaStack::PushNil()
{
    info->pushnil(State);
}

void LuaStack::PushBoolean(LuaBol Value)
{
    info->pushboolean(State, Value);
}

void LuaStack::PushInteger(LuaInt Value)
{
    info->pushinteger(State, Value);
}

void LuaStack::PushNumber(LuaNum Value)
{
    info->pushnumber(State, Value);
}

void LuaStack::PushString(LuaStr Str, size_t Len)
{
    info->pushlstring(State, Str, Len);
}

void LuaStack::PushCFunction(LuaCFunc Func)
{
    info->pushcclosure(State, Func, 0);
}

void LuaStack::PushLightUserdata(void* Ptr)
{
    return info->pushlightuserdata(State, Ptr);
}

void LuaStack::PushExternalString(const char* Str, size_t Len)
{
    if (info->pushexternalstring) {
        info->pushexternalstring(State, Str, Len, nullptr, nullptr);
    }
    else {
        PushString(Str, Len);
    }
}

LuaType LuaStack::GetType(int Idx) const
{
    auto type = info->type(State, Idx);
    return Native2Type(type);
}

const char* LuaStack::GetTypeName(int Idx) const
{
    return GetNameOfType(GetType(Idx));
}

const char* LuaStack::GetNameOfType(LuaType Type) const
{
    switch (Type) {
    case LuaType::Nil:           return "nil";
    case LuaType::Boolean:       return "boolean";
    case LuaType::LightUserdata: return "lightuserdata";
    case LuaType::Number:        return "number";
    case LuaType::String:        return "string";
    case LuaType::Table:         return "table";
    case LuaType::Function:      return "function";
    case LuaType::Userdata:      return "userdata";
    case LuaType::Thread:        return "thread";
    default:                     return "none";
    }
}

bool LuaStack::IsNil(int Index) const
{
    return GetType(Index) == LuaType::Nil;
}

bool LuaStack::IsVoid(int Index) const
{
    auto type = GetType(Index);
    return type == LuaType::None || type == LuaType::Nil;
}

bool LuaStack::IsValid(int Index) const
{
    return !IsVoid(Index);
}

bool LuaStack::IsUserdata(int Index) const
{
    auto type = GetType(Index);
    return type == LuaType::Userdata || type == LuaType::LightUserdata;
}

bool LuaStack::IsFullUserdata(int Index) const
{
    return GetType(Index) == LuaType::Userdata;
}

bool LuaStack::IsLightUserdata(int Index) const
{
    return GetType(Index) == LuaType::LightUserdata;
}

bool LuaStack::IsInteger(int Index) const
{
    if (info->isinteger) {
        return info->isinteger(State, Index);
    }

    if (GetType(Index) != LuaType::Number) {
        return false;
    }

    auto num = ToNumber(Index);
    return std::floor(num) == num;
}

LuaBol LuaStack::ToBoolean(int Index) const
{
    return info->toboolean(State, Index);
}

LuaInt LuaStack::ToInteger(int Index) const
{
    if (info->tointegerx) {
        int isInt = 0;
        auto result = info->tointegerx(State, Index, &isInt);
        return result;
    }
    return info->tointeger(State, Index);
}

LuaNum LuaStack::ToNumber(int Index) const
{
    if (info->tonumberx) {
        int isNum = 0;
        auto result = info->tonumberx(State, Index, &isNum);
        return result;
    }
    return info->tonumber(State, Index);
}

LuaStr LuaStack::ToString(int Idx, size_t* pLen) const
{
    return info->tolstring(State, Idx, pLen);
}

LuaUdt LuaStack::ToUserdata(int Index) const
{
    return info->touserdata(State, Index);
}

LuaBol LuaStack::CheckBoolean(int Index)
{
    if (GetType(Index) != LuaType::Boolean) {
        TypeError(Index, GetNameOfType(LuaType::Boolean));
    }
    return ToBoolean(Index);
}

LuaInt LuaStack::CheckInteger(int Index)
{
    if (!IsInteger(Index)) {
        TypeError(Index, "integer");
    }
    return ToInteger(Index);
}

LuaNum LuaStack::CheckNumber(int Index)
{
    if (GetType(Index) != LuaType::Number) {
        TypeError(Index, GetNameOfType(LuaType::Number));
    }
    return ToNumber(Index);
}

LuaStr LuaStack::CheckString(int Index, size_t* pLen)
{
    if (GetType(Index) != LuaType::String) {
        TypeError(Index, GetNameOfType(LuaType::String));
    }
    return ToString(Index, pLen);
}

LuaUdt LuaStack::CheckUserdata(int Index)
{
    if (GetType(Index) != LuaType::Userdata) {
        TypeError(Index, GetNameOfType(LuaType::Userdata));
    }
    return ToUserdata(Index);
}


#pragma warning(push)
#pragma warning(disable: 4645)
#pragma warning(disable: 4646)
int LuaStack::Error(const char* errorMsg)
{
    PushString(errorMsg);
    return info->error(State);
}

int LuaStack::TypeError(int Index, const char* Expected)
{
    return info->typeerror(State, Index, Expected);
}

int LuaStack::ArgumentError(int Index, const char* extraMsg)
{
    return info->argerror(State, Index, extraMsg);
}

#pragma warning(pop)

LuaStatus LuaStack::LoadBuffer(const char* Buffer, size_t Size, const char* ChunkName, const char* Mode)
{
    struct CallbackData {
        const char* buffer;
        size_t size;
    };

    auto callback = [](lua_State* L, void* Data, size_t* Size) -> const char* {
        auto buffer = static_cast<CallbackData*>(Data);
        if (buffer->size == 0) {
            *Size = 0;
            return nullptr;
        }
        *Size = buffer->size;
        buffer->size = 0;
        return buffer->buffer;
    };

    CallbackData data = {
        .buffer = Buffer,
        .size = Size
    };

    return Native2Status(info->load(State, callback, &data, ChunkName, Mode));
}

void LuaStack::Call(size_t ArgCount, size_t RetCount)
{
    auto _ArgCount = static_cast<int>(ArgCount);
    auto _RetCount = static_cast<int>(RetCount);
    if (info->callk) {
        info->callk(State, _ArgCount, _RetCount, 0, 0);
        return;
    }
    info->call(State, _ArgCount, _RetCount);
}

LuaStatus LuaStack::SafeCall(size_t ArgCount, size_t RetCount, int ErrFuncIdx)
{
    auto _ArgCount = static_cast<int>(ArgCount);
    auto _RetCount = static_cast<int>(RetCount);
    if (info->pcallk) {
        return Native2Status(info->pcallk(State, _ArgCount, _RetCount, ErrFuncIdx, 0, 0));
    }
    return Native2Status(info->pcall(State, _ArgCount, _RetCount, ErrFuncIdx));
}

void LuaStack::NewTable(int Count, int Records)
{
    return info->createtable(State, Count, Records);
}

void* LuaStack::NewUserdata(size_t Size)
{
    return info->newuserdatauv(State, Size, 1);
}

LuaType LuaStack::ReadTableField(int IndexOfTableInStack, bool RawMode)
{
    if (RawMode) {
        return Native2Type(info->rawget(State, IndexOfTableInStack));
    }
    return Native2Type(info->gettable(State, IndexOfTableInStack));
}

void LuaStack::WriteTableField(int IndexOfTableInStack, bool RawMode)
{
    if (RawMode) {
        return info->rawset(State, IndexOfTableInStack);
    }
    return info->settable(State, IndexOfTableInStack);
}

LuaType LuaStack::GetField(int Index, const char* Key)
{
    return Native2Type(info->getfield(State, Index, Key));
}

void LuaStack::SetField(int Index, const char* Key)
{
    return info->setfield(State, Index, Key);
}

LuaType LuaStack::GetField(int Index, LuaInt Key)
{
    PushInteger(Key);
    return ReadTableField(Index, true);
}

void LuaStack::SetField(int Index, LuaInt Key)
{
    PushInteger(Key);
    MoveTopTo(-2);
    WriteTableField(Index, true);
}

LuaType LuaStack::GetGlobalField(const char* Name)
{
    if (info->getglobal) {
        return Native2Type(info->getglobal(State, Name));
    }
    return Native2Type(info->getfield(State, info->GlobalTableIndex, Name));
}

void LuaStack::SetGlobalField(const char* Name)
{
    if (info->setglobal) {
        return info->setglobal(State, Name);
    }
    return info->setfield(State, info->GlobalTableIndex, Name);
}

LuaType LuaStack::GetRegistryField(const char* Name)
{
    return Native2Type(info->getfield(State, info->RegistryTableIndex, Name));
}

void LuaStack::SetRegistryField(const char* Name)
{
    return info->setfield(State, info->RegistryTableIndex, Name);
}

void LuaStack::PushRegistry()
{
    CopyToTop(info->RegistryTableIndex);
}

bool LuaStack::GetMetatable(int Index)
{
    return info->getmetatable(State, Index);
}

void LuaStack::SetMetatable(int Index)
{
    info->setmetatable(State, Index);
}

bool LuaStack::NewRegistration(const char* Name)
{
    if (GetRegistryField(Name) != LuaType::Nil) {
        return false;
    }

    Popup();
    NewTable(0, 2);
    PushString(Name);
    SetField(-2, "__name"); // metatable.__name = Name
    CopyToTop(-1);
    SetRegistryField(Name); // registry.name = metatable
    return true;
}

