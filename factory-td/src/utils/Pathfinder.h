#pragma once
// =====================================================================
// Pathfinder.h —— 路径工具（预设路径 + A*寻路）
//
// 本作敌人沿用Python的"预设路径点"移动方式（PATH_POINTS折线），
// buildPathTiles() 把折线路径点展开为完整格子路径；
// buildPixelWaypoints() 生成敌人逐点移动所需的像素坐标序列。
// findPathAStar() 为通用A*寻路，当前游戏未使用（预留：如以后改为
// 动态寻路/敌人绕行时可启用）。
// =====================================================================
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <functional>
#include <queue>
#include <vector>
#include <SFML/System/Vector2.hpp>

/// 由折线路径点展开为完整格子路径（Python GameMap._generate_path 逻辑）
/// 水平段沿起点行铺满，垂直段沿终点列铺满。
inline std::vector<sf::Vector2i> buildPathTiles(const std::vector<sf::Vector2i>& points,
                                                 int gridW, int gridH) {
    std::vector<sf::Vector2i> out;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const sf::Vector2i s = points[i], e = points[i + 1];
        // 水平段
        for (int x = std::min(s.x, e.x); x <= std::max(s.x, e.x); ++x)
            if (x >= 0 && x < gridW && s.y >= 0 && s.y < gridH)
                out.push_back({x, s.y});
        // 垂直段
        for (int y = std::min(s.y, e.y); y <= std::max(s.y, e.y); ++y)
            if (e.x >= 0 && e.x < gridW && y >= 0 && y < gridH)
                out.push_back({e.x, y});
    }
    return out;
}

/// 生成敌人移动的像素路径点序列（每个点=格子中心，Python Enemy 逻辑）
inline std::vector<sf::Vector2f> buildPixelWaypoints(const std::vector<sf::Vector2i>& points,
                                                      int tileSize) {
    std::vector<sf::Vector2f> out;
    out.reserve(points.size());
    for (const auto& p : points)
        out.emplace_back(p.x * tileSize + tileSize / 2.0f,
                         p.y * tileSize + tileSize / 2.0f);
    return out;
}

/// 通用 A* 寻路（4方向网格，预留工具）
/// walkable: 一维可通行数组（0不可通行/1可通行），尺寸 w*h。
inline std::vector<sf::Vector2i> findPathAStar(const std::vector<uint8_t>& walkable,
                                                int w, int h,
                                                sf::Vector2i start, sf::Vector2i goal) {
    auto idx = [w](sf::Vector2i p) { return p.y * w + p.x; };
    auto inB = [w, h](sf::Vector2i p) { return p.x >= 0 && p.x < w && p.y >= 0 && p.y < h; };
    static const std::array<sf::Vector2i, 4> DIRS = {{{0,-1},{1,0},{0,1},{-1,0}}};

    if (!inB(start) || !inB(goal)) return {};
    std::vector<sf::Vector2i> came(w * static_cast<size_t>(h), {-1, -1});
    std::vector<int> g(w * static_cast<size_t>(h), INT32_MAX);
    auto hcost = [](sf::Vector2i a, sf::Vector2i b) { return std::abs(a.x-b.x) + std::abs(a.y-b.y); };
    using Node = std::pair<int, sf::Vector2i>; // (f值, 坐标)
    // sf::Vector2 无比较运算符，自定义比较器（按f值小顶堆）
    struct NodeCmp {
        bool operator()(const Node& a, const Node& b) const { return a.first > b.first; }
    };
    std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
    g[idx(start)] = 0;
    open.push({hcost(start, goal), start});

    while (!open.empty()) {
        auto [f, cur] = open.top(); open.pop();
        if (cur == goal) break;
        if (f != g[idx(cur)] + hcost(cur, goal)) continue; // 过期节点
        for (auto d : DIRS) {
            sf::Vector2i n = cur + d;
            if (!inB(n) || !walkable[idx(n)]) continue;
            int ng = g[idx(cur)] + 1;
            if (ng < g[idx(n)]) {
                g[idx(n)] = ng;
                came[idx(n)] = cur;
                open.push({ng + hcost(n, goal), n});
            }
        }
    }
    std::vector<sf::Vector2i> path;
    if (g[idx(goal)] == INT32_MAX) return path; // 不可达
    for (sf::Vector2i p = goal; p != start; p = came[idx(p)]) {
        path.push_back(p);
        if (came[idx(p)] == sf::Vector2i{-1, -1}) return {};
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}
