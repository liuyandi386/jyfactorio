#pragma once
// =====================================================================
// Turret.h —— 炮塔组件
//
// 分两类：
//   弹药塔(basic/rapid/sniper)：消耗弹药攻击，弹药由传送带补给
//   电力塔(electric)：接入电网供电，每次射击消耗内部电力10点
// 索敌模式：优先攻击"距终点最近"的敌人（Python默认 TARGET_MODE_NEAREST_TO_END）
// =====================================================================
#include <cstdint>
#include <entt/entity/entity.hpp>   // entt::null 常量定义
#include "GameConfig.h"

/// 炮塔组件
struct Turret {
    cfg::TurretType type = cfg::TurretType::Basic;
    float range = 150.0f;        // 射程(像素)
    int damage = 20;             // 单发伤害
    float fireRate = 1.0f;       // 射速(发/秒)
    float bulletSpeed = 10.0f;   // 子弹速度
    float cooldown = 0.0f;       // 剩余冷却时间(秒)

    bool isElectric = false;     // 是否电力塔
    int ammo = 0;                // 弹药塔当前弹药数
    float power = 50.0f;         // 电力塔内部电力
    float maxPower = 50.0f;      // 电力塔内部电力上限
    bool gridPowered = false;    // 电网供电标志(由PowerSystem设置)

    uint8_t barrelDir = 0;       // 炮管朝向(8方向，用于贴图)
    entt::entity target = entt::null; // 当前锁定目标敌人

    /// 是否有弹药（弹药塔）
    bool hasAmmo() const { return ammo > 0; }

    /// 是否有足够电力（电力塔: 供电且内部电力>=10）
    bool hasPower() const { return gridPowered && power >= cfg::ELECTRIC_TOWER_SHOT_COST; }
};
