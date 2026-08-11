#pragma once
#include <cstdint>
#include <vector>
#include "LuaBase.hpp"
#include "LuaStack.hpp"
/*
* LuaVM.hpp
* lua5.1~lua5.5兼容层
*/

#include <memory>
#include <string>
using LuaTransfer = int(*)(class LuaVM*);


struct CallableBase {
    virtual ~CallableBase() = default;
    virtual int invoke(LuaVM* L) = 0;
    static int __stdcall Forward(CallableBase* self, LuaVM* L)
    {
        return self->invoke(L);
    }
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

struct VMClass
{
    VMClass* Parent;
    LuaVM* VM;
    std::string Name;
    std::string SymbolName;

    VMClass(LuaVM* vm, std::string name, VMClass* parent = nullptr)
        : Parent(parent), VM(vm), Name(std::move(name))
    {
        _InitRegistry();
    }

    void _InitRegistry();
    
    template<class ReturnType, class... Params>
    VMClass& RegFunction(char const* Name, ReturnType(*Function)(Params...));
};

void VMClass::_InitRegistry()
{
    if (!Parent) {
        //VM->Stack.PushRegistry();
        //VM->Stack.PushLightUserdata(this);
        //VM->Stack.WriteTableField(-2);
    }
}

template<class ReturnType, class ...Params>
inline VMClass& VMClass::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{

    return *this;
}

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
    void RegFunction(char const* Name, ReturnType(*Function)(Params...));

    template<typename Callable>
    void PushNativeFunction(const char* Name, Callable Object);
    void _PushNativeFunction(const char* Name, CallableBase* Object);

    template<typename Class>
    VMClass RegClass(const char* Name);

    VMClass Global();

private:
    LuaC::Info m_CInfo = {};
    bool m_External = false;
    bool m_ = false;
    lua_State* m_State = nullptr;

    std::vector<CallableBase*> m_FuncObjects;

    std::vector<LuaCFunc> m_VirtualFuncs;
    LuaCFunc _UnfoldToLuaC(CallableBase* Object);
};

template<class ReturnType, class ...Params>
inline void LuaVM::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{
    using FnType = decltype(Function);
    PushNativeFunction(Name, [=](LuaVM* vm) -> int {
        BridgingFactory<FnType> factory;
        factory.Function = Function;
        return factory.Transition(vm);
    });
    Stack.SetGlobalField(Name);
}

template<typename Callable>
inline void LuaVM::PushNativeFunction(const char* Name, Callable Object)
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
    _PushNativeFunction(Name, obj);
    m_FuncObjects.push_back(obj);
}

template<typename Class>
inline VMClass LuaVM::RegClass(const char* Name)
{
    VMClass result(this, Name);
    result.SymbolName = typeid(Class).raw_name();
    return result;
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