// =====================================================================
// Game.cpp —— 游戏主控实现
// 对应 Python core/Game.py（主循环）+ main.py GameScene（场景总控）。
// =====================================================================
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <random>
#include <set>
#include "ConfigLoader.h"
#include "ui/GameUI.h"
#include "systems/PipeSystem.h"
#include "systems/MeSystem.h"
#include "systems/MachineSystem.h"
#include "systems/TurretSystem.h"
#include "systems/EnemySystem.h"
#include "systems/ItemSystem.h"
#include "systems/PlayerSystem.h"
#include "systems/Narrative.h"
#include "systems/RenderSystem.h"
#include "SaveSystem.h"
#include "Settings.h"
#include "utils/Pathfinder.h"
#include "utils/Profiler.h"

// ---------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------
Game::Game(GameMode m) : mode(m) {
    // 按启动菜单里选择的显示模式创建窗口（窗口化分辨率 / 无边框全屏）
    // 统一走 gset::applyToWindow：全屏用无边框窗口实现，不用独占全屏（避免闪屏黑屏/鼠标漂移）
    gset::applyToWindow(window, cfg::SCREEN_TITLE);

    // 先加载 JSON 配置（覆盖塔/敌人/配方/成本等数值，改数值无需重编译）
    cfg::loadConfig("assets/config.json");

    // 金币/生命在 member initializer 里用的是旧默认值，loadConfig 之后需再取一次
    // 否则 initial_gold / initial_lives 改配置文件不会生效（一直是内置 500/10）
    gold = cfg::INITIAL_GOLD;
    lives = cfg::INITIAL_LIVES;

    // 初始化UI
    ui = new GameUI();
    ui->init(this);

    // 加载资源（贴图+字体）
    assets.load("assets");

    // 摄像机：先设置屏幕尺寸(窗口创建后的实际大小)，再定位地图中心
    camera.setScreenSize(static_cast<float>(window.getSize().x),
                         static_cast<float>(window.getSize().y));
    camera.init();
    camera.speedScale = gset::cameraSpeedScale();   // 应用"摄像机速度"设置

    // 生成地形/路径/矿点/玩家资源/敌人路径点
    generateTerrain();
    generateOreDeposits();
    initPlayerInventory();
    enemyWaypoints = buildPixelWaypoints(cfg::PATH_POINTS, cfg::TILE_SIZE);

    // 帧率/垂直同步：统一走 gset::applyFrameMode（默认垂直同步 → 跟随显示器刷新率，
    // 144Hz 屏即 144 帧；旧的 setFramerateLimit(60) 会强制关掉垂直同步并锁 60 帧）
    gset::applyFrameMode(window);

    // ---- 模式化入口（两种模式完全独立，互不嵌套）----
    //   · 新手教程：由主菜单「新手教程」按钮进入 → 独立教学关卡
    //   · 普通关卡：由「普通关卡 / 继续游戏」进入 → 不接入引导系统，也不被 F2 拉起
    // 引导层是否出现只取决于本局模式（Game::mode），与"是否存在主存档"无关。
    if (mode == GameMode::Tutorial) {
        // 教程进度存于独立的 saves/tutorial.json，与主存档 factory_td.json 完全隔离：
        // 有未完成进度则续接，否则（无进度 / 上次已学完）从第一步重新教起。
        if (!tutorial::loadProgressFile(tutorial) || !tutorial.active) {
            tutorial::begin(*this);
        } else {
            ui->showToast("新手教程已续接上次进度 · F2 跳过");
        }
    }

    // ---- 叙事层：着陆播报（织女星的第一次开口）----
    // 教程模式自带完整旁白，这里不叠加，避免两套声音打架。
    narrative::reset();
    if (mode == GameMode::Normal) {
        narrative::announce(*this, narrative::Event::Landing);
    }
}

Game::~Game() { delete ui; }

// ---------------------------------------------------------------------
// 地形生成（Python GameMap._generate_path）
// ---------------------------------------------------------------------
void Game::generateTerrain() {
    terrain.assign(static_cast<size_t>(grid.w) * grid.h, 0);  // 默认草地
    const auto pathTiles = buildPathTiles(cfg::PATH_POINTS, grid.w, grid.h);
    for (const auto& p : pathTiles)
        terrain[static_cast<size_t>(p.y) * grid.w + p.x] = 1;  // 路径
}

// ---------------------------------------------------------------------
// 矿点生成（固定种子42，8种矿石全图随机混布、避开路径、互不重叠；储量模式见 cfg::ORE_INFINITE）
// ---------------------------------------------------------------------
void Game::generateOreDeposits() {
    std::mt19937 rng(cfg::ORE_RANDOM_SEED);
    std::uniform_int_distribution<int> rx(cfg::ORE_X_MIN, cfg::ORE_X_MAX);
    std::uniform_int_distribution<int> ry(cfg::ORE_Y_MIN, cfg::ORE_Y_MAX);
    std::set<std::pair<int, int>> placed;   // 已占用的矿点位置（防重叠）

    auto gen = [&](cfg::ItemType type, int count) {
        int ok = 0;
        int attempts = 0;
        while (ok < count && attempts < count * 100) {
            attempts++;
            const int x = rx(rng), y = ry(rng);
            if (terrain[static_cast<size_t>(y) * grid.w + x] == 1) continue;  // 避开路径
            if (placed.count({x, y})) continue;                               // 不与其他矿重叠
            const auto e = reg.create();
            reg.emplace<GridPos>(e, x, y);
            reg.emplace<OreDeposit>(e, type, cfg::ORE_DEPOSIT_AMOUNT);        // 初始储量（无限模式不消耗）
            placed.insert({x, y});
            ok++;
        }
    };
    // 8 种矿石：每种独立矿点，随机散布全图
    gen(cfg::ItemType::IronOre, cfg::ORE_IRON_COUNT);
    gen(cfg::ItemType::CopperOre, cfg::ORE_COPPER_COUNT);
    gen(cfg::ItemType::Coal, cfg::ORE_COAL_COUNT);
    gen(cfg::ItemType::GoldOre, cfg::ORE_GOLD_COUNT);
    gen(cfg::ItemType::DiamondOre, cfg::ORE_DIAMOND_COUNT);
    gen(cfg::ItemType::NickelOre, cfg::ORE_NICKEL_COUNT);
    gen(cfg::ItemType::SilverOre, cfg::ORE_SILVER_COUNT);
    gen(cfg::ItemType::LeadOre, cfg::ORE_LEAD_COUNT);
}

