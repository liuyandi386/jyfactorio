#pragma once
// =====================================================================
// PowerSystem.h —— 电网系统（格雷科技式 EU 电力系统）
//
// 移植并合并 Python 的两套电力代码：
//   systems/power/power_manager.py + power_network.py（工业EU电网）
//   core/PowerGrid.py（旧版电线杆电网）
//
// 拓扑：电线按"相邻格子接触"自动连接（Python行为），电线杆按150px
//       半径连接设备/其他电线杆（旧版行为），二者合并为统一电网。
// 路由：每帧从运行中的发电机出发，按电线四面配置 BFS 传播电力；
//       电量按"发电-耗电"平衡，盈余充入电容库，不足时电容库放电，
//       供电不足的塔进入断电状态。
// =====================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <entt/entity/fwd.hpp>

class Game;

/// 单个电力网络（一个连通分量）
struct PowerNetwork {
    int id = 0;
    std::vector<entt::entity> generators;  // 发电机
    std::vector<entt::entity> capacitors;  // 电容库
    std::vector<entt::entity> consumers;   // 用电设备（电力塔/采矿机）
    std::vector<entt::entity> wires;       // 电力线缆
    std::vector<entt::entity> poles;       // 电线杆
    // 统计（供UI显示）
    float totalGeneration = 0.0f;   // 发电 EU/秒
    float totalConsumption = 0.0f;  // 耗电 EU/秒
    float totalStorage = 0.0f;      // 储能
    float totalCapacity = 0.0f;     // 容量
    float deficit = 0.0f;           // 电力缺口
    std::string status = "offline"; // normal / low_power / offline
};

/// 全局电网状态（由 PowerSystem 维护）
struct PowerState {
    std::vector<PowerNetwork> networks; // 所有电力网络
    bool dirty = true;                  // 拓扑需要重建
    // 全局统计
    float totalGeneration = 0.0f;
    float totalConsumption = 0.0f;
    float totalStorage = 0.0f;
    float totalCapacity = 0.0f;
    std::string status = "offline";
};

/// 电网系统
class PowerSystem {
public:
    /// 标记拓扑脏（放置/拆除电网设施后调用）
    static void markDirty(Game& g);

    /// 重建网络拓扑（BFS找连通分量，Python _rebuild_networks）
    static void rebuild(Game& g);

    /// 每帧更新：路由电力 + 充放电 + 供电判定 + 统计（Python PowerNetwork.update）
    static void update(Game& g, float dt);

    /// 更新发电机燃料燃烧（Python generator.update / PowerGenerator.update）
    static void updateGenerators(Game& g, float dt);
};
