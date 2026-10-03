#pragma once
// =====================================================================
// Storage.h —— 储物桶 / 分流器 / 矿点组件
//
// Bucket:        储物桶，FIFO先进先出，按输出间隔定时向OUTPUT面输送
// SplitterQueue: 分流器内部队列（自动链接四邻，智能轮询均分到可用出口）
// OreDeposit:    矿点标记（储量模式见 cfg::ORE_INFINITE：默认无限；有限时采尽消失）
// =====================================================================
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include "GameConfig.h"
#include "Position.h"

/// 储物桶组件
struct Bucket {
    std::deque<cfg::ItemType> items; // FIFO物品队列
    float outputTimer = 0.0f;        // 输出计时器(秒)

    /// 每个输出面的过滤白名单 [UP, RIGHT, DOWN, LEFT]。
    /// 空 = 该面不过滤（什么都能出）；非空 = 只出白名单里的物品。
    /// 白名单可以放多种物品（一个面同时放行铁+铜等）；不在任何白名单里的物品
    /// 就留在桶里等着 —— 这正是"用桶暂存暂时不用的矿石"的用法。
    std::array<std::vector<cfg::ItemType>, 4> faceFilter;

    bool isFull() const { return static_cast<int>(items.size()) >= cfg::BUCKET_CAPACITY; }
    bool isEmpty() const { return items.empty(); }

    /// 该面的过滤是否放行此物品
    bool faceAllows(int d, cfg::ItemType t) const {
        const auto& f = faceFilter[static_cast<size_t>(d)];
        if (f.empty()) return true;   // 空 = 不过滤
        return std::find(f.begin(), f.end(), t) != f.end();
    }

    /// 把物品加入/移出某个面的过滤白名单（面编辑器的过滤面板调用）
    void toggleFaceFilter(int d, cfg::ItemType t) {
        auto& f = faceFilter[static_cast<size_t>(d)];
        const auto it = std::find(f.begin(), f.end(), t);
        if (it == f.end()) f.push_back(t);
        else f.erase(it);
    }
    /// 是否能接收物品（容量+违禁品检查，电力类物品禁止入桶）
    bool canAccept(cfg::ItemType t) const {
        if (isFull()) return false;
        return cfg::ITEM_INFOS[static_cast<size_t>(t)].transportable;
    }
};

/// 分流器组件（自动链接四邻，无需面配置）
struct SplitterQueue {
    std::deque<cfg::ItemType> queue;   // 内部物品队列
    float transferTimer = 0.0f;        // 传输冷却计时器
    int outputIndex = 0;               // 轮询输出索引（跳过满出口）
    uint8_t connMask = 0;              // bit d: 该方向相邻是管道/分流器（用于渲染）

    bool isFull() const { return static_cast<int>(queue.size()) >= cfg::SPLITTER_MAX_QUEUE; }
};

/// 矿点组件（储量模式见 cfg::ORE_INFINITE：默认无限开采，有限时采尽后矿点消失）
struct OreDeposit {
    cfg::ItemType type = cfg::ItemType::IronOre; // 矿种
    int amount = cfg::ORE_DEPOSIT_AMOUNT;        // 剩余储量（无限模式不消耗）
};
