#include "LuaVM.hpp"
#include <cmath>
#include <Windows.h>

LuaVM::~LuaVM()
{
    Cleanup();
}

bool LuaVM::Startup()
{
    Cleanup();
    auto allocator = [](void* ud, void* ptr, size_t osize, size_t nsize) -> void* {
        if (nsize) {
            return realloc(ptr, nsize);
        }
        free(ptr);
        return nullptr;
    };
    m_State = m_CInfo.newstate(allocator, nullptr, 0);
    m_External = false;

    if (!m_State) return false;
    return true;
}

void LuaVM::Cleanup()
{
    if (m_External) {
        return;
    }

    if (m_State) {
        m_CInfo.close(m_State);
        m_State = nullptr;
    }

    for (auto ptr : m_FuncObjects) {
        delete ptr;
    }
    m_FuncObjects.clear();

    for (auto virtualMem : m_VirtualFuncs) {
        VirtualFree(virtualMem, 0, MEM_RELEASE);
    }
    m_VirtualFuncs.clear();
}

void LuaVM::FromState(lua_State* L)
{
    Cleanup();
    m_State = L;
    m_External = true;
}

lua_State* LuaVM::GetState()
{
    return m_State;
}

LuaC::Info* LuaVM::GetCInfo()
{
    return &m_CInfo;
}

int LuaVM::GetTop() const
{
    return m_CInfo.gettop(m_State);
}

void LuaVM::SetTop(int Index)
{
    m_CInfo.settop(m_State, Index);
}

void LuaVM::Popup(int Count)
{
    return SetTop(-Count - 1);
}

void LuaVM::Assign(int FromIndex, int ToIndex)
{
    m_CInfo.copy(m_State, FromIndex, ToIndex);
}

void LuaVM::Remove(int Index)
{
    if (m_CInfo.rotate) {
        m_CInfo.rotate(m_State, Index, -1);
        Popup(); return;
    }

    m_CInfo.remove(m_State, Index);
}

void LuaVM::CopyToTop(int Index)
{
    m_CInfo.pushvalue(m_State, Index);
}

void LuaVM::MoveTopTo(int Index)
{
    if (m_CInfo.insert) {
        return m_CInfo.insert(m_State, Index);
    }
    m_CInfo.rotate(m_State, Index, 1);
}

void LuaVM::Replace(int Index)
{
    Assign(-1, Index);
    Popup();
}

int LuaVM::SureStackSpace(int Count)
{
    return m_CInfo.checkstack(m_State, Count);
}

void LuaVM::PushNil()
{
    m_CInfo.pushnil(m_State);
}

void LuaVM::PushBoolean(LuaBol Value)
{
    m_CInfo.pushboolean(m_State, Value);
}

void LuaVM::PushInteger(LuaInt Value)
{
    m_CInfo.pushinteger(m_State, Value);
}

void LuaVM::PushNumber(LuaNum Value)
{
    m_CInfo.pushnumber(m_State, Value);
}

void LuaVM::PushString(LuaStr Str, size_t Len)
{
    m_CInfo.pushlstring(m_State, Str, Len);
}

void LuaVM::PushCFunction(LuaCFunc Func)
{
    m_CInfo.pushcclosure(m_State, Func, 0);
}

void LuaVM::PushLightUserdata(void* Ptr)
{
    return m_CInfo.pushlightuserdata(m_State, Ptr);
}

void LuaVM::PushExternalString(const char* Str, size_t Len)
{
    if (m_CInfo.pushexternalstring) {
        m_CInfo.pushexternalstring(m_State, Str, Len, nullptr, nullptr);
    }
    else {
        PushString(Str, Len);
    }
}

LuaType LuaVM::GetType(int Idx) const
{
    auto type = m_CInfo.type(m_State, Idx);
    return Native2Type(type);
}

const char* LuaVM::GetTypeName(int Idx) const
{
    return GetNameOfType(GetType(Idx));
}

const char* LuaVM::GetNameOfType(LuaType Type) const
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

bool LuaVM::IsNil(int Index) const
{
    return GetType(Index) == LuaType::Nil;
}

bool LuaVM::IsVoid(int Index) const
{
    auto type = GetType(Index);
    return type == LuaType::None || type == LuaType::Nil;
}

bool LuaVM::IsValid(int Index) const
{
    return !IsVoid(Index);
}

bool LuaVM::IsBoolean(int Index) const
{
    return GetType(Index) == LuaType::Boolean;
}

bool LuaVM::IsUserdata(int Index) const
{
    auto type = GetType(Index);
    return type == LuaType::Userdata || type == LuaType::LightUserdata;
}

