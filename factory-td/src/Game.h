#pragma once
// =====================================================================
// Game.h —— 游戏主控类
//
// 对应 Python 的 core/Game.py（主循环）+ main.py GameScene（场景总控）：
//   - 拥有 ECS 注册表(EnTT)、200×200网格、地形、摄像机、资源、UI
//   - 建筑放置/拆除、波次状态机、存档读写
//   - update() 按 Python GameScene.update 的顺序编排各系统
// =====================================================================
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>

#include "GameConfig.h"
#include "Camera.h"
#include "AssetManager.h"
#include "systems/PowerSystem.h"
#include "systems/TutorialSystem.h"
#include "components/Position.h"
#include "components/FaceConfig.h"
#include "components/Building.h"
#include "components/Item.h"
#include "components/Pipe.h"
#include "components/Me.h"
#include "components/Machine.h"
#include "components/Power.h"
#include "components/Turret.h"
#include "components/Enemy.h"
#include "components/Bullet.h"
#include "components/Storage.h"
#include "utils/SpatialGrid.h"

class GameUI;

// ---------------------------------------------------------------------
// 网格
// ---------------------------------------------------------------------
/// 网格单元：该格建筑
struct GridCell {
    entt::entity building = entt::null; // 该格上的建筑（2×2建筑在其所有格子登记）
};

/// 200×200 游戏网格（SoA思路：单元紧凑排列，缓存友好）
struct Grid {
    int w = 0, h = 0;
    std::vector<GridCell> cells;

    Grid() = default;
    Grid(int w_, int h_) : w(w_), h(h_), cells(static_cast<size_t>(w_) * h_) {}

    /// 取格子（x/y为网格坐标）
    GridCell& at(int x, int y) { return cells[static_cast<size_t>(y) * w + x]; }
    const GridCell& at(int x, int y) const { return cells[static_cast<size_t>(y) * w + x]; }
    /// 坐标是否在网格内
    bool inBounds(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h; }
};

// ---------------------------------------------------------------------
// 波次状态机（Python GameScene）
// ---------------------------------------------------------------------
enum class WaveState : uint8_t {
    Countdown,  // 开局倒计时
    Spawning,   // 出怪中
    WaveEnd     // 波间等待
};

// ---------------------------------------------------------------------
// 游戏模式（两种模式完全独立，互不嵌套）
// ---------------------------------------------------------------------
//   Normal   —— 普通关卡：常规塔防流程，不带任何新手引导介入
//   Tutorial —— 新手教程：独立的教学关卡，由主菜单「新手教程」按钮进入
//
// 由 main.cpp 在创建 Game 时确定，整局不可变；各系统据此决定是否接入引导层。
enum class GameMode : uint8_t {
    Normal,
    Tutorial
};

// ---------------------------------------------------------------------
// 游戏主控
// ---------------------------------------------------------------------
class Game {
public:
    /// mode 决定本局走「普通关卡」还是「新手教程」流程（默认普通关卡）
    explicit Game(GameMode mode = GameMode::Normal);
    ~Game();

    /// 游戏主循环（main.cpp调用）
    void run();

    // ================= 核心数据 =================
    GameMode mode = GameMode::Normal; // 本局模式（构造时确定，两种模式互不嵌套）
    /// 是否新手教程模式
    bool isTutorial() const { return mode == GameMode::Tutorial; }
    entt::registry reg;              // ECS注册表（SoA布局，views批量查询）
    Grid grid{cfg::GRID_WIDTH, cfg::GRID_HEIGHT};
    std::vector<uint8_t> terrain;    // 0草地 1路径（与Python tiles一致）
    std::vector<sf::Vector2f> enemyWaypoints; // 敌人像素路径点序列
    Camera camera;                   // 摄像机
    AssetManager assets;             // 资源管理
    GameUI* ui = nullptr;            // 游戏UI（Game.cpp中创建）
    PowerState power;                // 电网状态（PowerSystem）

    // ================= 玩家状态 =================
    int gold = cfg::INITIAL_GOLD;    // 金币（击杀奖励，作为兑换材料的货币）
    int lives = cfg::INITIAL_LIVES;  // 基地生命
    std::unordered_map<cfg::ItemType, int> playerInv; // 背包物品（从商店购买所得）
    std::array<int, cfg::BUILDING_COUNT> backpackMachines{}; // 背包机器库存
    bool hasSelection = false;       // 是否选中建筑
    cfg::BuildingType selected = cfg::BuildingType::TowerBasic;
    bool previewActive = false;      // 放置预览（悬停格）
    sf::Vector2i previewTile{0, 0};
    bool previewCanBuild = false;    // 预览格是否可放置
    bool hasFreePlace = false;       // 商店兑换机器后的一次免费放置机会
    cfg::BuildingType freePlaceType = cfg::BuildingType::TowerBasic;
    entt::entity faceEditTarget = entt::null; // 面配置编辑目标实体

