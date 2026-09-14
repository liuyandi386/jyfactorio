#pragma once
// =====================================================================
// Narrative.h —— 叙事层（织女星通讯 / 开发日志）
//
// 世界观（一句话）：
//   你是「织星工业」的外派工程师，被派往一颗颗星球建厂、清障、交付指标。
//   每颗星球是一个独立关卡；交付完成后公司回收产线，你前往下一颗。
//   没有阴谋，没有反转——公司就是一家正常（而且有点啰嗦）的公司。
//
// 三个叙事载体，全部轻量、非模态、可随时无视：
//   1. 织女星（Vega）：随船 AI，播报任务、提示操作、偶尔吐槽公司。
//   2. 开发日志：波次之间的简短进度报告。
//   3. 星球简报：每颗星球着陆时的一段开场白（内容随关卡数据一起配置）。
//
// 本模块只回答"什么时候说什么话"，不持有任何游戏状态：
//   - 台词池是编译期常量表，同一事件的多条台词轮换播出，避免复读。
//   - 输出统一交给 GameUI::showComms（右下通讯条），不打断操作。
//   - 新手教程自带完整旁白（TutorialSystem），不使用本模块，避免两套声音打架。
// =====================================================================

#include <cstdint>

class Game;

namespace narrative {

/// 公司名（主菜单 / 关于 / 日志抬头统一引用，便于一次性改名）
constexpr const char* kCompany  = "织星工业";
/// 随船 AI 名
constexpr const char* kAiName   = "织女星";
/// 玩家的对外职称（AI 对玩家的称呼）
constexpr const char* kRoleName = "工程师";

/// 织女星的播报时机
enum class Event : uint8_t {
    Landing,      // 着陆：抵达新星球
    FirstBuild,   // 第一座建筑落成
    FirstCombat,  // 首次遇敌
    FirstKill,    // 首次击杀
    WaveCleared,  // 一波结束（清空）
    LowLives,     // 防护告急
    Delivered,    // 开发指标达成（关卡指标系统落地后调用）
    Roast,        // 吐槽公司
    Count
};

/// 播报一条织女星台词（同一事件的多条台词轮换，避免复读）
void announce(Game& g, Event ev);

/// 与 announce 相同，但整局只播一次（用于"第一次……"这类里程碑）
void announceOnce(Game& g, Event ev);

/// 波次之间的开发日志（简短进度报告）
void waveLog(Game& g, int wave, int eliminated, int lives);

/// 推进叙事计时（游戏内定期吐槽公司；由 Game::update 每帧调用）
void update(Game& g, float dt);

/// 新的一局开始：重置轮换指针、里程碑标记与计时
void reset();

} // namespace narrative
