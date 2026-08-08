#pragma once
#include <cstdint>
struct lua_State;
typedef void* (*lua_Alloc) (void* ud, void* ptr, size_t osize, size_t nsize);
typedef const char* (*lua_Reader) (lua_State* L, void* ud, size_t* sz);

typedef ptrdiff_t lua_KContext;
typedef int (*lua_KFunction) (lua_State* L, int status, lua_KContext ctx);

/*
* LuaVM.hpp
* lua5.1~lua5.5兼容层
*/

enum class LuaType
{
    None = -1,
    Nil,
    Boolean,
    LightUserdata,
    Number,
    String,
    Table,
    Function,
    Userdata,
    Thread
};

enum class LuaStatus
{
    Ok = 0,
    Yield = 1,
    ErrorRun = 2,
    ErrorSyntax = 3,
    ErrorMemory = 4,
    Error = 5
};

using LuaInt = __int64;
using LuaNum = double;
using LuaStr = const char*;
using LuaBol = bool;
using LuaUdt = void*;

using LuaCFunc = int (*)(lua_State*);
#include <functional>
#include <memory>
#include <string>
using LuaCFuncWrap = std::function<int(class LuaVM*)>;
using LuaCFuncWrap2 = int(*)(class LuaVM*);

namespace LuaC
{
    using tgettop = int(*)(lua_State* L);
    using tsettop = void(*)(lua_State* L, int idx);
    using tcopy = void(*)(lua_State* L, int fromidx, int toidx);
    using trotate = void(*)(lua_State* L, int idx, int n);
    using tremove = void(*)(lua_State* L, int idx);
    using tinsert = void(*)(lua_State* L, int idx);
    using tcheckstack = int(*)(lua_State* L, int n);

    using tpushnil = void(*)(lua_State* L);
    using tpushboolean = void(*)(lua_State* L, int b);
    using tpushinteger = void(*)(lua_State* L, LuaInt n);
    using tpushnumber = void(*)(lua_State* L, double n);
    using tpushlstring = void(*)(lua_State* L, const char* s, size_t len);
    using tpushcclosure = void(*)(lua_State* L, LuaCFunc f, int n);
    using tpushlightuserdata = void(*)(lua_State* L, void* p);
    using tlua_pushexternalstring = const char* (*)(lua_State* L, const char* s, size_t len, lua_Alloc falloc, void* ud);

    using tlua_isinteger = int(*)(lua_State* L, int idx);
    using ttoboolean = int(*)(lua_State* L, int idx);
    using ttointeger = LuaInt(*)(lua_State* L, int idx);
    using ttointegerx = LuaInt(*)(lua_State* L, int idx, int* isnum);
    using ttonumber = double(*)(lua_State* L, int idx);
    using ttonumberx = double(*)(lua_State* L, int idx, int* isnum);
    using ttolstring = const char* (*)(lua_State* L, int idx, size_t* pLen);
    using tocfunction = LuaCFunc(*)(lua_State* L, int idx);
    using ttouserdata = void* (*)(lua_State* L, int idx);

    using tgettable = void(*)(lua_State* L, int idx);
    using tsettable = void(*)(lua_State* L, int idx);
    using tgetfield = void(*)(lua_State* L, int idx, const char* k);
    using tsetfield = void(*)(lua_State* L, int idx, const char* k);
    using trawget = void(*)(lua_State* L, int idx);
    using trawset = void(*)(lua_State* L, int idx);
    // 第三个参数存在历史遗留问题，所以使用替代方案
    //using trawgeti = void(*)(lua_State* L, int idx, LuaInt n);
    //using trawseti = void(*)(lua_State* L, int idx, LuaInt n);
    using tcreatetable = void(*)(lua_State* L, int narr, int nrec);
    using tgetglobal = void(*)(lua_State* L, const char* name);
    using tsetglobal = void(*)(lua_State* L, const char* name);
    using tlua_getmetatable = int(*)(lua_State* L, int idx);
    using tlua_setmetatable = int(*)(lua_State* L, int idx);

