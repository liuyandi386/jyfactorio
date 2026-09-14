// =====================================================================
// SaveSystem.cpp —— 存档系统实现（JSON 格式，同 Python 版 save_system.py）
// 使用 nlohmann/json：保存/加载全部游戏数据（建筑/物品/敌人/电网/配方）。
// =====================================================================
#include "SaveSystem.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Game.h"
#include "systems/ItemSystem.h"
#include "systems/MeSystem.h"

using nlohmann::json;

namespace {
/// 存档路径（保存前自动创建目录，Python save_system 同款行为）
std::string savePath() {
    std::error_code ec;
    std::filesystem::create_directories("saves", ec);
    return "saves/factory_td.json";
}

/// 物品计数表 → JSON 对象（键为物品键名）
json itemsToJson(const std::unordered_map<cfg::ItemType, int>& items) {
    json j = json::object();
    for (const auto& [t, n] : items) j[ItemSystem::key(t)] = n;
    return j;
}
} // namespace

bool hasSave() {
    std::ifstream f(savePath());
    return f.good();
}

// ---------------------------------------------------------------------
// 保存
// ---------------------------------------------------------------------
bool saveGame(Game& g) {
    try {
        // 保存前刷新通物网络拓扑，确保 me_networks 是最新状态
        // （刚放置通物设备后立即存档时，网络可能还挂着脏标记未重建）
        MeSystem::rebuildNetworks(g);
        json j;
        j["v"] = 1;
        j["gold"] = g.gold;
        j["lives"] = g.lives;
        j["wave"] = g.currentWave;
        j["wave_state"] = static_cast<int>(g.waveState);
        j["spawned"] = g.enemiesSpawned;
        j["per_wave"] = g.enemiesPerWave;
        j["spawn_timer"] = g.spawnTimer;
        j["wave_timer"] = g.waveTimer;
        j["countdown"] = g.countdown;
        j["inv"] = itemsToJson(g.playerInv);
        // 背包机器数（按 BuildingType 枚举序）
        json mach = json::array();
        for (int i = 0; i < cfg::BUILDING_COUNT; ++i)
            mach.push_back(g.backpackMachines[static_cast<size_t>(i)]);
        j["machines"] = std::move(mach);
        j["cam_x"] = g.camera.x;      // 摄像机位置
        j["cam_y"] = g.camera.y;

        // ---- 新手引导进度（分步教学：完成步骤/耗时/误操作，供继续游戏续接） ----
        {
            json tj;
            tj["active"] = g.tutorial.active;
            tj["skipped"] = g.tutorial.skipped;
            tj["finished"] = g.tutorial.finished;
            tj["step"] = g.tutorial.step;
            tj["mistakes"] = g.tutorial.mistakes;
            tj["total_elapsed"] = g.tutorial.totalElapsed;
            tj["done"] = g.tutorial.done;             // 每步是否完成
            tj["step_times"] = g.tutorial.stepTimes;  // 每步耗时（学习进度参考）
            j["tutorial"] = std::move(tj);
        }

        // ---- 矿点（先保存，加载时先恢复以便矿机定位；含有限储量） ----
        j["ores"] = json::array();
        for (auto [e, pos, ore] : g.reg.view<GridPos, OreDeposit>().each())
            j["ores"].push_back({{"type", ItemSystem::key(ore.type)}, {"x", pos.x},
                                 {"y", pos.y}, {"amount", ore.amount}});

        // ---- 建筑（全部类型的状态都保存，不遗漏） ----
        j["buildings"] = json::array();
        for (auto [e, b] : g.reg.view<Building>().each()) {
            json bj;
            bj["type"] = static_cast<int>(b.type);
            bj["x"] = b.pos.x;
            bj["y"] = b.pos.y;
            bj["dir"] = b.dir;

            switch (b.type) {
                case cfg::BuildingType::TowerBasic:
                case cfg::BuildingType::TowerRapid:
                case cfg::BuildingType::TowerSniper:
                    bj["ammo"] = g.reg.get<Turret>(e).ammo;
                    bj["barrel"] = static_cast<int>(g.reg.get<Turret>(e).barrelDir);
                    break;
                case cfg::BuildingType::TowerElectric:
                    bj["power"] = g.reg.get<Turret>(e).power;
                    bj["barrel"] = static_cast<int>(g.reg.get<Turret>(e).barrelDir);
                    break;
                case cfg::BuildingType::Miner:
                case cfg::BuildingType::MinerL2:
                case cfg::BuildingType::MinerL3:
                case cfg::BuildingType::MinerVoid: {
                    const auto& m = g.reg.get<Machine>(e);
                    bj["level"] = m.level;
                    bj["void"] = m.voidMiner;
                    bj["acc"] = m.acc;
                    // 采集模式（原有模式 / 固定矿点模式）+ 矿种筛选 + 矿点绑定
                    bj["fixed_ore"] = m.fixedOre;
                    bj["ore_filter"] = ItemSystem::key(m.oreFilter);
                    bj["has_bound"] = m.hasBound;
                    bj["bound_x"] = m.boundX;
                    bj["bound_y"] = m.boundY;
                    // 采矿机库存（已开采未输出的矿石也必须保存）
                    bj["items"] = itemsToJson(g.reg.get<Inventory>(e).items);
                    break;
                }
                case cfg::BuildingType::Furnace:
                case cfg::BuildingType::AlloyFurnace:
                case cfg::BuildingType::Assembler: {
                    const auto& m = g.reg.get<Machine>(e);
                    const auto& inv = g.reg.get<Inventory>(e);
                    bj["has_job"] = m.hasJob;
                    bj["job_input"] = ItemSystem::key(m.jobInput);
                    bj["job_time"] = m.jobTime;
                    bj["job_total"] = m.jobTotal;
                    bj["recipe"] = m.recipeId;
                    bj["items"] = itemsToJson(inv.items);
                    break;
                }
                case cfg::BuildingType::Generator:
                case cfg::BuildingType::PowerGenerator: {
                    const auto& gen = g.reg.get<PowerGeneratorNode>(e);
                    const auto& inv = g.reg.get<Inventory>(e);
                    bj["coal"] = inv.count(cfg::ItemType::Coal);
                    bj["fuel_time"] = gen.fuelTime;
                    bj["burn"] = gen.burnProgress;
                    break;
                }
                case cfg::BuildingType::Bucket: {
                    const auto& bucket = g.reg.get<Bucket>(e);
                    bj["items"] = json::array();
                    for (auto t : bucket.items) bj["items"].push_back(ItemSystem::key(t));
                    bj["output_timer"] = bucket.outputTimer;
                    break;
                }
                case cfg::BuildingType::Capacitor:
                    bj["energy"] = g.reg.get<PowerCapacitor>(e).energy;
                    break;
                case cfg::BuildingType::Splitter: {
                    const auto& sp = g.reg.get<SplitterQueue>(e);
                    bj["queue"] = json::array();
                    for (auto t : sp.queue) bj["queue"].push_back(ItemSystem::key(t));
                    bj["output_index"] = sp.outputIndex;
                    break;
                }
                case cfg::BuildingType::MeInterface: {
                    const auto& iface = g.reg.get<MeInterface>(e);
                    json f = json::array();
                    for (auto t : iface.filter) f.push_back(ItemSystem::key(t));
                    bj["filter"] = std::move(f);
                    break;
                }
                default:
                    break;   // 电线/管道/电线杆/通物存储/通物终端：无额外状态
            }
            // 面配置（所有带 FaceConfig 的建筑统一保存：采矿机/熔炉/合金炉/
            // 组装机/发电机/储物桶/电线等——右键旋转过的面必须保留）
            if (g.reg.all_of<FaceConfig>(e)) {
                const auto& fc = g.reg.get<FaceConfig>(e);
                bj["faces"] = {static_cast<int>(fc.get(0)), static_cast<int>(fc.get(1)),
                               static_cast<int>(fc.get(2)), static_cast<int>(fc.get(3))};
            }
            j["buildings"].push_back(bj);
        }

        // ---- 物品管道（含缓冲物品） ----
        j["pipes"] = json::array();
        for (auto [e, b, p] : g.reg.view<Building, Pipe>().each()) {
            json cj{{"x", b.pos.x}, {"y", b.pos.y}};
            json buf = json::array();
            for (auto t : p.buffer) buf.push_back(ItemSystem::key(t));
            cj["buffer"] = std::move(buf);
            j["pipes"].push_back(cj);
        }

        // ---- 通物网络存储（按网络编号顺序；编号行主序确定，读档可复现） ----
        j["me_networks"] = json::array();
        for (const auto& net : MeSystem::networks()) {
            json items = json::object();
            for (const auto& [t, n] : net.items) items[ItemSystem::key(t)] = n;
            j["me_networks"].push_back({{"items", std::move(items)}});
        }

        // ---- 敌人 ----
        j["enemies"] = json::array();
        for (auto [e, en] : g.reg.view<Enemy>().each())
            j["enemies"].push_back({{"type", static_cast<int>(en.type)},
                                    {"x", en.pos.x}, {"y", en.pos.y},
                                    {"hp", en.health}, {"pi", en.pathIndex},
                                    {"tx", en.target.x}, {"ty", en.target.y}});

        std::ofstream out(savePath());
        out << j.dump(2);
        return out.good();
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------
// 加载（顺序：清空 → 矿点 → 建筑 → 传送带 → 敌人 → 状态）
// ---------------------------------------------------------------------
bool loadGame(Game& g) {
    std::ifstream f(savePath());
    if (!f) return false;
    // 先解析再清空：存档损坏时直接返回，不破坏当前游戏状态
    json j;
    try {
        f >> j;
    } catch (...) {
        return false;
    }
    try {
        // ---- 清空现有实体与网格（Python load_game 行为） ----
        g.reg.clear();
        for (auto& cell : g.grid.cells) {
            cell.building = entt::null;
        }
        g.power.networks.clear();
        g.power.dirty = true;
        g.faceEditTarget = entt::null;

        // ---- 1. 矿点（矿机放置依赖矿点；恢复有限储量） ----
        for (const auto& oj : j.value("ores", json::array())) {
            if (auto t = ItemSystem::parse(oj.value("type", ""))) {
                const auto e = g.reg.create();
                g.reg.emplace<GridPos>(e, oj.value("x", 0), oj.value("y", 0));
                g.reg.emplace<OreDeposit>(e, *t, oj.value("amount", cfg::ORE_DEPOSIT_AMOUNT));
            }
        }

        // ---- 2. 建筑 ----
        for (const auto& bj : j.value("buildings", json::array())) {
            const auto bt = static_cast<cfg::BuildingType>(bj.value("type", 0));
            const entt::entity e = g.placeBuilding(bj.value("x", 0), bj.value("y", 0),
                                                   bt, bj.value("dir", 2), false);
            if (e == entt::null) continue;

            switch (bt) {
                case cfg::BuildingType::TowerBasic:
                case cfg::BuildingType::TowerRapid:
                case cfg::BuildingType::TowerSniper:
                    g.reg.get<Turret>(e).ammo = bj.value("ammo", 0);
                    g.reg.get<Turret>(e).barrelDir =
                        static_cast<uint8_t>(bj.value("barrel", bj.value("dir", 2)));
                    break;
                case cfg::BuildingType::TowerElectric:
                    g.reg.get<Turret>(e).power = bj.value("power", 0.0f);
                    g.reg.get<Turret>(e).barrelDir =
                        static_cast<uint8_t>(bj.value("barrel", bj.value("dir", 2)));
                    break;
                case cfg::BuildingType::Miner:
                case cfg::BuildingType::MinerL2:
                case cfg::BuildingType::MinerL3:
                case cfg::BuildingType::MinerVoid: {
                    // placeBuilding 已按类型设好等级/速率，这里恢复生产累加器与库存
                    auto& m = g.reg.get<Machine>(e);
                    m.level = bj.value("level", m.level);
                    m.voidMiner = bj.value("void", m.voidMiner);
                    m.acc = bj.value("acc", 0.0f);
                    // 采集模式/矿种/绑定矿点：旧存档没有这些键 → 默认回到"原有模式"
                    // （位置与朝向直接沿用存档值，不做吸附，避免读档后建筑乱跑）
                    m.fixedOre = bj.value("fixed_ore", false) && !m.voidMiner;
                    if (auto o = ItemSystem::parse(bj.value("ore_filter", "iron_ore")))
                        m.oreFilter = *o;
                    m.hasBound = bj.value("has_bound", false);
                    m.boundX = bj.value("bound_x", -1);
                    m.boundY = bj.value("bound_y", -1);
                    auto& inv = g.reg.get<Inventory>(e);
                    // 注意：value() 必须绑定一次再取 begin/end，
                    // 否则两个临时对象的迭代器悬空（读档崩溃丢数据的元凶）
                    const json& items = bj.value("items", json::object());
                    for (auto it = items.begin(); it != items.end(); ++it)
                        if (auto t = ItemSystem::parse(it.key()))
                            inv.add(*t, it.value().get<int>());
                    break;
                }
                case cfg::BuildingType::Furnace:
                case cfg::BuildingType::AlloyFurnace:
                case cfg::BuildingType::Assembler: {
                    auto& m = g.reg.get<Machine>(e);
                    m.hasJob = bj.value("has_job", false);
                    if (auto t = ItemSystem::parse(bj.value("job_input", "iron_ore")))
                        m.jobInput = *t;
                    m.jobTime = bj.value("job_time", 0.0f);
                    m.jobTotal = bj.value("job_total", m.jobTotal);
                    m.recipeId = bj.value("recipe", 0);
                    auto& inv = g.reg.get<Inventory>(e);
                    const json& items = bj.value("items", json::object());
                    for (auto it = items.begin(); it != items.end(); ++it)
                        if (auto t = ItemSystem::parse(it.key()))
                            inv.add(*t, it.value().get<int>());
                    break;
                }
                case cfg::BuildingType::Generator:
                case cfg::BuildingType::PowerGenerator: {
                    auto& gen = g.reg.get<PowerGeneratorNode>(e);
                    auto& inv = g.reg.get<Inventory>(e);
                    inv.add(cfg::ItemType::Coal, bj.value("coal", 0));
                    gen.fuelTime = bj.value("fuel_time", 0.0f);
                    gen.burnProgress = bj.value("burn", 0.0f);
                    break;
                }
                case cfg::BuildingType::Bucket: {
                    auto& bucket = g.reg.get<Bucket>(e);
                    for (const auto& ik : bj.value("items", json::array()))
                        if (auto t = ItemSystem::parse(ik.get<std::string>()))
                            bucket.items.push_back(*t);
                    bucket.outputTimer = bj.value("output_timer", 0.0f);
                    break;
                }
                case cfg::BuildingType::Capacitor:
                    g.reg.get<PowerCapacitor>(e).energy = bj.value("energy", 0.0f);
                    break;
                case cfg::BuildingType::Splitter: {
                    auto& sp = g.reg.get<SplitterQueue>(e);
                    for (const auto& ik : bj.value("queue", json::array()))
                        if (auto t = ItemSystem::parse(ik.get<std::string>()))
                            sp.queue.push_back(*t);
                    sp.outputIndex = bj.value("output_index", 0);
                    break;
                }
                case cfg::BuildingType::MeInterface: {
                    auto& iface = g.reg.get<MeInterface>(e);
                    iface.filter.clear();
                    for (const auto& fk : bj.value("filter", json::array()))
                        if (auto t = ItemSystem::parse(fk.get<std::string>()))
                            iface.filter.push_back(*t);
                    break;
                }
                default:
                    break;   // 电线/管道/电线杆/通物存储/通物终端：无额外状态
            }
            // 面配置统一恢复（采矿机/熔炉/合金炉/组装机/发电机/储物桶/电线等）
            if (g.reg.all_of<FaceConfig>(e) && bj.contains("faces")) {
                auto& fc = g.reg.get<FaceConfig>(e);
                const auto& arr = bj["faces"];
                for (int d = 0; d < 4 && d < static_cast<int>(arr.size()); ++d)
                    fc.set(d, static_cast<cfg::FaceMode>(arr.at(d).get<int>()));
            }
        }

        // ---- 3. 物品管道（含缓冲物品） ----
        // 管道实体已在步骤2随 buildings 创建（保存时管道同时记录于两段），
        // 这里直接回填缓冲，不再重复 placeBuilding（会因格子占用而失败导致物品丢失）
        for (const auto& cj : j.value("pipes", json::array())) {
            const int px2 = cj.value("x", 0), py2 = cj.value("y", 0);
            const entt::entity e = g.grid.inBounds(px2, py2)
                                       ? g.grid.at(px2, py2).building
                                       : entt::null;
            if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Pipe>(e)) continue;
            auto& pbuf = g.reg.get<Pipe>(e).buffer;
            for (const auto& ik : cj.value("buffer", json::array()))
                if (auto t = ItemSystem::parse(ik.get<std::string>()))
                    pbuf.push_back(*t);
        }

        // ---- 3.5 通物网络存储恢复（重建网络后按编号回填，容量截断） ----
        MeSystem::rebuildNetworks(g);
        const auto& netsArr = j.value("me_networks", json::array());
        for (size_t i = 0; i < netsArr.size() && i < MeSystem::networks().size(); ++i) {
            MeNetwork& net = MeSystem::networksMutable()[i];
            net.items.clear();
            net.totalItems = 0;
            const json& items = netsArr[i].value("items", json::object());
            for (auto it = items.begin(); it != items.end(); ++it)
                if (auto t = ItemSystem::parse(it.key()))
                    MeSystem::addItem(net, *t, it.value().get<int>());
        }

        // ---- 4. 敌人 ----
        for (const auto& ej : j.value("enemies", json::array())) {
            const auto e = g.reg.create();
            auto& en = g.reg.emplace<Enemy>(e);
            en.type = static_cast<cfg::EnemyType>(ej.value("type", 0));
            const auto& st = cfg::ENEMY_STATS[static_cast<size_t>(en.type)];
            en.maxHealth = st.health;
            en.speed = st.speed;
            en.reward = st.reward;
            en.pos = {ej.value("x", 0.0f), ej.value("y", 0.0f)};
            en.health = ej.value("hp", en.maxHealth);
            en.pathIndex = ej.value("pi", 1);
            en.target = {ej.value("tx", 0.0f), ej.value("ty", 0.0f)};
        }

        // ---- 5. 全局状态 ----
        g.gold = j.value("gold", g.gold);
        g.lives = j.value("lives", g.lives);
        g.camera.x = j.value("cam_x", g.camera.x);
        g.camera.y = j.value("cam_y", g.camera.y);
        g.currentWave = j.value("wave", 1);
        g.waveState = static_cast<WaveState>(j.value("wave_state", 0));
        g.enemiesSpawned = j.value("spawned", 0);
        g.enemiesPerWave = j.value("per_wave", cfg::WAVE_ENEMIES);
        g.spawnTimer = j.value("spawn_timer", 0.0f);
        g.waveTimer = j.value("wave_timer", 0.0f);
        g.countdown = j.value("countdown", cfg::INITIAL_COUNTDOWN);
        g.playerInv.clear();
        const json& invJson = j.value("inv", json::object());
        for (auto it = invJson.begin(); it != invJson.end(); ++it)
            if (auto t = ItemSystem::parse(it.key()))
                g.playerInv[*t] = it.value().get<int>();
        // 兼容旧存档：缺失物品补 100000（背包测试版预置）
        for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
            const auto t = static_cast<cfg::ItemType>(i);
            if (g.playerInv.count(t) == 0) g.playerInv[t] = 100000;
        }
        // 背包机器数（旧存档缺失则保持 initPlayerInventory 的 100000）
        const json& machJson = j.value("machines", json::array());
        for (int i = 0; i < static_cast<int>(machJson.size()) && i < cfg::BUILDING_COUNT; ++i)
            g.backpackMachines[static_cast<size_t>(i)] = machJson[static_cast<size_t>(i)].get<int>();

        // ---- 6. 新手引导进度 ----
        if (j.contains("tutorial")) {
            const json& tj = j["tutorial"];
            g.tutorial.finished = tj.value("finished", false);
            g.tutorial.skipped = tj.value("skipped", false);
            g.tutorial.mistakes = tj.value("mistakes", 0);
            g.tutorial.totalElapsed = tj.value("total_elapsed", 0.0f);
            g.tutorial.done = tj.value("done", std::vector<uint8_t>{});
            g.tutorial.stepTimes = tj.value("step_times", std::vector<float>{});
            g.tutorial.step = tj.value("step", 0);
            g.tutorial.active = tj.value("active", false);
        }
        // 脚本步数变化时补齐数组，避免越界（版本升级安全）
        {
            const int n = tutorial::stepCount();
            g.tutorial.done.resize(static_cast<size_t>(n), 0);
            g.tutorial.stepTimes.resize(static_cast<size_t>(n), 0.0f);
            g.tutorial.step = std::clamp(g.tutorial.step, 0, n);
        }
        g.tutorial.progressCounter = 0;
        g.tutorial.stepElapsed = 0.0f;
        g.tutorial.moveAccum = 0.0f;

        g.power.dirty = true;   // 重建电网拓扑
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[loadGame] 异常: %s\n", e.what());
        return false;
    } catch (...) {
        std::fprintf(stderr, "[loadGame] 未知异常\n");
        return false;
    }
}
