#include "LuaVM.hpp"

#include <Windows.h>
std::unordered_map<lua_State*, LuaVM*> LuaVM::vmmap{};
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

void LuaVM::InitStateEnv(lua_State* L){
    Stack.info = &m_CInfo;
    Stack.State = m_State;
    Stack.NewTable();
    Stack.SetGlobalField("G_ClassInfos");
    
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
    if (!m_State) return false;
    m_External = false;
    vmmap[m_State] = this;
    InitStateEnv(m_State);
    return true;
}

void LuaVM::Cleanup()
{
    class_map.clear();
    if (!m_External && m_State) {
        m_CInfo.close(m_State);
        vmmap.erase(m_State);
        m_State = nullptr;
        for (auto c : ref_class)c->class_ref = 0,c->ref = 0,c->stk = 0;
    }
    for (auto c : ref_class)delete c;
    ref_class.clear();
    if (m_External) return;
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
    InitStateEnv(m_State);
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

VMClass& LuaVM::_RegClass(const char* name, ClassWrapper* cw){
    Stack.GetGlobalField("G_ClassInfos");
    Stack.GetField(-1,name);
    VMClass* vmc = nullptr;
    if (Stack.GetType(-1) == LuaType::Userdata)vmc = (VMClass*)Stack.ToUserdata(-1);
    else {
        Stack.Popup();
        vmc = new VMClass(this, cw, name);
        ref_class.push_back(vmc);
        Stack.PushLightUserdata(vmc);
        Stack.SetField(-2,name);
        class_map[cw->GetClass()->TypeIndex()] = vmc;
    }
    return *vmc;
}

void LuaVM::InitObject(int index,std::type_index type){
    if (class_map.count(type)) {
        auto vmc = class_map.at(type);
        index = Stack.ToRawIndex(index);
        Stack.GetRef(vmc->ref);
        Stack.SetMetatable(index);
    }
}

void LuaVM::_RegNativeFunction(const char* Name, CallableBase* Object)
{
    Stack.PushCFunction(_UnfoldToLuaC(Object));
    Stack.SetGlobalField(Name);
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

VMClass::VMClass(LuaVM* vm, ClassWrapper* cw, const char* name) : vm(vm),cw(cw){
    stk = &vm->Stack;L = vm->GetState();fs = vm->GetCInfo();p_class_map = &vm->class_map;
    stk->NewTable(); //name : Class
    stk->CopyToTop(-1); //copy Class
    class_ref = stk->Ref(); //ref Class and pop Class_clone

    stk->NewTable(); // meta
    stk->CopyToTop(-1); //copy meta
    ref = stk->Ref(); //ref meta and pop meta_clone

    stk->SetField(-2,"meta"); //class.meta = meta (pop meta)

    stk->PushLightUserdata(cw);
    stk->SetField(-2,"cw"); //class.cw = cw (pop cw)
    stk->NewTable();
    stk->CopyToTop(-1);
    ref_o = stk->Ref();
    stk->SetField(-2,"reftable");
    stk->SetGlobalField(name);//_G.classname = class
    _InitMetaMethod();
}
VMClass::~VMClass(){
    if (stk && ref)stk->Unref(ref);
    if (stk && class_ref)stk->Unref(class_ref);
    for (auto& cr : ref_vec)delete cr;
    delete cw;
}
static std::optional<Value> lua_cast_cpp(LuaStack& stk,int idx) {
    switch (stk.GetType(idx)) {
        case LuaType::None:
        case LuaType::Nil:return Value();
        case LuaType::Number:
            return stk.IsInteger(idx) ? stk.ToInteger(idx) : stk.ToNumber(idx);
        case LuaType::Boolean:return stk.ToBoolean(idx);
    }
    return std::optional<Value>{};
};
static void pushvalue_tolua(LuaStack& stk,const Value& value) {
    if (auto p = std::get_if<long long>(&value))stk.PushInteger(*p);
    else if (auto p = std::get_if<double>(&value))stk.PushNumber(*p);
    else if (auto p = std::get_if<bool>(&value))stk.PushBoolean(*p);
    else stk.PushNil();
}
static int __call_c(lua_State* L) { //实际c层call
    auto& stk = LuaVM::vmmap.at(L)->Stack;
    if (stk.GetType(1) != LuaType::Userdata)return stk.LError("type error of calling");
    auto self = *(void**)stk.ToUserdata(1); //拿到触发者
    auto target = stk.ToUserdata(stk.UpvalueI(1)); //拿到上层派发的触发者
    if (self != target)return stk.LError("object error of calling"); //身份校验
    auto func = (FunctionImp*)stk.ToUserdata(stk.UpvalueI(2)); //拿到函数包装器
    auto func_argcount = func->argcount();
    if (stk.GetTop() <= func_argcount)return 0;//参数数量校验
    thread_local static std::vector<Value> args;
    if(args.size()<func_argcount)args.resize(func_argcount);
    for (size_t index = 1;index <= func_argcount;index++) {
        auto value = lua_cast_cpp(stk,index + 1);
        if (value.has_value()) {
            args[index - 1] = *value;
        }else return stk.LError("type error of calling");//若转换失败直接拒绝调用
    }
    pushvalue_tolua(stk,func->call(self,args));
    return 1;
}
static int __call_c2(lua_State* L) { //手写C层时的call
    auto& stk = LuaVM::vmmap.at(L)->Stack;
    if (stk.GetType(1) != LuaType::Userdata)
        return stk.LError("type error of calling");
    auto self = *(void**)stk.ToUserdata(1);
    auto target = stk.ToUserdata(stk.UpvalueI(1));
    if (self != target)return stk.LError("object error of calling");
    auto func = (LuaClassFuncBaseWrapper*)stk.ToUserdata(stk.UpvalueI(2));
    return func->Call(self,&stk);
}
static int __index(lua_State* L) {//首先通过self拿到对象 然后直接通过上值拿到注册信息
    auto& stk = LuaVM::vmmap.at(L)->Stack; //stk对象
    
    if (stk.GetType(2) != LuaType::String)return 0;
    stk.CopyToTop(2);
    stk.ReadTableField(stk.UpvalueI(1));
    if (stk.GetType(-1) != LuaType::LightUserdata)return 0;
    auto ref_info = (ClassMemberRef*)stk.ToUserdata(-1);stk.Popup();
    auto self = *(void**)stk.ToUserdata(1); //拿到触发者
    switch (ref_info->type) {
    case RefType::FUNC: {
        stk.PushLightUserdata(self);//用于身份校验
        stk.PushLightUserdata(ref_info->value.func);//实际函数
        stk.PushCFunction(__call_c,2);
        break;
    }
    case RefType::CFUNC: {
        stk.PushLightUserdata(self);
        stk.PushLightUserdata(ref_info->value.cfunc);
        stk.PushCFunction(__call_c2, 2);
        break;
    }
    default: stk.PushNil();
    }
    return 1;
}
void VMClass::_InitMetaMethod() {
    stk->GetRef(ref); //meta表入栈
    stk->GetRef(ref_o); //注册信息表入栈
    stk->PushCFunction(__index,1); //弹出注册表以及压入闭包
    stk->SetField(-2,"__index");
    stk->Popup();
}
static int __gc(lua_State* L) {
    auto& stk = LuaVM::vmmap.at(L)->Stack;
    auto cw = ((ClassWrapper*)stk.ToUserdata(stk.UpvalueI(1)))->GetClass();
    auto obj = *(void**)stk.ToUserdata(1);
    cw->Delete(obj);
    return 0;
}
VMClass& VMClass::OpenGC(){
    stk->GetRef(ref); //get metatable
    stk->PushLightUserdata(cw);
    stk->PushCFunction(__gc,1);
    stk->SetField(-2,"__gc");
    stk->Popup();
    open_gc = true;
    return *this;
}
static int __default_build(lua_State* L) {
    auto vm = LuaVM::vmmap.at(L);
    auto& stk = vm->Stack;
    if (stk.GetType(1) != LuaType::Table)return 0;
    stk.GetField(1,"cw");
    auto cw = ((ClassWrapper*)stk.ToUserdata(-1))->GetClass();
    stk.Popup();
    auto* obj = cw->Build();
    *(void**)stk.NewUserdata(sizeof(obj)) = obj;
    vm->InitObject(-1, cw->TypeIndex());
    return 1;
};
VMClass& VMClass::AddDefaultBulidMethod(const char* name){
    stk->GetRef(class_ref);
    stk->PushCFunction(__default_build);
    stk->SetField(-2,name);
    stk->Popup();
    return *this;
}

VMClass& VMClass::_AddStdFunc(FunctionImp* func, const char* name){
    if (!name || !func)return*this;
    stk->GetRef(ref_o);
    auto ref = new ClassMemberRef();
    ref->type = RefType::FUNC;
    ref->value.func = func;
    ref_vec.push_back(ref);
    stk->PushLightUserdata(ref);
    stk->SetField(-2,name);
    stk->Popup();
    return *this;
}

VMClass& VMClass::_AddCFunc(LuaClassFuncBaseWrapper* func, const char* name){
    if (!name || !func)return*this;
    stk->GetRef(ref_o);
    auto ref = new ClassMemberRef();
    ref->type = RefType::CFUNC;
    ref->value.cfunc = func;
    ref_vec.push_back(ref);
    stk->PushLightUserdata(ref);
    stk->SetField(-2, name);
    stk->Popup();
    return *this;
}

VMClass& VMClass::_DerivedForm(VMClass& base){
    parent = &base;
    stk->GetRef(ref_o); //自身注册信息表
    stk->NewTable(); //元表
    stk->GetRef(base.ref_o);
    stk->SetField(-2,"__index");
    stk->SetMetatable(-2);
    stk->Popup();
    return *this;
}

VMClass& VMClass::_DerivedForm(VMClass* base){
    return base ? _DerivedForm(*base) : *this;
}

VMClass& VMClass::_DerivedForm(const char* name){
     stk->GetGlobalField("G_ClassInfos");
     stk->GetField(-1,name);
     if (stk->GetType(-1) != LuaType::LightUserdata)return *this;//继承失败静默
     auto base = (VMClass*)stk->ToUserdata(-1);
     return _DerivedForm(base);
}

ClassMemberRef::~ClassMemberRef(){
    switch (type) {
    case RefType::FUNC:
        delete value.func;value.func = nullptr;
        break;
    case RefType::CFUNC:
        delete value.cfunc;value.cfunc = nullptr;
        break;
    }
}
