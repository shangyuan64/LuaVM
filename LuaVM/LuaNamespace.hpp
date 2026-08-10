#pragma once
#include "LuaVM.hpp"

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

class LuaNamespace;

namespace LuaVMDetail
{
    // 属性 getter 表、setter 表、类方法表在元表里的固定键。
    inline void* GetPropGetKey()
    {
        static char key;
        return &key;
    }

    inline void* GetPropSetKey()
    {
        static char key;
        return &key;
    }

    inline void* GetMethodKey()
    {
        static char key;
        return &key;
    }

    // 每个 C++ 类型在注册类时记下一个字符串类名。
    // 不依赖 RTTI，push/get 都只拿这个名字。
    template<class T>
    inline std::string& GetClassNameStorage()
    {
        static std::string name;
        return name;
    }

    template<class T>
    inline const char* GetClassName()
    {
        auto& name = GetClassNameStorage<T>();
        return name.empty() ? nullptr : name.c_str();
    }

    template<class T>
    inline void SetClassName(const char* name)
    {
        GetClassNameStorage<T>() = name ? name : "";
    }

    template<class T>
    struct CallableTraits : CallableTraits<decltype(&T::operator())>
    {
    };

    template<class R, class... P>
    struct CallableTraits<R (*)(P...)>
    {
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class R, class... P>
    struct CallableTraits<R (*)(P...) noexcept>
    {
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class R, class... P>
    struct CallableTraits<std::function<R(P...)>>
    {
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class T, class R, class... P>
    struct CallableTraits<R (T::*)(P...)>
    {
        using ClassType = T;
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);
        static constexpr bool IsConst = false;

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class T, class R, class... P>
    struct CallableTraits<R (T::*)(P...) const>
    {
        using ClassType = const T;
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);
        static constexpr bool IsConst = true;

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class T, class R, class... P>
    struct CallableTraits<R (T::*)(P...) noexcept>
    {
        using ClassType = T;
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);
        static constexpr bool IsConst = false;

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class T, class R, class... P>
    struct CallableTraits<R (T::*)(P...) const noexcept>
    {
        using ClassType = const T;
        using Return = R;
        using Args = std::tuple<P...>;
        static constexpr size_t Arity = sizeof...(P);
        static constexpr bool IsConst = true;

        template<size_t I>
        using Arg = std::tuple_element_t<I, Args>;
    };

    template<class T, class Enable = void>
    struct StackBridge;

    template<class T>
    struct StackBridge<const T> : StackBridge<T>
    {
    };

    template<>
    struct StackBridge<bool>
    {
        static bool Get(LuaVM* vm, int index)
        {
            return vm->Stack.CheckBoolean(index);
        }

        static void Push(LuaVM* vm, bool value)
        {
            vm->Stack.PushBoolean(value);
        }
    };

    template<class T>
    struct StackBridge<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
    {
        static T Get(LuaVM* vm, int index)
        {
            return static_cast<T>(vm->Stack.CheckInteger(index));
        }

        static void Push(LuaVM* vm, T value)
        {
            vm->Stack.PushInteger(static_cast<LuaInt>(value));
        }
    };

    template<class T>
    struct StackBridge<T, std::enable_if_t<std::is_floating_point_v<T>>>
    {
        static T Get(LuaVM* vm, int index)
        {
            return static_cast<T>(vm->Stack.CheckNumber(index));
        }

        static void Push(LuaVM* vm, T value)
        {
            vm->Stack.PushNumber(static_cast<LuaNum>(value));
        }
    };

    template<class T>
    struct StackBridge<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        static T Get(LuaVM* vm, int index)
        {
            return static_cast<T>(vm->Stack.CheckInteger(index));
        }

        static void Push(LuaVM* vm, T value)
        {
            vm->Stack.PushInteger(static_cast<LuaInt>(value));
        }
    };

    // 已注册的类按值绑定：Push 时包装成 userdata，Get 时取出对象引用。
    template<class T>
    struct StackBridge<T, std::enable_if_t<std::is_class_v<T> &&
        !std::is_same_v<T, std::string> &&
        !std::is_same_v<T, std::string_view>>>
    {
        static T Get(LuaVM* vm, int index)
        {
            const char* typeName = GetClassName<T>();
            if (!typeName) {
                vm->Stack.TypeError(index, "object");
            }
            auto* object = static_cast<T*>(vm->Stack.GetClassObjectPointer(index, typeName));
            return *object;
        }

        static void Push(LuaVM* vm, const T& value)
        {
            const char* typeName = GetClassName<T>();
            if (!typeName) {
                vm->Stack.TypeError(-1, "object");
            }
            vm->Stack.PushObjectWithClass(const_cast<T*>(&value), typeName, false);
        }
    };