    using tlua_load = int(*)(lua_State* L, lua_Reader reader, void* data, const char* chunkname, const char* mode);
    using tlua_call = void(*)(lua_State* L, int nargs, int nresults);
    using tlua_callk = void(*)(lua_State* L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k);
    using tlua_pcall = int(*)(lua_State* L, int nargs, int nresults, int errfunc);
    using tlua_pcallk = int(*)(lua_State* L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
    using tlua_error = int(*)(lua_State* L);
    using tlua_typeerror = int(*)(lua_State* L, int idx, const char* tname);
    using tlua_argumenterror = int(*)(lua_State* L, int arg, const char* extramsg);

    using tlua_newstate = lua_State*(*)(lua_Alloc f, void* ud, unsigned seed);
    using tlua_close = void(*)(lua_State* L);
    //using tlua_gc = int(*)(lua_State* L, int what, int data);
    using tlua_type = int(*)(lua_State* L, int idx);

    struct Info
    {
        int GlobalTableIndex; // 可选：优先使用get/setglobal
        int RegistryTableIndex;
        
        tgettop gettop;
        tsettop settop;
        tcopy copy;
        struct {
            trotate rotate; /*opt1*/

            /*opt2*/
            tremove remove;
            tinsert insert;
        };
        tcheckstack checkstack;

        tpushnil pushnil;
        tpushboolean pushboolean;
        tpushinteger pushinteger;
        tpushnumber pushnumber;
        tpushlstring pushlstring;
        tpushcclosure pushcclosure;
        tpushlightuserdata pushlightuserdata;
        tlua_pushexternalstring pushexternalstring; /*nullable*/

        tlua_isinteger isinteger; /*nullable*/
        ttoboolean toboolean;
        struct {
            ttointeger tointeger; /*opt1*/
            ttointegerx tointegerx; /*opt2*/
        };

        struct {
            ttonumber tonumber; /*opt1*/
            ttonumberx tonumberx; /*opt2*/
        };

        ttolstring tolstring;
        tocfunction tocfunction;
        ttouserdata touserdata;

        tgettable gettable;
        tsettable settable;
        tgetfield getfield;
        tsetfield setfield;
        trawget rawget;
        trawset rawset;
        //trawgeti rawgeti;
        //trawseti rawseti;
        tcreatetable createtable;
        tgetglobal getglobal; /*nullable*/
        tsetglobal setglobal; /*nullable*/
        tlua_getmetatable getmetatable;
        tlua_setmetatable setmetatable;

        tlua_load load;
        // 优先使用k版本（如果有）
        struct {
            tlua_call call; /*opt1*/
            tlua_callk callk; /*opt2*/
        };

        struct {
            tlua_pcall pcall; /*opt1*/
            tlua_pcallk pcallk; /*opt2*/
        };
        
        tlua_error error;
        tlua_typeerror typeerror;
        tlua_argumenterror argerror;

        // Startup/Cleanup需要
        tlua_newstate newstate; /*nullable*/
        tlua_close close; /*nullable*/

        //tlua_gc gc;
        tlua_type type;

        void* _temp;
    };
}

class LUAVM_API LuaVM
{
public:
    LuaVM() = default; ~LuaVM();
    LuaVM(LuaVM&& other) = default; // 默认移动构造
    LuaVM& operator=(LuaVM&& other) = default;
    LuaVM(const LuaVM&) = delete; // 禁用拷贝
    LuaVM& operator=(const LuaVM&) = delete;

    // 创建/销毁Lua虚拟机
    bool Startup();
    void Cleanup();

    // 附加到已有的lua_State
    void FromState(lua_State* L);

    // 获取底层lua_State指针
    lua_State* GetState();

    // 绑定C接口
    LuaC::Info* GetCInfo();


    /*====================栈操作====================*/

    int GetStackTop() const;
    void SetStackTop(int Index);

    // 弹出栈顶元素
    void Popup(int Count = 1);

    // 将指定元素赋值到指定位置（覆盖）
    void Assign(int FromIndex, int ToIndex);

    // 移除指定索引的元素
    void Remove(int Index);

    // 将栈顶元素移动到指定索引位置
    void MoveTopTo(int Index);

    // 弹出栈顶元素，并用它替换指定索引位置的元素
    void Replace(int Index);

    // 检查栈空间是否足够，如果不足则扩展栈空间
    int SureStackSpace(int Count);

    /*====================压入数据====================*/

    void PushNil();
    void PushBoolean(LuaBol Value);
    void PushInteger(LuaInt Value);
    void PushNumber(LuaNum Value);
    void PushString(LuaStr Str, size_t Len = 0);
    void PushCFunction(LuaCFunc Func);
    void PushLightUserdata(void* Ptr);
    void PushExternalString(const char* Str, size_t Len = 0);

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

    /*====================表操作====================*/

    // 创建一个表
    void CreateTable(int Count = 0, int Records = 0);

    // (gettable/rawget)弹出key，读取指定索引的表的key值
    void ReadTableField(int IndexOfTableInStack, bool RawMode = false);

    // (settable/rawset)弹出key和value，设置指定索引的表的key值
    void WriteTableField(int IndexOfTableInStack, bool RawMode = false);

