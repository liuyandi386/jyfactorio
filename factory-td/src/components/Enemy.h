#pragma once
// =====================================================================
// Enemy.h —— 敌人组件
//
// 敌人沿预设路径(PATH_POINTS)逐点移动：
//   speed 单位是"格/秒"，实际像素速度 = speed × TILE_SIZE。
// 到达终点后扣除玩家生命并移除；被击杀时给予金币奖励（不产生掉落）。
// =====================================================================
#include <cstdint>
#include <SFML/System/Vector2.hpp>
#include "GameConfig.h"
#include "Position.h"

/// 敌人组件
struct Enemy {
    cfg::EnemyType type = cfg::EnemyType::Basic;
    WorldPos pos;                 // 世界像素坐标(左上角)
    int health = 100;             // 当前血量
    int maxHealth = 100;          // 最大血量
    float speed = 1.5f;           // 移动速度(格/秒)
    int reward = 20;              // 击杀金币奖励

    int pathIndex = 0;            // 当前目标路径点索引
    sf::Vector2f target;          // 当前目标路径点(像素坐标)
    bool reachedEnd = false;      // 是否已到达终点
};
