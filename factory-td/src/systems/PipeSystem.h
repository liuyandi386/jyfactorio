#pragma once
// =====================================================================
// PipeSystem.h —— 物品管道系统（EnderIO / Pipez / AE2 式）
//
// 替代原传送带系统，核心差异：
//   1. 无动画：物品不在管道上逐格移动，而是"即时路由"——
//      每个转移周期从管道缓冲做一次 BFS，找到最近的可接收容器直接送达
//      （AE2 电缆网络模型：有路径即送达，管道只承担网络拓扑）
//   2. 自动链接：管道/分流器自动连接四邻的容器、机器与其他管道，
//      取消面配置九宫格（EnderIO 导管的自动连接逻辑）
//   3. 分流器重写：自动吸入相邻管道的物品，智能轮询均分到"可用的"
//      出口（管道缓冲/桶/塔），出口满时跳过（不再盲目轮询）
//   4. 机器从相邻管道"主动拉取"所需原料（AE2 输入总线式），
//      杜绝无关物品污染机器库存
// =====================================================================
#include "Game.h"

class PipeSystem {
public:
    /// 机器所需原料清单（数据驱动配方表输入去重；管道拉取与ME导出共用）
    static std::vector<cfg::ItemType> wantedInputs(const Game& g, const Machine& m);
    /// 重算 (tx,ty) 处及其四邻物流设备（管道/分流器/ME设备）的连接掩码
    static void updateNeighbors(Game& g, int tx, int ty);

    /// 分流器：吸入相邻管道物品 + 智能轮询均分输出
    static void updateSplitters(Game& g, float dt);

    /// 管道：BFS 即时路由，把缓冲物品送到最近可接收容器
    static void updatePipes(Game& g, float dt);

    /// 机器/发电机：从相邻管道拉取所需原料（每帧每邻管至多1件）
    static void updateMachinePulls(Game& g);
};
