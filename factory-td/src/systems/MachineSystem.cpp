// =====================================================================
// MachineSystem.cpp —— 机器系统实现
// 采矿机/熔炉/组装机/发电机/储物桶的生产与输出逻辑。
// 移植 Miner.py / AmmoFactory.py / Generator.py / Bucket.py 及
// GameScene 的 _update_miner_output / _update_factory_output /
// _update_bucket_output。
// =====================================================================
#include "systems/MachineSystem.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <random>
#include <vector>
#include "Game.h"
#include "systems/MeSystem.h"
#include "systems/PipeSystem.h"
#include "utils/Profiler.h"

namespace {
/// 采矿场选矿用随机数生成器（线程安全：仅主循环调用）
std::mt19937& oreRng() {
    static std::mt19937 rng(42);
    return rng;
}

/// 网格邻居实体（越界返回entt::null）
entt::entity neighborAt(const Game& g, int tx, int ty) {
    return g.grid.inBounds(tx, ty) ? g.grid.at(tx, ty).building : entt::null;
}

/// 目标是通物设备（接口/存储单元/终端）时返回其网络id，否则返回-1
/// （所有通物设备都是网络入口——机器/管道可向任意通物设备入网）
int meNetworkOf(const Game& g, entt::entity e) {
    if (e == entt::null || !g.reg.valid(e)) return -1;
    if (!g.reg.all_of<MeInterface>(e) && !g.reg.all_of<MeDrive>(e) &&
        !g.reg.all_of<MeTerminal>(e))
        return -1;
    return MeSystem::networkIdOf(g, e);
}

/// 目标机器/桶的"对面"是否为INPUT（格雷科技式面配置检查）
bool faceAccepts(const Game& g, entt::entity target, int fromDir) {
    if (target == entt::null || !g.reg.valid(target)) return false;
    if (!g.reg.all_of<FaceConfig>(target)) return true;  // 无面配置(塔等)恒接受
    const auto& fc = g.reg.get<FaceConfig>(target);
    return fc.get(cfg::Dir::OPPOSITE[fromDir]) == cfg::FaceMode::INPUT;
}
} // namespace

