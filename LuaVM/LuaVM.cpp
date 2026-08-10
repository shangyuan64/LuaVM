#include "LuaVM.hpp"
#include "LuaVM.hpp"

#include <Windows.h>
#include <stdexcept>

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

    for (auto* info : m_ClassInfos) {
        delete info;
    }
    m_ClassInfos.clear();
    m_ClassInfoByName.clear();
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

void LuaVM::EnsureNamespaceTable(const std::string& Path)
{
    if (Path.empty()) {
        Stack.PushGlobalTable();
        return;
    }

    int base = Stack.GetTop();
    size_t dot = Path.rfind('.');
    std::string parentPath = dot == std::string::npos ? std::string() : Path.substr(0, dot);
    std::string name = dot == std::string::npos ? Path : Path.substr(dot + 1);

    EnsureNamespaceTable(parentPath);
    int parentIndex = Stack.GetTop();
    Stack.PushString(name.c_str(), name.size());
    Stack.ReadTableField(parentIndex, true);

    int nsIndex = parentIndex + 1;
    if (Stack.IsNil(-1)) {
        Stack.Popup();
        Stack.NewTable();
        nsIndex = Stack.GetTop();

        Stack.NewTable();
        int metaIndex = Stack.GetTop();

        auto indexCallable = new LuaVMDetail::NamespaceIndexCallable;
        Stack.PushCFunction(_UnfoldToLuaC(indexCallable));
        Stack.RawSetField(metaIndex, "__index");
        OwnCallable(indexCallable);

        auto newIndexCallable = new LuaVMDetail::NamespaceNewIndexCallable;
        Stack.PushCFunction(_UnfoldToLuaC(newIndexCallable));
        Stack.RawSetField(metaIndex, "__newindex");
        OwnCallable(newIndexCallable);

        Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
        Stack.NewTable();
        Stack.WriteTableField(metaIndex, true);

        Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
        Stack.NewTable();
        Stack.WriteTableField(metaIndex, true);

        Stack.SetMetatable(nsIndex);

        Stack.PushString(name.c_str(), name.size());
        Stack.CopyToTop(nsIndex);
        Stack.WriteTableField(parentIndex, true);
    }
    else if (Stack.GetType(-1) != LuaType::Table) {
        Stack.SetTop(base);
        throw std::runtime_error("namespace path collides with a non-table value");
    }

    Stack.PushRegistry();
    int registryIndex = Stack.GetTop();
    Stack.PushString(Path.c_str(), Path.size());
    Stack.CopyToTop(nsIndex);
    Stack.WriteTableField(registryIndex, true);
    Stack.Popup();

    Stack.Remove(parentIndex);
}

void LuaVM::PushNamespaceTable(const std::string& Path)
{
    if (Path.empty()) {
        Stack.PushGlobalTable();
        return;
    }

    Stack.PushRegistry();
    int registryIndex = Stack.GetTop();
    Stack.PushString(Path.c_str(), Path.size());
    Stack.ReadTableField(registryIndex, true);
    Stack.Remove(registryIndex);
}

// 给全局表 _G 安装一套属性元表，让根命名空间也能注册变量/属性。
void LuaVM::EnsureGlobalMetatable()
{
    Stack.PushGlobalTable();
    int globalIndex = Stack.GetTop();
    if (Stack.GetMetatable(globalIndex)) {
        Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
        Stack.ReadTableField(-2, true);
        bool hasPropTable = !Stack.IsNil(-1);
        Stack.Popup(2);
        Stack.Popup();
        if (hasPropTable) {
            return;
        }
        throw std::runtime_error("_G already has a metatable without LuaVM property tables");
    }

    Stack.NewTable();
    int metaIndex = Stack.GetTop();

    auto indexCallable = new LuaVMDetail::NamespaceIndexCallable;
    Stack.PushCFunction(_UnfoldToLuaC(indexCallable));
    Stack.RawSetField(metaIndex, "__index");
    OwnCallable(indexCallable);

    auto newIndexCallable = new LuaVMDetail::NamespaceNewIndexCallable;
    Stack.PushCFunction(_UnfoldToLuaC(newIndexCallable));
    Stack.RawSetField(metaIndex, "__newindex");
    OwnCallable(newIndexCallable);

    Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
    Stack.NewTable();
    Stack.WriteTableField(metaIndex, true);

    Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
    Stack.NewTable();
    Stack.WriteTableField(metaIndex, true);

    Stack.SetMetatable(globalIndex);
    Stack.Popup();
}

void LuaVM::InstallFunction(const char* Name, CallableBase* Function, const std::string& TablePath)
{
    PushNamespaceTable(TablePath);
    int tableIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(tableIndex, Name);
    Stack.Popup();
}

