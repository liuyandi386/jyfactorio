#pragma once
// =====================================================================
// GameUI.h —— 游戏界面（工业扁平化浅色主题）
//
// 移植 ui/GameUI.py：
//   顶部资源栏（铁/铜/煤/弹药+金币+波次+生命+倒计时）
//   右侧建筑面板（15种建筑按钮+保存/加载）
//   方向选择悬浮窗（塔8方向圆形、机器4方向十字）
//   格雷科技式面配置编辑器（右键电线/分流器/机器打开）
//   机器悬停提示 / 弹窗提示(Toast) / 电网信息面板
// =====================================================================
#include <array>
#include <string>
#include <vector>
#include <entt/entity/entity.hpp>   // entt::entity / entt::null
#include <SFML/Graphics.hpp>
#include "GameConfig.h"
#include "Settings.h"               // SETTING_ROW_COUNT（设置子页与主菜单共用）
#include "SaveSystem.h"             // SAVE_SLOT_COUNT / SaveSlotInfo（存档槽位面板）

class Game;

/// 游戏UI
class GameUI {
public:
    void init(Game* g);

    /// 处理UI事件（返回true表示已消费，世界不再处理）
    bool handleEvent(const sf::Event& e);
    /// 更新（Toast计时等）
    void update(float dt);
    /// 绘制全部UI
    void draw(sf::RenderTarget& rt);

    // ---- 场景接口 ----
    void selectBuilding(cfg::BuildingType t);
    void showToast(const std::string& msg);
    /// 织女星通讯 / 开发日志（叙事层）：独立通道，不会被系统提示挤掉
    void showComms(const std::string& speaker, const std::string& text);
    /// 是否有通讯正在显示（调试 / 自检用）
    bool commsActive() const { return commsTimer_ > 0.0f; }
    void showDirectionPopup(cfg::BuildingType b, sf::Vector2i tile, sf::Vector2f screen);
    bool directionPopupActive() const { return popupActive_; }
    void hideDirectionPopup();
    void setTooltip(const std::string& title, const std::vector<std::string>& lines);
    void clearTooltip() { tooltipActive_ = false; }

    /// 某建筑按钮的屏幕矩形（新手引导高亮目标用；未布局时宽高为 0）
    sf::FloatRect machineButtonRect(cfg::BuildingType t) const {
        const size_t i = static_cast<size_t>(t);
        return i < machineRects_.size() ? machineRects_[i] : sf::FloatRect{};
    }

    /// 组装机配方选择菜单（右键组装机打开；选定后只按该配方合成，可重复选择）
    void showRecipePopup(entt::entity e, sf::Vector2f screen);
    void hideRecipePopup();
    bool recipePopupActive() const { return recipePopupActive_; }

    /// 商店（金币+电路板兑换矿石/合金/机器，价目表在 config.json）
    void toggleShop();
    bool shopOpen() const { return shopOpen_; }
    /// 随身工作台（泰拉瑞亚/MC式手工合成，B键外的V键）
    void toggleWorkbench();
    bool workbenchOpen() const { return workbenchOpen_; }
    /// 关闭商店/工作台/世界地图/通物面板（ESC）
    void closePanels() {
        shopOpen_ = false;
        workbenchOpen_ = false;
        worldMapOpen_ = false;
        mePanelActive_ = false;
        mePanelEntity_ = entt::null;
        filterPanelActive_ = false;
        filterPanelEntity_ = entt::null;
        minerPanelEntity_ = entt::null;
        helpOpen_ = false;
    }

    /// 是否有任何面板正打开（ESC 用它决定"先关面板"还是"开暂停面板"）
    bool anyPanelOpen() const {
        return shopOpen_ || workbenchOpen_ || worldMapOpen_ || mePanelActive_ ||
               filterPanelActive_ || minerPanelActive() || helpOpen_ || recipePopupActive_;
    }

    /// 游戏说明书 / 新手引导（H 或 F1 打开；新档首次进入自动弹出）
    void toggleHelp();
    void openHelp(int tab);
    void hideHelp() { helpOpen_ = false; }
    bool helpOpen() const { return helpOpen_; }

