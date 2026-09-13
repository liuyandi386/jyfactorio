// =====================================================================
// EnemySystem.cpp —— 敌人系统实现
// 移植 Enemy.py 移动逻辑与 GameScene 的波次/结算逻辑。
// =====================================================================
#include "systems/EnemySystem.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "Game.h"
#include "utils/Profiler.h"

// ---------------------------------------------------------------------
// 生成敌人
// ---------------------------------------------------------------------
void EnemySystem::spawnEnemy(Game& g, cfg::EnemyType type) {
    // 路径点不足时无法生成（防御性检查）
    if (g.enemyWaypoints.size() < 2) return;
    // 从路径起点生成（Python Enemy.__init__: 起始于PATH_POINTS[0]）
    const sf::Vector2f start = g.enemyWaypoints.front();
    const auto e = g.reg.create();
    auto& en = g.reg.emplace<Enemy>(e);
    en.type = type;
    const auto& st = cfg::ENEMY_STATS[static_cast<size_t>(en.type)];
    en.health = en.maxHealth = st.health;
    en.speed = st.speed;
    en.reward = st.reward;
    en.pos = {start.x - cfg::TILE_SIZE / 2.0f, start.y - cfg::TILE_SIZE / 2.0f};
    en.pathIndex = 1;                          // Python: 初始目标为第2个路径点
    en.target = g.enemyWaypoints[1];
    en.reachedEnd = false;
}

// ---------------------------------------------------------------------
// 敌人移动 + 到达终点
// ---------------------------------------------------------------------
void EnemySystem::update(Game& g, float dt) {
    FT_PROFILE;
    auto view = g.reg.view<Enemy>();
    for (auto [e, en] : view.each()) {
        const float cx = en.pos.x + cfg::TILE_SIZE / 2.0f;
        const float cy = en.pos.y + cfg::TILE_SIZE / 2.0f;
        const float dx = en.target.x - cx;
        const float dy = en.target.y - cy;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < cfg::ENEMY_WAYPOINT_REACH) {
            // 到达当前路径点
            if (en.pathIndex >= static_cast<int>(g.enemyWaypoints.size()) - 1) {
                en.reachedEnd = true;          // 到达终点
            } else {
                en.pathIndex++;                // 取下一个路径点
                en.target = g.enemyWaypoints[en.pathIndex];
            }
        } else {
            // 向目标移动：像素速度 = speed × TILE_SIZE（Python行为）
            const float move = en.speed * cfg::TILE_SIZE * dt;
            en.pos.x += (dx / dist) * move;
            en.pos.y += (dy / dist) * move;
        }
    }

    // 到达终点的敌人：扣生命并移除（Python GameScene.update）
    // 先收集再销毁，避免迭代中删除实体
    std::vector<entt::entity> arrived;
    for (auto [e, en] : view.each())
        if (en.reachedEnd) arrived.push_back(e);
    for (entt::entity e : arrived) {
        g.lives = std::max(0, g.lives - 1);
        g.reg.destroy(e);
        if (g.lives <= 0) g.gameOver = true;   // 基地血量归零 → 游戏结束
    }
}

// ---------------------------------------------------------------------
// 击杀结算：金币奖励（按需求不产生物品掉落）
// ---------------------------------------------------------------------
void EnemySystem::processKills(Game& g) {
    FT_PROFILE;
    auto view = g.reg.view<Enemy>();
    std::vector<entt::entity> dead;
    for (auto [e, en] : view.each())
        if (en.health <= 0) dead.push_back(e);
    for (auto e : dead) {
        const auto& en = g.reg.get<Enemy>(e);
        g.gold += en.reward;                   // 金币奖励（可兑换材料的货币）
        g.reg.destroy(e);
    }
}

// ---------------------------------------------------------------------
// 波次状态机（Python _update_wave）
// ---------------------------------------------------------------------
void EnemySystem::updateWaves(Game& g, float dt) {
    FT_PROFILE;
    // 测试期关闭波次自动生成（按Z键手动生成敌人）；开关见 GameConfig.WAVE_AUTO_SPAWN
    if (!cfg::WAVE_AUTO_SPAWN) return;
    switch (g.waveState) {
        case WaveState::Countdown:
            // 开局倒计时
            g.countdown -= dt;
            if (g.countdown <= 0.0f) {
                g.countdown = 0.0f;
                g.waveState = WaveState::Spawning;
                g.waveTimer = 0.0f;
            }
            break;

        case WaveState::Spawning:
            // 每 SPAWN_INTERVAL 秒生成一个敌人
            if (g.enemiesSpawned < g.enemiesPerWave) {
                g.spawnTimer += dt;
                if (g.spawnTimer >= cfg::SPAWN_INTERVAL) {
                    spawnEnemy(g, cfg::WAVE_ENEMY_TYPE);
                    g.spawnTimer = 0.0f;
                    g.enemiesSpawned++;
                }
            } else if (g.reg.view<Enemy>().begin() == g.reg.view<Enemy>().end()) {
                // 波内敌人清空 → 进入波间等待
                g.waveState = WaveState::WaveEnd;
                g.waveTimer = cfg::WAVE_INTERVAL;
            }
            break;

        case WaveState::WaveEnd:
            // 波间等待 → 下一波（数量 = 5 + 2×波数）
            g.waveTimer -= dt;
            if (g.waveTimer <= 0.0f) {
                g.currentWave++;
                g.enemiesSpawned = 0;
                g.enemiesPerWave = cfg::WAVE_ENEMIES + g.currentWave * 2;
                g.waveState = WaveState::Spawning;
                g.waveTimer = 0.0f;
            }
            break;
    }
}
