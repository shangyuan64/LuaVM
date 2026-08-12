#pragma once
#include <cstdint>
#include <vector>
#include "LuaBase.hpp"
/*
* LuaVM.hpp
* lua5.1~lua5.5兼容层
*/

#include <memory>
#include <string>
using LuaTransfer = int(*)(class LuaVM*);

template<typename T, typename V>
T union_cast(V Pointer) {
    union {
        T Target = T();
        V Value;
    } res;
    res.Value = Pointer;
    return res.Target;
}

struct CallableBase {
    virtual ~CallableBase() = default;
    virtual int invoke(LuaVM* L) = 0;
    static int __stdcall Forward(CallableBase* self, LuaVM* L)
    {
        return self->invoke(L);
    }
};

struct CallableBaseWithObject {
    virtual ~CallableBaseWithObject() = default;
    virtual int invoke(void* Object, LuaVM* L) = 0;
    static int __stdcall Forward(void* Object, CallableBaseWithObject* self, LuaVM* L)
    {
        return self->invoke(Object, L);
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

struct LUAVM_API VMClass
{
    VMClass* Parent;
    LuaVM* VM;
    std::string Name;
    std::string SymbolName;

    VMClass(LuaVM* vm, std::string name, VMClass* parent = nullptr)
        : Parent(parent), VM(vm), Name(std::move(name))
    {
    }

    template<class ReturnType, class... Params>
    VMClass& RegFunction(char const* Name, ReturnType(*Function)(Params...));
};

template<class ReturnType, class ...Params>
inline VMClass& VMClass::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{

    return *this;
}

class LUAVM_API LuaVM
{
public:
    LuaVM() = default; ~LuaVM();
    LuaVM(LuaVM&& other) = default; // 默认移动构造
    LuaVM& operator=(LuaVM&& other) = default;
    LuaVM(const LuaVM&) = delete;   // 禁用拷贝
    LuaVM& operator=(const LuaVM&) = delete;

    // 创建/销毁Lua虚拟机
    bool Startup();
    void Cleanup();

    // 附加到已有的lua_State
    void FromState(lua_State* L);

    lua_State* GetState();  // 获取底层lua_State指针
    LuaC::Info* GetCInfo(); // 绑定C接口

    /*====================栈操作====================*/

    int GetTop() const;
    void SetTop(int Index);

    void Popup(int Count = 1);     // 弹出栈顶元素
    void Assign(int From, int To); // 将指定元素赋值到指定位置（覆盖）
    void Remove(int Index);        // 移除指定索引的元素
    void CopyToTop(int Index);     // 将指定索引处的元素复制到栈顶
    void MoveTopTo(int Index);     // 将栈顶元素移动到指定索引位置
    void Replace(int Index);       // 弹出栈顶元素，并用它替换指定索引位置的元素
    int SureStackSpace(int Count); // 检查栈空间是否足够，如果不足则扩展栈空间

    /*====================压入数据====================*/
    void PushNil();
    void PushBoolean(LuaBol Value);
    void PushInteger(LuaInt Value);
    void PushNumber(LuaNum Value);
    void PushString(LuaStr Str, size_t Len = 0);
    void PushCFunction(LuaCFunc Func);
    void PushLightUserdata(void* Ptr);
    void PushExternalString(const char* Str, size_t Len = 0);
    //void PushNativeFunction();

    /*====================类型====================*/

    LuaType GetType(int Idx) const;
    const char* GetTypeName(int Idx) const; // 获取栈元素类型名
    const char* GetNameOfType(LuaType Type) const; // 获取LuaType对应的类型名

    bool IsNil(int Index) const;
    bool IsVoid(int Index) const; // nil和none都算void
    bool IsValid(int Index) const; // IsVoid的反义
    bool IsUserdata(int Index) const;
    bool IsFullUserdata(int Index) const;
    bool IsLightUserdata(int Index) const;
    bool IsInteger(int Index) const;


    /*====================获取数据====================*/

    LuaBol ToBoolean(int Index) const;
    LuaInt ToInteger(int Index) const;
    LuaNum ToNumber(int Index) const;
    LuaStr ToString(int Idx, size_t* pLen = nullptr) const;
    LuaUdt ToUserdata(int Index) const;

    LuaBol CheckBoolean(int Index);
    LuaInt CheckInteger(int Index);
    LuaNum CheckNumber(int Index);
    LuaStr CheckString(int Index, size_t* pLen = nullptr);
    LuaUdt CheckUserdata(int Index);

    /*====================调用====================*/

    [[noreturn]] int Throw(const char* errorMsg);
    [[noreturn]] int ThrowFmt(const char* format, ...);
    [[noreturn]] int TypeError(int Index, const char* Expected);
    [[noreturn]] int ArgumentError(int Index, const char* extraMsg);

    LuaStatus LoadBuffer(const char* Buffer, size_t Size,
        const char* ChunkName = nullptr, const char* Mode = nullptr);

    void      Call(size_t ArgCount, size_t RetCount);
    LuaStatus SafeCall(size_t ArgCount, size_t RetCount, int ErrFuncIdx = 0);

    /*====================模板方法====================*/

    template<typename T> T Get(int Index);
    template<typename T> T Cast(int Index);
    template<typename T> void Push(T Value);
    void PushWithTag(void* Object, const char* Name);


    /*====================表操作====================*/

    // 创建一个表
    void NewTable(int Count = 0, int Records = 0);
    // 创建用户数据
    void* NewUserdata(size_t Size);

    template<typename T>
    T* NewUserdata();

    // (gettable/rawget)弹出key，读取指定索引的表的key值
    LuaType ReadTableField(int IndexOfTableInStack, bool RawMode = false);
    // (settable/rawset)弹出key和value，设置指定索引的表的key值
    void WriteTableField(int IndexOfTableInStack, bool RawMode = false);

    LuaType GetField(int Index, const char* Key); // 获取表字符串键的值
    void SetField(int Index, const char* Key); // 设置表字符串键的值
    LuaType GetField(int Index, LuaInt Key); // 获取表整数键的值
    void SetField(int Index, LuaInt Key); // 设置表整数键的值

    LuaType GetGlobalField(const char* Name);   // 获取全局表中的字段
    void SetGlobalField(const char* Name);   // 设置全局表中的字段

    LuaType GetRegistryField(const char* Name); // 获取注册表中的字段
    void SetRegistryField(const char* Name);

    void PushRegistry(); // 将注册表压入栈顶

    bool GetMetatable(int Index); // 获取元表
    void SetMetatable(int Index); // 设置元表

    // luaL_setmetatable
    void SetMetaType(const char* Name);

    // 存在返回false，不存在则创建返回true
    bool NewRegistration(const char* Name);
   
    LuaStatus ExecuteScript(const char* Script);
    // TODO: LuaStatus ExecuteFile(const char* Filename);
    

    /*====================绑定====================*/

    template<class ReturnType, class... Params>
    void RegFunction(char const* Name, ReturnType(*Function)(Params...));

    template<typename Callable>
    void PushNativeFunction(Callable Object);
    void _PushNativeFunction(CallableBase* Object);

    template<typename Class>
    VMClass RegClass(const char* Name);

    VMClass Global();

    template<typename Callable>
    LuaCFunc Unfold(Callable Object);

    // 非 const 成员方法
    template<typename T>
    LuaMethod<T> Unfold2(int(T::* method)(LuaVM*));

    // const 成员方法
    template<typename T>
    LuaMethodConst<T> Unfold2(int(T::* method)(LuaVM*) const);


private:
    LuaC::Info m_CInfo = {};
    bool m_External = false;
    lua_State* m_State = nullptr;

    std::vector<void*> m_VirtualFuncs;
    std::vector<CallableBase*> m_FuncObjects;
    std::vector<CallableBaseWithObject*> m_MethodObjects;

    LuaCFunc _UnfoldToLuaC(CallableBase* Object);
    LuaMethod<struct T199863> _UnfoldToLuaC_Method(CallableBaseWithObject* Object);
};

// 主模板：默认不可调用
template <typename T, typename = void>
struct is_callable_impl : std::false_type {};

// 特化：检测是否有 operator()
template <typename T>
struct is_callable_impl<T, std::void_t<decltype(&T::operator())>>
    : std::true_type {};

// 对外接口：自动去掉引用和cv限定
template <typename T>
struct is_callable : is_callable_impl<std::remove_cv_t<std::remove_reference_t<T>>> {};

template <typename T>
inline constexpr bool is_callable_v = is_callable<T>::value;


template<typename T>
struct iFunction_pointer : std::false_type {};

template<typename T>
struct iFunction_pointer<T*> : std::is_function<T> {};

// 辅助别名（C++14）
template<typename T>
inline constexpr bool iFunction_pointer_v = iFunction_pointer<T>::value;


template<typename T>
inline T LuaVM::Get(int Index)
{
    if constexpr (std::is_same_v<T, bool>) {
        return CheckBoolean(Index);
    }
    else if constexpr (std::is_same_v<T, int8_t> or std::is_same_v<T, char>) {
        return static_cast<int8_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int16_t>) {
        return static_cast<int16_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int32_t>) {
        return static_cast<int32_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int64_t>) {
        return static_cast<int64_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint8_t>) {
        return static_cast<uint8_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint16_t>) {
        return static_cast<uint16_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint32_t>) {
        return static_cast<uint32_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint64_t>) {
        return static_cast<uint64_t>(CheckInteger(Index));
    }
    else if constexpr (std::is_same_v<T, float>) {
        return static_cast<float>(CheckNumber(Index));
    }
    else if constexpr (std::is_same_v<T, double>) {
        return static_cast<double>(CheckNumber(Index));
    }
    else if constexpr (std::is_same_v<T, const char*>) {
        return CheckString(Index);
    }
    else if constexpr (std::is_same_v<T, void*>
        or std::is_same_v<T, const void*> or iFunction_pointer_v<T>)
    {
        if (IsUserdata(Index)) {
            return static_cast<T>(ToUserdata(Index));
        }
        else if (IsInteger(Index)) {
            return reinterpret_cast<T>(ToInteger(Index));
        }
        char buffer[256] = { 0 };
        snprintf(buffer, sizeof(buffer),
            "Bad Get: [%d]%s", GetType(Index), GetTypeName(Index));
        Throw(buffer);
    }
    else if constexpr (std::is_pointer_v<T>) {
        if (IsUserdata(Index)) {
            return static_cast<T>(ToUserdata(Index));
        }
        else if (IsInteger(Index)) {
            return reinterpret_cast<T>(ToInteger(Index));
        }
    }
}

template<typename T>
inline T LuaVM::Cast(int Index)
{
    if constexpr (std::is_same_v<T, bool>) {
        return ToBoolean(Index);
    }
    else if constexpr (std::is_same_v<T, int8_t> or std::is_same_v<T, char>) {
        return static_cast<int8_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int16_t>) {
        return static_cast<int16_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int32_t>) {
        return static_cast<int32_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, int64_t>) {
        return static_cast<int64_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint8_t>) {
        return static_cast<uint8_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint16_t>) {
        return static_cast<uint16_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint32_t>) {
        return static_cast<uint32_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, uint64_t>) {
        return static_cast<uint64_t>(ToInteger(Index));
    }
    else if constexpr (std::is_same_v<T, const char*>) {
        return ToString(Index);
    }
    else if constexpr (std::is_pointer_v<T>) {
        if (IsUserdata(Index)) {
            return static_cast<T>(ToUserdata(Index));
        }
        else if (IsInteger(Index)) {
            return reinterpret_cast<T>(ToInteger(Index));
        }
    }
    //return luabridge::LuaRef::fromStack(m_State, Index).cast<T>();
}

template<typename T>
void LuaVM::Push(T Value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        PushBoolean(Value);
    }
    else if constexpr (std::is_same_v<T, char>
        or std::is_same_v<T, int8_t> or std::is_same_v<T, int16_t>
        or std::is_same_v<T, int32_t> or std::is_same_v<T, int64_t>)
    {
        PushInteger(static_cast<int64_t>(Value));
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        PushBoolean(Value);
    }
    else if constexpr (std::is_same_v<T, uint8_t> or std::is_same_v<T, uint16_t>
        or std::is_same_v<T, uint32_t> or std::is_same_v<T, uint64_t>)
    {
        PushInteger(static_cast<int64_t>(Value));
    }
    else if constexpr (std::is_same_v<T, float>
        or std::is_same_v<T, double>)
    {
        PushNumber(static_cast<double>(Value));
    }
    else if constexpr (std::is_same_v<T, const char*>) {
        PushString(Value, strlen(Value));
    }
    else if constexpr (std::is_same_v<T, std::string>
        or std::is_same_v<T, std::string_view>) {
        PushString(Value.data(), Value.length());
    }
    else if constexpr (std::is_same_v<T, LuaCFunc>) {
        PushCFunction(Value);
    }
    else if constexpr (is_callable_v<T>
        or std::is_same_v<T, int(*)(LuaVM*)>/*
        or std::is_same_v<T, LuaCFuncWrap2>*/) {
        PushNativeFunction(Value);
    }
    else if constexpr (std::is_pointer_v<T>) {
        static_assert(false, "unknown type1");
        //luabridge::push(m_State, Value);
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        PushNil();
    }
    else {
        static_assert(false, "unknown type");
    }
}

template<typename T>
inline T* LuaVM::NewUserdata()
{
    return (T*)NewUserdata(sizeof(T));
}

template<class ReturnType, class ...Params>
inline void LuaVM::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{
    using FnType = decltype(Function);
    PushNativeFunction([=](LuaVM* vm) -> int {
        BridgingFactory<FnType> factory;
        factory.Function = Function;
        return factory.Transition(vm);
    });
    SetGlobalField(Name);
}

template<typename Callable>
inline void LuaVM::PushNativeFunction(Callable Object)
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
    _PushNativeFunction(obj);
    m_FuncObjects.push_back(obj);
}