// ---------------------------------------------------------------------
// 玩家资源（测试版无限资源）
// ---------------------------------------------------------------------
void Game::initPlayerInventory() {
    playerInv.clear();
    // 背包：所有物品 + 机器各 100000（测试版预置，正式版需从商店购买）
    for (int i = 0; i < cfg::ITEM_COUNT; ++i)
        playerInv[static_cast<cfg::ItemType>(i)] = 100000;
    for (int i = 0; i < cfg::BUILDING_COUNT; ++i)
        backpackMachines[static_cast<size_t>(i)] = 100000;
}

// ---------------------------------------------------------------------
// 成本检查（Python _can_afford / _deduct_cost）
// ---------------------------------------------------------------------
bool Game::canAfford(cfg::BuildingType t) const {
    // 背包机器数 > 0 即可放置（材料成本已由"机器成品"替代）
    return backpackMachines[static_cast<size_t>(t)] > 0;
}

void Game::deductCost(cfg::BuildingType t) {
    if (backpackMachines[static_cast<size_t>(t)] > 0)
        backpackMachines[static_cast<size_t>(t)]--;
}

void Game::refundCost(cfg::BuildingType t) {
    backpackMachines[static_cast<size_t>(t)]++;
}

// ---------------------------------------------------------------------
// 占用与可放置检查
// ---------------------------------------------------------------------
bool Game::isOccupied(int tx, int ty) const {
    return grid.inBounds(tx, ty) && grid.at(tx, ty).building != entt::null;
}

bool Game::canPlace(int tx, int ty, cfg::BuildingType t) const {
    // 占地：旧版发电机2×2，其余1×1
    const int w = (t == cfg::BuildingType::Generator) ? 2 : 1;
    const int h = (t == cfg::BuildingType::Generator) ? 2 : 1;
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx) {
            const int x = tx + dx, y = ty + dy;
            if (!grid.inBounds(x, y)) return false;
            // 地形检查：所有建筑要求草地（管道可铺在任意地形）
            if (t != cfg::BuildingType::Pipe &&
                terrain[static_cast<size_t>(y) * grid.w + x] != 0)
                return false;
            // 占用检查
            if (grid.at(x, y).building != entt::null) return false;
        }
    return true;
}

