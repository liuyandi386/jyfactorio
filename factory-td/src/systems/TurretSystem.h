#pragma once
// =====================================================================
// TurretSystem.h —— 炮塔系统（锁定+射击+子弹）
//
// 移植 Tower.py / Bullet.py：
//   索敌：SpatialGrid空间哈希查询射程内敌人，
//        默认优先"距终点最近"（TARGET_MODE_NEAREST_TO_END）
//   射击：冷却完成且（弹药塔有弹药 / 电力塔有电）时发射追踪弹
//   子弹：ease-out缓动加速，距离<12像素命中
// =====================================================================
class Game;

/// 炮塔系统
class TurretSystem {
public:
    /// 炮塔索敌+射击（并行查询，串行生成子弹）
    static void updateTowers(Game& g, float dt);

    /// 子弹飞行+命中判定
    static void updateBullets(Game& g, float dt);
};
