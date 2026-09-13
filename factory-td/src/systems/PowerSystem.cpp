// =====================================================================
// PowerSystem.cpp —— 电网系统实现
//
// 移植并合并 Python 的电力代码：
//   power_manager.py  → 设备注册 + BFS重建网络拓扑 + 统计
//   power_network.py  → 按电线四面配置BFS路由 + 电容充放电 + 供电判定
//   core/PowerGrid.py → 电线杆150px半径连接（旧版电网，合并入统一电网）
//   generator.py      → 燃煤发电机(32EU/秒, 煤燃5秒)
//   entities/Generator.py → 旧版大功率发电机(1煤→3000EU/3秒)
//
// 单位说明：电力单位为 EU/秒（真实时间），与帧率解耦。
// =====================================================================
#include "systems/PowerSystem.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Game.h"
#include "utils/Profiler.h"

namespace {

/// 实体是否为电网设备
bool isGen(const Game& g, entt::entity e) { return g.reg.all_of<PowerGeneratorNode>(e); }
bool isCap(const Game& g, entt::entity e) { return g.reg.all_of<PowerCapacitor>(e); }
bool isConsumer(const Game& g, entt::entity e) { return g.reg.all_of<PowerConsumer>(e); }
bool isPole(const Game& g, entt::entity e) { return g.reg.all_of<PowerPole>(e); }
bool isWire(const Game& g, entt::entity e) {
    return g.reg.valid(e) && g.reg.all_of<Building, FaceConfig>(e) &&
           g.reg.get<Building>(e).type == cfg::BuildingType::PowerWire;
}

/// 获取实体所在网格坐标（建筑）
sf::Vector2i tileOf(const Game& g, entt::entity e) {
    return {g.reg.get<Building>(e).pos.x, g.reg.get<Building>(e).pos.y};
}

} // namespace

// ---------------------------------------------------------------------
// 拓扑重建
// ---------------------------------------------------------------------
void PowerSystem::markDirty(Game& g) { g.power.dirty = true; }

void PowerSystem::rebuild(Game& g) {
    FT_PROFILE;
    auto& state = g.power;
    state.networks.clear();
    state.dirty = false;

    // ---- 收集全部电网设备 ----
    std::vector<entt::entity> devices;
    // 单组件view直接遍历得到实体（EnTT 3.13: 单组件view迭代器解引用为entity）
    for (auto e : g.reg.view<PowerGeneratorNode>()) devices.push_back(e);
    for (auto e : g.reg.view<PowerCapacitor>()) devices.push_back(e);
    for (auto e : g.reg.view<PowerConsumer>()) devices.push_back(e);
    for (auto e : g.reg.view<PowerPole>()) devices.push_back(e);
    for (auto e : g.reg.view<Building>()) {
        const auto& b = g.reg.get<Building>(e);
        if (b.type == cfg::BuildingType::PowerWire) devices.push_back(e);
    }
    if (devices.empty()) return;

    // ---- 构建邻接表 ----
    // 规则1（Python _scan_wire_neighbors）：电线与相邻格子的设备自动连接
    // 规则2（Python PowerGrid.add_pole）：电线杆150px半径内互相连接/连接设备
    std::unordered_map<entt::entity, std::vector<entt::entity>> adj;
    for (entt::entity d : devices) adj[d];

    for (entt::entity d : devices) {
        const sf::Vector2i t = tileOf(g, d);
        if (isWire(g, d)) {
            // 电线：检查4个相邻格子
            for (int dir = 0; dir < 4; ++dir) {
                const int nx = t.x + cfg::Dir::OFFSETS[dir][0];
                const int ny = t.y + cfg::Dir::OFFSETS[dir][1];
                if (!g.grid.inBounds(nx, ny)) continue;
                const entt::entity nb = g.grid.at(nx, ny).building;
                if (nb == entt::null) continue;
                if (isWire(g, nb) || isGen(g, nb) || isCap(g, nb) ||
                    isConsumer(g, nb) || isPole(g, nb)) {
                    adj[d].push_back(nb);
                    adj[nb].push_back(d);
                }
            }
        } else if (isPole(g, d)) {
            // 电线杆：150px半径内连接其他设备/电线杆（恒导通）
            const auto center = g.buildingCenter(g.reg.get<Building>(d));
            for (entt::entity other : devices) {
                if (other == d) continue;
                const auto oc = g.buildingCenter(g.reg.get<Building>(other));
                const float dx = oc.x - center.x, dy = oc.y - center.y;
                if (dx * dx + dy * dy <= cfg::POWER_POLE_RADIUS * cfg::POWER_POLE_RADIUS) {
                    adj[d].push_back(other);
                    adj[other].push_back(d);
                }
            }
        }
    }

    // ---- BFS找连通分量（Python _rebuild_networks） ----
    std::unordered_set<entt::entity> visited;
    int nextId = 0;
    for (entt::entity start : devices) {
        if (visited.count(start)) continue;
        PowerNetwork net;
        net.id = nextId++;
        std::queue<entt::entity> q;
        q.push(start);
        visited.insert(start);
        while (!q.empty()) {
            const entt::entity cur = q.front(); q.pop();
            if (isGen(g, cur)) net.generators.push_back(cur);
            else if (isCap(g, cur)) net.capacitors.push_back(cur);
            else if (isConsumer(g, cur)) net.consumers.push_back(cur);
            else if (isWire(g, cur)) net.wires.push_back(cur);
            else if (isPole(g, cur)) net.poles.push_back(cur);
            for (entt::entity nb : adj[cur]) {
                if (!visited.count(nb)) {
                    visited.insert(nb);
                    q.push(nb);
                }
            }
        }
        state.networks.push_back(std::move(net));
    }
}

