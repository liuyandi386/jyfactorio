#pragma once
// =====================================================================
// TutorialSystem.h —— 新手引导系统（分步教学 / 渐进难度 / 即时反馈）
//
// 设计目标：
//   1. 分步教学：把"移动 → 选中 → 建造 → 冶炼 → 物流 → 防御 → 电力 → 进阶"
//      拆成一串可验证的小目标，一次只推进一步，避免信息轰炸。
//   2. 渐进难度：步骤按章节(Chapter)由易到难排列，前一章掌握后才进入下一章，
//      后期章节引入更复杂的机制（炮塔+敌人测试、供电网络、自动化）。
//   3. 即时反馈：常驻目标横幅（目标/按键/进度）+ 目标处脉冲高亮 +
//      完成打勾提示 + 误操作红线纠正。
//   4. 叙事融合：全部文案以"织星工业AI 织女星"的通讯口吻给出，引导表现为
//      同伴的通讯，而非弹窗打断；引导层不阻断任何操作，玩家可自由游玩。
//   5. 可跳过：F2 或横幅上的「跳过引导」按钮，随时退出，且状态被记录。
//   6. 进度记录：每步的完成情况/耗时/误操作次数写入 saves/tutorial.json，
//      作为独立于主存档的学习进度档案。
//
// 模式归属（重要）：
//   本系统只服务于「新手教程」模式（Game::mode == GameMode::Tutorial）。
//   普通关卡是完全独立的模式，不接入引导层：不进 begin、不推进、不绘制；
//   事件钩子虽由各系统统一调用，但引导未激活时内部会立即 return。
//
// 与其它系统的关系：
//   - Game 持有一个 tutorial::State（见 Game.h），本系统只做"纯函数式"操作它。
//   - 事件钩子由 PlayerSystem / Game::placeBuilding / EnemySystem 调用。
//   - 绘制由 GameUI::draw 末尾调用（保证位于 UI 最上层，但仍是半透明的非模态层）。
// =====================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "GameConfig.h"

class Game;

namespace tutorial {

// ---------------------------------------------------------------------
// 章节（渐进式难度：按顺序解锁）
// ---------------------------------------------------------------------
enum class Chapter : uint8_t {
    Basics,      // 第一章 · 观察与移动
    Production,  // 第二章 · 建造与生产
    Logistics,   // 第三章 · 物流与冶炼
    Defense,     // 第四章 · 防御与战斗
    Power,       // 第五章 · 电力网络
    Automation,  // 第六章 · 自动化与进阶
    Count
};

/// 章节中文名
const char* chapterName(Chapter c);

// ---------------------------------------------------------------------
// 步骤完成判定类型（数据驱动脚本的"条件"字段）
// ---------------------------------------------------------------------
enum class Task : uint8_t {
    ReadNarration,      // 阅读型：计时 autoSeconds 后自动完成
    MoveCamera,         // 用 WASD 移动摄像机（累计位移达标）
    SelectBuilding,     // 选中指定建筑（点按钮 / 数字快捷键）
    PlaceBuilding,      // 成功放置指定建筑（count 个）
    RightClickBuilding, // 右键指定建筑（旋转输出面 / 打开面板）
    SpawnEnemy,         // 手动生成敌人（Z/X/C/U）
    KillEnemy,          // 击杀至少 count 个敌人
    OpenHelp,           // 打开说明书（H / F1）
    Finish              // 引导结束
};

/// 单个教学步骤
struct Step {
    Chapter chapter = Chapter::Basics;
    const char* title = "";      // 目标标题（横幅主行）
    const char* keys = "";       // 按键/操作提示（横幅副行）
    const char* narration = "";  // 叙事旁白（织女星通讯，底部对话框）
    const char* hint = "";       // 达成后的即时反馈
    const char* mistake = "";    // 误操作时的纠正提示
    Task task = Task::ReadNarration;
    cfg::BuildingType building = cfg::BuildingType::TowerBasic;  // 关联建筑
    int count = 1;               // 需要的次数
    float autoSeconds = 0.0f;    // ReadNarration 自动前进秒数
};

/// 引导进度（仅新手教程模式使用；镜像到独立的 saves/tutorial.json）
struct State {
    bool active = false;           // 引导进行中
    bool skipped = false;          // 玩家主动跳过
    bool finished = false;         // 全程完成
    int step = 0;                  // 当前步骤索引
    int mistakes = 0;              // 累计误操作次数
    int progressCounter = 0;       // 当前步骤内的进度计数（如已放置数量）
    float stepElapsed = 0.0f;      // 当前步骤已用时
    float totalElapsed = 0.0f;     // 引导总用时
    float moveAccum = 0.0f;        // 移动步骤的累计位移（像素）
    std::vector<uint8_t> done;     // 每步是否完成
    std::vector<float> stepTimes;  // 每步完成耗时（学习进度参考）
    bool justAdvanced = false;     // 本帧是否有步骤刚完成（用于打勾动画）
};

// ---------------------------------------------------------------------
// 脚本（只读表）
// ---------------------------------------------------------------------
const std::vector<Step>& script();
int stepCount();
/// 安全取当前步骤（越界时返回最后一步）
const Step& stepAt(const State& s, int index);

// ---------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------
/// 开始引导（进入新手教程模式 / 按 F2 重开）。会重置进度。
void begin(Game& g);
/// 每帧推进（在 Game::update 末尾调用）
void update(Game& g, float dt);
/// 跳过引导（记录 skipped 并落盘）
void skip(Game& g);
/// 直接结束（走完最后一步）
void finish(Game& g);

// ---------------------------------------------------------------------
// 事件钩子（由其它系统调用；引导未激活时内部直接 return）
// ---------------------------------------------------------------------
void onKey(Game& g, sf::Keyboard::Key k);
void onBuildingSelected(Game& g, cfg::BuildingType t);
void onBuildingPlaced(Game& g, cfg::BuildingType t);
void onRightClickBuilding(Game& g, cfg::BuildingType t);
void onEnemyKilled(Game& g, int n = 1);
/// 摄像机本帧位移（像素），用于 MoveCamera 判定
void onCameraMoved(Game& g, float distance);

/// 引导层事件（横幅上的「跳过引导」按钮）。返回 true 表示已消费。
bool handleEvent(Game& g, const sf::Event& e);

// ---------------------------------------------------------------------
// 绘制与高亮
// ---------------------------------------------------------------------
/// 当前步骤是否需要高亮提示
bool hasHighlight(Game& g);
/// 高亮目标矩形（屏幕坐标；宽高为 0 表示无）
sf::FloatRect highlightRect(Game& g);
/// 绘制引导层（目标横幅 + 叙事对话框 + 高亮脉冲 + 完成打勾）
void drawOverlay(Game& g, sf::RenderTarget& rt);

// ---------------------------------------------------------------------
// 持久化
// ---------------------------------------------------------------------
/// 写入 saves/tutorial.json（学习进度档案，供后续参考）
void saveProgressFile(const State& s);
/// 读取 saves/tutorial.json（存在则覆盖 out，返回是否成功）
bool loadProgressFile(State& out);
/// 命令行日志打印一行进度摘要
void logProgress(const State& s);

} // namespace tutorial