// ---------------------------------------------------------------------
// 建筑放置（Python _place_building）
// ---------------------------------------------------------------------
entt::entity Game::placeBuilding(int tx, int ty, cfg::BuildingType t, int dir, bool deduct) {
    (void)deduct;  // 成本由调用方处理（Python: _deduct_cost在放置成功后调用）
    if (!canPlace(tx, ty, t)) {
        ui->showToast("该位置已被占用!");
        return entt::null;
    }

    // ---- 创建实体与组件 ----
    const entt::entity e = reg.create();
    auto& b = reg.emplace<Building>(e);
    b.type = t;
    b.pos = {tx, ty};
    b.dir = dir % 4;

    switch (t) {
        case cfg::BuildingType::TowerBasic:
        case cfg::BuildingType::TowerRapid:
        case cfg::BuildingType::TowerSniper:
        case cfg::BuildingType::TowerElectric: {
            auto& turret = reg.emplace<Turret>(e);
            const size_t idx = t == cfg::BuildingType::TowerBasic ? 0
                             : t == cfg::BuildingType::TowerRapid ? 1
                             : t == cfg::BuildingType::TowerSniper ? 2 : 3;
            turret.type = static_cast<cfg::TurretType>(idx);
            const auto& st = cfg::TURRET_STATS[idx];
            turret.range = st.range;
            turret.damage = st.damage;
            turret.fireRate = st.fireRate;
            turret.bulletSpeed = st.bulletSpeed;
            turret.isElectric = (idx == 3);
            turret.barrelDir = static_cast<uint8_t>(dir % 8);  // 放置时选定炮管朝向
            if (turret.isElectric) {
                // 电力塔：接入电网（8EU/秒），内部电力100，初始为0
                turret.maxPower = cfg::ELECTRIC_TOWER_MAX_POWER;
                turret.power = 0.0f;
                turret.gridPowered = false;
                reg.emplace<PowerConsumer>(e, cfg::ELECTRIC_TOWER_GRID_NEED, false);
            } else {
                // 弹药塔：初始弹药0，需传送带补给（Python行为）
                turret.maxPower = 50.0f;
                turret.power = 50.0f;
                reg.emplace<Inventory>(e, cfg::TURRET_AMMO_MAX_SLOTS, cfg::TURRET_AMMO_MAX_STACK);
            }
            break;
        }
        case cfg::BuildingType::Miner:
        case cfg::BuildingType::MinerL2:
        case cfg::BuildingType::MinerL3:
        case cfg::BuildingType::MinerVoid: {
            // GT式采矿场：采集 5×5/9×9/13×13 范围内所有类型矿石；
            // 虚空采矿场无需矿点，每秒产出全类型矿石
            auto& m = reg.emplace<Machine>(e);
            m.kind = MachineKind::Miner;
            m.level = (t == cfg::BuildingType::MinerL2) ? 2
                    : (t == cfg::BuildingType::MinerL3) ? 3 : 1;
            m.voidMiner = (t == cfg::BuildingType::MinerVoid);
            m.rate = (t == cfg::BuildingType::MinerL2) ? cfg::MINER_RATE_L2
                   : (t == cfg::BuildingType::MinerL3) ? cfg::MINER_RATE_L3
                   : (t == cfg::BuildingType::MinerVoid) ? cfg::VOID_MINER_RATE
                                                         : cfg::MINER_RATE_L1;
            m.powered = true;   // 测试版免供电（MINER_FREE_POWER）
            reg.emplace<Inventory>(e, cfg::MINER_MAX_SLOTS * 4, cfg::MINER_MAX_STACK * 8);
            // 面配置：仅放置方向为OUTPUT
            auto fc = FaceConfig::makeAll(cfg::FaceMode::NONE, 3);
            fc.set(dir % 4, cfg::FaceMode::OUTPUT);
            reg.emplace<FaceConfig>(e, fc);
            if (!cfg::MINER_FREE_POWER)
                reg.emplace<PowerConsumer>(e, cfg::MINER_POWER_NEED, false);
            break;
        }
        case cfg::BuildingType::Furnace: {
            // 熔炉（数据驱动 FURNACE_RECIPES）：冶炼矿石成锭；放置方向=输出面方向
            auto& m = reg.emplace<Machine>(e);
            m.kind = MachineKind::Furnace;
            reg.emplace<Inventory>(e, cfg::FURNACE_MAX_SLOTS, cfg::FURNACE_MAX_STACK);
            auto fc = FaceConfig::makeAll(cfg::FaceMode::INPUT, 3);
            fc.set(dir % 4, cfg::FaceMode::OUTPUT);   // 选定方向输出，其余三面输入
            reg.emplace<FaceConfig>(e, fc);
            break;
        }
        case cfg::BuildingType::AlloyFurnace: {
            // 合金炉（数据驱动 ALLOY_RECIPES）：锭 → 合金（选定配方，无需电力）；放置方向=输出面
            auto& m = reg.emplace<Machine>(e);
            m.kind = MachineKind::AlloyFurnace;
            reg.emplace<Inventory>(e, cfg::ALLOY_MAX_SLOTS, cfg::ALLOY_MAX_STACK);
            auto fc = FaceConfig::makeAll(cfg::FaceMode::INPUT, 3);
            fc.set(dir % 4, cfg::FaceMode::OUTPUT);
            reg.emplace<FaceConfig>(e, fc);
            break;
        }
        case cfg::BuildingType::Assembler: {
            // 组装机（替代原弹药制造机）：2铁锭+1铜锭→1弹药；放置方向=输出面方向
            auto& m = reg.emplace<Machine>(e);
            m.kind = MachineKind::Assembler;
            reg.emplace<Inventory>(e, cfg::ASSEMBLER_MAX_SLOTS, cfg::ASSEMBLER_MAX_STACK);
            auto fc = FaceConfig::makeAll(cfg::FaceMode::INPUT, 3);
            fc.set(dir % 4, cfg::FaceMode::OUTPUT);
            reg.emplace<FaceConfig>(e, fc);
            break;
        }
        case cfg::BuildingType::Generator: {
            // 旧版大功率燃煤发电机（2×2，1煤→3000EU/3秒）
            b.w = 2;
            b.h = 2;
            auto& gen = reg.emplace<PowerGeneratorNode>(e);
            gen.legacyMode = true;
            gen.burnTotal = cfg::LEGACY_GEN_BURN_TIME;
            gen.outputRate = 0.0f;
            reg.emplace<Inventory>(e, 1, cfg::LEGACY_GEN_MAX_COAL);
            reg.emplace<FaceConfig>(e, FaceConfig::makeAll(cfg::FaceMode::INPUT, 3));
            break;
        }
        case cfg::BuildingType::PowerGenerator: {
            // 工业燃煤发电机（32EU/秒，煤燃5秒）
            auto& gen = reg.emplace<PowerGeneratorNode>(e);
            gen.legacyMode = false;
            gen.burnTotal = cfg::POWERGEN_COAL_BURN_TIME;
            gen.outputRate = 0.0f;
            reg.emplace<Inventory>(e, 2, 64);
            reg.emplace<FaceConfig>(e, FaceConfig::makeAll(cfg::FaceMode::INPUT, 3));
            break;
        }
        case cfg::BuildingType::PowerPole:
            // 电线杆：150px半径连接电网
            reg.emplace<PowerPole>(e);
            break;
        case cfg::BuildingType::PowerWire:
            // 电力线缆：4面全TRANSFER（可编辑为NONE/INPUT/TRANSFER/OUTPUT）
            reg.emplace<FaceConfig>(e, FaceConfig::makeAll(cfg::FaceMode::TRANSFER, 4));
            break;
        case cfg::BuildingType::Capacitor:
            reg.emplace<PowerCapacitor>(e);
            break;
        case cfg::BuildingType::Pipe:
            // 物品管道：自动链接四邻容器/管道（连接掩码在下方统一刷新）
            reg.emplace<Pipe>(e);
            break;
        case cfg::BuildingType::Bucket: {
            reg.emplace<Bucket>(e);
            auto fc = FaceConfig::makeAll(cfg::FaceMode::INPUT, 3);
            fc.set(dir % 4, cfg::FaceMode::OUTPUT);   // 选定方向输出，其余三面输入
            reg.emplace<FaceConfig>(e, fc);
            break;
        }
        case cfg::BuildingType::Splitter:
            // 分流器：自动链接四邻，智能轮询均分输出
            reg.emplace<SplitterQueue>(e);
            break;
        case cfg::BuildingType::MeInterface:
            // 通物接口：桥接物理世界与网络，自动链接四邻通物设备
            reg.emplace<MeInterface>(e);
            break;
        case cfg::BuildingType::MeDrive:
            // 通物存储单元：为网络提供容量
            reg.emplace<MeDrive>(e);
            break;
        case cfg::BuildingType::MeTerminal:
            // 通物终端：右键查看全网物品
            reg.emplace<MeTerminal>(e);
            break;
        default:
            break;
    }

    registerToGrid(e, b);

    // 管道/分流器/通物设备：刷新自身与四邻的连接掩码（自动链接）
    if (t == cfg::BuildingType::Pipe || t == cfg::BuildingType::Splitter ||
        t == cfg::BuildingType::MeInterface || t == cfg::BuildingType::MeDrive ||
        t == cfg::BuildingType::MeTerminal) {
        PipeSystem::updateNeighbors(*this, tx, ty);
        MeSystem::markDirty();
    }

    // 电网相关建筑 → 标记拓扑重建
    const bool isMiner = t == cfg::BuildingType::Miner || t == cfg::BuildingType::MinerL2 ||
                         t == cfg::BuildingType::MinerL3 || t == cfg::BuildingType::MinerVoid;
    if (t == cfg::BuildingType::PowerWire || t == cfg::BuildingType::PowerPole ||
        t == cfg::BuildingType::PowerGenerator || t == cfg::BuildingType::Generator ||
        t == cfg::BuildingType::Capacitor || t == cfg::BuildingType::TowerElectric ||
        t == cfg::BuildingType::AlloyFurnace || isMiner)
        power.dirty = true;

    // 新手引导：记录放置（步骤判定 + 错误纠正）
    tutorial::onBuildingPlaced(*this, t);
    // 叙事层：第一座建筑落成（仅普通关卡；教程模式由旁白负责）
    narrative::announceOnce(*this, narrative::Event::FirstBuild);
    return e;
}

