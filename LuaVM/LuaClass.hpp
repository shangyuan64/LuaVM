#pragma once
#include "LuaNamespace.hpp"

// LuaClass<T>：把 C++ 类注册成 Lua 用户数据类型。
// 对象指针放在 LuaObjectHeader 里，方法/属性由实例元表驱动。
namespace LuaVMDetail
{
    // 成员函数绑定：第一个栈参数是对象，后面的参数从索引 2 开始。
    // BoundClass 是实际绑定到 Lua 的类，MemFn 可以是其基类的成员函数。
    template<class BoundClass, class MemFn>
    struct MemberFunctionBridge
    {
        using Traits = CallableTraits<MemFn>;

        template<size_t... I>
        static int InvokeImpl(LuaVM* vm, BoundClass* object, const MemFn& function,
            std::index_sequence<I...>)
        {
            using Return = typename Traits::Return;
            if constexpr (std::is_void_v<Return>) {
                (object->*function)(
                    StackBridge<typename Traits::template Arg<I>>::Get(vm, static_cast<int>(I) + 2)...);
                return 0;
            }
            else {
                auto result = (object->*function)(
                    StackBridge<typename Traits::template Arg<I>>::Get(vm, static_cast<int>(I) + 2)...);
                StackBridge<std::decay_t<Return>>::Push(vm, result);
                return 1;
            }
        }

        static int Invoke(LuaVM* vm, BoundClass* object, const MemFn& function)
        {
            return InvokeImpl(vm, object, function,
                std::make_index_sequence<Traits::Arity>{});
        }
    };

    // 构造器：从 Lua 参数创建 new T，并作为“Lua 拥有”的对象压栈。
    template<class T, class... Args>
    struct ConstructorBridge
    {
        template<size_t... I>
        static int InvokeImpl(LuaVM* vm, std::index_sequence<I...>)
        {
            auto* object = new T(
                StackBridge<Args>::Get(vm, static_cast<int>(I) + 2)...);
            const char* typeName = GetClassName<T>();
            if (!typeName || !vm->Stack.TryPushObjectWithClass(object, typeName, true)) {
                delete object;
                return vm->Stack.Error("class is not registered");
            }
            return 1;
        }

        static int Invoke(LuaVM* vm)
        {
            return InvokeImpl(vm, std::make_index_sequence<sizeof...(Args)>{});
        }
    };

    // 兼容 LuaBridge 风格：RegConstructor<void(*)()> 表示无参构造。
    template<class T, class... Args>
    struct ConstructorSelector
    {
        using Type = ConstructorBridge<T, Args...>;
    };

    template<class T, class R, class... P>
    struct ConstructorSelector<T, R (*)(P...)>
    {
        using Type = ConstructorBridge<T, P...>;
    };

    template<class T, class R, class... P>
    struct ConstructorSelector<T, R (*)(P...) noexcept>
    {
        using Type = ConstructorBridge<T, P...>;
    };

    // 普通自由函数，但第一个参数约定为对象指针，常用于替代手写绑定的复杂场景。
    template<class T, class R, class... P, size_t... I>
    int InvokeFreeObjectFunction(LuaVM* vm, R (*function)(T*, P...),
        T* object, std::index_sequence<I...>)
    {
        if constexpr (std::is_void_v<R>) {
            function(object, StackBridge<P>::Get(vm, static_cast<int>(I) + 2)...);
            return 0;
        }
        else {
            auto result = function(object,
                StackBridge<P>::Get(vm, static_cast<int>(I) + 2)...);
            StackBridge<std::decay_t<R>>::Push(vm, result);
            return 1;
        }
    }

    template<class T, class R, class... P, size_t... I>
    int InvokeConstFreeObjectFunction(LuaVM* vm, R (*function)(const T*, P...),
        const T* object, std::index_sequence<I...>)
    {
        if constexpr (std::is_void_v<R>) {
            function(object, StackBridge<P>::Get(vm, static_cast<int>(I) + 2)...);
            return 0;
        }
        else {
            auto result = function(object,
                StackBridge<P>::Get(vm, static_cast<int>(I) + 2)...);
            StackBridge<std::decay_t<R>>::Push(vm, result);
            return 1;
        }
    }

    // 实例元表的 __index：先查方法表，再查属性 getter。
    struct ClassIndexCallable final : CallableBase
    {
        int Invoke(LuaVM* vm) override
        {
            LuaStack& stack = vm->Stack;

            stack.GetMetatable(1);
            stack.PushLightUserdata(GetMethodKey());
            stack.ReadTableField(-2, true);
            stack.CopyToTop(2);
            stack.ReadTableField(-2, true);
            if (!stack.IsNil(-1)) {
                stack.Remove(-2);
                stack.Remove(-2);
                return 1;
            }
            stack.Popup(3);

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
                stack.CopyToTop(1);
                stack.Call(1, 1);
                return 1;
            }

