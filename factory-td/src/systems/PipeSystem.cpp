// =====================================================================
// PipeSystem.cpp —— 物品管道系统实现
//
// 与旧传送带的区别：
//   - 取消逐格动画：物品在管道网络内"即时路由"（每个转移周期 BFS
//     一次，找到最近可接收容器直接送达），管道只承担网络拓扑；
//   - 取消面配置九宫格：管道/分流器自动链接四邻容器/机器/管道；
//   - 分流器重写：智能轮询均分——出口满时跳过，不再盲目轮流；
//   - 机器主动从相邻管道拉取所需原料，无关物品不会进入机器库存。
// =====================================================================
#include "systems/PipeSystem.h"
#include <algorithm>
#include <deque>
#include <vector>
#include "systems/MeSystem.h"
#include "utils/Profiler.h"

namespace {

/// 网格邻居建筑实体（越界返回entt::null）
entt::entity neighborAt(const Game& g, int tx, int ty) {
    return g.grid.inBounds(tx, ty) ? g.grid.at(tx, ty).building : entt::null;
}

/// 实体是否为管道/分流器（网络节点）
bool isNetworkNode(const Game& g, entt::entity e) {
    return e != entt::null && g.reg.valid(e) &&
           (g.reg.all_of<Pipe>(e) || g.reg.all_of<SplitterQueue>(e));
}

/// 实体是否为 ME 设备（接口/存储单元/终端）
bool isMeDevice(const Game& g, entt::entity e) {
    return e != entt::null && g.reg.valid(e) &&
           (g.reg.all_of<MeInterface>(e) || g.reg.all_of<MeDrive>(e) ||
            g.reg.all_of<MeTerminal>(e));
}

/// ME 设备连接掩码（bit d = 该方向相邻是 ME 设备）
uint8_t computeMeMask(const Game& g, int x, int y) {
    uint8_t mask = 0;
    for (int d = 0; d < 4; ++d) {
        const int nx = x + cfg::Dir::OFFSETS[d][0];
        const int ny = y + cfg::Dir::OFFSETS[d][1];
        if (isMeDevice(g, neighborAt(g, nx, ny))) mask |= static_cast<uint8_t>(1u << d);
    }
    return mask;
}

/// 计算 (x,y) 处管道/分流器的连接掩码（bit d = 该方向是网络节点）
uint8_t computeMask(const Game& g, int x, int y);

/// 把某实体（管道/分流器/通物设备）的连接掩码重算到其组件
void refreshMask(Game& g, entt::entity e, int x, int y) {
    if (g.reg.all_of<Pipe>(e))
        g.reg.get<Pipe>(e).connMask = computeMask(g, x, y);
    else if (g.reg.all_of<SplitterQueue>(e))
        g.reg.get<SplitterQueue>(e).connMask = computeMask(g, x, y);
    else if (g.reg.all_of<MeInterface>(e))
        g.reg.get<MeInterface>(e).connMask = computeMeMask(g, x, y);
    else if (g.reg.all_of<MeDrive>(e))
        g.reg.get<MeDrive>(e).connMask = computeMeMask(g, x, y);
    else if (g.reg.all_of<MeTerminal>(e))
        g.reg.get<MeTerminal>(e).connMask = computeMeMask(g, x, y);
}

/// 计算 (x,y) 处管道/分流器的连接掩码（bit d = 该方向是网络节点）
uint8_t computeMask(const Game& g, int x, int y) {
    uint8_t mask = 0;
    for (int d = 0; d < 4; ++d) {
        const int nx = x + cfg::Dir::OFFSETS[d][0];
        const int ny = y + cfg::Dir::OFFSETS[d][1];
        if (isNetworkNode(g, neighborAt(g, nx, ny))) mask |= static_cast<uint8_t>(1u << d);
    }
    return mask;
}

/// 端点是否能接收该物品（管道自动链接，不检查面配置）
bool acceptsItem(const Game& g, entt::entity e, cfg::ItemType item) {
    if (e == entt::null || !g.reg.valid(e)) return false;
    if (g.reg.all_of<Bucket>(e)) return g.reg.get<Bucket>(e).canAccept(item); // 储物桶
    if (g.reg.all_of<Turret>(e)) {                                            // 弹药塔
        const auto& t = g.reg.get<Turret>(e);
        return item == cfg::ItemType::Ammo && !t.isElectric &&
               t.ammo < cfg::TURRET_AMMO_MAX_SLOTS * cfg::TURRET_AMMO_MAX_STACK;
    }
    // 机器（熔炉/合金炉/组装机）：只收自己配方所需的原料
    if (g.reg.all_of<Machine, Inventory>(e)) {
        const auto& m = g.reg.get<Machine>(e);
        auto& inv = g.reg.get<Inventory>(e);
        if (inv.isFull()) return false;
        // 组装机/合金炉每种原料缓存上限：防止单种原料囤满堵死其它原料/挤占网络
        if ((m.kind == MachineKind::Assembler &&
             inv.count(item) >= cfg::ASSEMBLER_ITEM_CAP) ||
            (m.kind == MachineKind::AlloyFurnace &&
             inv.count(item) >= cfg::ALLOY_FURNACE_ITEM_CAP)) return false;
        for (auto t : PipeSystem::wantedInputs(g, m))
            if (t == item) return true;
        return false;
    }
    // 发电机：只收煤矿
    if (g.reg.all_of<PowerGeneratorNode, Inventory>(e))
        return item == cfg::ItemType::Coal && !g.reg.get<Inventory>(e).isFull();
    // 通物设备（接口/存储单元/终端）：网络有容量即收——管道自动连接所有类型容器
    if (g.reg.all_of<MeInterface>(e) || g.reg.all_of<MeDrive>(e) ||
        g.reg.all_of<MeTerminal>(e)) {
        const int nid = MeSystem::networkIdOf(g, e);
        if (nid < 0 || nid >= static_cast<int>(MeSystem::networks().size())) return false;
        const auto& net = MeSystem::networks()[static_cast<size_t>(nid)];
        return net.totalItems < net.capacity;
    }
    return false;
}

/// 向端点交付1件物品，成功返回true
bool deliverItem(Game& g, entt::entity e, cfg::ItemType item) {
    if (g.reg.all_of<Bucket>(e)) {
        auto& b = g.reg.get<Bucket>(e);
        if (!b.canAccept(item)) return false;
        b.items.push_back(item);
        return true;
    }
    if (g.reg.all_of<Turret>(e)) {
        auto& t = g.reg.get<Turret>(e);
        if (item != cfg::ItemType::Ammo || t.isElectric ||
            t.ammo >= cfg::TURRET_AMMO_MAX_SLOTS * cfg::TURRET_AMMO_MAX_STACK)
            return false;
        t.ammo++;
        return true;
    }
    // 机器/发电机：物品入库存
    if (g.reg.all_of<Machine, Inventory>(e) ||
        g.reg.all_of<PowerGeneratorNode, Inventory>(e)) {
        return g.reg.get<Inventory>(e).add(item, 1) > 0;
    }
    // 通物设备：物品入网（数字化存储）
    if (g.reg.all_of<MeInterface>(e) || g.reg.all_of<MeDrive>(e) ||
        g.reg.all_of<MeTerminal>(e)) {
        const int nid = MeSystem::networkIdOf(g, e);
        if (nid < 0 || nid >= static_cast<int>(MeSystem::networks().size())) return false;
        return MeSystem::addItem(MeSystem::networksMutable()[static_cast<size_t>(nid)],
                                 item, 1) > 0;
    }
    return false;
}

// ---- BFS 即时路由（有路径即送达） ----
// 用版本戳避免每帧分配大数组；路径可穿过管道与分流器（分流器为直通节点）
std::vector<uint32_t> visited_;
uint32_t stamp_ = 0;

/// 从 (sx,sy) 出发找最近的可接收容器并交付，成功返回true
bool routeFrom(Game& g, int sx, int sy, cfg::ItemType item) {
    const size_t cellCount = static_cast<size_t>(g.grid.w) * g.grid.h;
    if (visited_.size() != cellCount) visited_.assign(cellCount, 0);
    if (++stamp_ == 0) {                       // 戳回绕：清空重来
        std::fill(visited_.begin(), visited_.end(), 0);
        stamp_ = 1;
    }
    auto mark = [&](int x, int y) {
        visited_[static_cast<size_t>(y) * g.grid.w + x] = stamp_;
    };
    auto isMarked = [&](int x, int y) {
        return visited_[static_cast<size_t>(y) * g.grid.w + x] == stamp_;
    };

    mark(sx, sy);
    std::deque<std::pair<int, int>> bfs;

    // 第0跳：直接检查四邻（端点优先）
    for (int d = 0; d < 4; ++d) {
        const int nx = sx + cfg::Dir::OFFSETS[d][0];
        const int ny = sy + cfg::Dir::OFFSETS[d][1];
        if (!g.grid.inBounds(nx, ny) || isMarked(nx, ny)) continue;
        const entt::entity nb = neighborAt(g, nx, ny);
        if (isNetworkNode(g, nb)) { mark(nx, ny); bfs.emplace_back(nx, ny); }
        else if (acceptsItem(g, nb, item) && deliverItem(g, nb, item)) return true;
    }

    int hops = cfg::PIPES_MAX_HOPS;
    while (!bfs.empty() && hops-- > 0) {
        const auto [x, y] = bfs.front();
        bfs.pop_front();
        for (int d = 0; d < 4; ++d) {
            const int nx = x + cfg::Dir::OFFSETS[d][0];
            const int ny = y + cfg::Dir::OFFSETS[d][1];
            if (!g.grid.inBounds(nx, ny) || isMarked(nx, ny)) continue;
            const entt::entity nb = neighborAt(g, nx, ny);
            if (isNetworkNode(g, nb)) { mark(nx, ny); bfs.emplace_back(nx, ny); }
            else if (acceptsItem(g, nb, item) && deliverItem(g, nb, item)) return true;
        }
    }
    return false;   // 无路可送：物品留在管道缓冲（背压）
}

} // namespace

