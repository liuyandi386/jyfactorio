// =====================================================================
// main.cpp —— 程序入口
//
// 创建游戏主控并进入主循环（对应 Python main.py 的 main()）。
// 帧时间由 sf::Clock 计算 dt 驱动（与帧率无关）；
// 帧率/垂直同步策略统一在 gset::applyFrameMode（见 Settings.h），
// 默认垂直同步 → 跟随显示器刷新率（144Hz 屏即 144 帧）。
//
// 附加命令行参数：
//   --selftest-save  运行存档往返自检（保存→清空→读档→逐项核对，
//                    自动备份/恢复玩家存档），退出码 0=全部通过，1=存在失败项
//   --safe-mode      黑屏应急启动：强制 1280×720 窗口化 + 垂直同步
//                    （不写回 saves/settings.json，下次正常启动仍用原设置）
//
// 启动顺序：开启高 DPI 感知 → 读取用户设置 → 入口系统（启动动画/标题菜单/设置/加载）
// → 按入口选择创建 Game（新游戏 or 读档继续）→ 游戏主循环。
// 游戏内暂停面板选「返回主界面」时回到入口系统（循环重进，见下）。
// =====================================================================
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Game.h"
#include "SaveSystem.h"
#include "Settings.h"
#include "ui/EntrySystem.h"
#include "systems/EnemySystem.h"
#include "systems/MeSystem.h"
#include "components/Me.h"