bool LuaVM::IsFullUserdata(int Index) const
{
    return GetType(Index) == LuaType::Userdata;
}

bool LuaVM::IsLightUserdata(int Index) const
{
    return GetType(Index) == LuaType::LightUserdata;
}

bool LuaVM::IsNumber(int Index) const
{
    return GetType(Index) == LuaType::Number;
}

bool LuaVM::IsInteger(int Index) const
{
    if (m_CInfo.isinteger) {
        return m_CInfo.isinteger(m_State, Index);
    }

    if (GetType(Index) != LuaType::Number) {
        return false;
    }

    auto num = ToNumber(Index);
    return std::floor(num) == num;
}

bool LuaVM::IsString(int Index) const
{
    return GetType(Index) == LuaType::String;
}

bool LuaVM::IsThread(int Index) const
{
    return GetType(Index) == LuaType::Thread;
}

bool LuaVM::IsFunction(int Index) const
{
    return GetType(Index) == LuaType::Function;
}

bool LuaVM::IsCFunction(int Index) const
{
    return m_CInfo.iscfunction(m_State, Index);
}

LuaBol LuaVM::ToBoolean(int Index) const
{
    return m_CInfo.toboolean(m_State, Index);
}

LuaInt LuaVM::ToInteger(int Index) const
{
    if (m_CInfo.tointegerx) {
        int isInt = 0;
        auto result = m_CInfo.tointegerx(m_State, Index, &isInt);
        return result;
    }
    return m_CInfo.tointeger(m_State, Index);
}

LuaNum LuaVM::ToNumber(int Index) const
{
    if (m_CInfo.tonumberx) {
        int isNum = 0;
        auto result = m_CInfo.tonumberx(m_State, Index, &isNum);
        return result;
    }
    return m_CInfo.tonumber(m_State, Index);
}

LuaStr LuaVM::ToString(int Idx, size_t* pLen) const
{
    return m_CInfo.tolstring(m_State, Idx, pLen);
}

LuaUdt LuaVM::ToUserdata(int Index) const
{
    return m_CInfo.touserdata(m_State, Index);
}

LuaBol LuaVM::CheckBoolean(int Index)
{
    if (GetType(Index) != LuaType::Boolean) {
        TypeError(Index, GetNameOfType(LuaType::Boolean));
    }
    return ToBoolean(Index);
}

LuaInt LuaVM::CheckInteger(int Index)
{
    if (!IsInteger(Index)) {
        TypeError(Index, "integer");
    }
    return ToInteger(Index);
}

LuaNum LuaVM::CheckNumber(int Index)
{
    if (GetType(Index) != LuaType::Number) {
        TypeError(Index, GetNameOfType(LuaType::Number));
    }
    return ToNumber(Index);
}

LuaStr LuaVM::CheckString(int Index, size_t* pLen)
{
    if (GetType(Index) != LuaType::String) {
        TypeError(Index, GetNameOfType(LuaType::String));
    }
    return ToString(Index, pLen);
}

LuaUdt LuaVM::CheckUserdata(int Index)
{
    if (GetType(Index) != LuaType::Userdata) {
        TypeError(Index, GetNameOfType(LuaType::Userdata));
    }
    return ToUserdata(Index);
}


#pragma warning(push)
#pragma warning(disable: 4645)
#pragma warning(disable: 4646)
int LuaVM::Throw(const char* errorMsg)
{
    PushString(errorMsg);
    return m_CInfo.error(m_State);
}

int LuaVM::ThrowFmt(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    // 栈缓冲区（通常 512 字节足够大部分错误信息）
    const size_t STACK_BUFFER_SIZE = 512;
    char stack_buffer[STACK_BUFFER_SIZE];

    // 第一次尝试：直接格式化到栈缓冲区
    va_list args_copy;
    va_copy(args_copy, args);
    auto len = (size_t)vsnprintf(stack_buffer, STACK_BUFFER_SIZE, format, args_copy);
    va_end(args_copy);

    if (len < 0) {
        va_end(args);
        PushString("Format error in ThrowFmt");
        return m_CInfo.error(m_State);
    }

    if (static_cast<size_t>(len) < STACK_BUFFER_SIZE) {
        // 栈缓冲区足够，直接使用
        va_end(args);
        PushString(stack_buffer);
        return m_CInfo.error(m_State);
    }

    // 栈缓冲区不够，使用堆分配（少见情况）
    // 注意：len 是所需长度（不含末尾 '\0'）
    char* heap_buffer = static_cast<char*>(malloc(len + 1));  // 或 malloc
    if (!heap_buffer) {
        va_end(args);
        PushString("Out of memory in ThrowFmt");
        return m_CInfo.error(m_State);
    }

    vsnprintf(heap_buffer, len + 1, format, args);
    va_end(args);

    PushString(heap_buffer);
    free(heap_buffer);

    return m_CInfo.error(m_State);
}