            stack.Popup(3);
            stack.PushNil();
            return 1;
        }
    };

    // 实例元表的 __newindex：只允许绑定过的属性 setter。
    // 实例字段注入需要每实例字段表，先不开放。
    struct ClassNewIndexCallable final : CallableBase
    {
        int Invoke(LuaVM* vm) override
        {
            LuaStack& stack = vm->Stack;

            stack.GetMetatable(1);
            stack.PushLightUserdata(GetPropSetKey());
            stack.ReadTableField(-2, true);
            stack.CopyToTop(2);
            stack.ReadTableField(-2, true);
            if (!stack.IsNil(-1)) {
                stack.Remove(-2);
                stack.Remove(-2);
                stack.CopyToTop(1);
                stack.CopyToTop(3);
                stack.Call(2, 0);
                return 0;
            }

            stack.Popup(3);
            return stack.Error("instance has no writable member");
        }
    };

    // __gc：Owner 为 true 时 delete 对象。
    struct ClassGcCallable final : CallableBase
    {
        std::function<void(void*)> mDestructor;

        int Invoke(LuaVM* vm) override
        {
            auto* header = static_cast<LuaObjectHeader*>(vm->Stack.ToUserdata(1));
            if (header && header->MagicValue == LuaObjectHeader::Magic &&
                header->Owner && mDestructor) {
                mDestructor(header->Pointer);
            }
            return 0;
        }
    };
}

template<class T>
class LuaClass
{
public:
    LuaClass(LuaVM* vm, const char* name, std::string namespacePath)
        : mVM(vm), mName(name), mPath(std::move(namespacePath))
    {
        bool created = false;
        auto destructor = [](void* pointer) {
            delete static_cast<T*>(pointer);
        };
        LuaVMDetail::SetClassName<T>(name);
        mInfo = mVM->GetOrCreateClassInfo(name, std::move(destructor), &created);
        if (created) {
            mVM->CreateClassTables(mInfo, mPath, mName.c_str());
        }
    }

    LuaClass(const LuaClass&) = default;
    LuaClass& operator=(const LuaClass&) = default;
    LuaClass(LuaClass&&) = default;
    LuaClass& operator=(LuaClass&&) = default;

    const std::string& GetName() const
    {
        return mName;
    }

    LuaNamespace EndClass() const
    {
        return LuaNamespace(mVM, mPath);
    }