// ---------------------------------------------------------------------
// 生产计时更新
// ---------------------------------------------------------------------
void MachineSystem::updateMachines(Game& g, float dt) {
    FT_PROFILE;
    // ---- 采矿场（GT式：范围内采集所有类型矿石 / 虚空采矿场） ----
    {
        auto view = g.reg.view<Building, Machine, Inventory>();
        for (auto [e, b, m, inv] : view.each()) {
            if (m.kind != MachineKind::Miner) continue;
            // 供电检查（测试版采矿机免供电，见 GameConfig.MINER_FREE_POWER）
            if (!m.powered && !cfg::MINER_FREE_POWER) { m.producing = false; continue; }
            if (inv.isFull()) { m.producing = false; continue; }

            m.acc += m.rate * dt;          // 累加产出（个/秒 × 秒）
            m.progress = std::min(1.0f, m.acc / std::max(1.0f, m.rate)); // 进度条(粗略)
            m.producing = m.acc >= 0.999f;

            while (m.acc >= 1.0f) {
                m.acc -= 1.0f;
                cfg::ItemType ore = cfg::ItemType::IronOre;
                bool mined = false;

                if (m.voidMiner) {
                    // 虚空采矿场：8种矿石轮流产出（总量4096/秒，均匀分布）
                    static int roundRobin = 0;
                    ore = cfg::ORE_TYPES[static_cast<size_t>(roundRobin % 8)];
                    roundRobin++;
                    mined = true;
                } else if (m.fixedOre) {
                    // ---- 固定矿点模式：只采绑定的那一个矿点（矿种 = oreFilter） ----
                    // 每轮仍然只结算 1 个物品，速率沿用 m.rate，与原有模式产量完全一致。
                    const entt::entity pick = MinerSystem::boundDeposit(g, b, m);
                    if (pick != entt::null) {
                        auto& dep = g.reg.get<OreDeposit>(pick);
                        ore = dep.type;
                        if (!cfg::ORE_INFINITE) {   // 无限开采：不扣储量、矿点永续
                            dep.amount--;           // 消耗矿点储量
                            if (dep.amount <= 0) {  // 采尽 → 矿点消失，绑定随之失效
                                g.reg.remove<OreDeposit>(pick);
                                g.reg.remove<GridPos>(pick);
                                g.reg.destroy(pick);
                                m.hasBound = false;
                            }
                        }
                        mined = true;
                    }
                } else {
                    // ---- 原有模式：覆盖范围内所有矿点等概率随机取一个（行为与旧版一致） ----
                    const int r = MinerSystem::radiusOf(m);
                    std::vector<entt::entity> candidates;
                    for (auto [oe, pos, dep] : g.reg.view<GridPos, OreDeposit>().each()) {
                        if (dep.amount <= 0) continue;
                        const int dx = std::abs(pos.x - b.pos.x);
                        const int dy = std::abs(pos.y - b.pos.y);
                        if (dx <= r && dy <= r) candidates.push_back(oe);
                    }
                    if (!candidates.empty()) {
                        auto& rng = oreRng();
                        const entt::entity pick = candidates[static_cast<size_t>(
                            rng() % static_cast<uint32_t>(candidates.size()))];
                        auto& dep = g.reg.get<OreDeposit>(pick);
                        ore = dep.type;
                        if (!cfg::ORE_INFINITE) {   // 无限开采：不扣储量、矿点永续
                            dep.amount--;           // 消耗矿点储量
                            if (dep.amount <= 0) {  // 采尽 → 矿点消失
                                g.reg.remove<OreDeposit>(pick);
                                g.reg.remove<GridPos>(pick);
                                g.reg.destroy(pick);
                            }
                        }
                        mined = true;
                    }
                }

                if (mined) inv.add(ore, 1);
                else { m.acc = 0.0f; break; }   // 无矿可采：清空累加，停止
            }
        }
    }

    // ---- 熔炉（数据驱动 FURNACE_RECIPES）：矿石 → 锭 ----
    {
        auto view = g.reg.view<Machine, Inventory>();
        for (auto [e, m, inv] : view.each()) {
            if (m.kind != MachineKind::Furnace) continue;
            // 无任务时按配方表轮询领取新任务（均衡烧制各矿种，避免永远只烧第一种）
            if (!m.hasJob) {
                const size_t n = cfg::FURNACE_RECIPES.size();
                for (size_t k = 0; k < n; ++k) {
                    // 从上次配方之后开始轮询，原料齐即开工
                    const size_t i = (static_cast<size_t>(m.recipeId) + k + 1) % n;
                    const auto& r = cfg::FURNACE_RECIPES[i];
                    bool enough = true;
                    for (auto [t, cnt] : r.inputs)
                        if (inv.count(t) < cnt) { enough = false; break; }
                    if (!enough) continue;
                    for (auto [t, cnt] : r.inputs) inv.remove(t, cnt);
                    m.hasJob = true;
                    m.recipeId = static_cast<int>(i);
                    m.jobInput = r.inputs.front().first;
                    m.jobTime = 0.0f;
                    m.jobTotal = r.craftTime;
                    break;
                }
            }
            // 冶炼计时（产物放不下时不丢弃，保留任务等待空间）
            if (m.hasJob) {
                if (m.jobTime < m.jobTotal) m.jobTime += dt;
                m.progress = std::min(1.0f, m.jobTime / m.jobTotal);
                m.producing = true;
                if (m.jobTime >= m.jobTotal) {
                    const auto& r = cfg::FURNACE_RECIPES[static_cast<size_t>(m.recipeId)];
                    int need = 0;
                    for (auto [t, n] : r.outputs) need += n;
                    // 库存满：移除过剩原料腾位（自愈历史存档里被塞满的矿石），保证锭产出
                    while (inv.totalItems + need > inv.maxSlots * inv.maxStackSize) {
                        bool removed = false;
                        for (auto [t, n] : r.inputs)
                            if (inv.remove(t, 1) > 0) { removed = true; break; }
                        if (!removed) break;
                    }
                    for (auto [t, n] : r.outputs) inv.add(t, n);   // 产出锭
                    m.hasJob = false;
                }
            } else {
                m.producing = false;
                m.progress = 0.0f;
            }
        }
    }

    // ---- 合金炉（数据驱动 ALLOY_RECIPES）：锭 → 合金（选定配方，无需电力） ----
    {
        auto view = g.reg.view<Machine, Inventory>();
        for (auto [e, m, inv] : view.each()) {
            if (m.kind != MachineKind::AlloyFurnace) continue;
            // 当前选定配方（右键合金炉可切换，越界保护）
            const int rid = std::min(m.recipeId,
                static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
            const auto& recipe = cfg::ALLOY_RECIPES[static_cast<size_t>(rid)];
            // 无任务时：检查当前配方原料齐备则领取任务
            if (!m.hasJob) {
                bool enough = true;
                for (auto [t, cnt] : recipe.inputs)
                    if (inv.count(t) < cnt) { enough = false; break; }
                if (enough) {
                    for (auto [t, cnt] : recipe.inputs) inv.remove(t, cnt);
                    m.hasJob = true;
                    m.jobTime = 0.0f;
                    m.jobTotal = recipe.craftTime;
                }
            }
            // 合金冶炼计时（产物放不下时不丢弃，保留任务等待空间）
            if (m.hasJob) {
                if (m.jobTime < m.jobTotal) m.jobTime += dt;
                m.progress = std::min(1.0f, m.jobTime / m.jobTotal);
                m.producing = true;
                if (m.jobTime >= m.jobTotal) {
                    const auto& r = cfg::ALLOY_RECIPES[static_cast<size_t>(rid)];
                    int need = 0;
                    for (auto [t, n] : r.outputs) need += n;
                    while (inv.totalItems + need > inv.maxSlots * inv.maxStackSize) {
                        bool removed = false;
                        for (auto [t, n] : r.inputs)
                            if (inv.remove(t, 1) > 0) { removed = true; break; }
                        if (!removed) break;
                    }
                    for (auto [t, n] : r.outputs) inv.add(t, n);   // 产出合金
                    m.hasJob = false;
                }
            } else {
                m.producing = false;
                m.progress = 0.0f;
            }
        }
    }

    // ---- 组装机（数据驱动 ASSEMBLER_RECIPES）：按"当前选定配方"周期性合成 ----
    {
        auto view = g.reg.view<Machine, Inventory>();
        for (auto [e, m, inv] : view.each()) {
            if (m.kind != MachineKind::Assembler) continue;
            // 当前配方（右键组装机可重新选择；越界保护）
            const int rid = std::min(m.recipeId,
                static_cast<int>(cfg::ASSEMBLER_RECIPES.size()) - 1);
            const auto& recipe = cfg::ASSEMBLER_RECIPES[static_cast<size_t>(rid)];
            m.craftTimer += dt;
            m.progress = std::min(1.0f, m.craftTimer / recipe.craftTime);
            if (m.craftTimer < recipe.craftTime) { m.producing = false; continue; }
            m.craftTimer = 0.0f;
            m.producing = true;
            // 检查配方原料
            bool enough = true;
            for (auto [t, n] : recipe.inputs)
                if (inv.count(t) < n) { enough = false; break; }
            if (!enough) continue;
            for (auto [t, n] : recipe.inputs) inv.remove(t, n);
            for (auto [t, n] : recipe.outputs) inv.add(t, n);  // 产出配方产物
        }
    }

    // ---- 发电机燃料燃烧逻辑位于 PowerSystem::updateGenerators ----

    // ---- 储物桶输出计时（Python Bucket.update） ----
    {
        auto view = g.reg.view<Bucket>();
        for (auto [e, b] : view.each()) b.outputTimer += dt;
    }
}

// ---------------------------------------------------------------------
// 输出推送：机器/桶 → 桶/传送带/相邻机器
// ---------------------------------------------------------------------
void MachineSystem::pushOutputs(Game& g, float dt) {
    FT_PROFILE;
    // ---- 采矿机输出（Python _update_miner_output: 优先桶→传送带→机器，成功即停） ----
    {
        auto view = g.reg.view<Building, Machine, Inventory, FaceConfig>();
        for (auto [e, b, m, inv, fc] : view.each()) {
            if (m.kind != MachineKind::Miner || inv.totalItems <= 0) continue;
            for (int d = 0; d < 4; ++d) {
                if (fc.get(d) != cfg::FaceMode::OUTPUT) continue;
                const int tx = b.pos.x + cfg::Dir::OFFSETS[d][0];
                const int ty = b.pos.y + cfg::Dir::OFFSETS[d][1];
                const entt::entity target = neighborAt(g, tx, ty);
                if (target == entt::null) continue;
                bool delivered = false;

                // 优先储物桶：全部物品转移
                if (g.reg.all_of<Bucket>(target)) {
                    auto& bucket = g.reg.get<Bucket>(target);
                    std::vector<cfg::ItemType> types;   // 快照，避免迭代中修改inventory
                    for (auto& [t, n] : inv.items) types.push_back(t);
                    for (auto t : types) {
                        while (inv.count(t) > 0 && bucket.canAccept(t)) {
                            bucket.items.push_back(t);
                            inv.remove(t, 1);
                        }
                    }
                    delivered = true;
                }
                // 物品管道：一次放一个进管道缓冲（有空间才放）
                else if (g.reg.all_of<Pipe>(target)) {
                    auto& pbuf = g.reg.get<Pipe>(target).buffer;
                    if (pbuf.size() < static_cast<size_t>(cfg::PIPES_MAX_BUFFER)) {
                        for (auto& [t, n] : inv.items) {
                            if (n > 0) {
                                pbuf.push_back(t);
                                inv.remove(t, 1);
                                delivered = true;
                                break;
                            }
                        }
                    }
                }
                // 分流器：放入内部队列（有空间才放）
                else if (g.reg.all_of<SplitterQueue>(target)) {
                    auto& sp = g.reg.get<SplitterQueue>(target);
                    if (sp.queue.size() < static_cast<size_t>(cfg::SPLITTER_MAX_QUEUE)) {
                        for (auto& [t, n] : inv.items) {
                            if (n > 0) {
                                sp.queue.push_back(t);
                                inv.remove(t, 1);
                                delivered = true;
                                break;
                            }
                        }
                    }
                }
                // 任意通物设备（接口/存储/终端）：批量转入网络（容量内至多64件/帧）
                else if (const int nid = meNetworkOf(g, target);
                         nid >= 0 && nid < static_cast<int>(MeSystem::networks().size())) {
                    MeNetwork& net = MeSystem::networksMutable()[static_cast<size_t>(nid)];
                    std::vector<cfg::ItemType> types;
                    for (auto& [t, n] : inv.items) types.push_back(t);
                    int moved = 0;
                    for (auto t : types) {
                        while (inv.count(t) > 0 && moved < 64) {
                            if (MeSystem::addItem(net, t, 1) > 0) {
                                inv.remove(t, 1);
                                moved++;
                                delivered = true;
                            } else break;   // 网络满
                        }
                        if (moved >= 64) break;
                    }
                }
                // 相邻机器（熔炉/组装机，对面INPUT面）：全部转移
                else if (g.reg.all_of<Machine, Inventory>(target) && faceAccepts(g, target, d)) {
                    auto& tinv = g.reg.get<Inventory>(target);
                    std::vector<std::pair<cfg::ItemType, int>> types;
                    for (auto& [t, n] : inv.items) types.emplace_back(t, n);
                    for (auto [t, n] : types) {
                        const int moved = tinv.add(t, n);
                        inv.remove(t, moved);
                    }
                    delivered = true;
                }
                if (delivered && inv.totalItems <= 0) break;
                if (delivered) break;  // Python: 首次成功输出后本帧不再尝试其他方向
            }
        }
    }

    // ---- 熔炉/合金炉/组装机输出（Python _update_factory_output 泛化） ----
    {
        auto view = g.reg.view<Building, Machine, Inventory, FaceConfig>();
        for (auto [e, b, m, inv, fc] : view.each()) {
            if (m.kind == MachineKind::Miner) continue;
            // 可输出物品：
            //   熔炉    → 配方表全部产物（各种锭）
            //   合金炉  → 配方表全部产物（各种合金）
            //   组装机  → 当前选定配方的产物
            std::vector<cfg::ItemType> outTypes;
            if (m.kind == MachineKind::Furnace) {
                for (const auto& r : cfg::FURNACE_RECIPES)
                    for (auto [t, n] : r.outputs) outTypes.push_back(t);
            } else if (m.kind == MachineKind::AlloyFurnace) {
                const int rid = std::min(m.recipeId,
                    static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
                const auto& rec = cfg::ALLOY_RECIPES[static_cast<size_t>(rid)];
                for (auto [t, n] : rec.outputs) outTypes.push_back(t);
                // 清理库存：
                // 1) 不属于当前配方的多余物品（如切换配方前囤积的煤/其它锭）
                // 2) 超过每种缓存上限的配方原料（避免单种原料囤满堵死其它原料）
                for (auto& [t, n] : inv.items) {
                    if (n <= 0) continue;
                    bool isInput = false, isOutput = false;
                    for (auto [it, ic] : rec.inputs) if (it == t) { isInput = true; break; }
                    for (auto [ot, oc] : rec.outputs) if (ot == t) { isOutput = true; break; }
                    if (isOutput) continue;                            // 产物已在上方加入
                    if (!isInput) { outTypes.push_back(t); continue; } // 非配方相关 → 清掉
                    if (n > cfg::ALLOY_FURNACE_ITEM_CAP) outTypes.push_back(t); // 原料超量 → 输出多余部分
                }
            } else {
                const int rid = std::min(m.recipeId,
                    static_cast<int>(cfg::ASSEMBLER_RECIPES.size()) - 1);
                for (auto [t, n] : cfg::ASSEMBLER_RECIPES[static_cast<size_t>(rid)].outputs)
                    outTypes.push_back(t);
            }

            for (int d = 0; d < 4; ++d) {
                if (fc.get(d) != cfg::FaceMode::OUTPUT) continue;
                const int tx = b.pos.x + cfg::Dir::OFFSETS[d][0];
                const int ty = b.pos.y + cfg::Dir::OFFSETS[d][1];
                const entt::entity target = neighborAt(g, tx, ty);
                if (target == entt::null) continue;
                bool delivered = false;
                for (cfg::ItemType outType : outTypes) {
                    if (inv.count(outType) <= 0) continue;
                    if (g.reg.all_of<Bucket>(target)) {
                        auto& bucket = g.reg.get<Bucket>(target);
                        if (bucket.canAccept(outType) && inv.remove(outType, 1) > 0) {
                            bucket.items.push_back(outType);
                            delivered = true;
                            break;
                        }
                    } else if (g.reg.all_of<Pipe>(target)) {
                        auto& pbuf = g.reg.get<Pipe>(target).buffer;
                        if (pbuf.size() < static_cast<size_t>(cfg::PIPES_MAX_BUFFER) &&
                            inv.remove(outType, 1) > 0) {
                            pbuf.push_back(outType);
                            delivered = true;
                            break;
                        }
                    } else if (g.reg.all_of<SplitterQueue>(target)) {
                        auto& sp = g.reg.get<SplitterQueue>(target);
                        if (sp.queue.size() < static_cast<size_t>(cfg::SPLITTER_MAX_QUEUE) &&
                            inv.remove(outType, 1) > 0) {
                            sp.queue.push_back(outType);
                            delivered = true;
                            break;
                        }
                    } else if (const int nid = meNetworkOf(g, target);
                               nid >= 0 &&
                               nid < static_cast<int>(MeSystem::networks().size())) {
                        // 任意通物设备：产物入网（满则退回机器库存）
                        if (inv.remove(outType, 1) > 0) {
                            MeNetwork& net = MeSystem::networksMutable()[static_cast<size_t>(nid)];
                            if (MeSystem::addItem(net, outType, 1) > 0) {
                                delivered = true;
                                break;
                            }
                            inv.add(outType, 1);   // 网络满：退回
                        }
                    }
                }
                if (delivered) break;  // 每帧每机器输出1个（Python行为）
            }
        }
    }

    // ---- 储物桶输出（Python _update_bucket_output） ----
    {
        auto view = g.reg.view<Building, Bucket, FaceConfig>();
        for (auto [e, b, bucket, fc] : view.each()) {
            if (bucket.isEmpty() || bucket.outputTimer < cfg::BUCKET_OUTPUT_INTERVAL) continue;
            for (int d = 0; d < 4; ++d) {
                if (fc.get(d) != cfg::FaceMode::OUTPUT) continue;
                const int tx = b.pos.x + cfg::Dir::OFFSETS[d][0];
                const int ty = b.pos.y + cfg::Dir::OFFSETS[d][1];
                const entt::entity target = neighborAt(g, tx, ty);
                if (target == entt::null) continue;
                // 物品管道 / 分流器（自动链接，无需面配置）
                if (g.reg.all_of<Pipe>(target)) {
                    auto& pbuf = g.reg.get<Pipe>(target).buffer;
                    if (pbuf.size() >= static_cast<size_t>(cfg::PIPES_MAX_BUFFER)) continue;
                    pbuf.push_back(bucket.items.front()); // FIFO取出
                    bucket.items.pop_front();
                    bucket.outputTimer = 0.0f;  // 成功输出才重置计时（Python行为）
                    break;
                }
                if (g.reg.all_of<SplitterQueue>(target)) {
                    auto& sp = g.reg.get<SplitterQueue>(target);
                    if (sp.queue.size() >= static_cast<size_t>(cfg::SPLITTER_MAX_QUEUE)) continue;
                    sp.queue.push_back(bucket.items.front());
                    bucket.items.pop_front();
                    bucket.outputTimer = 0.0f;
                    break;
                }
                // 任意通物设备：桶内物品入网
                if (const int nid = meNetworkOf(g, target);
                    nid >= 0 && nid < static_cast<int>(MeSystem::networks().size())) {
                    MeNetwork& net = MeSystem::networksMutable()[static_cast<size_t>(nid)];
                    if (MeSystem::addItem(net, bucket.items.front(), 1) > 0) {
                        bucket.items.pop_front();
                        bucket.outputTimer = 0.0f;
                        break;
                    }
                }
            }
        }
    }
}

// =====================================================================
// 采矿场模式管理（右键设置面板）
//   原有模式：半径内所有矿点等概率随机采集（旧行为，量不变）
//   固定矿点模式：吸附在矿点旁边，只采 oreFilter 指定的那一个矿点（量不变）
// =====================================================================
namespace MinerSystem {

int radiusOf(const Machine& m) {
    return (m.level == 2) ? cfg::MINER_RADIUS_L2
         : (m.level == 3) ? cfg::MINER_RADIUS_L3
                          : cfg::MINER_RADIUS_L1;
}

namespace {
/// 半径内离建筑最近的矿点（ore == ItemType::COUNT 表示不限矿种）；无则 entt::null
entt::entity nearestDeposit(const Game& g, const Building& b, int r,
                            cfg::ItemType ore) {
    entt::entity best = entt::null;
    int bestDist = 1 << 30;
    for (auto [oe, pos, dep] : g.reg.view<GridPos, OreDeposit>().each()) {
        if (dep.amount <= 0) continue;
        if (ore != cfg::ItemType::COUNT && dep.type != ore) continue;
        const int dx = std::abs(pos.x - b.pos.x);
        const int dy = std::abs(pos.y - b.pos.y);
        const int dist = dx > dy ? dx : dy;      // 切比雪夫距离（方形覆盖范围）
        if (dist > r) continue;
        if (dist < bestDist) { bestDist = dist; best = oe; }
    }
    return best;
}

/// 把采矿场吸附到矿点"旁边"：在矿点的 8 邻格中，优先正邻格、并优先离原位最近的一格。
/// 吸附失败（四周被占满/越界）时保持原位，返回 false（仍可正常绑定采集）。
bool snapBeside(Game& g, entt::entity e, const GridPos& ore) {
    if (!g.reg.all_of<Building>(e)) return false;
    const auto& b = g.reg.get<Building>(e);
    struct Cand { int x, y, rank; };
    std::vector<Cand> cands;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;                 // 矿点自身那格不占
            const int x = ore.x + dx, y = ore.y + dy;
            if (!g.grid.inBounds(x, y)) continue;
            const bool orth = (dx == 0 || dy == 0);           // 正邻格优先
            const int move = std::abs(x - b.pos.x) + std::abs(y - b.pos.y);
            cands.push_back({x, y, (orth ? 0 : 1000) + move});
        }
    }
    std::stable_sort(cands.begin(), cands.end(),
                     [](const Cand& a, const Cand& c) { return a.rank < c.rank; });
    for (const auto& c : cands)
        if (g.moveBuilding(e, c.x, c.y)) return true;
    return false;
}

/// 绑定到指定矿种最近的矿点并吸附（ore == COUNT 表示不限矿种）
bool bindAndSnap(Game& g, entt::entity e, cfg::ItemType ore) {
    if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Building, Machine>(e))
        return false;
    auto& b = g.reg.get<Building>(e);
    auto& m = g.reg.get<Machine>(e);
    const entt::entity oe = nearestDeposit(g, b, radiusOf(m), ore);
    if (oe == entt::null) { m.hasBound = false; return false; }
    const GridPos pos = g.reg.get<GridPos>(oe);
    m.oreFilter = g.reg.get<OreDeposit>(oe).type;   // 以实际矿点矿种为准
    m.boundX = pos.x;
    m.boundY = pos.y;
    m.hasBound = true;
    snapBeside(g, e, pos);                          // 固定矿点模式：生成在矿点旁边
    return true;
}
} // namespace

