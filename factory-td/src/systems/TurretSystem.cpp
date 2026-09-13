// =====================================================================
// TurretSystem.cpp —— 炮塔系统实现
// 移植 Tower.py（索敌/射击）与 Bullet.py（追踪弹）。
// 性能：敌人先插入SpatialGrid空间哈希，每塔只查询射程覆盖的格子，
//       避免Python的 O(塔数×敌数) 全量距离计算。
// =====================================================================
#include "systems/TurretSystem.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "Game.h"
#include "utils/Profiler.h"

namespace {
/// 计算"距终点最近"评分（Python TARGET_MODE_NEAREST_TO_END）
/// 分数越小越优先：敌人到终点距离 - 已走路径长度
float nearestToEndScore(const Game& g, const Enemy& en) {
    const float cx = en.pos.x + cfg::TILE_SIZE / 2.0f;
    const float cy = en.pos.y + cfg::TILE_SIZE / 2.0f;
    const auto& end = g.enemyWaypoints.back();
    const float distEnd = std::sqrt((cx - end.x) * (cx - end.x) + (cy - end.y) * (cy - end.y));
    return distEnd - en.pathIndex * cfg::TILE_SIZE;
}
} // namespace

// ---------------------------------------------------------------------
// 炮塔索敌 + 射击
// ---------------------------------------------------------------------
void TurretSystem::updateTowers(Game& g, float dt) {
    FT_PROFILE;
    // ---- 收集炮塔实体（EnTT view 批量查询，SoA布局） ----
    auto towerView = g.reg.view<Building, Turret>();
    if (towerView.begin() == towerView.end()) return;   // EnTT 3.13 无 empty()
    std::vector<entt::entity> towers(towerView.begin(), towerView.end());

    // ---- 敌人插入空间哈希 ----
    SpatialGrid sg(cfg::TILE_SIZE);
    auto enemyView = g.reg.view<Enemy>();
    for (auto [e, en] : enemyView.each())
        sg.insert(e, en.pos.x + cfg::TILE_SIZE / 2.0f, en.pos.y + cfg::TILE_SIZE / 2.0f);

    // ---- 每塔独立索敌/冷却（std::execution::par 并行） ----
    // 每个线程只写自己下标对应的结果槽，避免任何共享写入竞争。
    struct Result {
        entt::entity tower = entt::null;
        entt::entity target = entt::null;
    };
    std::vector<Result> results(towers.size());
    std::vector<size_t> indices(towers.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;

    ft::parForEach(indices.begin(), indices.end(), [&](size_t i) {
        const entt::entity e = towers[i];
        auto& b = g.reg.get<Building>(e);
        auto& t = g.reg.get<Turret>(e);
        t.cooldown -= dt;

        // 空间哈希查询射程内的候选敌人（每线程独立的候选缓冲，避免数据竞争）
        std::vector<entt::entity> cand;
        const auto center = g.buildingCenter(b);
        sg.query(center.x, center.y, t.range, cand);
        entt::entity best = entt::null;
        float bestScore = 1e30f;
        for (entt::entity c : cand) {
            if (!g.reg.valid(c) || !g.reg.all_of<Enemy>(c)) continue;
            const auto& en = g.reg.get<Enemy>(c);
            const float cx = en.pos.x + cfg::TILE_SIZE / 2.0f;
            const float cy = en.pos.y + cfg::TILE_SIZE / 2.0f;
            const float dx = cx - center.x, dy = cy - center.y;
            if (dx * dx + dy * dy > t.range * t.range) continue; // 圆形精确判定
            const float score = nearestToEndScore(g, en);
            if (score < bestScore) { bestScore = score; best = c; }
        }

        t.target = best;
        if (best == entt::null || t.cooldown > 0.0f) return;

        // ---- 攻击条件检查（Python Tower.attack） ----
        if (t.isElectric) {
            if (!t.hasPower()) return;       // 电力塔：供电且电力>=10
            t.power -= cfg::ELECTRIC_TOWER_SHOT_COST; // 消耗10电力
        } else {
            if (!t.hasAmmo()) return;        // 弹药塔：无弹药不攻击
            t.ammo--;                        // 消耗1弹药
        }

        // 炮管朝向（8方向贴图）
        const auto& en = g.reg.get<Enemy>(best);
        const float dx2 = (en.pos.x + cfg::TILE_SIZE / 2.0f) - center.x;
        const float dy2 = (en.pos.y + cfg::TILE_SIZE / 2.0f) - center.y;
        const float deg = std::atan2(dx2, -dy2) * 180.0f / 3.14159265f;
        t.barrelDir = static_cast<uint8_t>(((static_cast<int>(std::round(deg / 45.0f)) % 8) + 8) % 8);

        results[i] = {e, best};   // 记录射击请求
        t.cooldown = 1.0f / t.fireRate;
    });

    // ---- 串行创建子弹实体（EnTT非线程安全） ----
    for (const auto& r : results) {
        if (r.tower == entt::null) continue;
        const auto& b = g.reg.get<Building>(r.tower);
        const auto& t = g.reg.get<Turret>(r.tower);
        const auto center = g.buildingCenter(b);
        const auto e = g.reg.create();
        auto& bullet = g.reg.emplace<Bullet>(e);
        bullet.pos = {center.x, center.y};
        bullet.startX = center.x;
        bullet.startY = center.y;
        bullet.speed = t.bulletSpeed;
        bullet.damage = t.damage;
        bullet.towerType = t.type;
        bullet.target = r.target;
    }
}

// ---------------------------------------------------------------------
// 子弹飞行 + 命中
// ---------------------------------------------------------------------
void TurretSystem::updateBullets(Game& g, float dt) {
    FT_PROFILE;
    auto view = g.reg.view<Bullet>();
    std::vector<entt::entity> dead;
    for (auto [e, b] : view.each()) {
        // 目标已消失 → 子弹销毁（Python行为）
        if (!g.reg.valid(b.target) || !g.reg.all_of<Enemy>(b.target)) {
            dead.push_back(e);
            continue;
        }
        auto& en = g.reg.get<Enemy>(b.target);
        const float cx = b.pos.x, cy = b.pos.y;
        const float tx = en.pos.x + cfg::TILE_SIZE / 2.0f;
        const float ty = en.pos.y + cfg::TILE_SIZE / 2.0f;
        const float dx = tx - cx, dy = ty - cy;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < cfg::BULLET_HIT_DISTANCE) {
            en.health -= b.damage;            // 命中扣血
            if (en.health < 0) en.health = 0;
            dead.push_back(e);
        } else {
            // ease-out cubic 缓动加速（Python Bullet.update）
            b.flightTime += dt;
            const float move = b.speed * cfg::BULLET_SPEED_MULT * dt;
            const float t = std::min(b.flightTime * 2.0f, 1.0f);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            b.pos.x += (dx / dist) * move * ease;
            b.pos.y += (dy / dist) * move * ease;
        }
    }
    for (auto e : dead) g.reg.destroy(e);
}
