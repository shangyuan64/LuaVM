#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include "LuaBase.hpp"
#include "LuaStack.hpp"
/*
* LuaVM.hpp
* lua5.1~lua5.5兼容层
*/

#include <memory>
#include <string>

class LuaVM;

// 所有绑定回调的公共基类。
// 通过机器码 trampoline 转成 lua_CFunction，Invoke 时拿到的是 LuaVM*。
struct CallableBase {
    virtual ~CallableBase() = default;
    virtual int Invoke(LuaVM* L) = 0;
    static int __stdcall Forward(CallableBase* self, LuaVM* L)
    {
        return self->Invoke(L);
    }
};

class LuaNamespace;
template<class T> class LuaClass;

// 每个字符串类名对应一份类信息。
// 元表/静态表通过 Registry 中的 lightuserdata 键保存，键地址就是下面这些 char 成员。
struct LuaClassInfo
{
    std::string Name;
    char MetaKey = 0;
    char StaticKey = 0;
    std::function<void(void*)> Destructor;

    LuaClassInfo(std::string name, std::function<void(void*)> destructor)
        : Name(std::move(name)), Destructor(std::move(destructor))
    {
    }
};

struct LUAVM_API VMStatusObject
{
    LuaStatus Status = LuaStatus::Ok;
    LuaVM* _VM = nullptr;
    size_t _Stack = 0;
    ~VMStatusObject();
    explicit operator bool() const {
        return Status == LuaStatus::Ok;
    }
    const char* ToString() const;
};

class LUAVM_API LuaVM
{
public:
    LuaVM() = default; ~LuaVM();
    LuaVM(LuaVM&& other) = default; // 默认移动构造
    LuaVM& operator=(LuaVM&& other) = default;
    LuaVM(const LuaVM&) = delete;   // 禁用拷贝
    LuaVM& operator=(const LuaVM&) = delete;

    LuaStack Stack;

    // 创建/销毁Lua虚拟机
    bool Startup();
    void Cleanup();

    // 附加到已有的lua_State
    void FromState(lua_State* L);

    lua_State* GetState();  // 获取底层lua_State指针
    LuaC::Info* GetCInfo(); // 绑定C接口


   
    VMStatusObject ExecuteScript(const char* Script);
    // TODO: LuaStatus ExecuteFile(const char* Filename);
    

    /*====================绑定====================*/

    template<class ReturnType, class... Params>
    LuaVM& RegFunction(char const* Name, ReturnType(*Function)(Params...));
    template<class ReturnType, class... Params>
    LuaVM& RegFunction(char const* Name, ReturnType(*Function)(Params...) noexcept);
    template<class Callable>
    LuaVM& RegFunction(char const* Name, Callable Function);
    LuaVM& RegFunction(char const* Name, LuaCFunc Function);

    template<typename Callable>
    LuaVM& RegNativeFunction(const char* Name, Callable Object);

    LuaNamespace Global();
    template<class T>
    LuaClass<T> BeginClass(const char* Name);

private:
    friend class LuaNamespace;
    template<class T> friend class LuaClass;

    LuaC::Info m_CInfo = {};
    bool m_External = false;
    lua_State* m_State = nullptr;

    void EnsureNamespaceTable(const std::string& Path);
    void PushNamespaceTable(const std::string& Path);
    void EnsureGlobalMetatable();
    void InstallFunction(const char* Name, CallableBase* Function, const std::string& TablePath);
    void InstallPropertyGetter(const char* Name, CallableBase* Function, const std::string& TablePath);
    void InstallPropertySetter(const char* Name, CallableBase* Function, const std::string& TablePath);
    void OwnCallable(CallableBase* Function);

    LuaClassInfo* FindClassInfo(const std::string& Name) const;
    LuaClassInfo* GetOrCreateClassInfo(
        const char* Name,
        std::function<void(void*)> Destructor,
        bool* Created);
    void CreateClassTables(LuaClassInfo* Info, const std::string& NamespacePath, const char* Name);
    void PushInstanceMetatable(LuaClassInfo* Info);
    void PushStaticTable(LuaClassInfo* Info);
    void InstallClassMethod(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallClassRawMethod(const char* Name, LuaCFunc Function, LuaClassInfo* Info);
    void InstallClassPropertyGetter(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallClassPropertySetter(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallStaticFunction(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallStaticPropertyGetter(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallStaticPropertySetter(const char* Name, CallableBase* Function, LuaClassInfo* Info);
    void InstallStaticMetamethod(const char* Name, CallableBase* Function, LuaClassInfo* Info);

    std::vector<CallableBase*> m_FuncObjects;
    std::vector<LuaClassInfo*> m_ClassInfos;
    std::unordered_map<std::string, LuaClassInfo*> m_ClassInfoByName;

    std::vector<LuaCFunc> m_VirtualFuncs;
    LuaCFunc _UnfoldToLuaC(CallableBase* Object);
};

#include "LuaNamespace.hpp"