bool hasOreInRange(const Game& g, const Building& b, const Machine& m, cfg::ItemType ore) {
    return nearestDeposit(g, b, radiusOf(m), ore) != entt::null;
}

entt::entity boundDeposit(const Game& g, const Building& b, Machine& m) {
    const int r = radiusOf(m);
    // 1) 已绑定且仍然有效：坐标一致 + 矿种一致 + 未被采尽 + 仍在覆盖半径内
    if (m.hasBound) {
        for (auto [oe, pos, dep] : g.reg.view<GridPos, OreDeposit>().each()) {
            if (pos.x != m.boundX || pos.y != m.boundY) continue;
            if (dep.amount <= 0 || dep.type != m.oreFilter) break;
            const int dx = std::abs(pos.x - b.pos.x);
            const int dy = std::abs(pos.y - b.pos.y);
            if (dx > r || dy > r) break;
            return oe;
        }
    }
    // 2) 回退：重新在半径内找最近的同矿种矿点并刷新绑定缓存
    const entt::entity oe = nearestDeposit(g, b, r, m.oreFilter);
    if (oe == entt::null) { m.hasBound = false; return entt::null; }
    const GridPos pos = g.reg.get<GridPos>(oe);
    m.boundX = pos.x;
    m.boundY = pos.y;
    m.hasBound = true;
    return oe;
}