int LuaVM::TypeError(int Index, const char* Expected)
{
    return m_CInfo.typeerror(m_State, Index, Expected);
}

int LuaVM::ArgumentError(int Index, const char* extraMsg)
{
    return m_CInfo.argerror(m_State, Index, extraMsg);
}

#pragma warning(pop)

LuaStatus LuaVM::LoadBuffer(const char* Buffer, size_t Size, const char* ChunkName, const char* Mode)
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

    return Native2Status(m_CInfo.load(m_State, callback, &data, ChunkName, Mode));
}

void LuaVM::Call(size_t ArgCount, size_t RetCount)
{
    auto _ArgCount = static_cast<int>(ArgCount);
    auto _RetCount = static_cast<int>(RetCount);
    if (m_CInfo.callk) {
        m_CInfo.callk(m_State, _ArgCount, _RetCount, 0, 0);
        return;
    }
    m_CInfo.call(m_State, _ArgCount, _RetCount);
}

LuaStatus LuaVM::SafeCall(size_t ArgCount, size_t RetCount, int ErrFuncIdx)
{
    auto _ArgCount = static_cast<int>(ArgCount);
    auto _RetCount = static_cast<int>(RetCount);
    if (m_CInfo.pcallk) {
        return Native2Status(m_CInfo.pcallk(m_State, _ArgCount, _RetCount, ErrFuncIdx, 0, 0));
    }
    return Native2Status(m_CInfo.pcall(m_State, _ArgCount, _RetCount, ErrFuncIdx));
}

void LuaVM::PushWithTag(void* Object, const char* Name)
{
    auto pp = NewUserdata<void*>();
    *pp = Object;
    SetMetaType(Name);
}

void LuaVM::NewTable(int Count, int Records)
{
    return m_CInfo.createtable(m_State, Count, Records);
}

void* LuaVM::NewUserdata(size_t Size)
{
    return m_CInfo.newuserdatauv(m_State, Size, 1);
}

LuaType LuaVM::ReadTableField(int IndexOfTableInStack, bool RawMode)
{
    if (RawMode) {
        return Native2Type(m_CInfo.rawget(m_State, IndexOfTableInStack));
    }
    return Native2Type(m_CInfo.gettable(m_State, IndexOfTableInStack));
}

void LuaVM::WriteTableField(int IndexOfTableInStack, bool RawMode)
{
    if (RawMode) {
        return m_CInfo.rawset(m_State, IndexOfTableInStack);
    }
    return m_CInfo.settable(m_State, IndexOfTableInStack);
}

LuaType LuaVM::GetField(int Index, const char* Key)
{
    return Native2Type(m_CInfo.getfield(m_State, Index, Key));
}

void LuaVM::SetField(int Index, const char* Key)
{
    return m_CInfo.setfield(m_State, Index, Key);
}

LuaType LuaVM::GetField(int Index, LuaInt Key)
{
    PushInteger(Key);
    return ReadTableField(Index, true);
}

void LuaVM::SetField(int Index, LuaInt Key)
{
    PushInteger(Key);
    MoveTopTo(-2);
    WriteTableField(Index, true);
}

LuaType LuaVM::GetGlobalField(const char* Name)
{
    if (m_CInfo.getglobal) {
        return Native2Type(m_CInfo.getglobal(m_State, Name));
    }
    return Native2Type(m_CInfo.getfield(m_State, m_CInfo.GlobalTableIndex, Name));
}

void LuaVM::SetGlobalField(const char* Name)
{
    if (m_CInfo.setglobal) {
        return m_CInfo.setglobal(m_State, Name);
    }
    return m_CInfo.setfield(m_State, m_CInfo.GlobalTableIndex, Name);
}

LuaType LuaVM::GetRegistryField(const char* Name)
{
    return Native2Type(m_CInfo.getfield(m_State, m_CInfo.RegistryTableIndex, Name));
}

void LuaVM::SetRegistryField(const char* Name)
{
    return m_CInfo.setfield(m_State, m_CInfo.RegistryTableIndex, Name);
}

void LuaVM::PushRegistry()
{
    CopyToTop(m_CInfo.RegistryTableIndex);
}

bool LuaVM::GetMetatable(int Index)
{
    return m_CInfo.getmetatable(m_State, Index);
}

void LuaVM::SetMetatable(int Index)
{
    m_CInfo.setmetatable(m_State, Index);
}

