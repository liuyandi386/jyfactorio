#pragma once
// =====================================================================
// EntrySystem.h —— 游戏入口系统（启动体验）
//
// 负责玩家在真正进入游戏之前看到的全部内容：
//   1) 启动动画（工作室 logo 淡入淡出，可跳过）
//   2) 标题界面 / 主菜单（新手教程·普通关卡·载入存档·设置·关于·退出）
//   3) 存档槽位界面（WorldBox 式 10 个独立槽位：读取 / 删除，均带确认）
//   4) 设置界面（显示模式 / 交互选项，即时生效并持久化）
//   5) 加载界面（真实执行配置读取·存档校验·路径预生成，带进度条）
// 结束后把玩家的选择（EntryAction + 槽位号）交回 main.cpp，由其创建 Game。
//
// 说明：入口系统自带独立窗口，进入游戏前销毁，因此 Game 可以按
// 设置里的显示模式重新创建窗口，互不干扰。
// =====================================================================
#include <array>
#include <random>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

#include "SaveSystem.h"   // SAVE_SLOT_COUNT / SaveSlotInfo / querySlot（存档槽位界面）

/// 入口流程的最终结果
/// 注意：Tutorial 与 NewGame 是两种完全独立的模式入口，互不为前置/子模式
enum class EntryAction {
    Tutorial,  // 新手教程：独立教学关卡（独立的引导流程）
    NewGame,   // 普通关卡 · 开始新游戏（从头开始）
    Continue,  // 普通关卡 · 继续游戏（读取存档）
    Quit       // 退出游戏
};

class EntrySystem {
public:
    EntrySystem();
    ~EntrySystem() = default;

    /// 运行完整入口流程，直到玩家选择了"开始/继续/退出"
    EntryAction run();

    /// 玩家在「载入存档」界面选中的槽位（0..9；未选择为 -1）
    /// 仅在 run() 返回 EntryAction::Continue 时有意义
    int chosenSlot() const { return chosenSlotIndex; }

private:
    // ---------------- 内部状态机 ----------------
    enum class State { Splash, Title, Settings, Slots, Loading, Finished };
    enum class Dialog { None, ConfirmQuit, ConfirmLoad, ConfirmDelete, About };

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
    void drawSlots();
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

    // ---------------- 存档槽位（载入存档） ----------------
    void openSlots();                        // 打开 10 槽位界面
    void closeSlots();                       // 返回主菜单
    void refreshSlots();                     // 重新扫描槽位状态与底部摘要
    void handleSlotsKey(const sf::Event::KeyEvent& key);
    void handleSlotsClick(float x, float y);
    void activateFocusedSlot();              // 读取该槽位（先确认）
    void deleteFocusedSlot();                // 删除该槽位（先确认）
    /// 槽位格子矩形（2 列 × 5 行）/ 底部按钮矩形（读取 · 删除 · 返回）
    std::array<sf::FloatRect, SAVE_SLOT_COUNT> slotTileRects() const;
    std::array<sf::FloatRect, 3> slotButtonRects() const;

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
    EntryAction pendingAction = EntryAction::NewGame;  // 加载结束后要执行的动作（教程/新游戏/继续）

    // ---- 设置 ----
    int settingRow = 0;
    int settingHover = -1;
    bool pendingDisplayApply = false;   // 显示模式变更延后到事件循环之外生效

    // ---- 存档槽位（载入存档） ----
    int slotFocus = 0;                  // 键盘焦点槽位
    int slotHover = -1;                 // 鼠标悬停槽位
    std::array<SaveSlotInfo, SAVE_SLOT_COUNT> slotInfos{};   // 槽位状态快照
    int chosenSlotIndex = -1;           // 决定载入的槽位（Continue 时交给 main.cpp）
    std::string slotNotice;             // 界面内提示（如"该槽位是空的"）
    float slotNoticeTimer = 0.0f;

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