    template<>
    struct StackBridge<const char*>
    {
        static const char* Get(LuaVM* vm, int index)
        {
            if (vm->Stack.IsNil(index)) {
                return nullptr;
            }
            return vm->Stack.CheckString(index);
        }

        static void Push(LuaVM* vm, const char* value)
        {
            if (value) {
                vm->Stack.PushString(value);
            }
            else {
                vm->Stack.PushNil();
            }
        }
    };

    template<>
    struct StackBridge<char*>
    {
        static char* Get(LuaVM* vm, int index)
        {
            return const_cast<char*>(StackBridge<const char*>::Get(vm, index));
        }

        static void Push(LuaVM* vm, char* value)
        {
            StackBridge<const char*>::Push(vm, value);
        }
    };

    template<>
    struct StackBridge<std::string>
    {
        static std::string Get(LuaVM* vm, int index)
        {
            size_t length = 0;
            const char* text = vm->Stack.CheckString(index, &length);
            return std::string(text ? text : "", text ? length : 0);
        }

        static void Push(LuaVM* vm, const std::string& value)
        {
            vm->Stack.PushString(value.data(), value.size());
        }
    };

    template<>
    struct StackBridge<std::string_view>
    {
        static std::string_view Get(LuaVM* vm, int index)
        {
            size_t length = 0;
            const char* text = vm->Stack.CheckString(index, &length);
            return std::string_view(text ? text : "", text ? length : 0);
        }

        static void Push(LuaVM* vm, std::string_view value)
        {
            vm->Stack.PushString(value.data(), value.size());
        }
    };

    template<>
    struct StackBridge<lua_State*>
    {
        static lua_State* Get(LuaVM* vm, int)
        {
            return vm->GetState();
        }

        static void Push(LuaVM* vm, lua_State* value)
        {
            vm->Stack.PushLightUserdata(value);
        }
    };

    template<>
    struct StackBridge<LuaCFunc>
    {
        static LuaCFunc Get(LuaVM* vm, int index)
        {
            return vm->Stack.info->tocfunction(vm->Stack.State, index);
        }

        static void Push(LuaVM* vm, LuaCFunc value)
        {
            vm->Stack.PushCFunction(value);
        }
    };

    template<class T>
    struct StackBridge<T*>
    {
        using Pointee = T;
        using PlainT = std::remove_cv_t<T>;

        // 已注册的类对象走完整 userdata，未注册的指针退回 lightuserdata。
        static T* Get(LuaVM* vm, int index)
        {
            if (vm->Stack.IsNil(index)) {
                return nullptr;
            }
            if (vm->Stack.IsLightUserdata(index)) {
                return static_cast<T*>(vm->Stack.ToUserdata(index));
            }
            if (vm->Stack.IsFullUserdata(index)) {
                const char* typeName = GetClassName<PlainT>();
                if (!typeName) {
                    vm->Stack.TypeError(index, "object");
                }
                return static_cast<T*>(vm->Stack.GetClassObjectPointer(index, typeName));
            }
            if (vm->Stack.IsInteger(index)) {
                return reinterpret_cast<T*>(vm->Stack.ToInteger(index));
            }
            vm->Stack.TypeError(index, "userdata");
        }

        static void Push(LuaVM* vm, T* value)
        {
            if (!value) {
                vm->Stack.PushNil();
                return;
            }
            const char* typeName = GetClassName<PlainT>();
            if (typeName && vm->Stack.TryPushObjectWithClass(value, typeName, false)) {
                return;
            }
            vm->Stack.PushLightUserdata(const_cast<void*>(static_cast<const void*>(value)));
        }
    };

    template<class T>
    struct StackBridge<T&>
    {
        using Value = std::decay_t<T>;

        static Value Get(LuaVM* vm, int index)
        {
            return StackBridge<Value>::Get(vm, index);
        }

        static void Push(LuaVM* vm, const Value& value)
        {
            StackBridge<Value>::Push(vm, value);
        }
    };

    template<class T>
    struct StackBridge<const T&> : StackBridge<T&>
    {
    };

    template<class T>
    struct StackBridge<T&&> : StackBridge<T&>
    {
    };

    template<class Callable>
    struct FunctionBridge
    {
        using Traits = CallableTraits<Callable>;

        template<size_t... I>
        static int InvokeImpl(LuaVM* vm, Callable& callable, std::index_sequence<I...>)
        {
            using Return = typename Traits::Return;
            if constexpr (std::is_void_v<Return>) {
                callable(StackBridge<typename Traits::template Arg<I>>::Get(vm, static_cast<int>(I) + 1)...);
                return 0;
            }
            else {
                auto result = callable(StackBridge<typename Traits::template Arg<I>>::Get(vm, static_cast<int>(I) + 1)...);
                StackBridge<std::decay_t<Return>>::Push(vm, result);
                return 1;
            }
        }