// ---------------------------------------------------------------------
// 发电机燃料燃烧（Python Generator.update / PowerGenerator.update）
// ---------------------------------------------------------------------
void PowerSystem::updateGenerators(Game& g, float dt) {
    FT_PROFILE;
    auto view = g.reg.view<PowerGeneratorNode, Inventory>();
    for (auto [e, gen, inv] : view.each()) {
        if (gen.legacyMode) {
            // ---- 旧版发电机：1煤 → 3000EU / 3秒 ----
            if (inv.count(cfg::ItemType::Coal) > 0) {
                gen.burnProgress += dt;
                if (gen.burnProgress >= cfg::LEGACY_GEN_BURN_TIME) {
                    inv.remove(cfg::ItemType::Coal, 1);   // 消耗1煤
                    gen.burnProgress = 0.0f;
                }
                gen.running = true;
            } else {
                gen.running = false;
                gen.burnProgress = 0.0f;
            }
            // Python: 输出 = 3000EU / 3秒 = 1000 EU/s
            gen.outputRate = gen.running
                ? cfg::LEGACY_GEN_POWER_OUTPUT / cfg::LEGACY_GEN_BURN_TIME
                : 0.0f;
        } else {
            // ---- 工业燃煤发电机：32EU/秒，每块煤燃烧5秒 ----
            if (cfg::POWERGEN_INFINITE_FUEL) {
                gen.running = true;                    // 测试模式：无限燃料（默认关闭）
                gen.outputRate = cfg::POWERGEN_OUTPUT_EUT;
                continue;
            }
            if (gen.fuelTime > 0.0f) {
                gen.fuelTime -= dt;
                gen.running = true;
                if (gen.fuelTime <= 0.0f) { gen.fuelTime = 0.0f; gen.running = false; }
            }
            if (gen.fuelTime <= 0.0f && inv.count(cfg::ItemType::Coal) > 0) {
                inv.remove(cfg::ItemType::Coal, 1);      // 投入一块煤
                gen.fuelTime = cfg::POWERGEN_COAL_BURN_TIME;
                gen.running = true;
            }
            gen.outputRate = gen.running ? cfg::POWERGEN_OUTPUT_EUT : 0.0f;
        }
    }
}