void LuaVM::SetMetaType(const char* Name)
{
    GetRegistryField(Name);
    SetMetatable(-2);
}

bool LuaVM::NewRegistration(const char* Name)
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

LuaStatus LuaVM::ExecuteScript(const char* Script)
{
    LuaStatus status = LoadBuffer(Script, strlen(Script), "chunk", nullptr);
    if (status != LuaStatus::Ok) {
        return status;
    }

    status = SafeCall(0, -1);
    if (status != LuaStatus::Ok) {
        return status;
    }

    return status;
}

void LuaVM::_PushNativeFunction(CallableBase* Object)
{
    PushCFunction(_UnfoldToLuaC(Object));
}

VMClass LuaVM::Global()
{
    return VMClass(this, "_G");
}

LuaCFunc LuaVM::_UnfoldToLuaC(CallableBase* Object)
{
    auto RealCFunc = (LuaCFunc)VirtualAlloc(nullptr, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!RealCFunc) { return nullptr; }

#ifdef _WIN64
    // push rbp
    // sub rsp,32
    // mov rdx, pCallableObj
    // mov rcx, this
    // mov rax,ForwardProxy
    // call rax
    // add rsp,32
    // pop rbp
    // ret
    BYTE Shell[] = {
        0x55,
        0x48, 0x83, 0xEC, 0x20,
        0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0x48, 0x83, 0xC4, 0x20,
        0x5D,
        0xC3
    };

    memcpy(RealCFunc, Shell, sizeof(Shell));
    *(uint64_t*)((BYTE*)RealCFunc + 7) = (uint64_t)this;
    *(uint64_t*)((BYTE*)RealCFunc + 17) = (uint64_t)Object;
    *(uint64_t*)((BYTE*)RealCFunc + 27) = (uint64_t)CallableBase::Forward;
#else
    // push pCallableObj
    // push this
    // mov eax,ForwardProxy
    // call eax
    // ret
    BYTE Shell[] = {
        0x68, 0x00, 0x00, 0x00, 0x00,
        0x68, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0, 0xC3
    };

    memcpy(RealCFunc, Shell, sizeof(Shell));
    *(DWORD*)((BYTE*)RealCFunc + 1) = (DWORD)this;
    *(DWORD*)((BYTE*)RealCFunc + 6) = (DWORD)Object;
    *(DWORD*)((BYTE*)RealCFunc + 11) = (DWORD)CallableBase::Forward;
#endif
    m_VirtualFuncs.push_back(RealCFunc); // 对象析构时统一清理
    return RealCFunc;
}


LuaMethod<struct T199863> LuaVM::_UnfoldToLuaC_Method(CallableBaseWithObject* Object)
{
    auto RealCFunc = (void*)VirtualAlloc(nullptr, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!RealCFunc) { return nullptr; }

#ifdef _WIN64
    // push rbp
    // sub rsp,32
    // mov r8 , pCallableObj
    // mov rdx, this
    // mov rax,ForwardProxy
    // call rax
    // add rsp,32
    // pop rbp
    // ret
    BYTE Shell[] = {
        0x55,
        0x48, 0x83, 0xEC, 0x20,
        0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xD0,
        0x48, 0x83, 0xC4, 0x20,
        0x5D,
        0xC3
    };

    memcpy(RealCFunc, Shell, sizeof(Shell));
    *(uint64_t*)((BYTE*)RealCFunc + 7) = (uint64_t)this;
    *(uint64_t*)((BYTE*)RealCFunc + 17) = (uint64_t)Object;
    *(uint64_t*)((BYTE*)RealCFunc + 27) = (uint64_t)CallableBaseWithObject::Forward;
#else
    // push pCallableObj
    // push this
    // mov eax,ForwardProxy
    // push ecx
    // call eax
    // ret
    BYTE Shell[] = {
        0x68, 0x00, 0x00, 0x00, 0x00,
        0x68, 0x00, 0x00, 0x00, 0x00,
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0x51, 0xFF, 0xD0, 0xC3
    };

    memcpy(RealCFunc, Shell, sizeof(Shell));
    *(DWORD*)((BYTE*)RealCFunc + 1) = (DWORD)this;
    *(DWORD*)((BYTE*)RealCFunc + 6) = (DWORD)Object;
    *(DWORD*)((BYTE*)RealCFunc + 11) = (DWORD)CallableBaseWithObject::Forward;
#endif
    m_VirtualFuncs.push_back(RealCFunc); // 对象析构时统一清理
    return union_cast<LuaMethod<struct T199863>>(RealCFunc);
}
