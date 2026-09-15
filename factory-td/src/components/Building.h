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
    int w = 1;                  // 占地宽(格)——由 cfg::buildingSize() 统一赋值，勿硬编码
    int h = 1;                  // 占地高(格)
    int dir = cfg::Dir::DOWN;   // 朝向（机器输出方向等4方向）

    /// 判断某格是否落在建筑占地范围内
    bool contains(int tx, int ty) const {
        return tx >= pos.x && tx < pos.x + w && ty >= pos.y && ty < pos.y + h;
    }
};

/// 建筑某一面「外侧」相邻的格子
///   当前建筑全部 1×1 → 该面 1 格；多格建筑则为并排多格。
///   物流/电力邻接扫描必须逐格查询，否则多格建筑只有"左上格"那一侧能被接上。
struct FaceTiles {
    GridPos t[4]{};   // 最多 4 格（当前建筑全部 1×1 → 1 格）
    int count = 0;
};

/// 取建筑某一面外侧的所有相邻格（不做越界判定，由调用方查 grid.inBounds）
inline FaceTiles faceNeighborTiles(const Building& b, int dir) {
    FaceTiles r;
    const int dx = cfg::Dir::OFFSETS[dir][0];
    const int dy = cfg::Dir::OFFSETS[dir][1];
    if (dx != 0) {                                  // 左/右面：沿 y 铺开 h 格
        const int bx = (dx > 0) ? b.pos.x + b.w : b.pos.x - 1;
        for (int i = 0; i < b.h && r.count < 4; ++i)
            r.t[r.count++] = {bx, b.pos.y + i};
    } else {                                        // 上/下面：沿 x 铺开 w 格
        const int by = (dy > 0) ? b.pos.y + b.h : b.pos.y - 1;
        for (int i = 0; i < b.w && r.count < 4; ++i)
            r.t[r.count++] = {b.pos.x + i, by};
    }
    return r;
}
