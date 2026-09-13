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

/// 目标是ME设备（接口/存储单元/终端）时返回其网络id，否则返回-1
/// （AE2式：所有ME设备都是网络入口——机器/管道可向任意ME设备入网）
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
                } else {
                    // 普通采矿场：从覆盖范围内的矿点随机选一个（按其储量加权）
                    const int r = (m.level == 2) ? cfg::MINER_RADIUS_L2
                                : (m.level == 3) ? cfg::MINER_RADIUS_L3
                                                 : cfg::MINER_RADIUS_L1;
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
                        dep.amount--;               // 消耗矿点储量
                        if (dep.amount <= 0) {      // 采尽 → 矿点消失
                            g.reg.remove<OreDeposit>(pick);
                            g.reg.remove<GridPos>(pick);
                            g.reg.destroy(pick);
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
                // 任意ME设备（接口/存储/终端）：批量转入网络（AE2式，容量内至多64件/帧）
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
                        // 任意ME设备：产物入网（满则退回机器库存）
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
                // 任意ME设备：桶内物品入网
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