void LuaVM::InstallPropertyGetter(const char* Name, CallableBase* Function, const std::string& TablePath)
{
    if (TablePath.empty()) {
        EnsureGlobalMetatable();
    }
    PushNamespaceTable(TablePath);
    int tableIndex = Stack.GetTop();
    if (!Stack.GetMetatable(tableIndex)) {
        Stack.Popup();
        throw std::runtime_error("namespace has no property getter table");
    }

    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
    Stack.ReadTableField(metaIndex, true);
    int propGetIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propGetIndex, Name);
    Stack.Popup(3);
}

void LuaVM::InstallPropertySetter(const char* Name, CallableBase* Function, const std::string& TablePath)
{
    if (TablePath.empty()) {
        EnsureGlobalMetatable();
    }
    PushNamespaceTable(TablePath);
    int tableIndex = Stack.GetTop();
    if (!Stack.GetMetatable(tableIndex)) {
        Stack.Popup();
        throw std::runtime_error("namespace has no property setter table");
    }

    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
    Stack.ReadTableField(metaIndex, true);
    int propSetIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propSetIndex, Name);
    Stack.Popup(3);
}

void LuaVM::OwnCallable(CallableBase* Function)
{
    m_FuncObjects.push_back(Function);
}

// 按字符串类名查找已经注册的类信息。
LuaClassInfo* LuaVM::FindClassInfo(const std::string& Name) const
{
    auto it = m_ClassInfoByName.find(Name);
    return it == m_ClassInfoByName.end() ? nullptr : it->second;
}

// 第一次注册某个类名时创建 LuaClassInfo，之后直接复用，方便后续继续注入方法/字段。
LuaClassInfo* LuaVM::GetOrCreateClassInfo(
    const char* Name,
    std::function<void(void*)> Destructor,
    bool* Created)
{
    if (auto* existing = FindClassInfo(Name)) {
        *Created = false;
        return existing;
    }

    auto* info = new LuaClassInfo(Name, std::move(Destructor));
    m_ClassInfos.push_back(info);
    m_ClassInfoByName[Name] = info;
    *Created = true;
    return info;
}

// 创建静态表和实例元表：静态表放进命名空间，实例元表放进 Registry。
void LuaVM::CreateClassTables(LuaClassInfo* Info, const std::string& NamespacePath, const char* Name)
{
    int base = Stack.GetTop();
    PushNamespaceTable(NamespacePath);
    int nsIndex = Stack.GetTop();

    // 静态表：类名在 Lua 里指向它，静态函数/静态属性都放这里。
    Stack.NewTable();
    int staticIndex = Stack.GetTop();
    Stack.NewTable();
    int staticMetaIndex = Stack.GetTop();

    auto staticIndexCallable = new LuaVMDetail::NamespaceIndexCallable;
    Stack.PushCFunction(_UnfoldToLuaC(staticIndexCallable));
    Stack.RawSetField(staticMetaIndex, "__index");
    OwnCallable(staticIndexCallable);

    auto staticNewIndexCallable = new LuaVMDetail::NamespaceNewIndexCallable;
    Stack.PushCFunction(_UnfoldToLuaC(staticNewIndexCallable));
    Stack.RawSetField(staticMetaIndex, "__newindex");
    OwnCallable(staticNewIndexCallable);

    Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
    Stack.NewTable();
    Stack.WriteTableField(staticMetaIndex, true);

    Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
    Stack.NewTable();
    Stack.WriteTableField(staticMetaIndex, true);

    Stack.SetMetatable(staticIndex);

    Stack.PushString(Name, strlen(Name));
    Stack.CopyToTop(staticIndex);
    Stack.WriteTableField(nsIndex, true);

    Stack.PushRegistry();
    int registryIndex = Stack.GetTop();
    Stack.PushLightUserdata(&Info->StaticKey);
    Stack.CopyToTop(staticIndex);
    Stack.WriteTableField(registryIndex, true);
    Stack.Popup();

    // 实例元表：方法、属性、析构都由它驱动。
    Stack.PushRegistry();
    registryIndex = Stack.GetTop();
    Stack.PushLightUserdata(&Info->MetaKey);
    Stack.ReadTableField(registryIndex, true);
    if (Stack.IsNil(-1)) {
        Stack.Popup();
        Stack.NewTable();
        int metaIndex = Stack.GetTop();

        auto indexCallable = new LuaVMDetail::ClassIndexCallable;
        Stack.PushCFunction(_UnfoldToLuaC(indexCallable));
        Stack.RawSetField(metaIndex, "__index");
        OwnCallable(indexCallable);

        auto newIndexCallable = new LuaVMDetail::ClassNewIndexCallable;
        Stack.PushCFunction(_UnfoldToLuaC(newIndexCallable));
        Stack.RawSetField(metaIndex, "__newindex");
        OwnCallable(newIndexCallable);

        auto gcCallable = new LuaVMDetail::ClassGcCallable;
        gcCallable->mDestructor = Info->Destructor;
        Stack.PushCFunction(_UnfoldToLuaC(gcCallable));
        Stack.RawSetField(metaIndex, "__gc");
        OwnCallable(gcCallable);

        Stack.PushLightUserdata(LuaVMDetail::GetMethodKey());
        Stack.NewTable();
        Stack.WriteTableField(metaIndex, true);

        Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
        Stack.NewTable();
        Stack.WriteTableField(metaIndex, true);

        Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
        Stack.NewTable();
        Stack.WriteTableField(metaIndex, true);

        Stack.PushLightUserdata(&Info->MetaKey);
        Stack.CopyToTop(metaIndex);
        Stack.WriteTableField(registryIndex, true);

        // 额外按字符串类名存一份，PushClassObject 只靠类名就能找到元表。
        std::string metaKey = "LuaVM.ClassMeta." + Info->Name;
        Stack.PushString(metaKey.c_str(), metaKey.size());
        Stack.CopyToTop(metaIndex);
        Stack.WriteTableField(registryIndex, true);
        Stack.Popup();
    }
    else {
        Stack.Popup(2);
    }

    Stack.SetTop(base);
}

