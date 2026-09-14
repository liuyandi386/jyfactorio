// =====================================================================
// Settings.cpp —— 用户设置读写实现（JSON，复用 nlohmann/json）
// =====================================================================
#include "Settings.h"

#include "GameConfig.h"   // cfg::FPS（「固定 60 帧」档的上限）
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <SFML/Window/VideoMode.hpp>

#ifdef _WIN32
// 仅为了把"无边框全屏"窗口置顶（Style::None 的窗口不是 topmost，会被任务栏/其它窗口盖住）
// 放在 SFML 头之后包含，避免 windows.h 的宏污染 SFML
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using nlohmann::json;

namespace gset {
namespace {

Settings g_settings;   // 全局唯一实例

/// 把枚举值钳制到合法范围（配置文件被手改坏时兜底）
int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

Settings& get() { return g_settings; }

// ---------------------------------------------------------------------
// 读取
// ---------------------------------------------------------------------
bool load(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) return false;   // 首次运行无配置文件 → 使用默认值
    try {
        json j;
        f >> j;
        if (j.contains("display_mode"))
            g_settings.displayMode = static_cast<DisplayMode>(
                clampInt(j["display_mode"].get<int>(), 0,
                         static_cast<int>(DisplayMode::Count) - 1));
        if (j.contains("show_tooltips"))
            g_settings.showTooltips = j["show_tooltips"].get<bool>();
        // 旧版 settings.json 的 auto_open_help 已废弃：新手教程改为主菜单独立入口，
        // 普通关卡不再接入引导系统，因此该键读取后直接忽略
        if (j.contains("camera_speed"))
            g_settings.cameraSpeed = static_cast<CameraSpeed>(
                clampInt(j["camera_speed"].get<int>(), 0,
                         static_cast<int>(CameraSpeed::Count) - 1));
        // 旧版本 settings.json 没有 frame_mode：保持默认（垂直同步），不会退回 60 帧限帧
        if (j.contains("frame_mode"))
            g_settings.frameMode = static_cast<FrameMode>(
                clampInt(j["frame_mode"].get<int>(), 0,
                         static_cast<int>(FrameMode::Count) - 1));
        return true;
    } catch (const std::exception&) {
        // 文件损坏：保留默认值，不打断启动
        return false;
    }
}

// ---------------------------------------------------------------------
// 写入
// ---------------------------------------------------------------------
bool save(const std::string& path) {
    try {
        std::error_code ec;
        const std::filesystem::path p(path);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path(), ec);

        json j;
        j["v"] = 1;
        j["display_mode"] = static_cast<int>(g_settings.displayMode);
        j["show_tooltips"] = g_settings.showTooltips;
        j["camera_speed"] = static_cast<int>(g_settings.cameraSpeed);
        j["frame_mode"] = static_cast<int>(g_settings.frameMode);

        std::ofstream o(path, std::ios::trunc);
        if (!o.good()) return false;
        o << j.dump(2);
        return o.good();
    } catch (const std::exception&) {
        return false;
    }
}

void resetToDefault() { g_settings = Settings{}; }

// ---------------------------------------------------------------------
// 便捷查询
// ---------------------------------------------------------------------
int windowWidth() {
    switch (g_settings.displayMode) {
        case DisplayMode::Window1280x720: return 1280;
        case DisplayMode::Window1600x900: return 1600;
        case DisplayMode::BorderlessFullscreen:
            return static_cast<int>(sf::VideoMode::getDesktopMode().width);
        default:                          return 1280;
    }
}

int windowHeight() {
    switch (g_settings.displayMode) {
        case DisplayMode::Window1280x720: return 720;
        case DisplayMode::Window1600x900: return 900;
        case DisplayMode::BorderlessFullscreen:
            return static_cast<int>(sf::VideoMode::getDesktopMode().height);
        default:                          return 720;
    }
}

bool borderless() { return g_settings.displayMode == DisplayMode::BorderlessFullscreen; }

float cameraSpeedScale() {
    switch (g_settings.cameraSpeed) {
        case CameraSpeed::Slow:   return 0.6f;
        case CameraSpeed::Normal: return 1.0f;
        case CameraSpeed::Fast:   return 1.6f;
        default:                  return 1.0f;
    }
}

const char* displayModeName(DisplayMode m) {
    switch (m) {
        case DisplayMode::Window1280x720:       return "窗口 1280 x 720";
        case DisplayMode::Window1600x900:       return "窗口 1600 x 900";
        case DisplayMode::BorderlessFullscreen: return "全屏 (无边框·桌面分辨率)";
        default:                                return "窗口 1280 x 720";
    }
}

const char* frameModeName(FrameMode m) {
    switch (m) {
        case FrameMode::Vsync:     return "垂直同步 (跟随显示器刷新率)";
        case FrameMode::Limit60:   return "固定 60 帧";
        case FrameMode::Limit120:  return "固定 120 帧";
        case FrameMode::Limit144:  return "固定 144 帧";
        case FrameMode::Unlimited: return "不限制";
        default:                   return "垂直同步 (跟随显示器刷新率)";
    }
}

