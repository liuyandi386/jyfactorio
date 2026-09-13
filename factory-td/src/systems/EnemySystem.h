#pragma once
// =====================================================================
// EnemySystem.h —— 敌人系统（寻路移动 + 波次）
//
// 移植 Enemy.py + GameScene 波次逻辑：
//   移动：沿预设路径点(PATH_POINTS)逐点前进，速度=格/秒×32像素
//   终点：到达后扣除基地生命（生命归零游戏结束）
//   击杀：金币 += 敌人reward（不产生掉落，按需求）
//   波次：倒计时180s → 每1秒生成1个（每波5+2×波数）→ 清空后等10s
// =====================================================================
#include "GameConfig.h"

class Game;

/// 敌人系统
class EnemySystem {
public:
    /// 敌人移动 + 到达终点结算
    static void update(Game& g, float dt);

    /// 死亡结算（金币奖励）
    static void processKills(Game& g);

    /// 波次状态机（移植 _update_wave）
    static void updateWaves(Game& g, float dt);

    /// 生成一个敌人（Z/X/C 分别生成 普通/快速/坦克，U 兼容普通）
    static void spawnEnemy(Game& g, cfg::EnemyType type = cfg::EnemyType::Basic);
};
