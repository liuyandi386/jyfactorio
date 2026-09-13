#pragma once
// =====================================================================
// Item.h —— 物品类型与堆叠
//
// Inventory: 通用库存组件（采矿机/熔炉/组装机/塔/发电机/矿点使用）
// =====================================================================
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include "GameConfig.h"

/// 通用库存组件（Python InventoryComponent 移植）
struct Inventory {
    int maxSlots = 10;        // 槽位数
    int maxStackSize = 50;    // 单槽最大堆叠
    std::unordered_map<cfg::ItemType, int> items; // 物品→数量
    int totalItems = 0;       // 总物品数

    Inventory() = default;
    Inventory(int slots, int stack) : maxSlots(slots), maxStackSize(stack) {}

    /// 添加物品，返回实际添加数量
    int add(cfg::ItemType t, int amount = 1) {
        const int space = maxSlots * maxStackSize - totalItems;
        const int can = std::min(amount, space);
        if (can <= 0) return 0;
        items[t] += can;
        totalItems += can;
        return can;
    }

    /// 移除物品，返回实际移除数量
    int remove(cfg::ItemType t, int amount = 1) {
        auto it = items.find(t);
        if (it == items.end()) return 0;
        const int can = std::min(amount, it->second);
        it->second -= can;
        totalItems -= can;
        if (it->second <= 0) items.erase(it);
        return can;
    }

    /// 获取某物品数量
    int count(cfg::ItemType t) const {
        auto it = items.find(t);
        return it == items.end() ? 0 : it->second;
    }

    /// 库存是否已满
    bool isFull() const { return totalItems >= maxSlots * maxStackSize; }
};
