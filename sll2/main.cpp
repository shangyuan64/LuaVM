#include "sll2/sll2.hpp"
#include <chrono>
class TestBase2 {
public:
	int id = 100;
	int SetId(int i) {
		return id = i;
	};
};
inline long long CurrentSteadyMillis() {
	using namespace std::chrono;
	auto now = steady_clock::now();
	return duration_cast<milliseconds>(now.time_since_epoch()).count();
}

int main() {
	auto L = luaL_newstate();
	luaL_openlibs(L);
	SllProxy lua_proxy{L};

	lua_proxy.RegClass<TestBase2>("TestBase")
		.AddDefaultBulid().OpenGC()
		.AddStdFunc(&TestBase2::SetId,"SetId");
	auto start = CurrentSteadyMillis();
	auto is_ok = luaL_loadfile(L, "Spt/main.lua");
	if (is_ok == LUA_OK) {
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
			printf("main.lua run error:%s\n", lua_tostring(L, -1));
	}else printf("main.lua load error\n");
	auto end = CurrentSteadyMillis();
	printf("runtime:%lld\n", end - start);
	lua_close(L);
	lua_proxy.Detached();
	return 0;
}