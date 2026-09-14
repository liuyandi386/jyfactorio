#pragma once
// =====================================================================
// Settings.h —— 跨场景的用户设置（启动菜单 ↔ 游戏内共用）
//
// 由启动菜单修改，持久化在 saves/settings.json；
// 启动时最先读取，然后据此创建窗口并影响若干游戏内表现
// （悬停提示 / 摄像机速度 / 帧率·垂直同步）。
//
// ⚠ 窗口创建统一走 applyToWindow()：全屏一律用"无边框窗口"实现，
//   绝不要用 sf::Style::Fullscreen（独占全屏），原因见 applyToWindow 注释。
// ⚠ 帧率/垂直同步统一走 applyFrameMode()：绝不要在其他地方直接调用
//   setFramerateLimit / setVerticalSyncEnabled（两者互相排斥，原因见 applyFrameMode）。
// =====================================================================
#include <string>
#include <SFML/Graphics/RenderWindow.hpp>

namespace gset {

/// 显示模式（窗口分辨率 / 无边框全屏）
/// 说明：第 2 档数值必须保持 2（旧 settings.json 的 display_mode 兼容）
enum class DisplayMode : int {
    Window1280x720       = 0,  // 1280×720 窗口
    Window1600x900       = 1,  // 1600×900 窗口
    BorderlessFullscreen = 2,  // 无边框窗口铺满屏幕（不切换系统显示模式）
    Count
};

/// 摄像机移动速度档位
enum class CameraSpeed : int {
    Slow   = 0,
    Normal = 1,
    Fast   = 2,
    Count
};

/// 帧率 / 垂直同步模式（高刷新率显示器适配的核心开关）
///
/// 背景：SFML 里 setVerticalSyncEnabled 与 setFramerateLimit 是**互斥**的——
/// setFramerateLimit(n>0) 内部会关掉垂直同步，setVerticalSyncEnabled(true) 内部会把
/// 帧率上限清零。所以只能"二选一"，且必须按固定顺序设置（见 applyFrameMode）。
enum class FrameMode : int {
    Vsync     = 0,  // 垂直同步：帧率跟随显示器刷新率(60/120/144/165…)，无撕裂（推荐/默认）
    Limit60   = 1,  // 固定 60 帧上限（不启用垂直同步）
    Limit120  = 2,  // 固定 120 帧上限
    Limit144  = 3,  // 固定 144 帧上限
    Unlimited = 4,  // 不限制，跑满 CPU/GPU（可能撕裂）
    Count
};

/// 用户设置（默认值与游戏内置默认一致）
struct Settings {
    DisplayMode displayMode = DisplayMode::Window1280x720; // 显示模式
    bool showTooltips = true;                              // 悬停建筑信息浮窗
    CameraSpeed cameraSpeed = CameraSpeed::Normal;         // 摄像机移动速度
    FrameMode frameMode = FrameMode::Vsync;                // 帧率/垂直同步
};

/// 全局唯一设置实例
Settings& get();

/// 从 JSON 读取设置（文件缺失/损坏时保持默认值；返回是否成功载入）
bool load(const std::string& path = "saves/settings.json");

/// 写入 JSON 设置（父目录不存在会自动创建）
bool save(const std::string& path = "saves/settings.json");

/// 恢复为默认设置（不落盘，调用方决定是否 save）
void resetToDefault();

// ---- 便捷查询（窗口创建 / 游戏逻辑使用） ----
int  windowWidth();            // 当前显示模式对应的窗口宽（无边框全屏时=桌面宽）
int  windowHeight();           // 当前显示模式对应的窗口高（无边框全屏时=桌面高）
bool borderless();             // 是否"无边框全屏"（铺满屏幕，但不切换系统显示模式）
float cameraSpeedScale();      // 慢0.6 / 标准1.0 / 快1.6
/// 显示模式的中文名（设置界面显示用）
const char* displayModeName(DisplayMode m);
/// 帧率/垂直同步模式的中文名（设置界面显示用）
const char* frameModeName(FrameMode m);

// ---------------------------------------------------------------------
// 设置项（启动菜单「设置」界面 与 游戏内暂停面板「设置」共用同一份定义）
// 两处界面都只调用这里的函数，保证"设置按钮的功能和主界面保持一致"。
// ---------------------------------------------------------------------
/// 设置界面总行数：4 项设置 + 「恢复默认设置」+ 「返回」
constexpr int SETTING_ROW_COUNT = 6;

/// 「帧率 / 垂直同步」所在行号。该行改动**不需要重建窗口**，调用方就地调用
/// applyFrameMode(window) 即可生效（启动菜单与暂停面板两处都按此常量判断，勿写死数字）。
constexpr int SETTING_ROW_FRAME_MODE = 3;

/// 行标题（0 显示模式 / 1 悬停提示 / 2 摄像机速度 / 3 帧率·垂直同步 /
///           4 恢复默认设置 / 5 返回）
const char* settingRowLabel(int row);

/// 行右侧的当前取值文本（0~3 为取值；4/5 为动作行，返回空串）
std::string settingRowValue(int row);

/// 是否为动作行（4 恢复默认设置 / 5 返回：无右侧取值）
bool settingRowIsAction(int row);

/// ←→ 切换设置项取值（含落盘）。返回 true 表示显示模式已变、调用方需重建窗口
bool cycleSettingRow(int row, int dir);

/// Enter 激活设置行后的结果
enum class SettingActivate {
    None,            // 无操作
    Changed,         // 取值已改，无需重建窗口
    DisplayChanged,  // 显示模式已改，调用方需重建窗口
    Back             // 请求关闭设置界面（「返回」行）
};
SettingActivate activateSettingRow(int row);

/// 按当前显示模式创建/重建窗口（启动菜单窗口与游戏窗口共用，策略只有这一处）。
///
/// - 无边框全屏：Style::None + 桌面尺寸 + 贴到 (0,0)，看起来就是全屏，但**不请求
///   切换系统显示模式**，因此不会闪屏/黑屏，鼠标坐标也不会因模式重设而错位漂移。
/// - 窗口化：指定分辨率 + Style::Default（可缩放/最大化）。
///
/// ⚠ 不要改成 `sf::Style::Fullscreen`：那是独占全屏，会切系统显示模式，
///   在部分驱动/多显示器/DPI 缩放下表现为反复黑屏闪烁 + 鼠标异常滑动。
/// - titleUtf8 传 UTF-8 编码的窗口标题
void applyToWindow(sf::RenderWindow& window, const std::string& titleUtf8);

/// 按当前「帧率/垂直同步」设置应用呈现策略（窗口创建后、以及设置项改动后调用）。
///
/// ⚠ 这是全项目**唯一**允许调用 setVerticalSyncEnabled / setFramerateLimit 的地方：
///   两个接口在 SFML 内部互相排斥（开限帧会关垂直同步，开垂直同步会清限帧），
///   分散调用会导致"设置了却没生效"，也是高刷新率显示器上画面异常/撕裂的常见来源。
///
/// - Vsync     → 垂直同步：144Hz 屏自动跑 144 帧，且不会撕裂；
///               垂直同步由驱动/合成器按显示器节奏阻塞，不存在"睡眠式限帧"带来的
///               交换链饥饿（表现为黑屏/卡成幻灯片）。
/// - LimitNNN  → 关垂直同步 + 固定帧率上限（老机器/录像用，会撕裂）。
/// - Unlimited → 两者都关，跑满。
void applyFrameMode(sf::RenderWindow& window);

} // namespace gset