void Game::registerToGrid(entt::entity e, const Building& b) {
    for (int dy = 0; dy < b.h; ++dy)
        for (int dx = 0; dx < b.w; ++dx)
            grid.at(b.pos.x + dx, b.pos.y + dy).building = e;
}

void Game::unregisterFromGrid(entt::entity e) {
    for (auto& cell : grid.cells)
        if (cell.building == e) cell.building = entt::null;
}

// ---------------------------------------------------------------------
// 移动建筑（1×1；供采矿场"固定矿点模式"吸附到矿点旁边使用）
//   与拆除/放置共用同一套判定（地形 + 占用），失败时保持原位。
// ---------------------------------------------------------------------
bool Game::moveBuilding(entt::entity e, int tx, int ty) {
    if (e == entt::null || !reg.valid(e) || !reg.all_of<Building>(e)) return false;
    auto& b = reg.get<Building>(e);
    if (b.pos.x == tx && b.pos.y == ty) return true;

    const int ox = b.pos.x, oy = b.pos.y;
    unregisterFromGrid(e);                 // 先注销自身占格，再判定目标格
    bool ok = true;
    for (int dy = 0; dy < b.h && ok; ++dy)
        for (int dx = 0; dx < b.w && ok; ++dx) {
            const int x = tx + dx, y = ty + dy;
            if (!grid.inBounds(x, y)) { ok = false; break; }
            if (b.type != cfg::BuildingType::Pipe &&
                terrain[static_cast<size_t>(y) * grid.w + x] != 0) { ok = false; break; }
            if (grid.at(x, y).building != entt::null) { ok = false; break; }
        }
    if (!ok) {                             // 回滚：恢复原占格
        registerToGrid(e, b);
        return false;
    }

    b.pos = {tx, ty};
    registerToGrid(e, b);

    // 物流/通物/电网：新旧位置的邻居关系都变了 → 两边都刷新
    PipeSystem::updateNeighbors(*this, ox, oy);
    PipeSystem::updateNeighbors(*this, tx, ty);
    MeSystem::markDirty();
    power.dirty = true;
    return true;
}

// ---------------------------------------------------------------------
// 拆除
// ---------------------------------------------------------------------
void Game::removeBuilding(entt::entity e, bool refund) {
    if (e == entt::null || !reg.valid(e)) return;
    const auto& b = reg.get<Building>(e);
    const auto type = b.type;
    if (refund) refundCost(type);
    const bool isPipeNode = type == cfg::BuildingType::Pipe ||
                            type == cfg::BuildingType::Splitter ||
                            type == cfg::BuildingType::MeInterface ||
                            type == cfg::BuildingType::MeDrive ||
                            type == cfg::BuildingType::MeTerminal;
    const int px = b.pos.x, py = b.pos.y;
    unregisterFromGrid(e);
    reg.destroy(e);
    // 管道/分流器/通物设备拆除 → 刷新四邻连接掩码（物品留在缓冲/网络中不丢失）
    if (isPipeNode) {
        PipeSystem::updateNeighbors(*this, px, py);
        MeSystem::markDirty();
    }
    // 电网建筑拆除 → 重建拓扑
    const bool isMiner = type == cfg::BuildingType::Miner || type == cfg::BuildingType::MinerL2 ||
                         type == cfg::BuildingType::MinerL3 || type == cfg::BuildingType::MinerVoid;
    if (type == cfg::BuildingType::PowerWire || type == cfg::BuildingType::PowerPole ||
        type == cfg::BuildingType::PowerGenerator || type == cfg::BuildingType::Generator ||
        type == cfg::BuildingType::Capacitor || type == cfg::BuildingType::TowerElectric ||
        type == cfg::BuildingType::AlloyFurnace || isMiner)
        power.dirty = true;
}

