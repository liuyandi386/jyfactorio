#pragma once
// =====================================================================
// MeSystem.h —— 通物网络存储物流系统（后期科技）
//
// 前期用物品管道（PipeSystem，逐段运输）；
// 后期用通物网络：物品作为"数据"存入网络，全网共享。
//
//   方块          | 作用
//   MeInterface  | 桥接器：从相邻管道/分流器吸入物品入网；
//                | 向相邻机器/发电机/塔/桶导出物品出网
//   MeDrive      | 存储单元：提供网络容量（每个 ME_DRIVE_CAPACITY 件）
//   MeTerminal   | 终端：右键查看全网物品清单
//
// 设备四邻自动链接，连通块 = 一个网络；网络存储全网共享。
// 网络编号按"网格行主序 BFS"确定 → 布局相同则编号相同（存档可复现）；
// 拓扑变化时以各旧网络的锚点(最左上设备格)决定物品归属，物品不丢失。
// =====================================================================
#include <unordered_map>
#include <vector>
#include "Game.h"

/// 一个通物网络的存储（全网物品共享）
struct MeNetwork {
    std::unordered_map<cfg::ItemType, int> items; // 物品类型 → 数量
    int totalItems = 0;                           // 物品总数
    int capacity = 0;                             // 总容量（各存储单元之和）
    int anchorX = -1, anchorY = -1;               // 锚点格（最左上设备，拓扑变化时物品归属依据）
    std::vector<entt::entity> interfaces;         // 本网络接口列表（重建时按确定顺序填充）
    int exportCursor = 0;                         // 轮询出口指针（round-robin 均分）
    float exportTimer = 0.0f;                     // 出网导出节流计时（秒）
};

class MeSystem {
public:
    /// 标记网络拓扑待重建（放置/拆除通物设备或加载存档后调用）
    static void markDirty() { dirty_ = true; }

    /// 重建网络（行主序BFS，编号确定）
    static void rebuildNetworks(Game& g);

    /// 每帧更新：入网吸入 + 出网导出
    static void update(Game& g, float dt);

    /// 网络存储表（UI/存档访问，只读）
    static const std::vector<MeNetwork>& networks() { return networks_; }
    /// 网络存储表（存档恢复/物流写入用）
    static std::vector<MeNetwork>& networksMutable() { return networks_; }

    /// 实体所属网络编号（无网络返回 -1）
    static int networkIdOf(const Game& g, entt::entity e);

    /// 网络加物品（容量内），返回实际加入数量
    static int addItem(MeNetwork& net, cfg::ItemType t, int n);
    /// 网络取物品，返回实际取出数量
    static int removeItem(MeNetwork& net, cfg::ItemType t, int n);
    /// 网络物品数量
    static int countItem(const MeNetwork& net, cfg::ItemType t);

    /// 让单个接口向四邻导出 1 件物品（物品级轮询；成功返回 true）
    static bool exportOne(Game& g, MeNetwork& net, const Building& b, MeInterface& iface);

private:
    /// 把指定物品交付给四邻中第一个能接收的目标（机器/发电机/塔/桶）
    static bool deliverItemToNeighbor(Game& g, MeNetwork& net, const Building& b, cfg::ItemType t);

    inline static bool dirty_ = true;
    inline static std::vector<MeNetwork> networks_;
};
