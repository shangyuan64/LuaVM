#pragma once
#include <cstdint>
#include <vector>
#include "LuaBase.hpp"
#include "LuaStack.hpp"
#include "ss_wrapper.hpp"
#include <unordered_map>
#include <optional>
/*
* LuaVM.hpp
* lua5.1~lua5.5兼容层
*/

#include <memory>
#include <string>
class LuaVM;
using LuaTransfer = int(*)(LuaVM*);
//using LuaTransfer = int(*)(LuaVM*,void);
struct CallableBase {
    virtual ~CallableBase() = default;
    virtual int invoke(LuaVM* L) = 0;
    static int __stdcall Forward(CallableBase* self, LuaVM* L)
    {
        return self->invoke(L);
    }
};
//注册到Lua的类的手写C函数的签名包装器
class LuaClassFuncBaseWrapper {
public:
    virtual void SetFunc(void*) = 0;
    virtual int Call(void*,LuaStack*) = 0;
};
template<typename Class>
class LuaClassFuncWrapper : public LuaClassFuncBaseWrapper { 
private:
    using Func = int(*)(Class*,LuaStack*);
    Func func = nullptr;
public:
    LuaClassFuncWrapper(Func func) : func(func) {};
    virtual void SetFunc(void* raw_func) override {
        func = (Func)raw_func;
    };
    virtual int Call(void* obj, LuaStack* stk) override {
         return func ? (*func)((Class*)obj,stk) : 0;
    };
};

enum class RefType {
    NONE,
    CFUNC,
    FUNC
};

struct ClassMemberRef //类成员注册时信息
{
    union {
        LuaClassFuncBaseWrapper* cfunc = nullptr;
        FunctionImp* func;
    } value;
    RefType type = RefType::NONE;
    ~ClassMemberRef();
};

struct LUAVM_API VMClass
{
    VMClass* parent = nullptr;
    LuaVM* vm = nullptr;
    std::unordered_map<std::type_index, VMClass*>* p_class_map;
    lua_State* L = nullptr;
    LuaC::Info* fs = nullptr;
    LuaStack* stk = nullptr;
    int ref = 0, class_ref = 0, ref_o = 0;
    bool open_gc = false;
    ClassWrapper* cw;
    VMClass(LuaVM* vm, ClassWrapper* cw, const char* name);
    ~VMClass();
    VMClass& OpenGC();
    VMClass& AddDefaultBulidMethod(const char* name = "new");
    VMClass& _AddStdFunc(FunctionImp* func, const char* name); //添加标准成员方法
    VMClass& _AddCFunc(LuaClassFuncBaseWrapper* func, const char* name); //添加手写C语言成员方法
    VMClass& _DerivedForm(VMClass&); //继承
    VMClass& _DerivedForm(VMClass*);
    VMClass& _DerivedForm(const char* name);
    void _InitMetaMethod();
    std::vector<ClassMemberRef*> ref_vec;

    template<typename Class, typename Ret, typename... Args>
    VMClass& AddStdFunc(Ret(Class::* func)(Args ...), const char* name) {
        return _AddStdFunc(new FuncWrapper(func), name);
    }
    template<typename Class>
    VMClass& DerivedForm() {
        auto typei = std::type_index(typeid(Class));
        if (p_class_map->count(typei))
            return _DerivedForm(p_class_map->operator[](typei));
        else return *this;
    }
    template<typename Class>
    VMClass& AddCFunc(int(*func)(Class*, LuaStack*),const char* name) {
        if (typeid(Class) != cw->GetClass()->TypeInfo())throw std::runtime_error("bind class type error");
        return (name && func) ? _AddCFunc(new LuaClassFuncWrapper(func),name) : *this;
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
    static std::unordered_map<lua_State*, LuaVM*> vmmap;
    LuaVM() = default; ~LuaVM();
    // 默认移动构造
    LuaVM(LuaVM&& other) = default; 
    LuaVM& operator=(LuaVM&& other) = default;
    // 禁用拷贝
    LuaVM(const LuaVM&) = delete;   
    LuaVM& operator=(const LuaVM&) = delete;
   
    LuaStack Stack;
    // 基于虚拟机初始化数据
    void InitStateEnv(lua_State* L);

    // 创建/销毁Lua虚拟机
    bool Startup();
    void Cleanup();

    // 附加到已有的lua_State
    void FromState(lua_State* L);

    lua_State* GetState();  // 获取底层lua_State指针
    LuaC::Info* GetCInfo(); // 绑定C接口

    VMStatusObject ExecuteScript(const char* Script);

    //Bind
    
    VMClass& _RegClass(const char* name,ClassWrapper* cw);
    void InitObject(int index, std::type_index);
    template<typename Class>
    VMClass& RegClass(const char* name) { return _RegClass(name, ClassWrapper::Create<Class>()); };
    //注册C全局函数
    template<class ReturnType, class... Params>
    void RegFunction(char const* Name, ReturnType(*Function)(Params...));

    template<typename Callable>
    void RegNativeFunction(const char* Name, Callable Object);
    void _RegNativeFunction(const char* Name, CallableBase* Object);
    std::unordered_map<std::type_index, VMClass*> class_map;

private:
    LuaCFunc _UnfoldToLuaC(CallableBase* Object);
    LuaC::Info m_CInfo = {};

    bool m_External = false;
    bool m_ = false;
    lua_State* m_State = nullptr;
    std::vector<CallableBase*> m_FuncObjects;
    std::vector<LuaCFunc> m_VirtualFuncs;
    std::vector<VMClass*> ref_class;
    
};


template<class FnPtr>
struct BridgingFactory {
    FnPtr Function;