// ---------------------------------------------------------------------
// 帧率 / 垂直同步（全项目唯一应用点）
// ---------------------------------------------------------------------
void applyFrameMode(sf::RenderWindow& window) {
    // ⚠ SFML 的这两个 setter 互相拆台，必须"先全关、再只开一个"：
    //   setFramerateLimit(>0)     → 内部调用 setVerticalSyncEnabled(false)
    //   setVerticalSyncEnabled(true) → 内部调用 setFramerateLimit(0)
    // 顺序反了就会出现"设了垂直同步却仍是 60 帧"之类的诡异现象。
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(0);

    switch (g_settings.frameMode) {
        case FrameMode::Vsync:
            // 垂直同步：present 由显示器刷新节奏驱动，144Hz 屏即 144 帧。
            // 由驱动阻塞而非 sf::sleep 空转，不会出现交换链饥饿导致的黑屏。
            window.setVerticalSyncEnabled(true);
            break;
        case FrameMode::Limit60:   window.setFramerateLimit(cfg::FPS); break;
        case FrameMode::Limit120:  window.setFramerateLimit(120); break;
        case FrameMode::Limit144:  window.setFramerateLimit(144); break;
        case FrameMode::Unlimited: break;   // 两者都不设 = 跑满
        default:                   window.setVerticalSyncEnabled(true); break;
    }
}

// ---------------------------------------------------------------------
// 设置项（启动菜单 与 游戏内暂停面板 共用）
// ---------------------------------------------------------------------
const char* settingRowLabel(int row) {
    static const char* const kLabels[SETTING_ROW_COUNT] = {
        "显示模式", "悬停提示", "摄像机速度", "帧率 / 垂直同步",
        "恢复默认设置", "返回",
    };
    return (row >= 0 && row < SETTING_ROW_COUNT) ? kLabels[row] : "";
}

std::string settingRowValue(int row) {
    static const char* const kSpeedNames[3] = {"慢", "标准", "快"};
    switch (row) {
        case 0: return displayModeName(g_settings.displayMode);
        case 1: return g_settings.showTooltips ? "开启" : "关闭";
        case 2: return kSpeedNames[clampInt(static_cast<int>(g_settings.cameraSpeed), 0, 2)];
        case 3: return frameModeName(g_settings.frameMode);
        default: return "";   // 4/5 为动作行，无取值文本
    }
}

bool settingRowIsAction(int row) { return row == 4 || row == 5; }

bool cycleSettingRow(int row, int dir) {
    const int dc = static_cast<int>(DisplayMode::Count);
    const int cc = static_cast<int>(CameraSpeed::Count);
    const int fc = static_cast<int>(FrameMode::Count);
    switch (row) {
        case 0: {   // 显示模式（切换后需重建窗口才能看到效果）
            int m = static_cast<int>(g_settings.displayMode) + dir;
            m = (m % dc + dc) % dc;
            g_settings.displayMode = static_cast<DisplayMode>(m);
            save();
            return true;
        }
        case 1: g_settings.showTooltips = !g_settings.showTooltips; save(); break;
        case 2: {
            int c = static_cast<int>(g_settings.cameraSpeed) + dir;
            c = (c % cc + cc) % cc;
            g_settings.cameraSpeed = static_cast<CameraSpeed>(c);
            save();
            break;
        }
        case 3: {   // 帧率/垂直同步：不需要重建窗口，调用方用 applyFrameMode 即时生效
            int f = static_cast<int>(g_settings.frameMode) + dir;
            f = (f % fc + fc) % fc;
            g_settings.frameMode = static_cast<FrameMode>(f);
            save();
            break;
        }
        default: break;
    }
    return false;
}

SettingActivate activateSettingRow(int row) {
    if (row >= 0 && row <= 3)
        return cycleSettingRow(row, 1) ? SettingActivate::DisplayChanged
                                       : SettingActivate::Changed;
    if (row == 4) {           // 恢复默认设置
        resetToDefault();
        save();
        return SettingActivate::DisplayChanged;
    }
    if (row == 5) return SettingActivate::Back;
    return SettingActivate::None;
}

// ---------------------------------------------------------------------
// 窗口创建（两个窗口创建点共用唯一策略）
// ---------------------------------------------------------------------
void applyToWindow(sf::RenderWindow& window, const std::string& titleUtf8) {
    // 无边框全屏 = 无边框窗口 + 桌面尺寸 + 贴到桌面左上角：
    // 视觉效果与全屏一致，但**不调用 SetDisplayMode**，绕开独占全屏的
    // 模式切换（反复黑屏闪烁的根源）与随之而来的鼠标坐标重映射（鼠标漂移的根源）。
    const sf::Uint32 style = borderless() ? sf::Style::None : sf::Style::Default;

    window.create(sf::VideoMode(static_cast<unsigned>(windowWidth()),
                                static_cast<unsigned>(windowHeight())),
                  sf::String::fromUtf8(titleUtf8.begin(), titleUtf8.end()), style);

    if (borderless()) {
        // Style::None 的窗口不会被窗口管理器自动居中，必须自己定位才能真铺满
        window.setPosition(sf::Vector2i(0, 0));
        window.requestFocus();   // 铺满后立即接管键盘/鼠标，避免"点了没反应"
#ifdef _WIN32
        // 无边框窗口默认不是 topmost：任务栏（以及任何置顶窗口）会盖在它上面，
        // 看起来就是"铺不满 / 露出一条任务栏"。置顶后才是真正的全屏观感。
        // 切回窗口化时窗口会被 create() 重建，不会残留置顶状态。
        ::SetWindowPos(static_cast<HWND>(window.getSystemHandle()), HWND_TOPMOST, 0, 0,
                       windowWidth(), windowHeight(), SWP_SHOWWINDOW);
#endif
    }
}

} // namespace gset
