#pragma once
// =====================================================================
// Pipe.h —— 物品管道组件
//
// 设计：
//   - 管道为1×1方块，自动链接四邻的容器/机器/其他管道（无面配置九宫格）
//   - 物品在管道网络内"即时路由"：每个转移周期做一次 BFS，
//     找到最近的可接收容器直接送达；无动画、无逐格移动
//   - 无处可送时物品停留在管道缓冲（背压），不消失
// =====================================================================
#include <cstdint>
#include <deque>
#include "GameConfig.h"
#include "Item.h"

/// 物品管道组件
struct Pipe {
    uint8_t connMask = 0;                // bit d: 该方向相邻是管道/分流器（用于渲染+路由）
    std::deque<cfg::ItemType> buffer;    // 管道内部缓冲队列
    float transferTimer = 0.0f;          // 路由转移计时器(秒)

    bool isFull() const { return static_cast<int>(buffer.size()) >= cfg::PIPES_MAX_BUFFER; }
};
