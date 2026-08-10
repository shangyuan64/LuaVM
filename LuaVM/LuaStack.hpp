#pragma once
#include "LuaBase.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <stdio.h>

// 绑定对象的 userdata 布局。
// TypeName 是对应注册时的字符串类名，Pointer 是真实 C++ 对象地址。
struct LuaObjectHeader
{
    static constexpr uint64_t Magic = 0x4C75614F626A5631ULL;
    uint64_t MagicValue = Magic;
    const char* TypeName = nullptr;
    void* Pointer = nullptr;
    bool Owner = false;
};

struct LUAVM_API LuaStack
{
    lua_State* State;
	LuaC::Info* info;

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
    void PushCFunction(LuaCFunc Func, int UpvalueCount = 0);
    void PushLightUserdata(void* Ptr);
    void PushExternalString(const char* Str, size_t Len = 0);

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
    bool IsTable(int Index) const;   // 是否是表


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

    [[noreturn]] int Error(const char* errorMsg);
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


    /*====================表操作====================*/

    // 创建一个表
    void NewTable(int Count = 0, int Records = 0);
    // 创建用户数据
    void* NewUserdata(size_t Size);

    // (gettable/rawget)弹出key，读取指定索引的表的key值
    void ReadTableField(int IndexOfTableInStack, bool RawMode = false);
    // (settable/rawset)弹出key和value，设置指定索引的表的key值
    void WriteTableField(int IndexOfTableInStack, bool RawMode = false);

    void GetField(int Index, const char* Key); // 获取表字符串键的值
    void SetField(int Index, const char* Key); // 设置表字符串键的值
    void GetField(int Index, LuaInt Key); // 获取表整数键的值
    void SetField(int Index, LuaInt Key); // 设置表整数键的值
    void RawGetField(int Index, const char* Key); // rawget 字符串键
    void RawSetField(int Index, const char* Key); // rawset 字符串键

    void GetGlobalField(const char* Name);   // 获取全局表中的字段
    void SetGlobalField(const char* Name);   // 设置全局表中的字段

    void PushGlobalTable();                  // 将全局表(_G)压入栈顶
    void PushRegistry(); // 将注册表压入栈顶

    // 按字符串类名把 C++ 对象指针包装成 userdata。
    bool TryPushObjectWithClass(void* Pointer, const char* TypeName, bool Owner);
    // 按字符串类名 push，类未注册时直接抛 Lua 错误，避免静默返回错误结果。
    bool PushObjectWithClass(void* Pointer, const char* ClassName, bool Owner);
    // 从 userdata 取出对象指针，并校验类名。
    void* GetClassObjectPointer(int Index, const char* TypeName);
    // 不校验类名，直接取出 userdata 里的对象指针。
    void* GetRawObjectPointer(int Index);

    bool GetMetatable(int Index); // 获取元表
    void SetMetatable(int Index); // 设置元表
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
inline T LuaStack::Get(int Index)
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
    else if constexpr (std::is_same_v<T, std::string>
        or std::is_same_v<T, std::string_view>) {
        const char* str = CheckString(Index);
        return T(str ? str : "");
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
        Error(buffer);
    }
    else if constexpr (std::is_pointer_v<T>) {
        if (IsUserdata(Index)) {
            return static_cast<T>(GetRawObjectPointer(Index));
        }
        else if (IsInteger(Index)) {
            return reinterpret_cast<T>(ToInteger(Index));
        }
        char buffer[256] = { 0 };
        snprintf(buffer, sizeof(buffer),
            "Bad Get: [%d]%s", GetType(Index), GetTypeName(Index));
        Error(buffer);
    }
    else {
        //return luabridge::get<T>(m_State, Index);
    }
}

template<typename T>
inline T LuaStack::Cast(int Index)
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
    else if constexpr (std::is_same_v<T, std::string>
        or std::is_same_v<T, std::string_view>) {
        const char* str = ToString(Index);
        return T(str ? str : "");
    }
    else if constexpr (std::is_pointer_v<T>) {
        if (IsUserdata(Index)) {
            return static_cast<T>(GetRawObjectPointer(Index));
        }
        else if (IsInteger(Index)) {
            return reinterpret_cast<T>(ToInteger(Index));
        }
    }
    //return luabridge::LuaRef::fromStack(m_State, Index).cast<T>();
}

template<typename T>
void LuaStack::Push(T Value)
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
    else if constexpr (std::is_same_v<T, char*>) {
        PushString(Value, strlen(Value));
    }
    else if constexpr (std::is_same_v<T, std::string>
        or std::is_same_v<T, std::string_view>) {
        PushString(Value.data(), Value.length());
    }
    else if constexpr (std::is_same_v<T, LuaCFunc>) {
        PushCFunction(Value);
    }
    else if constexpr (std::is_pointer_v<T>) {
        if (Value) {
            PushLightUserdata(const_cast<void*>(static_cast<const void*>(Value)));
        }
        else {
            PushNil();
        }
    }
    else {
        static_assert(false, "unknown type");
    }
}