    /// 暂停面板（ESC 打开）：继续游戏 / 保存存档 / 载入存档 / 设置 / 返回主界面
    void openPausePanel();
    void closePausePanel();
    bool pausePanelOpen() const { return pausePanelOpen_; }

    /// 存档槽位面板（暂停面板子页）：Save = 保存到槽位，Load = 从槽位读取
    enum class SlotPanelMode { Save, Load };
    /// 打开槽位面板（暂停菜单的「保存存档 / 载入存档」与 F5 / F9 都走这里）
    void openSlotPanel(SlotPanelMode mode);
    bool slotPanelOpen() const { return pausePanelOpen_ && pauseSlotOpen_; }
    /// 取出"需按新显示模式重建窗口"的待处理标记（由 Game 主循环在事件循环外消费）
    bool consumeDisplayApply() {
        const bool v = pauseDisplayApply_;
        pauseDisplayApply_ = false;
        return v;
    }

    /// 世界地图（Xaero's World Map 式全屏地图，M键切换）
    void toggleWorldMap();
    bool worldMapOpen() const { return worldMapOpen_; }

    /// 通物网络物品清单（右键任意通物设备打开，通物终端式）
    void showMePanel(entt::entity e);
    void hideMePanel() { mePanelActive_ = false; mePanelEntity_ = entt::null; }

    /// 通物接口过滤面板（右键接口打开：点选锁定输出物品）
    void showFilterPanel(entt::entity e);
    void hideFilterPanel() { filterPanelActive_ = false; filterPanelEntity_ = entt::null; }
    bool filterPanelActive() const { return filterPanelActive_; }

    /// 采矿场设置面板（右键矿机打开）：切换采集模式 / 选择矿种 / 旋转输出面
    void showMinerPanel(entt::entity e);
    void hideMinerPanel() { minerPanelEntity_ = entt::null; }
    bool minerPanelActive() const { return minerPanelEntity_ != entt::null; }

    /// 重新计算布局（窗口缩放/最大化后调用，按钮与面板跟随窗口尺寸）
    void updateLayout();

private:
    // ---- 事件 ----
    void handlePopupClick(sf::Vector2f pos);
    void handleRecipePopupClick(sf::Vector2f pos);
    void handleShopClick(sf::Vector2f pos);
    void handleWorkbenchClick(sf::Vector2f pos);
    bool handleBackpackClick(sf::Vector2f pos);
    void handleFaceEditorClick(sf::Vector2f pos);
    void handleFilterPanelClick(sf::Vector2f pos);
    void handleMinerPanelClick(sf::Vector2f pos);
    void handleHelpClick(sf::Vector2f pos);

    // ---- 暂停面板 ----
    void handlePauseKey(const sf::Event::KeyEvent& k);
    void handlePauseClick(sf::Vector2f pos);
    void activatePauseItem(int i);      // 主菜单项：继续/保存/载入/设置/返回主界面
    /// 暂停面板主菜单按钮矩形（5 项）/ 设置子页行矩形（绘制与点击共用）
    std::array<sf::FloatRect, 5> pauseMenuRects() const;
    std::array<sf::FloatRect, gset::SETTING_ROW_COUNT> pauseSettingRects() const;

    // ---- 暂停面板 · 存档槽位子页（WorldBox 式 10 槽位） ----
    void refreshSlotInfos();                    // 重新扫描各槽位状态（打开/写盘后调用）
    void handleSlotKey(const sf::Event::KeyEvent& k);
    void handleSlotClick(sf::Vector2f pos);
    void activateFocusedSlot();                 // 主操作：保存到该槽位 / 从该槽位读取
    void deleteFocusedSlot();                   // 「删除该槽位」
    void runSlotConfirm();                      // 二次确认对话框点「确定」后真正执行
    /// 槽位格子矩形（2 列 × 5 行）/ 底部按钮矩形（主操作·删除·返回）
    std::array<sf::FloatRect, SAVE_SLOT_COUNT> slotTileRects() const;
    std::array<sf::FloatRect, 3> slotButtonRects() const;
    /// 二次确认对话框：面板矩形 / 两个按钮矩形
    sf::FloatRect slotConfirmPanelRect() const;
    std::array<sf::FloatRect, 2> slotConfirmRects() const;
    /// 按当前确认类型填充文案（标题 / 正文 / 确定按钮文字）
    void slotConfirmText(std::string& title, std::vector<std::string>& lines,
                         std::string& okLabel) const;

