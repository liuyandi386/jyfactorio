#pragma once
// =====================================================================
// Settings.h —— 跨场景的用户设置（启动菜单 ↔ 游戏内共用）
//
// 由启动菜单修改，持久化在 saves/settings.json；
// 启动时最先读取，然后据此创建窗口并影响若干游戏内表现
// （悬停提示 / 新手引导 / 摄像机速度）。
//
// ⚠ 窗口创建统一走 applyToWindow()：全屏一律用"无边框窗口"实现，
//   绝不要用 sf::Style::Fullscreen（独占全屏），原因见 applyToWindow 注释。
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

/// 用户设置（默认值与游戏内置默认一致）
struct Settings {
    DisplayMode displayMode = DisplayMode::Window1280x720; // 显示模式
    bool showTooltips = true;                              // 悬停建筑信息浮窗
    bool autoOpenHelp = true;                              // 新开局自动弹出说明书
    CameraSpeed cameraSpeed = CameraSpeed::Normal;         // 摄像机移动速度
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

// ---------------------------------------------------------------------
// 设置项（启动菜单「设置」界面 与 游戏内暂停面板「设置」共用同一份定义）
// 两处界面都只调用这里的函数，保证"设置按钮的功能和主界面保持一致"。
// ---------------------------------------------------------------------
/// 设置界面总行数：4 项设置 + 「恢复默认设置」+ 「返回」
constexpr int SETTING_ROW_COUNT = 6;

/// 行标题（0 显示模式 / 1 悬停提示 / 2 新手引导 / 3 摄像机速度 /
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

} // namespace gset
