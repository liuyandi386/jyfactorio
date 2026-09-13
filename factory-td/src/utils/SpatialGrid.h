#pragma once
// =====================================================================
// SpatialGrid.h —— 空间哈希（均匀网格）
//
// 用于加速"圆形范围内实体查询"（炮塔索敌等）。
// Python版每帧对所有塔×所有敌人做 O(N塔×N敌) 全量距离计算；
// 这里先按格子分桶，查询时只遍历覆盖范围内的格子，大幅减少比较次数。
// 格子尺寸取 TILE_SIZE(32px)，塔射程150~250px 只需查询周边 5~8 格范围。
// =====================================================================
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <entt/entity/fwd.hpp>

class SpatialGrid {
public:
    explicit SpatialGrid(float cellSize = 32.0f) : cellSize_(cellSize) {}

    /// 清空所有桶（每帧开始时调用）
    void clear() { cells_.clear(); }

    /// 将实体插入其所在格子桶
    void insert(entt::entity e, float x, float y) {
        cells_[key(cellX(x), cellY(y))].push_back(e);
    }

    /// 查询以(cx,cy)为圆心、radius为半径的圆覆盖格子内的所有实体
    void query(float cx, float cy, float radius, std::vector<entt::entity>& out) const {
        out.clear();
        const int r = static_cast<int>(radius / cellSize_) + 1;
        const int x0 = cellX(cx - radius), x1 = cellX(cx + radius);
        const int y0 = cellY(cy - radius), y1 = cellY(cy + radius);
        for (int gy = y0; gy <= y1; ++gy) {
            for (int gx = x0; gx <= x1; ++gx) {
                auto it = cells_.find(key(gx, gy));
                if (it != cells_.end()) out.insert(out.end(), it->second.begin(), it->second.end());
            }
        }
        (void)r; // r保留给调试/注释
    }

private:
    /// 世界坐标 → 格子坐标
    int cellX(float x) const { return static_cast<int>(x / cellSize_); }
    int cellY(float y) const { return static_cast<int>(y / cellSize_); }

    /// 二维格子坐标 → 一维哈希键
    static int64_t key(int gx, int gy) {
        return (static_cast<int64_t>(gx) << 32) ^ (static_cast<int64_t>(gy) & 0xFFFFFFFFLL);
    }

    float cellSize_;                                       // 格子边长(像素)
    std::unordered_map<int64_t, std::vector<entt::entity>> cells_; // 格子→实体列表
};
