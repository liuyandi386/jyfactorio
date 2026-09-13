// =====================================================================
// main.cpp —— 程序入口
//
// 创建游戏主控并进入主循环（对应 Python main.py 的 main()）。
// 帧率由 sf::Clock 计算dt + setFramerateLimit(60) 双重稳定。
//
// 附加：`factory-td.exe --selftest-save` 运行存档往返自检
// （保存→清空→读档→逐项核对，自动备份/恢复玩家存档），
// 退出码 0=全部通过，1=存在失败项。
//
// 启动顺序：读取用户设置 → 入口系统（启动动画/标题菜单/设置/加载）
// → 按入口选择创建 Game（新游戏 or 读档继续）→ 游戏主循环。
// 游戏内暂停面板选「返回主界面」时回到入口系统（循环重进，见下）。
// =====================================================================
#include <cmath>
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

namespace {

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
    // ME网络
    MeSystem::rebuildNetworks(game);
    check("ME网络已重建", !MeSystem::networks().empty());
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

    check("ME网络物品(铁锭10+铜锭6)", !MeSystem::networks().empty() &&
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
    // 最先读取用户设置：入口系统与游戏窗口都据此决定显示模式
    // 首次运行（或配置文件损坏）时写入一份默认设置，方便玩家直接编辑
    if (!gset::load("saves/settings.json")) gset::save("saves/settings.json");

    if (argc > 1 && std::string(argv[1]) == "--selftest-save")
        return runSelftestSave();

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
            Game game;                      // 初始化窗口/资源/地形/矿点
            if (action == EntryAction::Continue) game.loadGame();   // 继续上次存档
            game.run();                     // 游戏主循环
            if (!game.returnToMenu) return 0;   // 关窗 = 正常退出
        }
        // returnToMenu = true → 回到主菜单，再走一遍入口系统
    }
}