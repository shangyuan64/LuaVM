#include "sll2.hpp"


ClassMemberRef::~ClassMemberRef() {
	switch (type) {
	case RefType::FUNC:delete value.func;
	}
}
inline void SllProxy::InitClassMap(){
	lua_newtable(L);
	lua_setglobal(L,"ClassMap");
}
SllProxy::SllProxy(lua_State* L) : L(L) {
	InitClassMap();
}

SllProxy::~SllProxy() {
	for (auto& p : proxys)delete p;proxys.clear();
}

lua_State* SllProxy::GetState() {
	return L;
}

void SllProxy::Detached() {
	L = nullptr;
	for (auto& p : proxys)p->NonClearLua();
}

LuaCProxy& SllProxy::_RegClass(C_Str name, ClassWrapper* cw){
	lua_getglobal(L, "ClassMap");lua_getfield(L, -1, name);
	LuaCProxy* proxy = nullptr;
	if (lua_islightuserdata(L, -1))
		proxy = static_cast<LuaCProxy*>(lua_touserdata(L,-1));
	lua_pop(L,1);
	if (!proxy) {
		proxy = new LuaCProxy(L,name,cw);
		proxys.push_back(proxy);
		lua_pushlightuserdata(L,proxy);
		lua_setfield(L,-2,name);
	}
	return *proxy;
}
inline static Value lua_cast_cpp(lua_State* L, int idx) {
	switch (lua_type(L,idx)) {
	case LUA_TNIL:return Value();
	case LUA_TNUMBER:
		return lua_isinteger(L,idx) ? lua_tointeger(L,idx) : lua_tonumber(L,idx);
	case LUA_TBOOLEAN:return (bool)lua_toboolean(L,idx);
	}
	return Value{};
};
inline static void pushvalue_tolua(lua_State* L, const Value& value) {
	if (auto p = std::get_if<long long>(&value))lua_pushinteger(L,*p);
	else if (auto p = std::get_if<double>(&value))lua_pushnumber(L,*p);
	else if (auto p = std::get_if<bool>(&value))lua_pushboolean(L,*p);
	else lua_pushnil(L);
}
thread_local static Value args_static[FastCallMax];
thread_local static std::vector<Value> args_hot;
static int __stdlcall(lua_State* L) {
	if (!lua_isuserdata(L, 1))return luaL_error(L, "the function is a member method!got a object(userdata)");
	auto self = *(void**)lua_touserdata(L, 1);
	auto func = (FunctionImp*)lua_touserdata(L,lua_upvalueindex(1));
	auto func_argcount = func->argcount();
	if (lua_gettop(L) <= func_argcount)return luaL_error(L, "arg num error");//参数数量校验
	Value* args = nullptr;
	if (func_argcount <= FastCallMax)args = args_static;
	else {
		if (args_hot.size() < func_argcount)args_hot.resize(func_argcount);
		args = args_hot.data();
	}
	for (size_t index = 1;index <= func_argcount;index++)
		args[index - 1] = lua_cast_cpp(L, index + 1);
	pushvalue_tolua(L, func->call(self, args));
	return 1;
}

static int __index(lua_State* L) {
	if (!lua_isstring(L,2))return luaL_error(L,"no has string key");
	lua_gettable(L,lua_upvalueindex(1)); //获取注册信息表
	if (!lua_istable(L,-1))return luaL_error(L, "no find this key");
	lua_rawgeti(L,-1,1); //获取注册信息
	auto refinfo = (ClassMemberRef*)lua_touserdata(L,-1);
	switch (refinfo->type) {
	case RefType::FUNC:
		lua_rawgeti(L, -2, 2);break;
	default:lua_pushnil(L);
	}
	return 1;
}
inline void LuaCProxy::InitMeta(int idx){
	idx = ToRawIdx(L,idx);
	lua_rawgeti(L, LUA_REGISTRYINDEX, rtable_ref);
	lua_pushcclosure(L,__index,1);
	lua_setfield(L,idx,"__index");
}
LuaCProxy::LuaCProxy(lua_State* L, C_Str name, ClassWrapper* cw) : L(L), cw(cw) {
	lua_newtable(L); //Class
	lua_pushvalue(L, -1);
	self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_newtable(L); //rtable
	lua_pushvalue(L, -1);
	rtable_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_setfield(L, -2, "rtable");
	lua_newtable(L); //meta
	lua_pushvalue(L, -1);
	meta_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	InitMeta(-1);
	lua_setfield(L, -2, "meta");
	lua_setglobal(L, name);
}

static inline void _unref(lua_State* L, int ref) {
	if (ref && L)luaL_unref(L, LUA_REGISTRYINDEX, ref); 
}
LuaCProxy::~LuaCProxy() {
	if (cw)delete cw;
	_unref(L, meta_ref), _unref(L, rtable_ref);
	for (auto& p : refs)delete p;refs.clear();
}

LuaCProxy& LuaCProxy::_AddStdFunc(FunctionImp* func, C_Str name){
	if (func && name) {
		auto ref = new ClassMemberRef();
		ref->type = RefType::FUNC;ref->value.func = func;
		refs.push_back(ref);
		lua_rawgeti(L,LUA_REGISTRYINDEX,rtable_ref);
		lua_pushstring(L, name);
		lua_createtable(L,2,0);
		lua_pushlightuserdata(L, ref);
		lua_rawseti(L, -2, 1);
		lua_pushlightuserdata(L, func);
		lua_pushcclosure(L, __stdlcall, 1);
		lua_rawseti(L, -2, 2);
		lua_rawset(L, -3);lua_pop(L,1);
	}
	return *this;
}

static int __default_build(lua_State* L) {
	auto* proxy = (LuaCProxy*)lua_touserdata(L,lua_upvalueindex(1));
	auto* obj = proxy->GetClass()->GetClass()->Build();
	*(void**)lua_newuserdata(L,sizeof(obj)) = obj;
	proxy->InitObject(-1);
	return 1;
};
LuaCProxy& LuaCProxy::AddDefaultBulid(){
	lua_rawgeti(L, LUA_REGISTRYINDEX,self_ref);
	lua_pushlightuserdata(L,this);
	lua_pushcclosure(L,__default_build,1);
	lua_setfield(L,-2, "new");
	lua_pop(L,1);
	return *this;
}
static int __gc(lua_State* L) {
	auto* proxy = (LuaCProxy*)lua_touserdata(L, lua_upvalueindex(1));
	auto* obj = *(void**)lua_touserdata(L, 1);
	proxy->GetClass()->GetClass()->Delete(obj);
	return 0;
}
LuaCProxy& LuaCProxy::OpenGC(){
	isgc = true;
	lua_rawgeti(L, LUA_REGISTRYINDEX, meta_ref);
	lua_pushlightuserdata(L,this);
	lua_pushcclosure(L,__gc,1);
	lua_setfield(L,-2,"__gc");
	lua_pop(L,1);
	return *this;
}

void LuaCProxy::NonClearLua(){
	L = 0;meta_ref = 0;rtable_ref = 0;
}

void LuaCProxy::InitObject(int idx){
	idx = ToRawIdx(L, idx);
	lua_rawgeti(L, LUA_REGISTRYINDEX, meta_ref);
	lua_setmetatable(L,-2);
}