void LuaVM::PushInstanceMetatable(LuaClassInfo* Info)
{
    Stack.PushRegistry();
    int registryIndex = Stack.GetTop();
    Stack.PushLightUserdata(&Info->MetaKey);
    Stack.ReadTableField(registryIndex, true);
    Stack.Remove(registryIndex);
}

void LuaVM::PushStaticTable(LuaClassInfo* Info)
{
    Stack.PushRegistry();
    int registryIndex = Stack.GetTop();
    Stack.PushLightUserdata(&Info->StaticKey);
    Stack.ReadTableField(registryIndex, true);
    Stack.Remove(registryIndex);
}

void LuaVM::InstallClassMethod(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushInstanceMetatable(Info);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetMethodKey());
    Stack.ReadTableField(metaIndex, true);
    int methodIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(methodIndex, Name);
    Stack.Popup(2);
}

void LuaVM::InstallClassRawMethod(const char* Name, LuaCFunc Function, LuaClassInfo* Info)
{
    PushInstanceMetatable(Info);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetMethodKey());
    Stack.ReadTableField(metaIndex, true);
    int methodIndex = Stack.GetTop();
    Stack.PushCFunction(Function);
    Stack.RawSetField(methodIndex, Name);
    Stack.Popup(2);
}

void LuaVM::InstallClassPropertyGetter(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushInstanceMetatable(Info);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
    Stack.ReadTableField(metaIndex, true);
    int propIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propIndex, Name);
    Stack.Popup(2);
}

void LuaVM::InstallClassPropertySetter(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushInstanceMetatable(Info);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
    Stack.ReadTableField(metaIndex, true);
    int propIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propIndex, Name);
    Stack.Popup(2);
}

void LuaVM::InstallStaticFunction(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushStaticTable(Info);
    int tableIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(tableIndex, Name);
    Stack.Popup();
}

void LuaVM::InstallStaticPropertyGetter(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushStaticTable(Info);
    int tableIndex = Stack.GetTop();
    Stack.GetMetatable(tableIndex);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropGetKey());
    Stack.ReadTableField(metaIndex, true);
    int propIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propIndex, Name);
    Stack.Popup(3);
}

void LuaVM::InstallStaticPropertySetter(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushStaticTable(Info);
    int tableIndex = Stack.GetTop();
    Stack.GetMetatable(tableIndex);
    int metaIndex = Stack.GetTop();
    Stack.PushLightUserdata(LuaVMDetail::GetPropSetKey());
    Stack.ReadTableField(metaIndex, true);
    int propIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(propIndex, Name);
    Stack.Popup(3);
}

void LuaVM::InstallStaticMetamethod(const char* Name, CallableBase* Function, LuaClassInfo* Info)
{
    PushStaticTable(Info);
    int tableIndex = Stack.GetTop();
    Stack.GetMetatable(tableIndex);
    int metaIndex = Stack.GetTop();
    Stack.PushCFunction(_UnfoldToLuaC(Function));
    Stack.RawSetField(metaIndex, Name);
    Stack.Popup(2);
}

LuaNamespace LuaVM::Global()
{
    return LuaNamespace(this, "");
}

LuaCFunc LuaVM::_UnfoldToLuaC(CallableBase* Object)
{
    auto RealCFunc = (LuaCFunc)VirtualAlloc(nullptr, 64,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!RealCFunc) { return nullptr; }

#ifdef _WIN64
    // push rbp
    // sub rsp,32
    // mov rcx, pCallableObj
    // mov rdx, this
    // mov rax, Forward
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
    // push this
    // push pCallableObj
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
    *(DWORD*)((BYTE*)RealCFunc + 11) = (DWORD)(uintptr_t)&CallableBase::Forward;
#endif
    m_VirtualFuncs.push_back(RealCFunc); // 对象析构时统一清理
    return RealCFunc;
}