        static int Invoke(LuaVM* vm, Callable& callable)
        {
            return InvokeImpl(vm, callable, std::make_index_sequence<Traits::Arity>{});
        }
    };

    struct NamespaceIndexCallable final : CallableBase
    {
        int Invoke(LuaVM* vm) override
        {
            LuaStack& stack = vm->Stack;

            stack.CopyToTop(1);
            stack.CopyToTop(2);
            stack.ReadTableField(-2, true);
            if (!stack.IsNil(-1)) {
                stack.Remove(-2);
                return 1;
            }
            stack.Popup();

            if (!stack.GetMetatable(1)) {
                stack.PushNil();
                return 1;
            }

            stack.PushLightUserdata(GetPropGetKey());
            stack.ReadTableField(-2, true);
            stack.CopyToTop(2);
            stack.ReadTableField(-2, true);
            if (!stack.IsNil(-1)) {
                stack.Remove(-2);
                stack.Remove(-2);
                stack.Call(0, 1);
                return 1;
            }

            stack.Popup(3);
            stack.PushNil();
            return 1;
        }
    };

    struct NamespaceNewIndexCallable final : CallableBase
    {
        int Invoke(LuaVM* vm) override
        {
            LuaStack& stack = vm->Stack;

            // 先找绑定过的 setter；找到就调用它。
            stack.GetMetatable(1);
            stack.PushLightUserdata(GetPropSetKey());
            stack.ReadTableField(-2, true);
            stack.CopyToTop(2);
            stack.ReadTableField(-2, true);
            if (!stack.IsNil(-1)) {
                stack.Remove(-2);
                stack.Remove(-2);
                stack.CopyToTop(3);
                stack.Call(1, 0);
                return 0;
            }

            // 没有绑定 setter 时直接 rawset，允许从 Lua 侧向命名空间注入字段。
            stack.Popup(3);
            stack.CopyToTop(1);
            stack.CopyToTop(2);
            stack.CopyToTop(3);
            stack.WriteTableField(-3, true);
            stack.Popup();
            return 0;
        }
    };

    template<class Callable>
    inline CallableBase* MakeCallable(Callable&& object)
    {
        using Stored = std::decay_t<Callable>;
        struct CallableAdv final : CallableBase
        {
            Stored mObject;

            explicit CallableAdv(Stored&& other)
                : mObject(std::move(other))
            {
            }

            int Invoke(LuaVM* L) override
            {
                return FunctionBridge<Stored>::Invoke(L, mObject);
            }
        };
        return new CallableAdv(std::move(object));
    }

    template<class Callable>
    inline CallableBase* MakeNativeCallable(Callable&& object)
    {
        using Stored = std::decay_t<Callable>;
        struct NativeCallableAdv final : CallableBase
        {
            Stored mObject;

            explicit NativeCallableAdv(Stored&& other)
                : mObject(std::move(other))
            {
            }

            int Invoke(LuaVM* L) override
            {
                return mObject(L);
            }
        };
        return new NativeCallableAdv(std::move(object));
    }
}

class LuaNamespace
{
public:
    LuaNamespace(LuaVM* vm, std::string path)
        : mVM(vm), mPath(std::move(path))
    {
    }

    LuaNamespace(const LuaNamespace&) = default;
    LuaNamespace& operator=(const LuaNamespace&) = default;
    LuaNamespace(LuaNamespace&&) = default;
    LuaNamespace& operator=(LuaNamespace&&) = default;

    const std::string& GetPath() const
    {
        return mPath;
    }

    LuaNamespace BeginNamespace(const char* Name)
    {
        std::string childPath = mPath.empty() ? std::string(Name) : mPath + "." + Name;
        mVM->EnsureNamespaceTable(childPath);
        mVM->Stack.Popup();
        return LuaNamespace(mVM, std::move(childPath));
    }

    template<class T>
    LuaClass<T> BeginClass(const char* Name);

    LuaNamespace EndNamespace() const
    {
        if (mPath.empty()) {
            throw std::logic_error("EndNamespace cannot be used on the global namespace");
        }

        size_t dot = mPath.rfind('.');
        std::string parentPath = dot == std::string::npos ? std::string() : mPath.substr(0, dot);
        return LuaNamespace(mVM, std::move(parentPath));
    }

    LuaNamespace& RegFunction(const char* Name, LuaCFunc Function)
    {
        mVM->PushNamespaceTable(mPath);
        int tableIndex = mVM->Stack.GetTop();
        mVM->Stack.PushCFunction(Function);
        mVM->Stack.RawSetField(tableIndex, Name);
        mVM->Stack.Popup();
        return *this;
    }

