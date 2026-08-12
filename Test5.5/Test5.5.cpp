// Test5.5.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <LuaVM.hpp>
#include <Lua/lua.hpp>
#include "LuaC5.5.hpp"

struct MyClass {

};


MyClass* Test0() {
    return {};
}

void Test1(int A) {
    return;
}

void Test2(float A) {
    return;
}


int main()
{
    system("chcp 65001 > nul");
    
    LuaVM vm;
    LUAC_55(vm.GetCInfo());
    vm.Startup();

    luaL_openlibs(vm.GetState());
    auto status = vm.ExecuteScript("");
    if (status != LuaStatus::Ok) {
        printf("Lua执行失败！%s", vm.ToString(-1));
        vm.Popup();
    }
}
