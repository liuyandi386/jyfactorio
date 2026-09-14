// =====================================================================
// MeSystem.cpp —— 通物网络存储物流系统实现（后期科技）
//
// 前期：物品管道（PipeSystem，逐段运输）；
// 后期：通物网络——物品作为"数据"存入网络，全网共享：
//   - MeDrive 提供容量，MeInterface 桥接物理世界（吸入管道/分流器，
//     导出给机器/发电机/塔/桶），MeTerminal 查询。
//   - 设备四邻自动链接，连通块 = 一个网络；编号按行主序BFS确定，
//     布局相同则编号相同（存档可复现）；拓扑变化时物品按锚点归属，
//     合并不丢失、分裂随锚点。
// =====================================================================
#include "systems/MeSystem.h"
#include <algorithm>
#include <deque>
#include <utility>
#include "systems/PipeSystem.h"
#include "utils/Profiler.h"

namespace {

entt::entity neighborAt(const Game& g, int tx, int ty) {
    return g.grid.inBounds(tx, ty) ? g.grid.at(tx, ty).building : entt::null;
}

bool isMeDevice(const Game& g, entt::entity e) {
    return e != entt::null && g.reg.valid(e) &&
           (g.reg.all_of<MeInterface>(e) || g.reg.all_of<MeDrive>(e) ||
            g.reg.all_of<MeTerminal>(e));
}

} // namespace

// ---------------------------------------------------------------------
// 网络物品操作
// ---------------------------------------------------------------------
int MeSystem::addItem(MeNetwork& net, cfg::ItemType t, int n) {
    const int can = std::min(n, net.capacity - net.totalItems);
    if (can <= 0) return 0;
    net.items[t] += can;
    net.totalItems += can;
    return can;
}

int MeSystem::removeItem(MeNetwork& net, cfg::ItemType t, int n) {
    auto it = net.items.find(t);
    if (it == net.items.end()) return 0;
    const int can = std::min(n, it->second);
    it->second -= can;
    net.totalItems -= can;
    if (it->second <= 0) net.items.erase(it);
    return can;
}

int MeSystem::countItem(const MeNetwork& net, cfg::ItemType t) {
    auto it = net.items.find(t);
    return it == net.items.end() ? 0 : it->second;
}

int MeSystem::networkIdOf(const Game& g, entt::entity e) {
    if (e == entt::null || !g.reg.valid(e)) return -1;
    if (g.reg.all_of<MeInterface>(e)) return g.reg.get<MeInterface>(e).networkId;
    if (g.reg.all_of<MeDrive>(e)) return g.reg.get<MeDrive>(e).networkId;
    if (g.reg.all_of<MeTerminal>(e)) return g.reg.get<MeTerminal>(e).networkId;
    return -1;
}

// ---------------------------------------------------------------------
// 网络重建（行主序BFS；旧网络物品按锚点归属迁移，不丢失）
// ---------------------------------------------------------------------
void MeSystem::rebuildNetworks(Game& g) {
    FT_PROFILE;
    // 1. 暂存旧网络物品（键 = 锚点格索引）
    std::unordered_map<size_t, MeNetwork> oldNets;
    for (auto& net : networks_)
        if (net.anchorX >= 0)
            oldNets[static_cast<size_t>(net.anchorY) * g.grid.w + net.anchorX] = net;
    networks_.clear();

    // 2. 行主序扫描 + BFS 连通块
    std::vector<int> comp(static_cast<size_t>(g.grid.w) * g.grid.h, -1);
    for (int ty = 0; ty < g.grid.h; ++ty) {
        for (int tx = 0; tx < g.grid.w; ++tx) {
            const size_t idx = static_cast<size_t>(ty) * g.grid.w + tx;
            if (comp[idx] != -1) continue;
            if (!isMeDevice(g, g.grid.at(tx, ty).building)) continue;

            const int id = static_cast<int>(networks_.size());
            MeNetwork net;
            net.anchorX = tx;
            net.anchorY = ty;
            std::deque<std::pair<int, int>> bfs;
            bfs.emplace_back(tx, ty);
            comp[idx] = id;
            while (!bfs.empty()) {
                const auto [x, y] = bfs.front();
                bfs.pop_front();
                const entt::entity de = g.grid.at(x, y).building;
                // 登记网络编号 + 累计容量
                if (g.reg.all_of<MeInterface>(de)) {
                    g.reg.get<MeInterface>(de).networkId = id;
                    net.interfaces.push_back(de);   // 收集接口（BFS 顺序确定）
                } else if (g.reg.all_of<MeDrive>(de)) {
                    g.reg.get<MeDrive>(de).networkId = id;
                    net.capacity += cfg::ME_DRIVE_CAPACITY;
                } else if (g.reg.all_of<MeTerminal>(de)) {
                    g.reg.get<MeTerminal>(de).networkId = id;
                }
                // 四邻扩展
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + cfg::Dir::OFFSETS[d][0];
                    const int ny = y + cfg::Dir::OFFSETS[d][1];
                    if (!g.grid.inBounds(nx, ny)) continue;
                    const size_t nidx = static_cast<size_t>(ny) * g.grid.w + nx;
                    if (comp[nidx] != -1) continue;
                    if (!isMeDevice(g, g.grid.at(nx, ny).building)) continue;
                    comp[nidx] = id;
                    bfs.emplace_back(nx, ny);
                }
            }
            // 3. 恢复物品：旧网络锚点落在本网络内 → 物品并入（按容量截断）
            for (const auto& [key, oldNet] : oldNets)
                if (comp[key] == id)
                    for (const auto& [t, n] : oldNet.items)
                        addItem(net, t, n);
            networks_.push_back(std::move(net));
        }
    }
    dirty_ = false;
}