    template<class R, class... P>
    LuaNamespace& RegFunction(const char* Name, R (*Function)(P...))
    {
        return RegCallable(Name, Function);
    }

    template<class R, class... P>
    LuaNamespace& RegFunction(const char* Name, R (*Function)(P...) noexcept)
    {
        return RegCallable(Name, Function);
    }

    template<class Callable>
    LuaNamespace& RegFunction(const char* Name, Callable Function)
    {
        return RegCallable(Name, std::move(Function));
    }

    template<class R, class... P>
    LuaNamespace& RegFunction(const char* Name, std::function<R(P...)> Function)
    {
        return RegCallable(Name, std::move(Function));
    }

    template<class Callable>
    LuaNamespace& RegCallable(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallFunction(Name, callable, mPath);
        return *this;
    }

    template<class Callable>
    LuaNamespace& RegNativeFunction(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallFunction(Name, callable, mPath);
        return *this;
    }

    template<class T, bool Writable = true>
    LuaNamespace& RegVariable(const char* Name, T* Value)
    {
        using ValueType = std::decay_t<T>;
        static_assert(!std::is_const_v<T> || !Writable,
            "RegVariable with a const pointer must be read-only");
        auto getter = [Value](LuaVM* vm) -> int {
            LuaVMDetail::StackBridge<ValueType>::Push(vm, *Value);
            return 1;
        };
        InstallPropertyGetterNativeCallable(Name, std::move(getter));

        if constexpr (Writable && std::is_copy_assignable_v<ValueType>) {
            auto setter = [Value](LuaVM* vm) -> int {
                *Value = LuaVMDetail::StackBridge<ValueType>::Get(vm, 1);
                return 0;
            };
            InstallPropertySetterNativeCallable(Name, std::move(setter));
        }
        else {
            auto setter = [Name](LuaVM* vm) -> int {
                return vm->Stack.Error("variable is read-only");
            };
            InstallPropertySetterNativeCallable(Name, std::move(setter));
        }

        return *this;
    }

    template<class T>
    LuaNamespace& RegConstant(const char* Name, const T& Value)
    {
        using ValueType = std::decay_t<T>;
        mVM->PushNamespaceTable(mPath);
        int tableIndex = mVM->Stack.GetTop();
        LuaVMDetail::StackBridge<ValueType>::Push(mVM, Value);
        mVM->Stack.RawSetField(tableIndex, Name);
        mVM->Stack.Popup();
        return *this;
    }

    template<class GetFn>
    LuaNamespace& RegProperty(const char* Name, GetFn Get)
    {
        InstallPropertyGetterCallable(Name, std::move(Get));
        return *this;
    }

    template<class GetFn, class SetFn>
    LuaNamespace& RegProperty(const char* Name, GetFn Get, SetFn Set)
    {
        RegProperty(Name, std::move(Get));
        InstallPropertySetterCallable(Name, std::move(Set));
        return *this;
    }

private:
    template<class Callable>
    LuaNamespace& InstallPropertyGetterNativeCallable(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallPropertyGetter(Name, callable, mPath);
        return *this;
    }

    template<class Callable>
    LuaNamespace& InstallPropertySetterNativeCallable(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallPropertySetter(Name, callable, mPath);
        return *this;
    }

    template<class Callable>
    LuaNamespace& InstallPropertyGetterCallable(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallPropertyGetter(Name, callable, mPath);
        return *this;
    }

    template<class Callable>
    LuaNamespace& InstallPropertySetterCallable(const char* Name, Callable Object)
    {
        auto callable = LuaVMDetail::MakeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallPropertySetter(Name, callable, mPath);
        return *this;
    }

    LuaVM* mVM = nullptr;
    std::string mPath;

    friend class LuaVM;
};

#include "LuaClass.hpp"

template<class ReturnType, class... Params>
inline LuaVM& LuaVM::RegFunction(char const* Name, ReturnType(*Function)(Params...))
{
    Global().RegFunction(Name, Function);
    return *this;
}

template<class ReturnType, class... Params>
inline LuaVM& LuaVM::RegFunction(char const* Name, ReturnType(*Function)(Params...) noexcept)
{
    Global().RegFunction(Name, Function);
    return *this;
}

template<class Callable>
inline LuaVM& LuaVM::RegFunction(char const* Name, Callable Function)
{
    Global().RegFunction(Name, std::move(Function));
    return *this;
}

inline LuaVM& LuaVM::RegFunction(char const* Name, LuaCFunc Function)
{
    Global().RegFunction(Name, Function);
    return *this;
}

template<class Callable>
inline LuaVM& LuaVM::RegNativeFunction(const char* Name, Callable Object)
{
    Global().RegNativeFunction(Name, std::move(Object));
    return *this;
}