    // 获取表字符串键的值
    void GetField(int Index, const char* Key);

    // 设置表字符串键的值
    void SetField(int Index, const char* Key);

    // 获取表整数键的值
    void GetField(int Index, LuaInt Key);

    // 设置表整数键的值
    void SetField(int Index, LuaInt Key);

    // 获取全局变量
    void GetGlobal(const char* Name);

    // 设置全局变量
    void SetGlobal(const char* Name);

    // 获取元表
    bool GetMetatable(int Index);

    // 设置元表
    void SetMetatable(int Index);

    /*====================调用====================*/

    [[noreturn]]
    int Error(const char* errorMsg);
    // TODO: int ErrorFormated(const char* fmt, ...);
    [[noreturn]]
    int TypeError(int Index, const char* Expected);
    [[noreturn]]
    int ArgumentError(int Index, const char* extraMsg);

    LuaStatus LoadBuffer(const char* Buffer, size_t Size,
        const char* ChunkName = nullptr, const char* Mode = nullptr);

    void      Call(size_t ArgCount, size_t RetCount);
    LuaStatus SafeCall(size_t ArgCount, size_t RetCount, int ErrFuncIdx = 0);
    LuaStatus ExecuteScript(const char* Script);
    // TODO: LuaStatus ExecuteFile(const char* Filename);

    /*====================其它====================*/

    // 执行垃圾回收
    // TODO: void CollectGarbage();

    // 获取栈元素类型
    LuaType GetType(int Idx) const;

    // 获取栈元素类型名
    const char* GetTypeName(int Idx) const;

    // 获取LuaType对应的类型名
    const char* GetNameOfType(LuaType Type) const;

    bool IsNil(int Index) const;
    bool IsVoid(int Index) const; // nil和none都算void
    bool IsValid(int Index) const; // IsVoid的反义

    bool IsUserdata(int Index) const;
    bool IsFullUserdata(int Index) const;
    bool IsLightUserdata(int Index) const;

    bool IsInteger(int Index) const;

    
    template<typename T>
    T Get(int Index);

    template<typename T>
    T Cast(int Index);

    template<typename T>
    void Push(T Value);

    template<typename Callable>
    LuaCFunc CallableUnfold(Callable Object);

    template<typename Callable>
    void PushFunction(Callable Value);





private:
    LuaC::Info m_CInfo = {};
    bool m_External = false;
    bool m_ = false;
    lua_State* m_State = nullptr;

    std::vector<std::unique_ptr<LuaCFuncWrap>> m_CallableObjs;
    std::vector<LuaCFunc> m_VirtualFuncs;
    static int __stdcall ForwardProxy(LuaVM* self, LuaCFuncWrap* obj);

    LuaCFunc _UnfoldCallableObject(std::unique_ptr<LuaCFuncWrap> Object);

    inline LuaStatus Native2Status(int Status) const;
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
struct is_function_pointer : std::false_type {};

template<typename T>
struct is_function_pointer<T*> : std::is_function<T> {};

// 辅助别名（C++14）
template<typename T>
inline constexpr bool is_function_pointer_v = is_function_pointer<T>::value;


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
    else if constexpr (std::is_same_v<T, const char*>) {
        return CheckString(Index);
    }
    else if constexpr (std::is_same_v<T, void*>
        or std::is_same_v<T, const void*> or is_function_pointer_v<T>)
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
    else {
        //return luabridge::get<T>(m_State, Index);
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
        PushString(Value,strlen(Value));
    }
    else if constexpr (std::is_same_v<T, std::string>
        or std::is_same_v<T, std::string_view>) {
        PushString(Value.data(), Value.length());
    }
    else if constexpr (std::is_same_v<T, LuaCFunc>) {
        PushCFunction(Value);
    }
    else if constexpr (is_callable_v<T>
        or std::is_same_v<T, LuaCFuncWrap>
        or std::is_same_v<T, LuaCFuncWrap2>) {
        PushFunction(Value);
    }
    else if constexpr (std::is_pointer_v<T>) {
        //static_assert(false, "unknown type1");
        //luabridge::push(m_State, Value);
    }
    else {
        static_assert(false, "unknown type");
    }
}

template<typename Callable>
LuaCFunc LuaVM::CallableUnfold(Callable Object)
{
    auto CallableObj = std::make_unique<LuaCFuncWrap>(Object);
    return _UnfoldCallableObject(std::move(CallableObj));
}

template<typename Callable>
inline void LuaVM::PushFunction(Callable Value)
{
    PushCFunction(CallableUnfold(Value));
}