void Game::deleteAtCursor() {
    // Delete键：拆除鼠标指向的建筑并返还材料（Python _delete_entity_at_cursor）
    const auto mouse = sf::Mouse::getPosition(window);
    const sf::Vector2f world = screenToWorld({static_cast<float>(mouse.x),
                                              static_cast<float>(mouse.y)});
    // 按Python从实体到基础设施的顺序检查（22种建筑全覆盖）
    const std::array<cfg::BuildingType, 22> order = {
        cfg::BuildingType::TowerBasic, cfg::BuildingType::TowerRapid,
        cfg::BuildingType::TowerSniper, cfg::BuildingType::TowerElectric,
        cfg::BuildingType::Miner, cfg::BuildingType::MinerL2,
        cfg::BuildingType::MinerL3, cfg::BuildingType::MinerVoid,
        cfg::BuildingType::Furnace, cfg::BuildingType::AlloyFurnace,
        cfg::BuildingType::Assembler, cfg::BuildingType::Generator,
        cfg::BuildingType::PowerPole, cfg::BuildingType::Bucket,
        cfg::BuildingType::Pipe, cfg::BuildingType::PowerGenerator,
        cfg::BuildingType::Capacitor, cfg::BuildingType::PowerWire,
        cfg::BuildingType::Splitter, cfg::BuildingType::MeInterface,
        cfg::BuildingType::MeDrive, cfg::BuildingType::MeTerminal,
    };
    for (auto t : order) {
        for (auto e : reg.view<Building>()) {
            const auto& b = reg.get<Building>(e);
            if (b.type != t) continue;
            const float x0 = b.pos.x * cfg::TILE_SIZE;
            const float y0 = b.pos.y * cfg::TILE_SIZE;
            if (world.x >= x0 && world.x < x0 + b.w * cfg::TILE_SIZE &&
                world.y >= y0 && world.y < y0 + b.h * cfg::TILE_SIZE) {
                removeBuilding(e, true);   // 拆除并返还材料
                ui->showToast(std::string("已拆除 ") +
                              cfg::BUILDING_INFOS[static_cast<size_t>(t)].nameZh + "，返还材料");
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------
// 建筑选择
// ---------------------------------------------------------------------
void Game::selectBuilding(cfg::BuildingType t) {
    hasSelection = true;
    selected = t;
    ui->selectBuilding(t);
    // 新手引导：选择事件（用于"选中指定建筑"步骤判定与错误纠正）
    tutorial::onBuildingSelected(*this, t);
}

void Game::placeBuildingWithDirection(cfg::BuildingType t, sf::Vector2i tile, int dir) {
    const entt::entity e = placeBuilding(tile.x, tile.y, t, dir);
    if (e != entt::null) {
        // 商店兑换的机器：一次免费放置
        if (hasFreePlace && selected == freePlaceType) hasFreePlace = false;
        else deductCost(t);
    }
}

void Game::cycleBuilding() {
    // TAB循环切换全部建筑（Python _cycle_building + 新增熔炉）
    if (!hasSelection) {
        selectBuilding(cfg::BuildingType::TowerBasic);
        return;
    }
    const int cur = static_cast<int>(selected);
    selectBuilding(static_cast<cfg::BuildingType>((cur + 1) % cfg::BUILDING_COUNT));
}

// ---------------------------------------------------------------------
// 敌人生成（U键由PlayerSystem调用EnemySystem::spawnEnemy）
// ---------------------------------------------------------------------
void Game::spawnEnemy() { EnemySystem::spawnEnemy(*this); }

// ---------------------------------------------------------------------
// 存档
// ---------------------------------------------------------------------
void Game::saveGame() {
    // 教程是独立模式、独立进度（saves/tutorial.json），不写主存档：
    // 避免教学过程中的临时建造覆盖玩家的普通关卡存档。
    if (mode == GameMode::Tutorial) {
        ui->showToast("新手教程模式不保存进度");
        return;
    }
    ui->showToast(::saveGame(*this) ? "保存成功!" : "保存失败!");
}

void Game::loadGame() {
    if (mode == GameMode::Tutorial) {
        ui->showToast("新手教程模式不读取存档");
        return;
    }
    if (::loadGame(*this)) {
        // 普通关卡不承载引导状态（教程是独立模式；旧档可能残留 active 标记）
        tutorial = tutorial::State{};
        ui->showToast("加载成功!");
    } else {
        ui->showToast("没有存档!");
    }
}

// ---------------------------------------------------------------------
// 坐标工具
// ---------------------------------------------------------------------
sf::Vector2f Game::screenToWorld(sf::Vector2f s) const {
    return camera.screenToWorld(s.x, s.y);
}

sf::Vector2f Game::worldToScreen(sf::Vector2f w) const {
    return camera.worldToScreen(w.x, w.y);
}

sf::Vector2i Game::tileAt(sf::Vector2f world) const {
    // 使用floor保证负坐标也正确取格（Python的 // 为向下取整）
    return {static_cast<int>(std::floor(world.x / cfg::TILE_SIZE)),
            static_cast<int>(std::floor(world.y / cfg::TILE_SIZE))};
}

sf::Vector2f Game::tileWorld(sf::Vector2i t) const {
    return {t.x * static_cast<float>(cfg::TILE_SIZE),
            t.y * static_cast<float>(cfg::TILE_SIZE)};
}

sf::Vector2f Game::buildingCenter(const Building& b) const {
    return {b.pos.x * cfg::TILE_SIZE + b.w * cfg::TILE_SIZE / 2.0f,
            b.pos.y * cfg::TILE_SIZE + b.h * cfg::TILE_SIZE / 2.0f};
}

// ---------------------------------------------------------------------
// 悬浮提示（Python _update_hover_tooltip）
// ---------------------------------------------------------------------
void Game::updateHoverTooltip() {
    const auto mouse = sf::Mouse::getPosition(window);
    const sf::Vector2f world = screenToWorld({static_cast<float>(mouse.x),
                                              static_cast<float>(mouse.y)});
    hoveredEntity = entt::null;

    auto hit = [&](entt::entity e) {
        const auto& b = reg.get<Building>(e);
        const float x0 = b.pos.x * cfg::TILE_SIZE;
        const float y0 = b.pos.y * cfg::TILE_SIZE;
        return world.x >= x0 && world.x < x0 + b.w * cfg::TILE_SIZE &&
               world.y >= y0 && world.y < y0 + b.h * cfg::TILE_SIZE;
    };
    // 悬停提示可在启动菜单"设置 → 悬停提示"里关闭（关闭后仍保留高亮/射程圈）
    const bool showTip = gset::get().showTooltips;
    auto setTip = [&](const std::string& title, const std::vector<std::string>& lines) {
        if (showTip) ui->setTooltip(title, lines);
    };

    // 采矿场（1/2/3级 + 虚空）：范围内采集全类型矿石
    for (auto [e, b, m] : reg.view<Building, Machine>().each()) {
        if ((b.type != cfg::BuildingType::Miner &&
             b.type != cfg::BuildingType::MinerL2 &&
             b.type != cfg::BuildingType::MinerL3 &&
             b.type != cfg::BuildingType::MinerVoid) || !hit(e)) continue;
        hoveredEntity = e;
        const int radius = (m.level == 2) ? cfg::MINER_RADIUS_L2
                         : (m.level == 3) ? cfg::MINER_RADIUS_L3
                                          : cfg::MINER_RADIUS_L1;
        std::string mode;
        if (m.voidMiner) {
            mode = "全类型矿石(无需矿点)";
        } else if (m.fixedOre) {
            mode = std::string("固定矿点: 仅采") + ItemSystem::nameZh(m.oreFilter);
        } else {
            mode = "范围采集(半径" + std::to_string(radius) + ")";
        }
        std::string extra;
        if (!m.voidMiner && m.fixedOre) {
            extra = "覆盖半径: " + std::to_string(radius) + " 格";
        }
        std::vector<std::string> tipLines = {
            "产出: " + std::to_string(static_cast<int>(m.rate)) + " 个/秒",
            "模式: " + mode};
        if (!extra.empty()) tipLines.push_back(extra);
        tipLines.push_back(std::string("状态: ") + (m.producing ? "工作中" : "待机"));
        tipLines.push_back("提示: 右键打开设置面板");
        setTip(std::string(cfg::BUILDING_INFOS[static_cast<size_t>(b.type)].nameZh), tipLines);
        return;
    }
    // 熔炉（数据驱动 FURNACE_RECIPES）
    for (auto [e, b, m, inv] : reg.view<Building, Machine, Inventory>().each()) {
        if (b.type != cfg::BuildingType::Furnace || !hit(e)) continue;
        hoveredEntity = e;
        setTip("熔炉", {
            std::string("状态: ") + (m.producing ? "冶炼中" : "待料"),
            std::string("配方: ") + (m.hasJob
                ? cfg::FURNACE_RECIPES[static_cast<size_t>(m.recipeId)].nameZh
                : std::string("任意矿石→锭")),
            "库存: 锭×" + std::to_string(inv.count(cfg::ItemType::IronIngot) +
                inv.count(cfg::ItemType::CopperIngot) + inv.count(cfg::ItemType::GoldIngot) +
                inv.count(cfg::ItemType::NickelIngot) + inv.count(cfg::ItemType::SilverIngot) +
                inv.count(cfg::ItemType::LeadIngot))});
        return;
    }
    // 合金炉（数据驱动 ALLOY_RECIPES，选定配方，无需电力）
    for (auto [e, b, m, inv] : reg.view<Building, Machine, Inventory>().each()) {
        if (b.type != cfg::BuildingType::AlloyFurnace || !hit(e)) continue;
        hoveredEntity = e;
        std::string prod = std::to_string(inv.count(cfg::ItemType::SteelIngot) +
            inv.count(cfg::ItemType::ElectrumIngot) + inv.count(cfg::ItemType::InvarIngot) +
            inv.count(cfg::ItemType::ConstantanIngot));
        const int ar = std::min(m.recipeId,
            static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
        const auto& arp = cfg::ALLOY_RECIPES[static_cast<size_t>(ar)];
        std::string recipeStr;
        for (size_t i = 0; i < arp.inputs.size(); ++i) {
            if (i) recipeStr += " + ";
            recipeStr += std::to_string(arp.inputs[i].second) +
                         ItemSystem::nameZh(arp.inputs[i].first);
        }
        recipeStr += " → ";
        for (size_t i = 0; i < arp.outputs.size(); ++i) {
            if (i) recipeStr += " + ";
            recipeStr += std::to_string(arp.outputs[i].second) +
                         ItemSystem::nameZh(arp.outputs[i].first);
        }
        setTip("合金炉", {
            std::string("状态: ") + (m.producing ? "冶炼中" : "待料"),
            "配方: " + std::string(arp.nameZh) + "(" + recipeStr + ")",
            "库存: 合金×" + prod,
            "提示: 右键可切换配方"});
        return;
    }
    // 组装机
    for (auto [e, b, m, inv] : reg.view<Building, Machine, Inventory>().each()) {
        if (b.type != cfg::BuildingType::Assembler || !hit(e)) continue;
        hoveredEntity = e;
        const int rid = std::min(m.recipeId, static_cast<int>(cfg::ASSEMBLER_RECIPES.size()) - 1);
        const auto& recipe = cfg::ASSEMBLER_RECIPES[static_cast<size_t>(rid)];
        // 配方说明串："2铁锭 + 1铜锭 → 1弹药"
        std::string recipeStr;
        for (size_t i = 0; i < recipe.inputs.size(); ++i) {
            if (i) recipeStr += " + ";
            recipeStr += std::to_string(recipe.inputs[i].second) +
                         ItemSystem::nameZh(recipe.inputs[i].first);
        }
        recipeStr += " → ";
        for (size_t i = 0; i < recipe.outputs.size(); ++i) {
            if (i) recipeStr += " + ";
            recipeStr += std::to_string(recipe.outputs[i].second) +
                         ItemSystem::nameZh(recipe.outputs[i].first);
        }
        setTip("组装机", {
            "配方: " + std::string(recipe.nameZh) + "(" + recipeStr + ")",
            "库存: 铁锭" + std::to_string(inv.count(cfg::ItemType::IronIngot)) +
                " 铜锭" + std::to_string(inv.count(cfg::ItemType::CopperIngot)),
            "产物: " + std::to_string(inv.count(recipe.outputs.empty()
                ? cfg::ItemType::Ammo : recipe.outputs.front().first)),
            "提示: 右键可切换配方"});
        return;
    }
    // 旧版发电机
    for (auto [e, b, gen, inv] : reg.view<Building, PowerGeneratorNode, Inventory>().each()) {
        if (b.type != cfg::BuildingType::Generator || !hit(e)) continue;
        hoveredEntity = e;
        setTip("发电机", {
            "燃料: " + std::to_string(inv.count(cfg::ItemType::Coal)),
            "电力: " + std::to_string(static_cast<int>(gen.outputRate)) + " EU/s"});
        return;
    }
    // 燃煤发电机
    for (auto [e, b, gen, inv] : reg.view<Building, PowerGeneratorNode, Inventory>().each()) {
        if (b.type != cfg::BuildingType::PowerGenerator || !hit(e)) continue;
        hoveredEntity = e;
        setTip("燃煤发电机", {
            "燃料(煤): " + std::to_string(inv.count(cfg::ItemType::Coal)),
            "输出: " + std::to_string(static_cast<int>(gen.outputRate)) + " EU/s",
            std::string("状态: ") + (gen.running ? "运行中" : "待燃料")});
        return;
    }
    // 储物桶
    for (auto [e, b, bucket] : reg.view<Building, Bucket>().each()) {
        if (!hit(e)) continue;
        hoveredEntity = e;
        std::vector<std::string> lines;
        std::unordered_map<cfg::ItemType, int> summary;
        for (auto t : bucket.items) summary[t]++;
        if (summary.empty()) lines = {"空桶"};
        else for (auto& [t, n] : summary)
            lines.push_back(std::string(ItemSystem::nameZh(t)) + ": " + std::to_string(n));
        setTip("储物桶", lines);
        return;
    }
    // 电容库
    for (auto [e, b, cap] : reg.view<Building, PowerCapacitor>().each()) {
        if (!hit(e)) continue;
        hoveredEntity = e;
        setTip("电容库", {
            "储能: " + std::to_string(static_cast<int>(cap.energy)) + "/" +
                std::to_string(static_cast<int>(cap.capacity)) + " EU"});
        return;
    }
    // 塔
    for (auto [e, b, t] : reg.view<Building, Turret>().each()) {
        if (!hit(e)) continue;
        hoveredEntity = e;
        std::vector<std::string> lines = {
            "伤害: " + std::to_string(t.damage),
            "射速: " + std::to_string(t.fireRate) + "/s",
            "射程: " + std::to_string(static_cast<int>(t.range))};
        if (t.isElectric)
            lines.push_back("电力: " + std::to_string(static_cast<int>(t.power)) + "/" +
                            std::to_string(static_cast<int>(t.maxPower)) + "  " +
                            (t.gridPowered ? "供电中" : "断电"));
        else
            lines.push_back("弹药: " + std::to_string(t.ammo) + "  " +
                            (t.ammo > 0 ? "有弹药" : "无弹药"));
        setTip(t.isElectric ? "电力塔" : "弹药塔", lines);
        return;
    }
    // 电线
    for (auto [e, b, fc] : reg.view<Building, FaceConfig>().each()) {
        if (b.type != cfg::BuildingType::PowerWire || !hit(e)) continue;
        hoveredEntity = e;
        int connected = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = b.pos.x + cfg::Dir::OFFSETS[d][0];
            const int ny = b.pos.y + cfg::Dir::OFFSETS[d][1];
            if (grid.inBounds(nx, ny) && grid.at(nx, ny).building != entt::null) connected++;
        }
        setTip("电力线缆", {
            "连接数: " + std::to_string(connected)});
        return;
    }
    // 分流器 / 物品管道（自动链接）
    for (auto [e, b, sp] : reg.view<Building, SplitterQueue>().each()) {
        if (b.type != cfg::BuildingType::Splitter || !hit(e)) continue;
        hoveredEntity = e;
        setTip("分流器", {"队列: " + std::to_string(sp.queue.size()),
                          "自动连接四邻，智能均分到可用出口"});
        return;
    }
    for (auto [e, b, p] : reg.view<Building, Pipe>().each()) {
        if (b.type != cfg::BuildingType::Pipe || !hit(e)) continue;
        hoveredEntity = e;
        // 统计整条管道网络（四邻连通的管道/分流器）的在线物品总数：
        // 物品瞬时传送，中间管道显示0，因此给出"网络全局缓存"更有意义
        int netTotal = 0, nodes = 0;
        std::vector<uint8_t> vis(static_cast<size_t>(grid.w) * grid.h, 0);
        std::deque<std::pair<int, int>> q;
        auto mark = [&](int x, int y) { vis[static_cast<size_t>(y) * grid.w + x] = 1; };
        q.emplace_back(b.pos.x, b.pos.y);
        mark(b.pos.x, b.pos.y);
        while (!q.empty()) {
            const auto [x, y] = q.front();
            q.pop_front();
            const entt::entity ne = grid.at(x, y).building;
            if (ne == entt::null || !reg.valid(ne)) continue;
            nodes++;
            if (reg.all_of<Pipe>(ne)) netTotal += static_cast<int>(reg.get<Pipe>(ne).buffer.size());
            else if (reg.all_of<SplitterQueue>(ne))
                netTotal += static_cast<int>(reg.get<SplitterQueue>(ne).queue.size());
            for (int d = 0; d < 4; ++d) {
                const int nx = x + cfg::Dir::OFFSETS[d][0];
                const int ny = y + cfg::Dir::OFFSETS[d][1];
                if (!grid.inBounds(nx, ny)) continue;
                if (vis[static_cast<size_t>(ny) * grid.w + nx]) continue;
                const entt::entity nn = grid.at(nx, ny).building;
                if (nn == entt::null || !reg.valid(nn)) continue;
                if (!reg.all_of<Pipe>(nn) && !reg.all_of<SplitterQueue>(nn)) continue;
                mark(nx, ny);
                q.emplace_back(nx, ny);
            }
        }
        setTip("物品管道", {"网络缓存: " + std::to_string(netTotal) + " 件（共 " +
                            std::to_string(nodes) + " 格）",
                            "本格: " + std::to_string(p.buffer.size()) + " / " +
                                std::to_string(cfg::PIPES_MAX_BUFFER),
                            "自动连接四邻容器/管道，无动画即时传输"});
        return;
    }
    // 通物网络设备（接口 / 存储单元 / 终端）
    for (auto [e, b] : reg.view<Building>().each()) {
        const bool isMe = b.type == cfg::BuildingType::MeInterface ||
                          b.type == cfg::BuildingType::MeDrive ||
                          b.type == cfg::BuildingType::MeTerminal;
        if (!isMe || !hit(e)) continue;
        hoveredEntity = e;
        const int nid = MeSystem::networkIdOf(*this, e);
        std::string stat = "未接入通物网络";
        if (nid >= 0 && nid < static_cast<int>(MeSystem::networks().size())) {
            const auto& net = MeSystem::networks()[static_cast<size_t>(nid)];
            stat = "物品 " + std::to_string(net.totalItems) + " / 容量 " +
                   std::to_string(net.capacity);
        }
        setTip(std::string(cfg::BUILDING_INFOS[static_cast<size_t>(b.type)].nameZh),
               {stat, "网络内物品全局共享，右键查看清单"});
        return;
    }

    // 敌人悬停（只显示血条，不显示提示面板，Python行为）
    for (auto [e, en] : reg.view<Enemy>().each()) {
        if (world.x >= en.pos.x && world.x < en.pos.x + cfg::TILE_SIZE &&
            world.y >= en.pos.y && world.y < en.pos.y + cfg::TILE_SIZE) {
            hoveredEntity = e;
            ui->clearTooltip();
            return;
        }
    }

    ui->clearTooltip();   // 无悬停目标
}

entt::entity Game::hoveredEnemy() const {
    if (hoveredEntity != entt::null && reg.valid(hoveredEntity) &&
        reg.all_of<Enemy>(hoveredEntity))
        return hoveredEntity;
    return entt::null;
}

// ---------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------
void Game::processEvents() {
    FT_PROFILE;
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
            return;
        }
        if (gameOver) return;   // Python: 游戏结束后不再处理其他事件

        // 窗口缩放/最大化：同步摄像机坐标变换、UI布局与视图
        if (event.type == sf::Event::Resized && event.size.width > 0 && event.size.height > 0) {
            const float w = static_cast<float>(event.size.width);
            const float h = static_cast<float>(event.size.height);
            camera.setScreenSize(w, h);
            window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, w, h)));
            ui->updateLayout();
            continue;
        }

        // 新手引导层（横幅上的「跳过引导」按钮）最优先（仅教程模式启用）
        if (mode == GameMode::Tutorial && tutorial::handleEvent(*this, event)) continue;
        // 优先UI（按钮/方向悬浮窗/面编辑器）
        if (ui->handleEvent(event)) continue;
        // 世界交互（放置/拆除/旋转/摄像机）
        PlayerSystem::handleEvent(*this, event);
    }
}