bool setFixedMode(Game& g, entt::entity e, bool fixed) {
    if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Building, Machine>(e))
        return false;
    auto& m = g.reg.get<Machine>(e);
    if (!fixed) {
        // → 原有模式：清除绑定，建筑位置保持不变（回到旧版行为）
        m.fixedOre = false;
        m.hasBound = false;
        return true;
    }
    // → 固定矿点模式：优先绑定当前筛矿矿种；该矿种在半径内不存在时退化为"最近的任意矿点"
    if (!bindAndSnap(g, e, m.oreFilter) && !bindAndSnap(g, e, cfg::ItemType::COUNT))
        return false;                       // 半径内根本没有矿点 → 保持原有模式
    g.reg.get<Machine>(e).fixedOre = true;
    return true;
}

bool setOreFilter(Game& g, entt::entity e, cfg::ItemType ore) {
    if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Building, Machine>(e))
        return false;
    auto& m = g.reg.get<Machine>(e);
    m.oreFilter = ore;
    if (!m.fixedOre) return true;           // 原有模式下仅记录偏好，不做绑定
    return bindAndSnap(g, e, ore);          // 固定矿点模式：换矿种即重新绑定/吸附
}

void rotateOutputFace(Game& g, entt::entity e) {
    if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Building>(e)) return;
    auto& b = g.reg.get<Building>(e);
    b.dir = (b.dir + 1) % 4;                // 顺时针旋转一格
    // 同步输出面：保证"贴图箭头方向 == 实际输出方向"（修复贴图与逻辑错位）
    if (g.reg.all_of<FaceConfig>(e)) {
        auto& fc = g.reg.get<FaceConfig>(e);
        for (int i = 0; i < 4; ++i) fc.set(i, cfg::FaceMode::NONE);
        fc.set(b.dir, cfg::FaceMode::OUTPUT);
    }
    PipeSystem::updateNeighbors(g, b.pos.x, b.pos.y);
    MeSystem::markDirty();
    g.power.dirty = true;
}

} // namespace MinerSystem