template<typename Class>
inline VMClass LuaVM::RegClass(const char* Name)
{
    VMClass result(this, Name);
    result.SymbolName = typeid(Class).raw_name();
    return result;
}

template<typename T>
inline LuaMethod<T> LuaVM::Unfold2(int(T::* method)(LuaVM*))
{
    using TMethod = int(__thiscall*)(void*,LuaVM*);
    struct CallableAdv : CallableBaseWithObject {
        TMethod _Object;
        CallableAdv(TMethod other)
            : _Object(other) {}
        ~CallableAdv() override = default;
        int invoke(void* Object, LuaVM* L) override {
            return _Object(Object, L);
        }
    };
    auto obj = new CallableAdv(union_cast<TMethod>(method));
    return union_cast<LuaMethod<T>>(_UnfoldToLuaC_Method(obj));
}

template<typename T>
inline LuaMethodConst<T> LuaVM::Unfold2(int(T::* method)(LuaVM*)const)
{
    using TMethod = int(__thiscall*)(void*, LuaVM*);
    struct CallableAdv : CallableBaseWithObject {
        TMethod _Object;
        CallableAdv(TMethod other)
            : _Object(other) {}
        ~CallableAdv() override = default;
        int invoke(void* Object, LuaVM* L) override {
            return _Object(L);
        }
    };
    auto obj = new CallableAdv(union_cast<TMethod>(method));
    return union_cast<LuaMethodConst<T>>(_UnfoldToLuaC_Method(obj));
}

template<typename Callable>
inline LuaCFunc LuaVM::Unfold(Callable Object)
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
    return _UnfoldToLuaC(obj);
}

template<class FnPtr> template<typename T>
inline T BridgingFactory<FnPtr>::ReadFromLua(LuaVM* vm, int index)
{
    return vm->Get<T>(index + 1);
}

template<class FnPtr> template<typename T>
inline void BridgingFactory<FnPtr>::PushToLua(LuaVM* vm, T&& value)
{
    vm->Push(value);
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