    template<class... Args>
    LuaClass& RegConstructor()
    {
        using Bridge = typename LuaVMDetail::ConstructorSelector<T, Args...>::Type;
        auto constructor = [](LuaVM* vm) -> int {
            return Bridge::Invoke(vm);
        };
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(constructor));
        mVM->OwnCallable(callable);
        mVM->InstallStaticMetamethod("__call", callable, mInfo);
        return *this;
    }

    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (T::*Function)(P...))
    {
        return RegMemberFunction(Name, Function);
    }

    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (T::*Function)(P...) const)
    {
        return RegMemberFunction(Name, Function);
    }

    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (T::*Function)(P...) noexcept)
    {
        return RegMemberFunction(Name, Function);
    }

    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (T::*Function)(P...) const noexcept)
    {
        return RegMemberFunction(Name, Function);
    }

    // 基类成员函数指针也允许绑定：例如 UIButton 注册 GetCaption 来自 UIControl。
    template<class MemFn, std::enable_if_t<std::is_member_function_pointer_v<MemFn>, int> = 0>
    LuaClass& RegFunction(const char* Name, MemFn Function)
    {
        auto wrapper = [Function](LuaVM* vm) -> int {
            T* object = LuaVMDetail::StackBridge<T*>::Get(vm, 1);
            return LuaVMDetail::MemberFunctionBridge<T, MemFn>::Invoke(vm, object, Function);
        };
        return InstallMethodNative(Name, std::move(wrapper));
    }

    // 普通 C 函数，第一个参数是 T*，Lua 侧调用时对象会自动放在第一个参数。
    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (*Function)(T*, P...))
    {
        auto wrapper = [Function](LuaVM* vm) -> int {
            T* object = LuaVMDetail::StackBridge<T*>::Get(vm, 1);
            return LuaVMDetail::InvokeFreeObjectFunction(vm, Function, object,
                std::make_index_sequence<sizeof...(P)>{});
        };
        return InstallMethodNative(Name, std::move(wrapper));
    }

    template<class R, class... P>
    LuaClass& RegFunction(const char* Name, R (*Function)(const T*, P...))
    {
        auto wrapper = [Function](LuaVM* vm) -> int {
            const T* object = LuaVMDetail::StackBridge<const T*>::Get(vm, 1);
            return LuaVMDetail::InvokeConstFreeObjectFunction(vm, Function, object,
                std::make_index_sequence<sizeof...(P)>{});
        };
        return InstallMethodNative(Name, std::move(wrapper));
    }

    // 原生 C 函数绑定：调用时第一个栈参数就是对象，可以自己手动取。
    LuaClass& RegFunction(const char* Name, LuaCFunc Function)
    {
        mVM->InstallClassRawMethod(Name, Function, mInfo);
        return *this;
    }

    template<class Callable>
    LuaClass& RegNativeFunction(const char* Name, Callable Object)
    {
        return InstallMethodNative(Name, std::move(Object));
    }

    template<class V>
    LuaClass& RegProperty(const char* Name, V T::* Member)
    {
        auto getter = [Member](LuaVM* vm) -> int {
            T* object = LuaVMDetail::StackBridge<T*>::Get(vm, 1);
            LuaVMDetail::StackBridge<V>::Push(vm, object->*Member);
            return 1;
        };
        auto setter = [Member](LuaVM* vm) -> int {
            T* object = LuaVMDetail::StackBridge<T*>::Get(vm, 1);
            object->*Member = LuaVMDetail::StackBridge<V>::Get(vm, 2);
            return 0;
        };
        InstallClassPropertyGetterNative(Name, std::move(getter));
        InstallClassPropertySetterNative(Name, std::move(setter));
        return *this;
    }

    template<class R, class... P>
    LuaClass& RegStaticFunction(const char* Name, R (*Function)(P...))
    {
        auto* callable = LuaVMDetail::MakeCallable(Function);
        mVM->OwnCallable(callable);
        mVM->InstallStaticFunction(Name, callable, mInfo);
        return *this;
    }

    template<class V, bool Writable = true>
    LuaClass& RegStaticVariable(const char* Name, V* Value)
    {
        using ValueType = std::decay_t<V>;
        auto getter = [Value](LuaVM* vm) -> int {
            LuaVMDetail::StackBridge<ValueType>::Push(vm, *Value);
            return 1;
        };
        InstallStaticPropertyGetterNative(Name, std::move(getter));

        if constexpr (Writable) {
            auto setter = [Value](LuaVM* vm) -> int {
                *Value = LuaVMDetail::StackBridge<ValueType>::Get(vm, 1);
                return 0;
            };
            InstallStaticPropertySetterNative(Name, std::move(setter));
        }
        else {
            auto setter = [Name](LuaVM* vm) -> int {
                return vm->Stack.Error("static variable is read-only");
            };
            InstallStaticPropertySetterNative(Name, std::move(setter));
        }
        return *this;
    }

    template<class V>
    LuaClass& RegStaticConstant(const char* Name, const V& Value)
    {
        using ValueType = std::decay_t<V>;
        mVM->PushStaticTable(mInfo);
        int tableIndex = mVM->Stack.GetTop();
        LuaVMDetail::StackBridge<ValueType>::Push(mVM, Value);
        mVM->Stack.RawSetField(tableIndex, Name);
        mVM->Stack.Popup();
        return *this;
    }

private:
    template<class MemFn>
    LuaClass& RegMemberFunction(const char* Name, MemFn Function)
    {
        auto wrapper = [Function](LuaVM* vm) -> int {
            T* object = LuaVMDetail::StackBridge<T*>::Get(vm, 1);
            return LuaVMDetail::MemberFunctionBridge<T, MemFn>::Invoke(vm, object, Function);
        };
        return InstallMethodNative(Name, std::move(wrapper));
    }

    template<class Callable>
    LuaClass& InstallMethodNative(const char* Name, Callable Object)
    {
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallClassMethod(Name, callable, mInfo);
        return *this;
    }

    template<class Callable>
    LuaClass& InstallClassPropertyGetterNative(const char* Name, Callable Object)
    {
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallClassPropertyGetter(Name, callable, mInfo);
        return *this;
    }

    template<class Callable>
    LuaClass& InstallClassPropertySetterNative(const char* Name, Callable Object)
    {
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallClassPropertySetter(Name, callable, mInfo);
        return *this;
    }

    template<class Callable>
    LuaClass& InstallStaticPropertyGetterNative(const char* Name, Callable Object)
    {
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallStaticPropertyGetter(Name, callable, mInfo);
        return *this;
    }

    template<class Callable>
    LuaClass& InstallStaticPropertySetterNative(const char* Name, Callable Object)
    {
        auto* callable = LuaVMDetail::MakeNativeCallable(std::move(Object));
        mVM->OwnCallable(callable);
        mVM->InstallStaticPropertySetter(Name, callable, mInfo);
        return *this;
    }

    LuaVM* mVM = nullptr;
    LuaClassInfo* mInfo = nullptr;
    std::string mName;
    std::string mPath;

    friend class LuaVM;
};

template<class T>
inline LuaClass<T> LuaNamespace::BeginClass(const char* Name)
{
    return LuaClass<T>(mVM, Name, mPath);
}

template<class T>
inline LuaClass<T> LuaVM::BeginClass(const char* Name)
{
    return Global().BeginClass<T>(Name);
}
