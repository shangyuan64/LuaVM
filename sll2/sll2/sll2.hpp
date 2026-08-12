#pragma once
#include "pch.h"

#define ToRawIdx(L,idx) (idx > 0 ? idx : (lua_gettop(L) + idx + 1));
#define FastCallMax 32
using C_Str = const char*;

enum class RefType {
	NONE,
	CFUNC,
	FUNC
};

struct ClassMemberRef{
	union {
		//LuaClassFuncBaseWrapper* cfunc = nullptr;
		FunctionImp* func = nullptr;
	} value;
	RefType type = RefType::NONE;
	~ClassMemberRef();
};

class LuaCProxy { //Lua C++ Class Proxy
private:
	ClassWrapper* cw;
	lua_State* L;
	int meta_ref = 0, rtable_ref = 0,self_ref = 0;
	bool isgc = false;
	std::vector<ClassMemberRef*> refs;
	inline void InitMeta(int idx);
	LuaCProxy& _AddStdFunc(FunctionImp*, C_Str);
public:
	LuaCProxy(lua_State*,C_Str,ClassWrapper*);
	~LuaCProxy();
	LuaCProxy& AddDefaultBulid();
	LuaCProxy& OpenGC();
	void NonClearLua();
	void InitObject(int);
	inline ClassWrapper* GetClass() const { return cw; }

	template<typename Class, typename Ret, typename... Args>
	LuaCProxy& AddStdFunc(Ret(Class::* f)(Args ...), C_Str name) {
		return _AddStdFunc(new FuncWrapper(f), name);
	}

	
	
};
class SllProxy { //Sera Link Lua Proxy
private:
	lua_State* L;
	std::vector<LuaCProxy*> proxys{};
	inline void InitClassMap();
	LuaCProxy& _RegClass(C_Str, ClassWrapper*);
public:
	SllProxy(lua_State*);
	~SllProxy();
	lua_State* GetState();
	void Detached();
	template<typename Class>
	LuaCProxy& RegClass(C_Str class_name) {
		return _RegClass(class_name,ClassWrapper::Create<Class>());
	};
protected:

};