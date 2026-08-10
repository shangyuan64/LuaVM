// Test5.5.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <LuaVM.hpp>
#include <Lua/lua.hpp>
#include "LuaC5.5.hpp"
#include <cstdio>
#include <string>

int GlobalCounter = 10;
std::string GlobalName = "LuaVM";
std::string RootTag = "RootTag";

int Add(int A, int B)
{
    return A + B;
}

double Scale(double Value, double Factor)
{
    return Value * Factor;
}

std::string Join(const std::string& Left, const std::string& Right)
{
    return Left + Right;
}

const char* Echo(const char* Text)
{
    return Text;
}

int PlayerCount = 0;

class Player
{
public:
    Player(std::string name, int hp)
        : mName(std::move(name)), mHp(hp)
    {
        ++PlayerCount;
    }

    std::string GetName() const
    {
        return mName;
    }

    void SetName(const std::string& name)
    {
        mName = name;
    }

    int GetHp() const
    {
        return mHp;
    }

    void Damage(int amount)
    {
        mHp -= amount;
    }

    std::string mName;
    int mHp = 0;
};

int GetPlayerHp(Player* player)
{
    return player ? player->GetHp() : -1;
}

Player* CreatePlayer(const char* name, int hp)
{
    return new Player(name, hp);
}

int PlayerRawName(lua_State* L)
{
    auto* header = static_cast<LuaObjectHeader*>(lua_touserdata(L, 1));
    auto* player = static_cast<Player*>(header->Pointer);
    lua_pushstring(L, player->GetName().c_str());
    return 1;
}

struct Entity
{
    virtual ~Entity() = default;
    virtual const char* TypeName() const
    {
        return "Entity";
    }
};

struct Monster : Entity
{
    const char* TypeName() const override
    {
        return "Monster";
    }

    const char* Roar() const
    {
        return "Roar!";
    }
};

class GuiState
{
public:
    GuiState() = default;
    GuiState(const GuiState&) = delete;
    GuiState& operator=(const GuiState&) = delete;

    void Inc()
    {
        ++mValue;
    }

    int mValue = 5;
};

GuiState gGui;

struct WidgetBase
{
    std::string_view GetCaption() const
    {
        return "Caption";
    }
};

struct Button : WidgetBase
{
};

int CreateMonster(LuaVM* vm)
{
    auto* monster = new Monster;
    return vm->Stack.PushObjectWithClass(monster, "Monster", false) ? 1 : 0;
}

int CreateUnregistered(LuaVM* vm)
{
    vm->Stack.PushObjectWithClass(reinterpret_cast<void*>(1), "NotRegistered", false);
    return 1;
}

int main()
{
    system("chcp 65001 > nul");

    LuaVM vm;
    LUAC_55(vm.GetCInfo());
    vm.Startup();

    int Sig = 123;

    vm.Global()
        .RegFunction("Add", Add)
        .RegFunction("Scale", Scale)
        .RegFunction("Join", Join)
        .RegFunction("Echo", Echo)
        .RegFunction("AddLambda", [&](int A, int B) { Sig = 114514; return A + B; })
        .RegVariable("RootTag", &RootTag)
        .BeginNamespace("Math")
            .RegFunction("Add", Add)
            .BeginNamespace("Constants")
                .RegConstant("Pi", 3.14)
            .EndNamespace()
        .EndNamespace()
        .BeginNamespace("Config")
            .RegVariable("Counter", &GlobalCounter)
            .RegVariable("Name", &GlobalName)
            .RegConstant("Version", std::string("0.1.0"))
            .RegProperty("Double", []() -> int { return GlobalCounter * 2; })
        .EndNamespace();

    vm.Global()
        .BeginClass<Player>("Player")
            .RegConstructor<std::string, int>()
            .RegFunction("GetName", &Player::GetName)
            .RegFunction("SetName", &Player::SetName)
            .RegFunction("GetHp", &Player::GetHp)
            .RegFunction("Damage", &Player::Damage)
            .RegFunction("FreeHp", GetPlayerHp)
            .RegFunction("RawName", PlayerRawName)
            .RegProperty("Hp", &Player::mHp)
            .RegStaticFunction("Create", CreatePlayer)
            .RegStaticVariable("Count", &PlayerCount)
        .EndClass()
        .BeginClass<Entity>("Entity")
            .RegFunction("TypeName", &Entity::TypeName)
        .EndClass()
        .BeginClass<Monster>("Monster")
            .RegFunction("TypeName", &Monster::TypeName)
            .RegFunction("Roar", &Monster::Roar)
        .EndClass()
        .BeginClass<GuiState>("GuiState")
            .RegFunction("Inc", &GuiState::Inc)
            .RegProperty("Value", &GuiState::mValue)
        .EndClass()
        .BeginClass<Button>("Button")
            .RegConstructor<>()
            .RegFunction("GetCaption", &WidgetBase::GetCaption)
        .EndClass()
        .RegVariable("Gui", &gGui)
        .RegNativeFunction("CreateMonster", CreateMonster)
        .RegNativeFunction("CreateUnregistered", CreateUnregistered);
    

    luaL_openlibs(vm.GetState());
    auto status = vm.ExecuteScript(R"(
        print("Add", Add(2, 3))
        print("Scale", Scale(1.5, 2))
        print("Join", Join("Hello ", "Lua"))
        print("Echo", Echo("ok"))
        print("AddLambda", AddLambda(4, 5))
        print("MathAdd", Math.Add(1, 2))
        print("Pi", Math.Constants.Pi)
        print("Counter", Config.Counter)
        Config.Counter = 42
        print("Counter2", Config.Counter)
        print("Double", Config.Double)
        print("Name", Config.Name)
        Config.Name = "LuaVM 5.5"
        print("Name2", Config.Name)
        print("Version", Config.Version)
        print("RootTag", RootTag)
        RootTag = "RootTagChanged"
        print("RootTag2", RootTag)
        Player.CustomField = 42
        print("CustomField", Player.CustomField)
        Config.Injected = "yes"
        print("Injected", Config.Injected)
        local p = Player("Alice", 100)
        print("Name", p:GetName())
        print("Hp", p:GetHp())
        p:Damage(30)
        print("Damaged", p:GetHp())
        print("FreeHp", p:FreeHp())
        p.Hp = 77
        print("PropertyHp", p.Hp)
        print("RawName", p:RawName())
        local q = Player.Create("Bob", 90)
        print("StaticCreate", q:GetName(), q:GetHp())
        print("PlayerCount", Player.Count)
        local m = CreateMonster()
        print("MonsterType", m:TypeName())
        print("MonsterRoar", m:Roar())
        print("GuiValue", Gui.Value)
        Gui:Inc()
        print("GuiValue2", Gui.Value)
        local btn = Button()
        print("ButtonCaption", btn:GetCaption())
        local ok, err = pcall(CreateUnregistered)
        print("Unregistered", ok, err)
    )");
    if (!status) {
        printf("Lua执行失败！%s", status.ToString());
    }
    else {
        printf("C++ counter: %d\n", GlobalCounter);
    }
    printf("------------%d\n", Sig);
    return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