#ifdef _WIN32
// 放在 SFML 头之后包含，避免 windows.h 的宏污染 SFML（与 Settings.cpp 一致）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// ---------------------------------------------------------------------
// 高 DPI 感知（必须在创建任何窗口之前调用）
// ---------------------------------------------------------------------
// 不做这一步时进程是"DPI 不感知"的：在 125%/150% 缩放的桌面上（2K/4K 高刷屏很常见）
// 系统会把桌面虚拟化成更小的逻辑尺寸，sf::VideoMode::getDesktopMode() 拿到的尺寸
// 与实际屏幕不一致 —— 无边框全屏窗口于是只占画面一角/露出桌面，
// 部分显卡驱动下还会因为后台缓冲尺寸与窗口客户区不匹配而整屏黑掉。
//
// 用 GetProcAddress 动态取地址，避免依赖新版本 Windows SDK 的符号：
//   -4 = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2（Win10 1703+）
//   退化路径 SetProcessDPIAware（Vista+）也聊胜于无。
void enableHighDpiAwareness() {
#ifdef _WIN32
    const HMODULE user32 = ::LoadLibraryW(L"user32.dll");
    if (!user32) return;
    using SetCtxFn   = BOOL(WINAPI*)(void*);
    using SetAwareFn = BOOL(WINAPI*)(void);
    if (auto setCtx = reinterpret_cast<SetCtxFn>(
            ::GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
        if (setCtx(reinterpret_cast<void*>(static_cast<std::intptr_t>(-4)))) return;
    }
    if (auto setAware = reinterpret_cast<SetAwareFn>(
            ::GetProcAddress(user32, "SetProcessDPIAware")))
        setAware();
#endif
}


struct Check {
    bool ok = false;
    std::string name;
};
std::vector<Check> checks;

void check(const std::string& name, bool ok) { checks.push_back({ok, name}); }

/// 找指定类型建筑的实体
entt::entity findBuilding(Game& g, cfg::BuildingType t) {
    for (auto [e, b] : g.reg.view<Building>().each())
        if (b.type == t) return e;
    return entt::null;
}

/// 建筑实体计数
int buildingCount(Game& g, cfg::BuildingType t) {
    int n = 0;
    for (auto [e, b] : g.reg.view<Building>().each())
        if (b.type == t) n++;
    return n;
}

int runSelftestSave() {
    namespace fs = std::filesystem;
    // ---- 备份玩家存档（自检会覆盖 saves/factory_td.json） ----
    const std::string save = "saves/factory_td.json";
    const std::string bak = "saves/factory_td.json.selftest_bak";
    std::error_code ec;
    fs::remove(bak, ec);
    const bool hadSave = fs::exists(save);
    if (hadSave) fs::copy_file(save, bak, ec);

    Game game;   // 初始化窗口/资源/地形/矿点

    // ---- 布置场景：覆盖全部建筑类型与状态 ----
    entt::entity miner = game.placeBuilding(50, 50, cfg::BuildingType::Miner, cfg::Dir::RIGHT, false);
    entt::entity furnace = game.placeBuilding(52, 50, cfg::BuildingType::Furnace, cfg::Dir::DOWN, false);
    entt::entity pipe = game.placeBuilding(51, 50, cfg::BuildingType::Pipe, 0, false);
    entt::entity bucket = game.placeBuilding(54, 50, cfg::BuildingType::Bucket, cfg::Dir::LEFT, false);
    entt::entity wire = game.placeBuilding(56, 50, cfg::BuildingType::PowerWire, 0, false);
    entt::entity tower = game.placeBuilding(58, 50, cfg::BuildingType::TowerBasic, 5, false);
    entt::entity assembler = game.placeBuilding(60, 50, cfg::BuildingType::Assembler, cfg::Dir::UP, false);
    entt::entity splitter = game.placeBuilding(62, 50, cfg::BuildingType::Splitter, 0, false);
    entt::entity alloy = game.placeBuilding(64, 50, cfg::BuildingType::AlloyFurnace, cfg::Dir::RIGHT, false);
    entt::entity gen = game.placeBuilding(66, 50, cfg::BuildingType::PowerGenerator, cfg::Dir::DOWN, false);
    entt::entity meI = game.placeBuilding(68, 50, cfg::BuildingType::MeInterface, 0, false);
    entt::entity meD = game.placeBuilding(69, 50, cfg::BuildingType::MeDrive, 0, false);
    entt::entity meT = game.placeBuilding(70, 50, cfg::BuildingType::MeTerminal, 0, false);
    check("放置13种建筑", miner != entt::null && furnace != entt::null && pipe != entt::null &&
                            bucket != entt::null && wire != entt::null && tower != entt::null &&
                            assembler != entt::null && splitter != entt::null && alloy != entt::null &&
                            gen != entt::null && meI != entt::null && meD != entt::null && meT != entt::null);

    // 机器库存 / 任务状态
    game.reg.get<Inventory>(miner).add(cfg::ItemType::CopperOre, 123);
    game.reg.get<Machine>(miner).acc = 1.5f;
    game.reg.get<Inventory>(furnace).add(cfg::ItemType::IronOre, 7);
    game.reg.get<Machine>(furnace).hasJob = true;
    game.reg.get<Machine>(furnace).recipeId = 1;
    game.reg.get<Machine>(furnace).jobTime = 0.8f;
    game.reg.get<Machine>(furnace).jobTotal = 3.0f;
    game.reg.get<Machine>(assembler).recipeId = 1;
    game.reg.get<Inventory>(assembler).add(cfg::ItemType::IronIngot, 5);
    game.reg.get<Inventory>(alloy).add(cfg::ItemType::SteelIngot, 3);
    // 管道缓冲
    auto& pbuf = game.reg.get<Pipe>(pipe).buffer;
    pbuf.push_back(cfg::ItemType::GoldOre);
    pbuf.push_back(cfg::ItemType::LeadOre);
    pbuf.push_back(cfg::ItemType::NickelOre);
    // 储物桶
    auto& bk = game.reg.get<Bucket>(bucket);
    bk.items.push_back(cfg::ItemType::DiamondOre);
    bk.items.push_back(cfg::ItemType::SilverOre);
    bk.outputTimer = 0.42f;
    // 电线面配置（旋转过的面必须保留）
    game.reg.get<FaceConfig>(wire).set(cfg::Dir::UP, cfg::FaceMode::OUTPUT);
    game.reg.get<FaceConfig>(wire).set(cfg::Dir::LEFT, cfg::FaceMode::INPUT);
    // 塔
    game.reg.get<Turret>(tower).ammo = 42;
    game.reg.get<Turret>(tower).barrelDir = 5;
    // 分流器
    auto& sp = game.reg.get<SplitterQueue>(splitter);
    sp.queue.push_back(cfg::ItemType::Coal);
    sp.queue.push_back(cfg::ItemType::Ammo);
    sp.outputIndex = 2;
    // 发电机
    game.reg.get<Inventory>(gen).add(cfg::ItemType::Coal, 9);
    game.reg.get<PowerGeneratorNode>(gen).fuelTime = 1.5f;
    // 通物网络
    MeSystem::rebuildNetworks(game);
    check("通物网络已重建", !MeSystem::networks().empty());
    if (!MeSystem::networks().empty()) {
        MeSystem::addItem(MeSystem::networksMutable().front(), cfg::ItemType::IronIngot, 10);
        MeSystem::addItem(MeSystem::networksMutable().front(), cfg::ItemType::CopperIngot, 6);
    }
    // 全局状态
    game.gold = 777;
    game.lives = 5;
    game.camera.x = 1234.5f;
    game.camera.y = 678.25f;
    EnemySystem::spawnEnemy(game, cfg::EnemyType::Fast);

    const size_t oreCountBefore = static_cast<size_t>(std::distance(
        game.reg.view<GridPos, OreDeposit>().begin(), game.reg.view<GridPos, OreDeposit>().end()));

    // ---- 保存 ----
    check("saveGame()成功", ::saveGame(game));

    // ---- 手动清空整个世界（模拟重新进游戏） ----
    game.reg.clear();
    for (auto& cell : game.grid.cells) cell.building = entt::null;

    // ---- 读档 ----
    check("loadGame()成功", ::loadGame(game));

    // ---- 逐项核对 ----
    check("建筑总数13", buildingCount(game, cfg::BuildingType::TowerBasic) == 1 &&
                        buildingCount(game, cfg::BuildingType::Miner) == 1 &&
                        buildingCount(game, cfg::BuildingType::Furnace) == 1 &&
                        buildingCount(game, cfg::BuildingType::Pipe) == 1 &&
                        buildingCount(game, cfg::BuildingType::Bucket) == 1 &&
                        buildingCount(game, cfg::BuildingType::PowerWire) == 1 &&
                        buildingCount(game, cfg::BuildingType::Assembler) == 1 &&
                        buildingCount(game, cfg::BuildingType::Splitter) == 1 &&
                        buildingCount(game, cfg::BuildingType::AlloyFurnace) == 1 &&
                        buildingCount(game, cfg::BuildingType::PowerGenerator) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeInterface) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeDrive) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeTerminal) == 1);

    entt::entity f2 = findBuilding(game, cfg::BuildingType::Furnace);
    check("熔炉库存(铁矿石×7)", f2 != entt::null &&
        game.reg.get<Inventory>(f2).count(cfg::ItemType::IronOre) == 7);
    check("熔炉任务状态", f2 != entt::null && game.reg.get<Machine>(f2).hasJob &&
        game.reg.get<Machine>(f2).recipeId == 1 &&
        std::abs(game.reg.get<Machine>(f2).jobTime - 0.8f) < 0.001f);

    entt::entity m2 = findBuilding(game, cfg::BuildingType::Miner);
    check("采矿机库存(铜矿石×123)", m2 != entt::null &&
        game.reg.get<Inventory>(m2).count(cfg::ItemType::CopperOre) == 123);

    entt::entity p2 = findBuilding(game, cfg::BuildingType::Pipe);
    check("管道缓冲3件", p2 != entt::null && game.reg.get<Pipe>(p2).buffer.size() == 3);

    entt::entity b2 = findBuilding(game, cfg::BuildingType::Bucket);
    check("储物桶2件+计时器", b2 != entt::null && game.reg.get<Bucket>(b2).items.size() == 2 &&
        std::abs(game.reg.get<Bucket>(b2).outputTimer - 0.42f) < 0.001f);

    entt::entity w2 = findBuilding(game, cfg::BuildingType::PowerWire);
    check("电线面配置保留", w2 != entt::null &&
        game.reg.get<FaceConfig>(w2).get(cfg::Dir::UP) == cfg::FaceMode::OUTPUT &&
        game.reg.get<FaceConfig>(w2).get(cfg::Dir::LEFT) == cfg::FaceMode::INPUT);

    entt::entity t2 = findBuilding(game, cfg::BuildingType::TowerBasic);
    check("塔弹药+炮管朝向", t2 != entt::null && game.reg.get<Turret>(t2).ammo == 42 &&
        game.reg.get<Turret>(t2).barrelDir == 5);

    entt::entity a2 = findBuilding(game, cfg::BuildingType::Assembler);
    check("组装机配方+库存", a2 != entt::null && game.reg.get<Machine>(a2).recipeId == 1 &&
        game.reg.get<Inventory>(a2).count(cfg::ItemType::IronIngot) == 5);

    entt::entity s2 = findBuilding(game, cfg::BuildingType::Splitter);
    check("分流器队列2件+轮询索引", s2 != entt::null &&
        game.reg.get<SplitterQueue>(s2).queue.size() == 2 &&
        game.reg.get<SplitterQueue>(s2).outputIndex == 2);

    entt::entity g2 = findBuilding(game, cfg::BuildingType::PowerGenerator);
    check("发电机煤×9+燃料", g2 != entt::null &&
        game.reg.get<Inventory>(g2).count(cfg::ItemType::Coal) == 9 &&
        std::abs(game.reg.get<PowerGeneratorNode>(g2).fuelTime - 1.5f) < 0.001f);

    check("通物网络物品(铁锭10+铜锭6)", !MeSystem::networks().empty() &&
        MeSystem::countItem(MeSystem::networks().front(), cfg::ItemType::IronIngot) == 10 &&
        MeSystem::countItem(MeSystem::networks().front(), cfg::ItemType::CopperIngot) == 6);

    check("矿点数量一致", static_cast<size_t>(std::distance(
        game.reg.view<GridPos, OreDeposit>().begin(), game.reg.view<GridPos, OreDeposit>().end())) == oreCountBefore);
    check("金币777", game.gold == 777);
    check("生命5", game.lives == 5);
    check("摄像机位置", std::abs(game.camera.x - 1234.5f) < 0.01f &&
        std::abs(game.camera.y - 678.25f) < 0.01f);
    check("敌人1个(快速)", [&] {
        int n = 0; bool fast = false;
        for (auto [e, en] : game.reg.view<Enemy>().each()) { n++; fast = en.type == cfg::EnemyType::Fast; }
        return n == 1 && fast;
    }());

    // ---- 恢复玩家存档 ----
    fs::remove(save, ec);
    if (hadSave) fs::rename(bak, save, ec);

    // ---- 汇总 ----
    int fails = 0;
    for (const auto& c : checks) {
        std::printf("[%s] %s\n", c.ok ? "PASS" : "FAIL", c.name.c_str());
        if (!c.ok) fails++;
    }
    std::printf("存档往返自检: %zu/%zu 通过\n", checks.size() - static_cast<size_t>(fails),
                checks.size());
    return fails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    // ⚠ 必须在创建任何窗口之前：开启高 DPI 感知（高刷屏常配 125%/150% 缩放）
    enableHighDpiAwareness();

    // 最先读取用户设置：入口系统与游戏窗口都据此决定显示模式
    // 首次运行（或配置文件损坏）时写入一份默认设置，方便玩家直接编辑
    if (!gset::load("saves/settings.json")) gset::save("saves/settings.json");

    // 黑屏应急：强制窗口化 + 垂直同步，绕开"无边框全屏 + 置顶窗口"这条路径。
    // 刻意不 save()：只在本次运行生效，玩家的 saves/settings.json 保持原样。
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--safe-mode" || a == "--windowed") {
            gset::get().displayMode = gset::DisplayMode::Window1280x720;
            gset::get().frameMode   = gset::FrameMode::Vsync;
        } else if (a == "--selftest-save") {
            return runSelftestSave();
        }
    }

    // ---- 游戏入口：启动动画 → 标题/主菜单 → 设置 → 加载 → 游戏 ----
    // 循环：游戏内暂停面板选「返回主界面」后重新进入入口系统（等价于回到上一级菜单）
    while (true) {
        EntryAction action = EntryAction::NewGame;
        {
            EntrySystem entry;              // 自带窗口；离开作用域即销毁
            action = entry.run();
        }
        if (action == EntryAction::Quit) return 0;

        {
            // 两种模式完全独立、互不嵌套：由主菜单各自的入口按钮决定走哪条流程
            const GameMode mode = (action == EntryAction::Tutorial) ? GameMode::Tutorial
                                                                    : GameMode::Normal;
            Game game(mode);                // 初始化窗口/资源/地形/矿点
            if (action == EntryAction::Continue) game.loadGame();   // 普通关卡 · 读取上次存档
            game.run();                     // 游戏主循环
            if (!game.returnToMenu) return 0;   // 关窗 = 正常退出
        }
        // returnToMenu = true → 回到主菜单，再走一遍入口系统
    }
}