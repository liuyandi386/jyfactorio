#pragma once
// =====================================================================
// Building.h —— 建筑基础组件
//
// 所有放置在网格上的建筑（炮塔/机器/电网/物流）都携带此组件，
// 记录建筑类型、网格坐标、占地尺寸与朝向。
// =====================================================================
#include <cstdint>
#include "GameConfig.h"
#include "Position.h"

/// 建筑基础组件
struct Building {
    cfg::BuildingType type = cfg::BuildingType::TowerBasic;
    GridPos pos{};              // 网格坐标（左上角格）
    int w = 1;                  // 占地宽(格)，旧版发电机为2×2
    int h = 1;                  // 占地高(格)
    int dir = cfg::Dir::DOWN;   // 朝向（机器输出方向等4方向）

    /// 判断某格是否落在建筑占地范围内
    bool contains(int tx, int ty) const {
        return tx >= pos.x && tx < pos.x + w && ty >= pos.y && ty < pos.y + h;
    }
};