// ---------------------------------------------------------------------
// 更新（Python GameScene.update 顺序）
// ---------------------------------------------------------------------
void Game::update(float dt) {
    FT_PROFILE;
    // 1. 摄像机（WASD）
    PlayerSystem::updateCamera(*this, dt);

    // 2. 测试版：采矿机免供电（Python"无限资源模式：自动给所有机器供电"）
    if (cfg::MINER_FREE_POWER) {
        for (auto [e, m] : reg.view<Machine>().each())
            if (m.kind == MachineKind::Miner) m.powered = true;
    }

    // 3. 机器生产计时（采矿/冶炼/组装/桶计时）
    MachineSystem::updateMachines(*this, dt);

    // 4. 发电机燃料燃烧 + 电网路由
    PowerSystem::updateGenerators(*this, dt);
    PowerSystem::update(*this, dt);

    // 5. 物流：通物网络 → 分流器均分 → 管道路由 → 机器拉取原料
    MeSystem::update(*this, dt);
    PipeSystem::updateSplitters(*this, dt);
    PipeSystem::updatePipes(*this, dt);
    PipeSystem::updateMachinePulls(*this);

    // 6. 机器输出推送
    MachineSystem::pushOutputs(*this, dt);

    // 7. 敌人移动 + 到达终点
    EnemySystem::update(*this, dt);

    // 8. 炮塔索敌射击 + 子弹飞行
    TurretSystem::updateTowers(*this, dt);
    TurretSystem::updateBullets(*this, dt);

    // 9. 击杀结算（金币）
    EnemySystem::processKills(*this);

    // 10. 波次状态机（教程模式不自动出怪：敌人由教学步骤手动生成 Z / X / C）
    if (mode != GameMode::Tutorial) EnemySystem::updateWaves(*this, dt);

    // 11. 悬浮提示 + 放置预览 + UI计时
    updateHoverTooltip();
    PlayerSystem::updatePreview(*this);
    ui->update(dt);

    // 12. 新手引导推进（步骤判定 / 计时 / 反馈动画；仅教程模式）
    if (mode == GameMode::Tutorial) tutorial::update(*this, dt);

    // 13. 叙事层（织女星定期吐槽公司；教程模式内部自动跳过）
    narrative::update(*this, dt);
}