// ---------------------------------------------------------------------
// 机器所需原料清单（数据驱动配方表输入去重）
// ---------------------------------------------------------------------
std::vector<cfg::ItemType> PipeSystem::wantedInputs(const Game& g, const Machine& m) {
    (void)g;
    std::vector<cfg::ItemType> wanted;
    switch (m.kind) {
        case MachineKind::Furnace:
            for (const auto& r : cfg::FURNACE_RECIPES)
                for (auto [t, n] : r.inputs) wanted.push_back(t);
            break;
        case MachineKind::AlloyFurnace: {
            const int rid = std::min(m.recipeId,
                static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
            for (auto [t, n] : cfg::ALLOY_RECIPES[static_cast<size_t>(rid)].inputs)
                wanted.push_back(t);
            break;
        }
        case MachineKind::Assembler: {
            const int rid = std::min(m.recipeId,
                static_cast<int>(cfg::ASSEMBLER_RECIPES.size()) - 1);
            for (auto [t, n] : cfg::ASSEMBLER_RECIPES[static_cast<size_t>(rid)].inputs)
                wanted.push_back(t);
            break;
        }
        default:
            return {};   // 采矿机不需要拉取
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    return wanted;
}

// ---------------------------------------------------------------------
// 连接掩码更新（管道/分流器/通物设备 放置与拆除后调用）
// ---------------------------------------------------------------------
void PipeSystem::updateNeighbors(Game& g, int tx, int ty) {
    FT_PROFILE;
    // 自身 + 四邻共5个位置
    const int xs[5] = {tx, tx, tx + 1, tx - 1, tx};
    const int ys[5] = {ty, ty + 1, ty, ty, ty - 1};
    for (int i = 0; i < 5; ++i) {
        const entt::entity e = neighborAt(g, xs[i], ys[i]);
        if (e != entt::null && g.reg.valid(e)) refreshMask(g, e, xs[i], ys[i]);
    }
}

// ---------------------------------------------------------------------
// 分流器（重写）：吸入不是职责——物品由机器/管道直接送入队列；
// 本函数只做"智能轮询均分输出"：从可用出口中轮流选择，满出口跳过
// ---------------------------------------------------------------------
void PipeSystem::updateSplitters(Game& g, float dt) {
    FT_PROFILE;
    auto view = g.reg.view<Building, SplitterQueue>();
    for (auto [e, b, sp] : view.each()) {
        if (b.type != cfg::BuildingType::Splitter) continue;
        sp.transferTimer += dt;
        if (sp.transferTimer < cfg::SPLITTER_TRANSFER_INTERVAL) continue;
        sp.transferTimer = 0.0f;

        // 每个周期输出至多4件（与4个方向的节奏一致）
        int moved = 0;
        while (!sp.queue.empty() && moved < 4) {
            // 1. 收集当前物品的可用出口（管道缓冲/桶/弹药塔；满的跳过）
            const cfg::ItemType item = sp.queue.front();
            std::vector<int> exits;
            for (int d = 0; d < 4; ++d) {
                const entt::entity nb = neighborAt(g, b.pos.x + cfg::Dir::OFFSETS[d][0],
                                                   b.pos.y + cfg::Dir::OFFSETS[d][1]);
                if (nb == entt::null || !g.reg.valid(nb)) continue;
                if (g.reg.all_of<Pipe>(nb)) {
                    if (!g.reg.get<Pipe>(nb).isFull()) exits.push_back(d);
                } else if (acceptsItem(g, nb, item)) {
                    exits.push_back(d);
                }
            }
            if (exits.empty()) break;   // 无可用出口：物品留在队列

            // 2. 轮询选择（outputIndex 保证均分），跳过本次轮到的满出口
            bool delivered = false;
            for (size_t k = 0; k < exits.size(); ++k) {
                const int d = exits[static_cast<size_t>(sp.outputIndex) % exits.size()];
                sp.outputIndex = (sp.outputIndex + 1) % static_cast<int>(exits.size());
                const entt::entity nb = neighborAt(g, b.pos.x + cfg::Dir::OFFSETS[d][0],
                                                   b.pos.y + cfg::Dir::OFFSETS[d][1]);
                if (g.reg.all_of<Pipe>(nb)) {
                    auto& pbuf = g.reg.get<Pipe>(nb).buffer;
                    if (pbuf.size() < static_cast<size_t>(cfg::PIPES_MAX_BUFFER)) {
                        pbuf.push_back(item);
                        delivered = true;
                    }
                } else if (deliverItem(g, nb, item)) {
                    delivered = true;
                }
                if (delivered) break;
            }
            if (!delivered) break;
            sp.queue.pop_front();
            moved++;
        }
    }
}

// ---------------------------------------------------------------------
// 管道：BFS 即时路由
// ---------------------------------------------------------------------
void PipeSystem::updatePipes(Game& g, float dt) {
    FT_PROFILE;
    auto view = g.reg.view<Building, Pipe>();
    for (auto [e, b, p] : view.each()) {
        if (b.type != cfg::BuildingType::Pipe) continue;
        if (p.buffer.empty()) continue;
        p.transferTimer += dt;
        if (p.transferTimer < cfg::PIPES_TRANSFER_INTERVAL) continue;
        p.transferTimer = 0.0f;

        int moved = 0;
        while (!p.buffer.empty() && moved < cfg::PIPES_PULL_PER_TICK) {
            const cfg::ItemType item = p.buffer.front();
            if (!routeFrom(g, b.pos.x, b.pos.y, item)) break;  // 无处可送 → 背压
            p.buffer.pop_front();
            moved++;
        }
    }
}

// ---------------------------------------------------------------------
// 机器主动拉取（输入总线式）：只取自己需要的原料，
// 无关物品不会流入机器库存。发电机拉煤，熔炉拉矿，组装机拉配方原料。
// ---------------------------------------------------------------------
void PipeSystem::updateMachinePulls(Game& g) {
    FT_PROFILE;
    auto view = g.reg.view<Building, Machine, Inventory, FaceConfig>();
    for (auto [e, b, m, inv, fc] : view.each()) {
        if (inv.isFull()) continue;
        std::vector<cfg::ItemType> wanted = wantedInputs(g, m);
        if (wanted.empty()) continue;   // 采矿机不需要拉取
        // 组装机/合金炉每种原料缓存上限：达到上限的原料本帧不再拉取（避免单种囤满堵死其它）
        const int cap = m.kind == MachineKind::Assembler ? cfg::ASSEMBLER_ITEM_CAP
                      : m.kind == MachineKind::AlloyFurnace ? cfg::ALLOY_FURNACE_ITEM_CAP : 0;
        if (cap > 0) {
            wanted.erase(std::remove_if(wanted.begin(), wanted.end(),
                [&](cfg::ItemType t){ return inv.count(t) >= cap; }),
                wanted.end());
            if (wanted.empty()) continue;
        }

        // 正在冶炼的机器给产物预留空间：否则拉料会填满库存，
        // 导致矿→锭产出时 add 失败、锭被丢弃（网络里锭不再增加）
        if (m.hasJob && (m.kind == MachineKind::Furnace ||
                         m.kind == MachineKind::AlloyFurnace)) {
            int reserve = 0;
            if (m.kind == MachineKind::Furnace) {
                const int rid = std::min(m.recipeId,
                    static_cast<int>(cfg::FURNACE_RECIPES.size()) - 1);
                for (auto [t, n] : cfg::FURNACE_RECIPES[static_cast<size_t>(rid)].outputs)
                    reserve += n;
            } else {
                const int rid = std::min(m.recipeId,
                    static_cast<int>(cfg::ALLOY_RECIPES.size()) - 1);
                for (auto [t, n] : cfg::ALLOY_RECIPES[static_cast<size_t>(rid)].outputs)
                    reserve += n;
            }
            if (inv.totalItems + reserve >= inv.maxSlots * inv.maxStackSize) continue;
        }

        // 从 INPUT 面的相邻管道/分流器/通物接口各取至多1件所需原料
        // 建筑占地的每个面可能并排多格，必须逐格查询占位
        int pulled = 0;
        for (int d = 0; d < 4 && pulled < 4; ++d) {
            if (fc.get(d) != cfg::FaceMode::INPUT) continue;   // 输出/无连接面不拉料
            const FaceTiles ft = faceNeighborTiles(b, d);
            for (int fi = 0; fi < ft.count && pulled < 4; ++fi) {
                const entt::entity nb = neighborAt(g, ft.t[fi].x, ft.t[fi].y);
                if (nb == entt::null || !g.reg.valid(nb)) continue;
                if (g.reg.all_of<Pipe>(nb)) {
                    auto& pbuf = g.reg.get<Pipe>(nb).buffer;
                    for (auto it = pbuf.begin(); it != pbuf.end(); ++it) {
                        if (std::find(wanted.begin(), wanted.end(), *it) == wanted.end())
                            continue;
                        if (inv.add(*it, 1) > 0) {
                            pbuf.erase(it);
                            pulled++;
                            break;
                        }
                    }
                } else if (g.reg.all_of<SplitterQueue>(nb)) {
                    auto& sp = g.reg.get<SplitterQueue>(nb);
                    if (sp.queue.empty()) continue;
                    const cfg::ItemType front = sp.queue.front();
                    if (std::find(wanted.begin(), wanted.end(), front) != wanted.end() &&
                        inv.add(front, 1) > 0) {
                        sp.queue.pop_front();
                        pulled++;
                    }
                } else if (g.reg.all_of<MeInterface>(nb)) {
                    // 通物接口：从所属网络取料（全网共享库存），并遵循接口输出过滤
                    const auto& iface = g.reg.get<MeInterface>(nb);
                    const int nid = iface.networkId;
                    if (nid < 0 || nid >= static_cast<int>(MeSystem::networks().size()))
                        continue;
                    MeNetwork& net = MeSystem::networksMutable()[static_cast<size_t>(nid)];
                    for (auto t : wanted) {
                        // 接口过滤白名单：非空时仅允许锁定的物品被拉出
                        if (!iface.filter.empty() &&
                            std::find(iface.filter.begin(), iface.filter.end(), t) == iface.filter.end())
                            continue;
                        if (MeSystem::countItem(net, t) <= 0) continue;
                        if (inv.add(t, 1) > 0) {
                            MeSystem::removeItem(net, t, 1);
                            pulled++;
                            break;
                        }
                    }
                }
            }
        }
    }

    // ---- 发电机：从相邻管道/通物接口拉煤 ----
    // 逐格遍历四个面的外侧相邻格（统一走 faceNeighborTiles，兼容任意占地）。
    auto gview = g.reg.view<Building, PowerGeneratorNode, Inventory>();
    for (auto [e, b, gen, inv] : gview.each()) {
        if (inv.isFull()) continue;
        bool got = false;   // 每帧每台发电机至多补 1 煤（与原行为一致）
        for (int d = 0; d < 4 && !got; ++d) {
            const FaceTiles ft = faceNeighborTiles(b, d);
            for (int fi = 0; fi < ft.count && !got; ++fi) {
                const entt::entity nb = neighborAt(g, ft.t[fi].x, ft.t[fi].y);
                if (nb == entt::null || !g.reg.valid(nb)) continue;
                if (g.reg.all_of<Pipe>(nb)) {
                    auto& pbuf = g.reg.get<Pipe>(nb).buffer;
                    for (auto it = pbuf.begin(); it != pbuf.end(); ++it) {
                        if (*it != cfg::ItemType::Coal) continue;
                        if (inv.add(cfg::ItemType::Coal, 1) > 0) {
                            pbuf.erase(it);
                            got = true;
                            break;
                        }
                    }
                } else if (g.reg.all_of<MeInterface>(nb)) {
                    const auto& iface = g.reg.get<MeInterface>(nb);
                    const int nid = iface.networkId;
                    if (nid < 0 || nid >= static_cast<int>(MeSystem::networks().size()))
                        continue;
                    // 接口过滤白名单：锁定非煤矿时不允许煤被拉出
                    if (!iface.filter.empty() &&
                        std::find(iface.filter.begin(), iface.filter.end(), cfg::ItemType::Coal) ==
                            iface.filter.end())
                        continue;
                    MeNetwork& net = MeSystem::networksMutable()[static_cast<size_t>(nid)];
                    if (MeSystem::countItem(net, cfg::ItemType::Coal) > 0 &&
                        inv.add(cfg::ItemType::Coal, 1) > 0) {
                        MeSystem::removeItem(net, cfg::ItemType::Coal, 1);
                        got = true;
                        break;
                    }
                }
            }
        }
    }
}
