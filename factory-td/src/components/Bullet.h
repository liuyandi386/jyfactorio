#pragma once
// =====================================================================
// Bullet.h —— 子弹组件
//
// 追踪弹：持续锁定目标敌人，带ease-out缓动加速，距离<12像素判定命中。
// 目标死亡/被移除后子弹自动销毁。
// =====================================================================
#include <cstdint>
#include <entt/entity/entity.hpp>   // entt::null 常量定义
#include "GameConfig.h"
#include "Position.h"

/// 子弹组件
struct Bullet {
    WorldPos pos;                 // 当前位置
    float startX = 0.0f, startY = 0.0f; // 发射起点（用于拖尾效果）
    float speed = 10.0f;          // 飞行速度(格/秒)
    int damage = 20;              // 命中伤害
    float flightTime = 0.0f;      // 已飞行时间（缓动计算）
    cfg::TurretType towerType = cfg::TurretType::Basic; // 来源塔类型(拖尾颜色)
    entt::entity target = entt::null; // 目标敌人实体
};