// ---------------------------------------------------------------------
// 每帧更新：入网吸入 + 出网导出
// ---------------------------------------------------------------------
void MeSystem::update(Game& g, float dt) {
    FT_PROFILE;
    if (dirty_) rebuildNetworks(g);

    // ---- 阶段1：入网吸入（接口从相邻管道/分流器吸物品入网） ----
    auto view = g.reg.view<Building, MeInterface>();
    for (auto [e, b, iface] : view.each()) {
        if (b.type != cfg::BuildingType::MeInterface) continue;
        const int nid = iface.networkId;
        if (nid < 0 || nid >= static_cast<int>(networks_.size())) continue;
        iface.timer += dt;
        if (iface.timer < cfg::ME_TRANSFER_INTERVAL) continue;
        iface.timer = 0.0f;
        MeNetwork& net = networks_[static_cast<size_t>(nid)];

        int imported = 0;
        for (int d = 0; d < 4 && imported < cfg::ME_IMPORT_PER_TICK; ++d) {
            const entt::entity nb = neighborAt(g, b.pos.x + cfg::Dir::OFFSETS[d][0],
                                               b.pos.y + cfg::Dir::OFFSETS[d][1]);
            if (nb == entt::null || !g.reg.valid(nb)) continue;
            if (g.reg.all_of<Pipe>(nb)) {
                auto& pbuf = g.reg.get<Pipe>(nb).buffer;
                while (!pbuf.empty() && imported < cfg::ME_IMPORT_PER_TICK) {
                    if (addItem(net, pbuf.front(), 1) > 0) { pbuf.pop_front(); imported++; }
                    else break;   // 网络已满
                }
            } else if (g.reg.all_of<SplitterQueue>(nb)) {
                auto& sp = g.reg.get<SplitterQueue>(nb);
                while (!sp.queue.empty() && imported < cfg::ME_IMPORT_PER_TICK) {
                    if (addItem(net, sp.queue.front(), 1) > 0) { sp.queue.pop_front(); imported++; }
                    else break;
                }
            }
        }
    }

    // ---- 阶段2：出网导出（round-robin 逐件均分，按网络 + 轮询指针） ----
    for (auto& net : networks_) {
        const size_t n = net.interfaces.size();
        if (n == 0) continue;
        net.exportTimer += dt;
        if (net.exportTimer < cfg::ME_TRANSFER_INTERVAL) continue;
        net.exportTimer = 0.0f;
        // 每轮每个接口导 1 件，最多 ME_EXPORT_PER_TICK 轮，逐件均分
        for (int round = 0; round < cfg::ME_EXPORT_PER_TICK; ++round) {
            bool anyDelivered = false;
            for (size_t k = 0; k < n; ++k) {
                const size_t idx = (static_cast<size_t>(net.exportCursor) + k) % n;
                const entt::entity e = net.interfaces[idx];
                if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<MeInterface>(e)) continue;
                if (exportOne(g, net, g.reg.get<Building>(e), g.reg.get<MeInterface>(e)))
                    anyDelivered = true;
            }
            if (!anyDelivered) break;   // 无接口能再导出（物品耗尽或出口全满）
        }
        net.exportCursor = static_cast<int>((net.exportCursor + 1) % n);   // 起点轮换
    }
}