    // ---------- 类型萃取 ----------
    template<typename T>
    struct FunctionTraits;

    template<typename ReturnType, typename... Params>
    struct FunctionTraits<ReturnType(*)(Params...)> {
        using Return = ReturnType;
        using ArgTypes = std::tuple<Params...>;
        static constexpr size_t arity = sizeof...(Params);

        template<size_t Index>
        using ArgAt = std::tuple_element_t<Index, ArgTypes>;
    };

    // ---------- Lua 栈操作（按需实现） ----------
    template<typename T>
    T ReadFromLua(LuaVM* vm, int index);

    template<typename T>
    void PushToLua(LuaVM* vm, T&& value);

    // ---------- 调用辅助：接受索引序列 ----------
    template<size_t... Indices>
    int CallImpl(LuaVM* vm, std::index_sequence<Indices...>);

    int Transition(LuaVM* vm);
};

template<class ReturnType, class ...Params>
inline void LuaVM::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{
    using FnType = decltype(Function);
    RegNativeFunction(Name, [=](LuaVM* vm) -> int {
        BridgingFactory<FnType> factory;
        factory.Function = Function;
        return factory.Transition(vm);
    });
}
template<typename Callable>
inline void LuaVM::RegNativeFunction(const char* Name, Callable Object)
{
    struct CallableAdv : CallableBase {
        Callable _Object;
        CallableAdv(Callable&& other)
            : _Object(std::move(other)) {}
        ~CallableAdv() override = default;
        int invoke(LuaVM* L) override {
            return _Object(L);
        }
    };
    auto obj = new CallableAdv(std::move(Object));
    _RegNativeFunction(Name, obj);
    m_FuncObjects.push_back(obj);
}

template<class FnPtr> template<typename T>
inline T BridgingFactory<FnPtr>::ReadFromLua(LuaVM* vm, int index)
{
    return vm->Stack.Get<T>(index + 1);
}

template<class FnPtr> template<typename T>
inline void BridgingFactory<FnPtr>::PushToLua(LuaVM* vm, T&& value)
{
    vm->Stack.Push(value);
}

template<class FnPtr> template<size_t ...Indices>
inline int BridgingFactory<FnPtr>::CallImpl(LuaVM* vm, std::index_sequence<Indices...>)
{
    using Traits = FunctionTraits<FnPtr>;
    using Return = typename Traits::Return;

    if constexpr (std::is_same_v<Return, void>) {
        Function(ReadFromLua<typename Traits::template ArgAt<Indices>>(vm, Indices)...);
        return 0;
    }
    else {
        auto result = Function(ReadFromLua<typename Traits::template ArgAt<Indices>>(vm, Indices)...);
        PushToLua(vm, result);
        return 1;
    }
}
template<class FnPtr>
inline int BridgingFactory<FnPtr>::Transition(LuaVM* vm)
{
    using Traits = FunctionTraits<FnPtr>;
    using Return = typename Traits::Return;
    constexpr size_t arity = Traits::arity;

    if constexpr (arity == 0) {
        if constexpr (std::is_same_v<Return, void>) {
            Function();
            return 0;
        }
        else {
            auto result = Function();
            PushToLua(vm, result);
            return 1;
        }
    }
    else {
        if constexpr (std::is_same_v<Return, void>) {
            // void 返回：直接调用，不接收结果
            CallImpl(vm, std::make_index_sequence<arity>{});
            return 0;
        }
        else {
            // 非 void 返回：接收结果并压栈
            auto result = CallImpl(vm, std::make_index_sequence<arity>{});
            PushToLua(vm, result);
            return 1;
        }
    }
}