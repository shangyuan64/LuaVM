#include "LuaVM.hpp"
#include "LuaVM.hpp"

#include <Windows.h>

VMStatusObject::~VMStatusObject()
{
    _VM->Stack.SetTop(_Stack);
}

const char* VMStatusObject::ToString() const
{
    return _VM->Stack.ToString(-1);
}

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
    Stack.info = &m_CInfo;
    Stack.State = m_State;

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
    Stack.info = &m_CInfo;
    Stack.State = L;
}

lua_State* LuaVM::GetState()
{
    return m_State;
}

LuaC::Info* LuaVM::GetCInfo()
{
    return &m_CInfo;
}

VMStatusObject LuaVM::ExecuteScript(const char* Script)
{
    VMStatusObject obj;
    obj._VM = this;
    obj._Stack = Stack.GetTop();

    obj.Status = Stack.LoadBuffer(Script, strlen(Script), "chunk", nullptr);
    if (obj.Status != LuaStatus::Ok) {
        return obj;
    }

    obj.Status = Stack.SafeCall(0, -1);
    if (obj.Status != LuaStatus::Ok) {
        return obj;
    }

    return obj;
}

void LuaVM::_RegNativeFunction(const char* Name, CallableBase* Object)
{
    Stack.PushCFunction(_UnfoldToLuaC(Object));
    Stack.SetGlobalField(Name);
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
    *(DWORD*)((BYTE*)RealCFunc + 11) = (DWORD)ForwardProxy;
#endif
    m_VirtualFuncs.push_back(RealCFunc); // 对象析构时统一清理
    return RealCFunc;
}