// ---------------------------------------------------------------------
// 单个接口向四邻导出 1 件（物品级轮询，多物品时均分调度）
// ---------------------------------------------------------------------
bool MeSystem::exportOne(Game& g, MeNetwork& net, const Building& b, MeInterface& iface) {
    // 过滤白名单：非空时仅导出列表中的物品（锁定输出）
    auto allows = [&iface](cfg::ItemType t) {
        if (iface.filter.empty()) return true;
        return std::find(iface.filter.begin(), iface.filter.end(), t) != iface.filter.end();
    };
    // 物品级 round-robin：从 itemCursor 起按 ItemType 枚举序找下一个可交付物品
    for (int offset = 0; offset < cfg::ITEM_COUNT; ++offset) {
        const cfg::ItemType t = static_cast<cfg::ItemType>(
            (iface.itemCursor + offset) % cfg::ITEM_COUNT);
        if (countItem(net, t) <= 0 || !allows(t)) continue;
        if (deliverItemToNeighbor(g, net, b, t)) {
            iface.itemCursor = (iface.itemCursor + offset + 1) % cfg::ITEM_COUNT;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------
// 把指定物品交付给四邻中第一个能接收的目标（机器/发电机/塔/桶）
// ---------------------------------------------------------------------
bool MeSystem::deliverItemToNeighbor(Game& g, MeNetwork& net, const Building& b, cfg::ItemType t) {
    for (int d = 0; d < 4; ++d) {
        const entt::entity nb = neighborAt(g, b.pos.x + cfg::Dir::OFFSETS[d][0],
                                           b.pos.y + cfg::Dir::OFFSETS[d][1]);
        if (nb == entt::null || !g.reg.valid(nb)) continue;
        // 机器：只收配方所需原料（INPUT 面 + 产物预留）
        if (g.reg.all_of<Machine, Inventory>(nb)) {
            auto& inv = g.reg.get<Inventory>(nb);
            if (inv.isFull()) continue;
            if (g.reg.all_of<FaceConfig>(nb) &&
                g.reg.get<FaceConfig>(nb).get(cfg::Dir::OPPOSITE[d]) != cfg::FaceMode::INPUT)
                continue;
            const auto& m = g.reg.get<Machine>(nb);
            if (m.hasJob && (m.kind == MachineKind::Furnace ||
                             m.kind == MachineKind::AlloyFurnace)) {
                int reserve = 0;
                if (m.kind == MachineKind::Furnace) {
                    const int rid = std::min(m.recipeId,
                        static_cast<int>(cfg::FURNACE_RECIPES.size()) - 1);
                    for (auto [tt, cnt] : cfg::FURNACE_RECIPES[static_cast<size_t>(rid)].outputs)
                        reserve += cnt;
                } else {
                    const int rid = std::min(m.recipeId,
                        static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
                    for (auto [tt, cnt] : cfg::ALLOY_RECIPES[static_cast<size_t>(rid)].outputs)
                        reserve += cnt;
                }
                if (inv.totalItems + reserve >= inv.maxSlots * inv.maxStackSize) continue;
            }
            // 机器只收自己配方所需的原料
            bool wants = false;
            for (auto w : PipeSystem::wantedInputs(g, m))
                if (w == t) { wants = true; break; }
            if (!wants) continue;
            // 组装机/合金炉每种原料缓存上限：防止单种原料囤满堵死其它原料/挤占网络
            if ((m.kind == MachineKind::Assembler &&
                 inv.count(t) >= cfg::ASSEMBLER_ITEM_CAP) ||
                (m.kind == MachineKind::AlloyFurnace &&
                 inv.count(t) >= cfg::ALLOY_FURNACE_ITEM_CAP)) continue;
            if (inv.add(t, 1) > 0) { removeItem(net, t, 1); return true; }
        }
        // 发电机：只收煤
        else if (g.reg.all_of<PowerGeneratorNode, Inventory>(nb)) {
            if (t != cfg::ItemType::Coal) continue;
            auto& inv = g.reg.get<Inventory>(nb);
            if (!inv.isFull() && inv.add(cfg::ItemType::Coal, 1) > 0) {
                removeItem(net, cfg::ItemType::Coal, 1);
                return true;
            }
        }
        // 弹药塔：只收弹药
        else if (g.reg.all_of<Turret>(nb)) {
            if (t != cfg::ItemType::Ammo) continue;
            auto& tt = g.reg.get<Turret>(nb);
            if (!tt.isElectric &&
                tt.ammo < cfg::TURRET_AMMO_MAX_SLOTS * cfg::TURRET_AMMO_MAX_STACK) {
                tt.ammo++;
                removeItem(net, cfg::ItemType::Ammo, 1);
                return true;
            }
        }
        // 储物桶：只收可运输物品
        else if (g.reg.all_of<Bucket>(nb)) {
            auto& bk = g.reg.get<Bucket>(nb);
            if (bk.canAccept(t)) {
                bk.items.push_back(t);
                removeItem(net, t, 1);
                return true;
            }
        }
    }
    return false;
}