    // ---- 绘制 ----
    void drawResourceBar(sf::RenderTarget& rt);
    void drawSidePanel(sf::RenderTarget& rt);
    void drawToast(sf::RenderTarget& rt);
    void drawComms(sf::RenderTarget& rt);
    void drawDirectionPopup(sf::RenderTarget& rt);
    void drawRecipePopup(sf::RenderTarget& rt);
    void drawShop(sf::RenderTarget& rt);
    void drawWorkbench(sf::RenderTarget& rt);
    void drawTooltip(sf::RenderTarget& rt);
    void drawFaceEditor(sf::RenderTarget& rt);
    void drawMePanel(sf::RenderTarget& rt);
    void drawFilterPanel(sf::RenderTarget& rt);
    void drawMinerPanel(sf::RenderTarget& rt);
    void layoutMinerPanel();
    void drawHelp(sf::RenderTarget& rt);
    void layoutHelp();
    void drawPausePanel(sf::RenderTarget& rt);   // 暂停面板（最顶层模态）
    void drawPauseMenu(sf::RenderTarget& rt);
    void drawPauseSettings(sf::RenderTarget& rt);
    void drawPauseSlots(sf::RenderTarget& rt);   // 存档槽位子页（10 槽位）
    void drawSlotConfirm(sf::RenderTarget& rt);  // 覆盖/删除/读取/返回的二次确认
    void drawOverlays(sf::RenderTarget& rt);  // 暂停/游戏结束
    void drawText(sf::RenderTarget& rt, const std::string& s, unsigned size,
                  sf::Vector2f pos, sf::Color color, bool centered = false,
                  const sf::Font* font = nullptr) const;

    // ---- 面板布局 ----
    void layoutShop();
    void layoutWorkbench();

    // ---- 小地图 / 世界地图 ----
    void rebuildMinimap();
    void rebuildWorldMap();
    void drawMinimap(sf::RenderTarget& rt);
    void drawWorldMap(sf::RenderTarget& rt);

    // ---- 面编辑器按钮矩形（与绘制共用，Python _draw_face_editor一致） ----
    std::array<sf::FloatRect, 4> faceEditorRects() const;

    Game* g_ = nullptr;

    // 当前窗口尺寸（运行时更新，窗口缩放/最大化后随之变化）
    float winW_ = cfg::SCREEN_WIDTH;
    float winH_ = cfg::SCREEN_HEIGHT;

    // 背包（机器格 + 物品格） + 左上角商店按钮
    sf::FloatRect shopBtn_{};
    std::array<sf::FloatRect, cfg::BUILDING_COUNT> machineRects_{};
    std::array<sf::FloatRect, cfg::ITEM_COUNT> itemRects_{};

    // Toast
    std::string toast_;
    float toastTimer_ = 0.0f;

    // 织女星通讯条（叙事层：台词 / 开发日志）
    std::string commsSpeaker_;   // 说话人（织女星 / 开发日志）
    std::string commsText_;
    float commsTimer_ = 0.0f;

    // 方向悬浮窗
    bool popupActive_ = false;
    cfg::BuildingType popupBuilding_ = cfg::BuildingType::TowerBasic;
    sf::Vector2i popupTile_{0, 0};
    sf::Vector2f popupScreen_{0.0f, 0.0f};
    std::array<sf::FloatRect, 8> popupRects_{};
    int popupCount_ = 0;

    // 配方选择菜单（组装机 / 合金炉共用）
    bool recipePopupActive_ = false;
    entt::entity recipePopupEntity_ = entt::null;
    bool recipePopupAlloy_ = false;   // 当前菜单是否为合金炉（否则为组装机）
    std::array<sf::FloatRect, 8> recipeRects_{};
    int recipeCount_ = 0;

