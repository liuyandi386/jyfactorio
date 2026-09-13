#pragma once
// =====================================================================
// EntrySystem.h —— 游戏入口系统（启动体验）
//
// 负责玩家在真正进入游戏之前看到的全部内容：
//   1) 启动动画（工作室 logo 淡入淡出，可跳过）
//   2) 标题界面 / 主菜单（开始新游戏·继续游戏·设置·关于·退出）
//   3) 设置界面（显示模式 / 交互选项，即时生效并持久化）
//   4) 加载界面（真实执行配置读取·存档校验·路径预生成，带进度条）
// 结束后把玩家的选择（EntryAction）交回 main.cpp，由其创建 Game。
//
// 说明：入口系统自带独立窗口，进入游戏前销毁，因此 Game 可以按
// 设置里的显示模式重新创建窗口，互不干扰。
// =====================================================================
#include <array>
#include <random>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

/// 入口流程的最终结果
enum class EntryAction {
    NewGame,   // 开始新游戏（从头开始）
    Continue,  // 继续游戏（读取存档）
    Quit       // 退出游戏
};

class EntrySystem {
public:
    EntrySystem();
    ~EntrySystem() = default;

    /// 运行完整入口流程，直到玩家选择了"开始/继续/退出"
    EntryAction run();

private:
    // ---------------- 内部状态机 ----------------
    enum class State { Splash, Title, Settings, Loading, Finished };
    enum class Dialog { None, ConfirmNewGame, ConfirmQuit, About };

    /// 主菜单项
    struct MenuItem {
        std::string label;
        std::string hint;
        bool enabled = true;
    };

    // ---------------- 窗口 / 资源 ----------------
    void createWindow();          // 按设置创建窗口
    void applyDisplayMode();      // 显示模式变更后重建窗口
    bool loadFont();              // 加载中文字体（与 AssetManager 同候选表）

    // ---------------- 背景动画 ----------------
    void initBackdrop();          // 按窗口尺寸重建背景粒子/剪影
    void drawBackdrop();
    void drawGear(float cx, float cy, float radius, int teeth, float rotation,
                  sf::Color color, float thickness);

    // ---------------- 事件 ----------------
    void handleEvents();
    void onKeyPressed(const sf::Event::KeyEvent& key);
    void onMouseClick(float x, float y);

    // ---------------- 更新 / 渲染 ----------------
    void update(float dt);
    void render();
    void drawSplash();
    void drawTitle();
    void drawSettings();
    void drawLoading();
    void drawDialog();
    void drawFadeOverlay();

    // ---------------- 绘制辅助 ----------------
    void drawText(const std::string& s, float x, float y, unsigned size, sf::Color color,
                  int align = 0 /*0左 1中 2右*/, bool bold = false, float letterSpacing = 1.0f);
    void drawButton(const sf::FloatRect& r, const std::string& text, const std::string& hint,
                    bool active, bool enabled);
    void drawSettingRow(const sf::FloatRect& r, const std::string& label,
                        const std::string& value, bool active);
    sf::Vector2f mousePos() const;

    // ---------------- 子流程 ----------------
    void openSettings();
    void closeSettings();
    void cycleSettingRow(int row, int dir);
    void activateSettingRow(int row);
    void activateMenu(int index);            // 触发主菜单项
    void requestStart(EntryAction action);   // 开始新游戏/继续游戏（含确认框）
    void beginLoading(EntryAction action);   // 进入加载界面（执行真实加载工作）
    void advanceLoading(float dt);

    // ---- 对话框 ----
    int dialogOptionCount() const;
    std::string dialogOptionLabel(int i) const;
    sf::FloatRect dialogOptionRect(int i) const;
    void activateDialogOption(int i);

    // ================= 成员 =================
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded = false;

    State state = State::Splash;
    Dialog dialog = Dialog::None;
    float stateTime = 0.0f;    // 当前状态停留时长
    float globalTime = 0.0f;   // 总运行时长（动画用）
    float fadeOut = 0.0f;      // 离场黑幕（0~1）

    // ---- 鼠标位置跟踪 ----
    // 只允许"真正移动了的鼠标"接管键盘焦点：指针停住时不再每帧把焦点弹回它所在的项，
    // 否则 ↑↓/←→ 会被鼠标悬停反复覆盖，看起来像"按键没反应"。
    sf::Vector2f lastMousePos = {0.f, 0.f};
    bool mousePosValid = false;

    // ---- 主菜单 ----
    std::vector<MenuItem> menu;
    int menuFocus = 0;         // 键盘焦点
    int menuHover = -1;        // 鼠标悬停
    bool hasSaveFile = false;
    std::string saveSummary;   // 存档摘要（大小）
    bool startNewGame = true;  // 加载结束后要执行的动作

    // ---- 设置 ----
    int settingRow = 0;
    int settingHover = -1;
    bool pendingDisplayApply = false;   // 显示模式变更延后到事件循环之外生效

    // ---- 加载 ----
    std::vector<std::string> loadSteps;
    std::vector<bool> loadDone;
    int loadIndex = 0;
    float loadStepTimer = 0.0f;
    float loadProgress = 0.0f;      // 0~1
    bool loadReady = false;         // 全部步骤完成
    float loadReadyTimer = 0.0f;
    std::vector<sf::Vector2i> previewPath;   // 真实计算出的敌人路径（迷你地图预览）
    std::vector<sf::Vector2i> previewOres;   // 真实计算出的矿点分布（迷你地图预览）

    // ---- 背景动画 ----
    struct Silo { float x, w, h; int chimneys; };
    std::vector<Silo> skyline;
    std::vector<sf::Vector2f> embers;
    std::vector<float> emberSpeed;
    std::vector<float> emberRadius;
    float skylineWidth = 0.0f;
    std::mt19937 rng{20260913u};

    EntryAction result = EntryAction::Quit;
};