// ---------------------------------------------------------------------
// 渲染
// ---------------------------------------------------------------------
void Game::render() {
    FT_PROFILE;
    RenderSystem::renderWorld(*this, window);
    ui->draw(window);
    window.display();
}

// ---------------------------------------------------------------------
// 主循环
// ---------------------------------------------------------------------
// 暂停面板里改了显示模式：按新设置重建窗口。
// 必须在事件循环之外调用（在 pollEvent 循环内重建 SFML 窗口会崩溃/丢事件）。
void Game::applyDisplayMode() {
    const bool wasPaused = paused;   // 面板还开着，重建后保持暂停
    gset::applyToWindow(window, cfg::SCREEN_TITLE);
    gset::applyFrameMode(window);   // 窗口重建后必须重新应用（上下文已更换）
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    camera.setScreenSize(w, h);
    window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, w, h)));
    ui->updateLayout();
    paused = wasPaused;
}

void Game::run() {
    // returnToMenu：暂停面板选择「返回主界面」→ 退出本局，main.cpp 会再进启动入口
    while (window.isOpen() && !returnToMenu) {
        // 帧时间（std::chrono驱动，钳制避免暂停后跳帧）
        const float dt = std::min(clock.restart().asSeconds(), 0.1f);

        processEvents();
        if (ui->consumeDisplayApply()) applyDisplayMode();   // 事件循环之外重建窗口
        if (!paused && !gameOver) update(dt);
        render();
        FT_FRAME;
    }
}