// ---------------------------------------------------------------------
// 每帧电力路由（Python PowerNetwork.update）
// ---------------------------------------------------------------------
void PowerSystem::update(Game& g, float dt) {
    FT_PROFILE;
    auto& state = g.power;
    if (state.dirty) rebuild(g);

    state.totalGeneration = 0.0f;
    state.totalConsumption = 0.0f;
    state.totalStorage = 0.0f;
    state.totalCapacity = 0.0f;

    for (auto& net : state.networks) {
        net.totalGeneration = 0.0f;
        net.totalConsumption = 0.0f;
        net.totalStorage = 0.0f;
        net.totalCapacity = 0.0f;
        net.deficit = 0.0f;

        // ---- 统计发电/储能/耗电 ----
        std::vector<entt::entity> runningGens;
        for (entt::entity ge : net.generators) {
            auto& gen = g.reg.get<PowerGeneratorNode>(ge);
            if (gen.running) {
                net.totalGeneration += gen.outputRate;
                runningGens.push_back(ge);
            }
        }
        for (entt::entity ce : net.capacitors) {
            const auto& cap = g.reg.get<PowerCapacitor>(ce);
            net.totalStorage += cap.energy;
            net.totalCapacity += cap.capacity;
        }
        for (entt::entity ce : net.consumers)
            net.totalConsumption += g.reg.get<PowerConsumer>(ce).rate;
        state.totalGeneration += net.totalGeneration;
        state.totalConsumption += net.totalConsumption;
        state.totalStorage += net.totalStorage;
        state.totalCapacity += net.totalCapacity;

        // ---- BFS：从运行中的发电机出发按电线面配置传播电力 ----
        // 导通规则（Python _propagate_from_device）：
        //   发电机/电容 → 电线：电线对面为 INPUT/TRANSFER
        //   电线 → 电线：本面 OUTPUT/TRANSFER 且 对面 INPUT/TRANSFER
        //   电线 → 电容/用电设备：本面 OUTPUT/TRANSFER
        //   电线杆：恒导通（旧版电网合并）
        std::unordered_set<entt::entity> powered;
        std::queue<entt::entity> q;
        for (entt::entity ge : runningGens) { powered.insert(ge); q.push(ge); }

        while (!q.empty()) {
            const entt::entity cur = q.front(); q.pop();
            const sf::Vector2i t = tileOf(g, cur);
            for (int dir = 0; dir < 4; ++dir) {
                const int nx = t.x + cfg::Dir::OFFSETS[dir][0];
                const int ny = t.y + cfg::Dir::OFFSETS[dir][1];
                if (!g.grid.inBounds(nx, ny)) continue;
                const entt::entity nb = g.grid.at(nx, ny).building;
                if (nb == entt::null || powered.count(nb)) continue;

                bool canReach = false;
                const bool curWire = isWire(g, cur);
                const bool nbWire = isWire(g, nb);

                if (isPole(g, cur) || isPole(g, nb)) {
                    canReach = true;                          // 电线杆恒导通
                } else if (curWire && nbWire) {
                    // 电线 → 电线：双方面配置匹配
                    const auto& myFc = g.reg.get<FaceConfig>(cur);
                    const auto& nbFc = g.reg.get<FaceConfig>(nb);
                    const auto myFace = myFc.get(dir);
                    const auto nbFace = nbFc.get(cfg::Dir::OPPOSITE[dir]);
                    canReach = (myFace == cfg::FaceMode::OUTPUT || myFace == cfg::FaceMode::TRANSFER) &&
                               (nbFace == cfg::FaceMode::INPUT || nbFace == cfg::FaceMode::TRANSFER);
                } else if (curWire) {
                    // 电线 → 设备（电容/用电设备）：本面OUTPUT/TRANSFER
                    const auto myFace = g.reg.get<FaceConfig>(cur).get(dir);
                    canReach = (myFace == cfg::FaceMode::OUTPUT || myFace == cfg::FaceMode::TRANSFER) &&
                               (isCap(g, nb) || isConsumer(g, nb));
                } else if (nbWire) {
                    // 设备（发电机/电容）→ 电线：对面INPUT/TRANSFER
                    const auto nbFace = g.reg.get<FaceConfig>(nb).get(cfg::Dir::OPPOSITE[dir]);
                    canReach = (nbFace == cfg::FaceMode::INPUT || nbFace == cfg::FaceMode::TRANSFER) &&
                               (isGen(g, cur) || isCap(g, cur));
                }

                if (canReach) {
                    powered.insert(nb);
                    q.push(nb);
                }
            }
            // 电线杆半径连接（恒导通，旧版电网规则）
            if (isPole(g, cur)) {
                const auto center = g.buildingCenter(g.reg.get<Building>(cur));
                for (entt::entity other : net.generators) {
                    if (powered.count(other)) continue;
                    const auto oc = g.buildingCenter(g.reg.get<Building>(other));
                    const float dx = oc.x - center.x, dy = oc.y - center.y;
                    if (dx * dx + dy * dy <= cfg::POWER_POLE_RADIUS * cfg::POWER_POLE_RADIUS) {
                        powered.insert(other);
                        q.push(other);
                    }
                }
                for (entt::entity other : net.capacitors) {
                    if (powered.count(other)) continue;
                    const auto oc = g.buildingCenter(g.reg.get<Building>(other));
                    const float dx = oc.x - center.x, dy = oc.y - center.y;
                    if (dx * dx + dy * dy <= cfg::POWER_POLE_RADIUS * cfg::POWER_POLE_RADIUS) {
                        powered.insert(other);
                        q.push(other);
                    }
                }
                for (entt::entity other : net.consumers) {
                    if (powered.count(other)) continue;
                    const auto oc = g.buildingCenter(g.reg.get<Building>(other));
                    const float dx = oc.x - center.x, dy = oc.y - center.y;
                    if (dx * dx + dy * dy <= cfg::POWER_POLE_RADIUS * cfg::POWER_POLE_RADIUS) {
                        powered.insert(other);
                        q.push(other);
                    }
                }
            }
        }

        // ---- 有电的电容库作为二级电源（Python行为） ----
        for (entt::entity ce : net.capacitors) {
            const auto& cap = g.reg.get<PowerCapacitor>(ce);
            if (cap.energy > 0.0f && powered.count(ce)) q.push(ce);
        }
        while (!q.empty()) {
            const entt::entity cur = q.front(); q.pop();
            const sf::Vector2i t = tileOf(g, cur);
            for (int dir = 0; dir < 4; ++dir) {
                const int nx = t.x + cfg::Dir::OFFSETS[dir][0];
                const int ny = t.y + cfg::Dir::OFFSETS[dir][1];
                if (!g.grid.inBounds(nx, ny)) continue;
                const entt::entity nb = g.grid.at(nx, ny).building;
                if (nb == entt::null || powered.count(nb)) continue;
                const bool nbWire = isWire(g, nb);
                if (isPole(g, nb)) { powered.insert(nb); q.push(nb); continue; }
                if (!nbWire) continue;
                const auto nbFace = g.reg.get<FaceConfig>(nb).get(cfg::Dir::OPPOSITE[dir]);
                if (nbFace == cfg::FaceMode::INPUT || nbFace == cfg::FaceMode::TRANSFER) {
                    powered.insert(nb);
                    q.push(nb);
                }
            }
        }

        // ---- 电力平衡：可用 = 发电 + 已通电电容最大输出 ----
        float available = net.totalGeneration;
        for (entt::entity ce : net.capacitors) {
            const auto& cap = g.reg.get<PowerCapacitor>(ce);
            if (powered.count(ce) && cap.energy > 0.0f)
                available += cap.maxOutput;
        }
        float needed = 0.0f;
        for (entt::entity ce : net.consumers)
            if (powered.count(ce)) needed += g.reg.get<PowerConsumer>(ce).rate;

        // ---- 电力平衡（单位: EU/秒，充放电量按本帧时间折算） ----
        float surplus = available - needed;   // EU/秒
        if (surplus > 0.0f) {
            // 盈余 → 充入已通电的电容库（本帧电量 = 速率 × dt）
            std::vector<entt::entity> caps;
            for (entt::entity ce : net.capacitors)
                if (powered.count(ce)) caps.push_back(ce);
            if (!caps.empty()) {
                const float perCap = surplus * dt / static_cast<float>(caps.size());
                for (entt::entity ce : caps)
                    g.reg.get<PowerCapacitor>(ce).charge(perCap, dt);
            }
        } else if (surplus < 0.0f) {
            // 不足 → 从已通电的电容库放电补充
            std::vector<entt::entity> caps;
            for (entt::entity ce : net.capacitors)
                if (powered.count(ce)) caps.push_back(ce);
            if (!caps.empty()) {
                const float perCap = -surplus * dt / static_cast<float>(caps.size());
                float discharged = 0.0f;
                for (entt::entity ce : caps)
                    discharged += g.reg.get<PowerCapacitor>(ce).discharge(perCap, dt);
                surplus += discharged / std::max(dt, 1e-6f);   // 折算回 EU/秒
            }
        }

        // ---- 供电判定（Python: powered + 供需平衡） ----
        const bool enough = needed <= 0.0f || surplus >= 0.0f;
        for (entt::entity ce : net.consumers) {
            auto& c = g.reg.get<PowerConsumer>(ce);
            const bool isPowered = powered.count(ce) && enough;
            c.powered = isPowered;
            // 同步到具体设备：电力塔 / 采矿机
            if (g.reg.all_of<Turret>(ce)) {
                auto& t = g.reg.get<Turret>(ce);
                t.gridPowered = isPowered;
                if (isPowered) t.power = t.maxPower; // Python: 供电时填满内部电力
            } else if (g.reg.all_of<Machine>(ce)) {
                g.reg.get<Machine>(ce).powered = isPowered;
            }
        }

        // ---- 网络状态 ----
        if (net.generators.empty() && net.totalStorage <= 0.0f) {
            net.status = "offline";
        } else if (surplus < 0.0f) {
            net.status = "low_power";
            net.deficit = -surplus;
        } else {
            net.status = "normal";
        }
    }

    // ---- 全局状态（Python _update_stats） ----
    if (state.networks.empty()) state.status = "offline";
    else {
        bool anyLow = false, anyNormal = false;
        for (const auto& n : state.networks) {
            if (n.status == "low_power") anyLow = true;
            if (n.status == "normal") anyNormal = true;
        }
        if (anyLow) state.status = "low_power";
        else if (!anyNormal) state.status = "offline";
        else state.status = "normal";
    }
}