    // 悬停提示
    bool tooltipActive_ = false;
    std::string tooltipTitle_;
    std::vector<std::string> tooltipLines_;
    bool backpackTooltip_ = false;   // 当前提示是否来自背包装饰（区分世界悬停）

    // 商店 / 随身工作台
    bool shopOpen_ = false;
    bool workbenchOpen_ = false;
    std::vector<sf::FloatRect> shopRects_;      // 每件商品一行
    std::vector<sf::FloatRect> craftRects_;     // 每个配方一行
    sf::FloatRect shopPanel_{}, craftPanel_{};

    // 小地图（Xaero式，左下角） / 世界地图（XWM式，M键全屏）
    bool worldMapOpen_ = false;
    sf::RenderTexture minimap_;
    sf::RenderTexture worldmap_;
    float minimapTimer_ = 0.0f;     // 小地图重建节流
    sf::View uiView_;               // UI绘制视图（窗口尺寸变化时更新）

    // 通物网络物品清单面板
    bool mePanelActive_ = false;
    entt::entity mePanelEntity_ = entt::null;
    sf::FloatRect mePanelRect_{};

    // 通物接口过滤面板（点选锁定输出物品）
    bool filterPanelActive_ = false;
    entt::entity filterPanelEntity_ = entt::null;
    sf::FloatRect filterPanelRect_{};
    std::array<sf::FloatRect, cfg::ITEM_COUNT> filterRects_{};

    // 采矿场设置面板（右键矿机：模式切换 / 矿种选择 / 旋转输出面）
    //   非 entt::null 即打开；布局在 layoutMinerPanel() 中按面板矩形计算
    entt::entity minerPanelEntity_ = entt::null;
    sf::FloatRect minerPanelRect_{};
    std::array<sf::FloatRect, 2> minerModeRects_{};                 // 0=原有模式 1=固定矿点模式
    std::array<sf::FloatRect, 8> minerOreRects_{};                  // 8 种矿石筛选按钮
    sf::FloatRect minerRotateRect_{};                               // 旋转输出面
    sf::FloatRect minerCloseRect_{};                                // 关闭

    // 游戏说明书 / 新手引导（H / F1，新档自动弹出）
    sf::FloatRect helpBtn_{};               // 顶部「帮助」按钮
    bool helpOpen_ = false;
    int helpTab_ = 0;                       // 当前标签页
    float helpScroll_ = 0.0f;               // 内容滚动偏移（滚轮）
    sf::FloatRect helpPanel_{};             // 内容区矩形
    std::array<sf::FloatRect, 6> helpTabRects_{};  // 标签按钮

    // 暂停面板（ESC 打开）：继续游戏 / 保存存档 / 载入存档 / 设置 / 返回主界面
    bool pausePanelOpen_ = false;
    bool pauseSettingsOpen_ = false;        // 「设置」子页（与主菜单设置共用逻辑）
    bool pauseDisplayApply_ = false;        // 显示模式已改 → 待 Game 重建窗口
    int pauseFocus_ = 0;                    // 主菜单焦点行
    int pauseSettingRow_ = 0;               // 设置子页焦点行
    std::array<sf::FloatRect, 5> pauseRects_{};                  // 主菜单 5 项
    std::array<sf::FloatRect, gset::SETTING_ROW_COUNT> pauseSettingRects_{}; // 设置子页各行矩形

    // ---- 存档槽位子页 ----
    bool pauseSlotOpen_ = false;            // 槽位子页是否打开
    SlotPanelMode slotMode_ = SlotPanelMode::Save;
    int slotFocus_ = 0;                     // 当前焦点槽位（0..9）
    std::array<SaveSlotInfo, SAVE_SLOT_COUNT> slotInfos_{};   // 槽位状态快照
    /// 需要二次确认的危险操作（WorldBox 式：覆盖 / 删除 / 读档 / 返回主界面）
    enum class SlotConfirm { None, Overwrite, Delete, Load, ReturnMenu };
    SlotConfirm slotConfirm_ = SlotConfirm::None;
    int slotConfirmFocus_ = 0;              // 0 = 确定，1 = 取消
};