    // ================= 波次状态 =================
    WaveState waveState = WaveState::Countdown;
    int currentWave = 1;
    int enemiesSpawned = 0;
    int enemiesPerWave = cfg::WAVE_ENEMIES;
    float spawnTimer = 0.0f;
    float waveTimer = 0.0f;
    float countdown = cfg::INITIAL_COUNTDOWN;

    // ================= 全局状态 =================
    bool paused = false;
    bool gameOver = false;
    /// 暂停面板选择「返回主界面」后置 true：run() 退出，main.cpp 重新进入启动入口系统
    bool returnToMenu = false;
    std::array<bool, 4> keys{};      // WASD按键状态
    tutorial::State tutorial;        // 新手引导进度（仅教程模式使用；普通关卡恒为空）

    // ================= 建筑放置 =================
    /// 检查资源是否足够（成本表来自BUILDING_INFOS）
    bool canAfford(cfg::BuildingType t) const;
    /// 扣除建造成本
    void deductCost(cfg::BuildingType t);
    /// 返还建造成本（拆除时）
    void refundCost(cfg::BuildingType t);
    /// 该格是否被建筑占用
    bool isOccupied(int tx, int ty) const;
    /// 指定占地是否可放置（地形+占用检查；传送带按Python跳过地形检查）
    bool canPlace(int tx, int ty, cfg::BuildingType t) const;
    /// 放置建筑（返回实体；失败返回entt::null）
    entt::entity placeBuilding(int tx, int ty, cfg::BuildingType t, int dir,
                               bool deduct = true);
    /// 拆除建筑并返还成本
    void removeBuilding(entt::entity e, bool refund = true);
    /// 移动建筑到新格（1×1；采矿场"固定矿点模式"吸附到矿点旁时使用）
    /// 目标格不可放置时保持原位并返回 false；成功返回 true
    bool moveBuilding(entt::entity e, int tx, int ty);
    /// 选择建筑（同步UI按钮高亮）
    void selectBuilding(cfg::BuildingType t);
    /// 用指定方向放置建筑（方向悬浮窗回调）
    void placeBuildingWithDirection(cfg::BuildingType t, sf::Vector2i tile, int dir);
    /// Delete键：删除鼠标指向的建筑
    void deleteAtCursor();
    /// 旋转选中建筑（R键，Python为空实现，保留）
    void rotateSelected() {}
    /// TAB循环切换建筑
    void cycleBuilding();

    // ================= 敌人/波次 =================
    void spawnEnemy();               // U键/波次生成一个敌人

    // ================= 存档 =================
    void saveGame();
    void loadGame();

    // ================= 电网辅助 =================
    /// 标记电网拓扑需要重建（放置/拆除电网设施或改面配置后调用）
    void rebuildPowerNetworks() { PowerSystem::markDirty(*this); }

    // ================= 工具函数 =================
    /// 屏幕坐标→世界坐标
    sf::Vector2f screenToWorld(sf::Vector2f s) const;
    /// 世界坐标→屏幕坐标
    sf::Vector2f worldToScreen(sf::Vector2f w) const;
    /// 世界坐标→屏幕坐标（双参数便捷重载）
    sf::Vector2f worldToScreen(float x, float y) const { return worldToScreen({x, y}); }
    /// 世界坐标→网格坐标
    sf::Vector2i tileAt(sf::Vector2f world) const;
    /// 网格坐标→世界像素坐标(左上角)
    sf::Vector2f tileWorld(sf::Vector2i t) const;
    /// 建筑中心像素坐标
    sf::Vector2f buildingCenter(const Building& b) const;
    /// 悬浮提示（鼠标悬停机器/塔/桶等时更新）
    void updateHoverTooltip();
    /// 鼠标悬停的建筑实体（渲染射程圈等用）
    entt::entity hoveredEntity = entt::null;
    /// 敌人悬停血条数据
    entt::entity hoveredEnemy() const;

    // 窗口在构造函数体内按用户设置（显示模式/分辨率）创建，先声明后创建
    sf::RenderWindow window;

private:
    /// 生成地形与路径
    void generateTerrain();
    /// 生成矿点（随机种子42，Python _generate_ore_deposits）
    void generateOreDeposits();
    /// 初始化玩家无限资源
    void initPlayerInventory();
    /// 注册建筑到网格单元
    void registerToGrid(entt::entity e, const Building& b);
    /// 从网格单元注销建筑
    void unregisterFromGrid(entt::entity e);
    /// 事件处理
    void processEvents();
    /// 更新游戏状态（Python GameScene.update 顺序）
    void update(float dt);
    /// 渲染
    void render();
    /// 按当前 gset 显示模式重建窗口（暂停面板改设置后调用，须在事件循环之外）
    void applyDisplayMode();

    sf::Clock clock;                 // 帧时钟（std::chrono驱动帧率稳定）
};
