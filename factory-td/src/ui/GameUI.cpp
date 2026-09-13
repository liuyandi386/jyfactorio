// =====================================================================
// GameUI.cpp —— 游戏界面实现
// 移植 ui/GameUI.py（工业扁平化浅色主题）+ 面配置编辑器。
// =====================================================================
#include "ui/GameUI.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include "Game.h"
#include "Settings.h"
#include "systems/ItemSystem.h"
#include "systems/MeSystem.h"
#include "components/Machine.h"
#include "components/Me.h"
#include "components/Power.h"
#include "components/Turret.h"
#include "components/Storage.h"

namespace {
// UI主题色（重写：暗色工业风，"像真正的游戏"）
const sf::Color UI_PANEL(38, 40, 46);         // 深灰面板
const sf::Color UI_PANEL_LIGHT(52, 55, 62);   // 稍亮面板/按钮
const sf::Color UI_BORDER(80, 84, 92);
const sf::Color UI_BORDER_LIGHT(105, 110, 120);
const sf::Color UI_ACCENT(230, 150, 60);      // 橙色主强调（工业风）
const sf::Color UI_ACCENT_GREEN(120, 190, 80);
const sf::Color UI_ACCENT_ORANGE(240, 140, 60);
const sf::Color UI_ACCENT_RED(210, 70, 70);
const sf::Color UI_TEXT(225, 225, 230);       // 浅色文字（暗底）
const sf::Color UI_TEXT_LIGHT(255, 255, 255);
const sf::Color UI_DIVIDER(70, 73, 80);       // 面板内分隔线

/// 建筑类别色（侧边栏按钮色块 / 视觉分组用）
sf::Color buildingCategoryColor(cfg::BuildingType t) {
    switch (t) {
        case cfg::BuildingType::TowerBasic:     return sf::Color(205, 82, 70);   // 红
        case cfg::BuildingType::TowerRapid:     return sf::Color(0, 200, 220);   // 青
        case cfg::BuildingType::TowerSniper:    return sf::Color(172, 92, 220);  // 紫
        case cfg::BuildingType::TowerElectric:  return sf::Color(72, 142, 255);  // 蓝
        case cfg::BuildingType::Miner:
        case cfg::BuildingType::MinerL2:
        case cfg::BuildingType::MinerL3:
        case cfg::BuildingType::MinerVoid:      return sf::Color(0, 180, 160);   // 青绿
        case cfg::BuildingType::Furnace:        return sf::Color(240, 150, 52);  // 橙
        case cfg::BuildingType::AlloyFurnace:   return sf::Color(180, 122, 255); // 紫
        case cfg::BuildingType::Assembler:      return sf::Color(122, 200, 92);  // 绿
        case cfg::BuildingType::Generator:      return sf::Color(232, 172, 60);  // 黄橙
        case cfg::BuildingType::PowerGenerator: return sf::Color(255, 122, 42);  // 橙
        case cfg::BuildingType::PowerPole:      return sf::Color(152, 162, 182); // 灰蓝
        case cfg::BuildingType::Capacitor:      return sf::Color(0, 162, 255);   // 蓝
        case cfg::BuildingType::PowerWire:      return sf::Color(220, 200, 60);  // 黄
        case cfg::BuildingType::Pipe:           return sf::Color(122, 132, 142); // 灰
        case cfg::BuildingType::Bucket:         return sf::Color(182, 132, 82);  // 棕
        case cfg::BuildingType::Splitter:       return sf::Color(0, 200, 220);   // 青
        case cfg::BuildingType::MeInterface:
        case cfg::BuildingType::MeDrive:
        case cfg::BuildingType::MeTerminal:     return sf::Color(0, 220, 255);   // 亮青(AE2)
        default: return UI_BORDER;
    }
}
} // namespace

// ---------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------
void GameUI::init(Game* g) {
    g_ = g;
    // 小地图（Xaero式，160px方块）/ 世界地图（1格=1像素）
    minimap_.create(160, 160);
    worldmap_.create(cfg::GRID_WIDTH, cfg::GRID_HEIGHT);
    worldmap_.setSmooth(false);
    updateLayout();
}

void GameUI::updateLayout() {
    // 读取窗口实际尺寸（窗口缩放/最大化后按钮与面板跟随布局）
    winW_ = static_cast<float>(g_->window.getSize().x);
    winH_ = static_cast<float>(g_->window.getSize().y);

    // 左上角商店按钮 / 帮助按钮（说明书·新手引导）
    shopBtn_ = {16.0f, 12.0f, 90.0f, 30.0f};
    helpBtn_ = {116.0f, 12.0f, 90.0f, 30.0f};

    // 背包布局（右侧面板：机器格 + 物品格，4 列小格）
    const float px = winW_ - cfg::ui::SIDE_PANEL_WIDTH;
    const float cell = 40.0f, gap = 4.0f;
    const float x0 = px + 8.0f;
    // 机器格（可点击放置）
    const float my = 60.0f;
    for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
        const int row = i / 4, col = i % 4;
        machineRects_[static_cast<size_t>(i)] =
            {x0 + col * (cell + gap), my + row * (cell + gap), cell, cell};
    }
    // 物品格（机器区下方，仅展示数量）
    const int mRows = (cfg::BUILDING_COUNT + 3) / 4;
    const float iy = my + mRows * (cell + gap) + 26.0f;
    for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
        const int row = i / 4, col = i % 4;
        itemRects_[static_cast<size_t>(i)] =
            {x0 + col * (cell + gap), iy + row * (cell + gap), cell, cell};
    }

    // 商店/工作台面板跟随新窗口尺寸
    if (shopOpen_) layoutShop();
    if (workbenchOpen_) layoutWorkbench();
}

// ---------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------
bool GameUI::handleEvent(const sf::Event& e) {
    // 暂停面板是最顶层模态：打开期间键盘/鼠标全部由它接管，不再穿透到世界与其它面板
    if (pausePanelOpen_) {
        if (e.type == sf::Event::KeyPressed) {
            handlePauseKey(e.key);
            return true;
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            handlePauseClick({static_cast<float>(e.mouseButton.x),
                              static_cast<float>(e.mouseButton.y)});
            return true;
        }
        // 鼠标真正移动时才让悬停行接管焦点（与启动菜单一致的"悬停即选中"，停住不会夺焦）
        if (e.type == sf::Event::MouseMoved) {
            const sf::Vector2f mp(static_cast<float>(e.mouseMove.x),
                                  static_cast<float>(e.mouseMove.y));
            if (pauseSettingsOpen_) {
                const auto rects = pauseSettingRects();
                for (int i = 0; i < gset::SETTING_ROW_COUNT; ++i)
                    if (rects[static_cast<size_t>(i)].contains(mp)) {
                        pauseSettingRow_ = i;
                        break;
                    }
            } else {
                const auto rects = pauseMenuRects();
                for (int i = 0; i < 4; ++i)
                    if (rects[static_cast<size_t>(i)].contains(mp)) {
                        pauseFocus_ = i;
                        break;
                    }
            }
            return true;
        }
        return true;   // 其余事件（滚轮等）一并吞掉
    }

    // 说明书/新手引导打开时：滚轮滚动内容、左键切标签或关闭
    // （键盘事件放行给 PlayerSystem，H/ESC 才能关闭面板）
    if (helpOpen_) {
        if (e.type == sf::Event::MouseWheelScrolled) {
            helpScroll_ -= e.mouseWheelScroll.delta * 30.0f;
            if (helpScroll_ < 0.0f) helpScroll_ = 0.0f;
            return true;
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            handleHelpClick({static_cast<float>(e.mouseButton.x),
                             static_cast<float>(e.mouseButton.y)});
            return true;
        }
        return false;
    }
    if (e.type != sf::Event::MouseButtonPressed || e.mouseButton.button != sf::Mouse::Left)
        return false;
    const sf::Vector2f pos(static_cast<float>(e.mouseButton.x),
                           static_cast<float>(e.mouseButton.y));

    // 世界地图激活时：所有点击由地图层处理（仅关闭）
    if (worldMapOpen_) { worldMapOpen_ = false; return true; }
    // ME网络面板激活时：面板内点击无操作，外部点击关闭
    if (mePanelActive_) {
        if (!mePanelRect_.contains(pos)) hideMePanel();
        return true;
    }
    // ME接口过滤面板激活时：所有点击由过滤面板处理
    if (filterPanelActive_) {
        handleFilterPanelClick(pos);
        return true;
    }
    // 商店/随身工作台面板激活时：所有点击由面板处理（模态）
    if (shopOpen_) { handleShopClick(pos); return true; }
    if (workbenchOpen_) { handleWorkbenchClick(pos); return true; }
    // 组装机配方菜单激活时：所有点击由菜单处理
    if (recipePopupActive_) {
        handleRecipePopupClick(pos);
        return true;
    }
    // 方向悬浮窗激活时：所有点击由悬浮窗处理（Python行为）
    if (popupActive_) {
        handlePopupClick(pos);
        return true;
    }
    // 面配置编辑器激活时：所有点击由编辑器处理（Python行为）
    if (g_->faceEditTarget != entt::null) {
        handleFaceEditorClick(pos);
        return true;
    }
    // 背包/商店按钮（点中则消费事件）
    return handleBackpackClick(pos);
}

// ---------------------------------------------------------------------
// 组装机配方选择菜单
// ---------------------------------------------------------------------
void GameUI::showRecipePopup(entt::entity e, sf::Vector2f screen) {
    recipePopupActive_ = true;
    recipePopupEntity_ = e;
    // 根据建筑类型选配方表（合金炉 → ALLOY_RECIPES，否则组装机 → ASSEMBLER_RECIPES）
    recipePopupAlloy_ = g_->reg.valid(e) && g_->reg.all_of<Building>(e) &&
        g_->reg.get<Building>(e).type == cfg::BuildingType::AlloyFurnace;
    recipeCount_ = static_cast<int>(recipePopupAlloy_ ? cfg::ALLOY_RECIPES.size()
                                                      : cfg::ASSEMBLER_RECIPES.size());
    // 按钮纵向排列在点击位置右下方
    const float bw = 140.0f, bh = 30.0f, gap = 6.0f;
    float y = screen.y + 10.0f;
    for (int i = 0; i < recipeCount_; ++i) {
        recipeRects_[static_cast<size_t>(i)] = {screen.x + 15.0f, y, bw, bh};
        y += bh + gap;
    }
}

void GameUI::hideRecipePopup() {
    recipePopupActive_ = false;
    recipePopupEntity_ = entt::null;
    recipeCount_ = 0;
}

void GameUI::handleRecipePopupClick(sf::Vector2f pos) {
    for (int i = 0; i < recipeCount_; ++i) {
        if (recipeRects_[static_cast<size_t>(i)].contains(pos)) {
            // 选定配方：该组装机此后只按此配方合成（可重复选择）
            if (g_->reg.valid(recipePopupEntity_) &&
                g_->reg.all_of<Machine>(recipePopupEntity_))
                g_->reg.get<Machine>(recipePopupEntity_).recipeId = i;
            hideRecipePopup();
            return;
        }
    }
    // 点击菜单外部 → 关闭
    hideRecipePopup();
}

void GameUI::handlePopupClick(sf::Vector2f pos) {
    for (int i = 0; i < popupCount_; ++i) {
        if (popupRects_[static_cast<size_t>(i)].contains(pos)) {
            const int dir = i;   // 4方向: 0上1右2下3左; 8方向: 对应dir8
            const cfg::BuildingType b = popupBuilding_;
            const sf::Vector2i tile = popupTile_;
            hideDirectionPopup();
            g_->placeBuildingWithDirection(b, tile, dir);  // 回调场景放置
            return;
        }
    }
}

bool GameUI::handleBackpackClick(sf::Vector2f pos) {
    // 左上角商店按钮
    if (shopBtn_.contains(pos)) { toggleShop(); return true; }
    // 左上角帮助按钮（游戏说明书 / 新手引导）
    if (helpBtn_.contains(pos)) { toggleHelp(); return true; }
    // 背包机器格：点击选择该机器进入放置模式
    for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
        if (machineRects_[static_cast<size_t>(i)].contains(pos)) {
            g_->selectBuilding(static_cast<cfg::BuildingType>(i));
            return true;
        }
    }
    return false;
}

void GameUI::handleFaceEditorClick(sf::Vector2f pos) {
    const entt::entity target = g_->faceEditTarget;
    if (target == entt::null || !g_->reg.valid(target)) {
        g_->faceEditTarget = entt::null;
        return;
    }
    const auto rects = faceEditorRects();
    for (int d = 0; d < 4; ++d) {
        if (rects[static_cast<size_t>(d)].contains(pos)) {
            // 循环切换该面模式（NONE→INPUT→…），渲染层按面配置实时绘制
            auto& fc = g_->reg.get<FaceConfig>(target);
            fc.cycle(d);
            g_->rebuildPowerNetworks();  // 面配置影响路由 → 重建网络
            return;
        }
    }
    // 点击编辑器外部 → 关闭（Python: 中心矩形膨胀3倍外即关闭）
    const auto& b = g_->reg.get<Building>(target);
    const auto center = g_->buildingCenter(b);
    const auto sc = g_->worldToScreen(center);
    const float size = cfg::TILE_SIZE * g_->camera.zoom;
    sf::FloatRect zone(sc.x - size * 1.5f, sc.y - size * 1.5f, size * 3.0f, size * 3.0f);
    if (!zone.contains(pos)) g_->faceEditTarget = entt::null;
}

std::array<sf::FloatRect, 4> GameUI::faceEditorRects() const {
    std::array<sf::FloatRect, 4> rects{};
    const entt::entity target = g_->faceEditTarget;
    if (target == entt::null || !g_->reg.valid(target)) return rects;
    const auto& b = g_->reg.get<Building>(target);
    const auto center = g_->buildingCenter(b);
    const auto sc = g_->worldToScreen(center);
    const float size = cfg::TILE_SIZE * g_->camera.zoom;
    const float bs = std::max(24.0f, 28.0f * g_->camera.zoom);
    const float cx = sc.x, cy = sc.y;
    // Python _draw_face_editor: 四个方向按钮围绕中心
    rects[cfg::Dir::UP]    = {cx - bs / 2.0f, cy - size - bs / 2.0f, bs, bs};
    rects[cfg::Dir::RIGHT] = {cx + size - bs / 2.0f, cy - bs / 2.0f, bs, bs};
    rects[cfg::Dir::DOWN]  = {cx - bs / 2.0f, cy + size - bs / 2.0f, bs, bs};
    rects[cfg::Dir::LEFT]  = {cx - size - bs / 2.0f, cy - bs / 2.0f, bs, bs};
    return rects;
}

// ---------------------------------------------------------------------
// 场景接口
// ---------------------------------------------------------------------
void GameUI::selectBuilding(cfg::BuildingType t) {
    // 高亮状态由 buttons_ 中的选中类型在绘制时判断（简化自Python active标志）
    g_->hasSelection = true;
    g_->selected = t;
}

void GameUI::showToast(const std::string& msg) {
    toast_ = msg;
    toastTimer_ = cfg::ui::TOAST_DURATION;
}

void GameUI::showDirectionPopup(cfg::BuildingType b, sf::Vector2i tile, sf::Vector2f screen) {
    popupActive_ = true;
    popupBuilding_ = b;
    popupTile_ = tile;
    popupScreen_ = screen;

    const float btnSize = 50.0f, spacing = 8.0f;
    const float cx = screen.x, cy = screen.y;
    const bool is8 = (b == cfg::BuildingType::TowerBasic ||
                      b == cfg::BuildingType::TowerRapid ||
                      b == cfg::BuildingType::TowerSniper ||
                      b == cfg::BuildingType::TowerElectric);
    popupCount_ = is8 ? 8 : 4;
    if (is8) {
        // 8方向圆形布局（Python: 每45°一个按钮）
        const float radius = btnSize + spacing;
        for (int i = 0; i < 8; ++i) {
            const float rad = (i * 45.0f - 90.0f) * 3.14159265f / 180.0f;
            popupRects_[static_cast<size_t>(i)] = {
                cx - btnSize / 2.0f + radius * std::cos(rad),
                cy - btnSize / 2.0f + radius * std::sin(rad),
                btnSize, btnSize};
        }
    } else {
        // 4方向十字布局
        popupRects_[cfg::Dir::UP]    = {cx - btnSize / 2.0f, cy - btnSize - spacing - btnSize, btnSize, btnSize};
        popupRects_[cfg::Dir::RIGHT] = {cx + spacing + btnSize, cy - btnSize / 2.0f, btnSize, btnSize};
        popupRects_[cfg::Dir::DOWN]  = {cx - btnSize / 2.0f, cy + spacing + btnSize, btnSize, btnSize};
        popupRects_[cfg::Dir::LEFT]  = {cx - btnSize - spacing - btnSize, cy - btnSize / 2.0f, btnSize, btnSize};
    }
}

void GameUI::hideDirectionPopup() {
    popupActive_ = false;
    popupCount_ = 0;
}

void GameUI::setTooltip(const std::string& title, const std::vector<std::string>& lines) {
    tooltipActive_ = true;
    tooltipTitle_ = title;
    tooltipLines_ = lines;
}

void GameUI::update(float dt) {
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;
    // 小地图节流重建（0.3秒一次；世界地图打开时同样节流）
    minimapTimer_ -= dt;
    if (minimapTimer_ <= 0.0f) {
        minimapTimer_ = 0.3f;
        rebuildMinimap();
        if (worldMapOpen_) rebuildWorldMap();
    }

    // 背包装饰悬停（显示全名）
    const sf::Vector2i m = sf::Mouse::getPosition(g_->window);
    const sf::Vector2f mp(static_cast<float>(m.x), static_cast<float>(m.y));
    bool hovering = false;
    for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
        if (machineRects_[static_cast<size_t>(i)].contains(mp)) {
            setTooltip(cfg::BUILDING_INFOS[i].nameZh,
                       {"数量: " + std::to_string(g_->backpackMachines[static_cast<size_t>(i)]),
                        "点击放置"});
            backpackTooltip_ = true;
            hovering = true;
            break;
        }
    }
    if (!hovering) {
        for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
            if (itemRects_[static_cast<size_t>(i)].contains(mp)) {
                const auto t = static_cast<cfg::ItemType>(i);
                setTooltip(ItemSystem::nameZh(t),
                           {"数量: " + std::to_string(g_->playerInv[t])});
                backpackTooltip_ = true;
                hovering = true;
                break;
            }
        }
    }
    if (!hovering && backpackTooltip_) {
        clearTooltip();
        backpackTooltip_ = false;
    }
}

// ---------------------------------------------------------------------
// 文本绘制辅助
// ---------------------------------------------------------------------
void GameUI::drawText(sf::RenderTarget& rt, const std::string& s, unsigned size,
                      sf::Vector2f pos, sf::Color color, bool centered,
                      const sf::Font* font) const {
    sf::Text text;
    text.setFont(font ? *font : g_->assets.font());
    text.setCharacterSize(size);
    text.setString(sf::String::fromUtf8(s.begin(), s.end()));
    text.setFillColor(color);
    text.setPosition(pos);
    if (centered) {
        const auto bounds = text.getLocalBounds();
        text.setPosition(pos.x - bounds.width / 2.0f, pos.y);
    }
    rt.draw(text);
}

// ---------------------------------------------------------------------
// 主绘制
// ---------------------------------------------------------------------
void GameUI::draw(sf::RenderTarget& rt) {
    drawResourceBar(rt);
    drawSidePanel(rt);
    if (toastTimer_ > 0.0f) drawToast(rt);
    if (popupActive_) drawDirectionPopup(rt);
    if (recipePopupActive_) drawRecipePopup(rt);
    if (shopOpen_) drawShop(rt);
    if (workbenchOpen_) drawWorkbench(rt);
    if (tooltipActive_) drawTooltip(rt);
    if (g_->faceEditTarget != entt::null) drawFaceEditor(rt);
    drawOverlays(rt);
    if (worldMapOpen_) drawWorldMap(rt);
    else drawMinimap(rt);
    if (mePanelActive_) drawMePanel(rt);
    if (filterPanelActive_) drawFilterPanel(rt);
    if (helpOpen_) drawHelp(rt);   // 说明书置顶
    if (pausePanelOpen_) drawPausePanel(rt);   // 暂停面板最顶层
}

// ---------------------------------------------------------------------
// 组装机配方选择菜单绘制
// ---------------------------------------------------------------------
void GameUI::drawRecipePopup(sf::RenderTarget& rt) {
    if (!recipePopupActive_) return;
    const int current = (g_->reg.valid(recipePopupEntity_) &&
                         g_->reg.all_of<Machine>(recipePopupEntity_))
                            ? g_->reg.get<Machine>(recipePopupEntity_).recipeId
                            : -1;
    // 面板背景
    const float bw = 140.0f, bh = 30.0f, gap = 6.0f;
    const float pw = bw + 20.0f;
    const float ph = recipeCount_ * (bh + gap) + 34.0f;
    const float px = recipeRects_[0].left - 10.0f;
    const float py = recipeRects_[0].top - 30.0f;
    sf::RectangleShape bg({pw, ph});
    bg.setPosition(px, py);
    bg.setFillColor(UI_PANEL);
    bg.setOutlineColor(UI_ACCENT);
    bg.setOutlineThickness(2.0f);
    rt.draw(bg);
    drawText(rt, "选择配方", 13, {px + pw / 2.0f, py + 6.0f}, UI_TEXT, true);

    for (int i = 0; i < recipeCount_; ++i) {
        const auto& rect = recipeRects_[static_cast<size_t>(i)];
        const bool active = (i == current);
        sf::RectangleShape btn({rect.width, rect.height});
        btn.setPosition(rect.left, rect.top);
        btn.setFillColor(active ? UI_ACCENT : UI_PANEL_LIGHT);
        btn.setOutlineColor(UI_BORDER_LIGHT);
        btn.setOutlineThickness(1.0f);
        rt.draw(btn);
        const std::string rname = recipePopupAlloy_
            ? cfg::ALLOY_RECIPES[static_cast<size_t>(i)].nameZh
            : cfg::ASSEMBLER_RECIPES[static_cast<size_t>(i)].nameZh;
        drawText(rt, rname, 12,
                 {rect.left + rect.width / 2.0f, rect.top + 7.0f},
                 active ? UI_TEXT_LIGHT : UI_TEXT, true);
    }
    drawText(rt, "点击配方即可切换", 10, {px + pw / 2.0f, py + ph - 16.0f}, UI_TEXT, true);
}

// ---------------------------------------------------------------------
// 商店（金币 + 电路板 兑换 矿石/合金/机器）
// ---------------------------------------------------------------------
void GameUI::toggleShop() {
    shopOpen_ = !shopOpen_;
    workbenchOpen_ = false;
    if (shopOpen_) layoutShop();
}

void GameUI::layoutShop() {
    // 全屏商店页：商品行居中排列
    const float bw = std::min(winW_ - 160.0f, 680.0f);
    const float bh = 38.0f, gap = 6.0f;
    const float px = (winW_ - bw) / 2.0f;
    shopRects_.clear();
    float y = 110.0f;
    for (size_t i = 0; i < cfg::SHOP_OFFERS.size(); ++i) {
        shopRects_.push_back({px, y, bw, bh});
        y += bh + gap;
    }
    shopPanel_ = {0.0f, 0.0f, winW_, winH_};
}

void GameUI::handleShopClick(sf::Vector2f pos) {
    // 点击面板外部 → 关闭
    if (!shopPanel_.contains(pos)) { shopOpen_ = false; return; }
    for (size_t i = 0; i < shopRects_.size(); ++i) {
        if (i >= cfg::SHOP_OFFERS.size() || !shopRects_[i].contains(pos)) continue;
        const auto& offer = cfg::SHOP_OFFERS[i];
        // 检查金币与材料
        if (g_->gold < offer.gold) {
            showToast("金币不足!");
            return;
        }
        for (auto [t, n] : offer.costItems)
            if (g_->playerInv[t] < n) {
                showToast(std::string(ItemSystem::nameZh(t)) + "不足!");
                return;
            }
        // 扣款并发放
        g_->gold -= offer.gold;
        for (auto [t, n] : offer.costItems) g_->playerInv[t] -= n;
        if (offer.giveBuilding >= 0) {
            // 兑换机器：机器成品进背包（数量+1，可在背包里点选放置）
            const auto bt = static_cast<cfg::BuildingType>(offer.giveBuilding);
            g_->backpackMachines[static_cast<size_t>(bt)]++;
            showToast("已购买 " + offer.nameZh + "（已放入背包）");
        } else {
            for (auto [t, n] : offer.giveItems) g_->playerInv[t] += n;
            showToast("购买成功: " + offer.nameZh);
        }
        return;
    }
}

void GameUI::drawShop(sf::RenderTarget& rt) {
    if (!shopOpen_) return;
    // 全屏背景（暗色遮罩）
    sf::RectangleShape bg({winW_, winH_});
    bg.setFillColor(sf::Color(16, 18, 22, 245));
    rt.draw(bg);
    // 标题
    drawText(rt, "商店", 30, {winW_ / 2.0f, 36.0f}, UI_TEXT_LIGHT, true);
    drawText(rt, "金币: " + std::to_string(g_->gold), 16,
             {winW_ / 2.0f, 74.0f}, sf::Color(255, 235, 150), true);

    for (size_t i = 0; i < shopRects_.size(); ++i) {
        if (i >= cfg::SHOP_OFFERS.size()) break;
        const auto& offer = cfg::SHOP_OFFERS[i];
        const auto& rect = shopRects_[i];
        sf::RectangleShape row({rect.width, rect.height});
        row.setPosition(rect.left, rect.top);
        row.setFillColor(UI_PANEL_LIGHT);
        row.setOutlineColor(UI_BORDER_LIGHT);
        row.setOutlineThickness(1.0f);
        rt.draw(row);
        // 商品名（左）
        drawText(rt, offer.nameZh, 14, {rect.left + 14.0f, rect.top + 11.0f}, UI_TEXT);
        // 价格（右）：金币 + 材料
        std::string price = std::to_string(offer.gold) + "金币";
        for (auto [t, n] : offer.costItems)
            price += " + " + std::string(ItemSystem::nameZh(t)) + "×" + std::to_string(n);
        drawText(rt, price, 13,
                 {rect.left + rect.width - 300.0f, rect.top + 12.0f}, UI_ACCENT_RED);
        // 可获得（中右）
        std::string give;
        if (offer.giveBuilding >= 0)
            give = "机器×1";
        else
            for (auto [t, n] : offer.giveItems)
                give += std::string(ItemSystem::nameZh(t)) + "×" + std::to_string(n) + " ";
        drawText(rt, give, 13, {rect.left + rect.width - 140.0f, rect.top + 12.0f},
                 UI_ACCENT_GREEN);
    }
    drawText(rt, "点击行购买 · 关闭: 点击外部/B键", 11,
             {winW_ / 2.0f, winH_ - 40.0f}, UI_TEXT, true);
}

// ---------------------------------------------------------------------
// 随身工作台（泰拉瑞亚/MC式手工合成，前期物品配方迁移至此）
// ---------------------------------------------------------------------
void GameUI::toggleWorkbench() {
    workbenchOpen_ = !workbenchOpen_;
    shopOpen_ = false;
    if (workbenchOpen_) layoutWorkbench();
}

void GameUI::layoutWorkbench() {
    const float bw = 560.0f, bh = 36.0f, gap = 4.0f;
    const float px = (winW_ - bw) / 2.0f;
    const float py = 80.0f;
    craftRects_.clear();
    float y = py + 46.0f;
    for (size_t i = 0; i < cfg::CRAFTING_RECIPES.size(); ++i) {
        craftRects_.push_back({px + 10.0f, y, bw - 20.0f, bh});
        y += bh + gap;
    }
    craftPanel_ = {px, py, bw, y - py + 14.0f};
}

void GameUI::handleWorkbenchClick(sf::Vector2f pos) {
    if (!craftPanel_.contains(pos)) { workbenchOpen_ = false; return; }
    for (size_t i = 0; i < craftRects_.size(); ++i) {
        if (i >= cfg::CRAFTING_RECIPES.size() || !craftRects_[i].contains(pos)) continue;
        const auto& r = cfg::CRAFTING_RECIPES[i];
        // 检查材料（playerInv，测试版无限资源下恒够）
        for (auto [t, n] : r.inputs)
            if (g_->playerInv[t] < n) {
                showToast(std::string(ItemSystem::nameZh(t)) + "不足!");
                return;
            }
        for (auto [t, n] : r.inputs) g_->playerInv[t] -= n;
        for (auto [t, n] : r.outputs) g_->playerInv[t] += n;
        showToast("合成: " + r.nameZh);
        return;
    }
}

void GameUI::drawWorkbench(sf::RenderTarget& rt) {
    if (!workbenchOpen_) return;
    const auto& panel = craftPanel_;
    sf::RectangleShape bg({panel.width, panel.height});
    bg.setPosition(panel.left, panel.top);
    bg.setFillColor(UI_PANEL);
    bg.setOutlineColor(UI_ACCENT_ORANGE);
    bg.setOutlineThickness(2.0f);
    rt.draw(bg);
    // 头部条
    sf::RectangleShape head({panel.width, 34.0f});
    head.setPosition(panel.left, panel.top + 2.0f);
    head.setFillColor(UI_ACCENT_ORANGE);
    rt.draw(head);
    drawText(rt, "随身工作台", 16, {panel.left + 14.0f, panel.top + 10.0f}, UI_TEXT_LIGHT);
    drawText(rt, "点击配方合成", 12,
             {panel.left + panel.width - 100.0f, panel.top + 13.0f}, sf::Color(255, 235, 210));

    for (size_t i = 0; i < craftRects_.size(); ++i) {
        if (i >= cfg::CRAFTING_RECIPES.size()) break;
        const auto& r = cfg::CRAFTING_RECIPES[i];
        const auto& rect = craftRects_[i];
        sf::RectangleShape row({rect.width, rect.height});
        row.setPosition(rect.left, rect.top);
        row.setFillColor(UI_PANEL_LIGHT);
        row.setOutlineColor(UI_BORDER_LIGHT);
        row.setOutlineThickness(1.0f);
        rt.draw(row);
        // 原料 → 产物
        std::string in;
        for (size_t k = 0; k < r.inputs.size(); ++k) {
            if (k) in += " + ";
            in += std::string(ItemSystem::nameZh(r.inputs[k].first)) +
                  "×" + std::to_string(r.inputs[k].second);
        }
        std::string out;
        for (size_t k = 0; k < r.outputs.size(); ++k) {
            if (k) out += " + ";
            out += std::string(ItemSystem::nameZh(r.outputs[k].first)) +
                   "×" + std::to_string(r.outputs[k].second);
        }
        drawText(rt, in + "  →  " + out, 13, {rect.left + 10.0f, rect.top + 10.0f}, UI_TEXT);
        // 合成次数（右）
        int minTimes = INT32_MAX;
        for (auto [t, n] : r.inputs)
            minTimes = std::min(minTimes, n > 0 ? g_->playerInv[t] / n : INT32_MAX);
        drawText(rt, "可合成×" + std::to_string(minTimes), 11,
                 {rect.left + rect.width - 110.0f, rect.top + 12.0f}, UI_ACCENT_GREEN);
    }
    drawText(rt, "关闭: 点击外部/V键 · 这些配方也可由组装机自动量产", 10,
             {panel.left + panel.width / 2.0f, panel.top + panel.height - 14.0f}, UI_TEXT, true);
}

// ---------------------------------------------------------------------
// 小地图（Xaero's Minimap 式：左下角方形地图，跟随摄像机）
// ---------------------------------------------------------------------
void GameUI::rebuildMinimap() {
    const int MS = 160;                       // 像素
    const int halfTiles = 50;                 // 覆盖半径（格）
    const float pxPerTile = static_cast<float>(MS) / (halfTiles * 2);   // 1.6
    minimap_.clear(sf::Color(24, 26, 30));
    minimap_.setSmooth(false);

    // 摄像机中心格
    const sf::Vector2f camCenter(g_->camera.x, g_->camera.y);
    const int cx = static_cast<int>(std::floor(camCenter.x / cfg::TILE_SIZE));
    const int cy = static_cast<int>(std::floor(camCenter.y / cfg::TILE_SIZE));

    // 网格内可见矿点（小范围内直接用 O(n) 扫描 + 落点判断）
    sf::VertexArray tiles(sf::Quads);
    for (int ty = cy - halfTiles; ty <= cy + halfTiles; ++ty) {
        for (int tx = cx - halfTiles; tx <= cx + halfTiles; ++tx) {
            if (!g_->grid.inBounds(tx, ty)) continue;
            const float px = (tx - cx + halfTiles) * pxPerTile;
            const float py = (ty - cy + halfTiles) * pxPerTile;
            sf::Color c = (g_->terrain[static_cast<size_t>(ty) * g_->grid.w + tx] == 1)
                              ? sf::Color(146, 120, 90)   // 路径
                              : sf::Color(56, 104, 50);   // 草地
            const entt::entity b = g_->grid.at(tx, ty).building;
            if (b != entt::null && g_->reg.valid(b))
                c = sf::Color(200, 200, 205);             // 建筑
            tiles.append(sf::Vertex({px, py}, c));
            tiles.append(sf::Vertex({px + pxPerTile, py}, c));
            tiles.append(sf::Vertex({px + pxPerTile, py + pxPerTile}, c));
            tiles.append(sf::Vertex({px, py + pxPerTile}, c));
        }
    }
    minimap_.draw(tiles);

    // 矿点（彩色小点，Xaero式实体标记）
    sf::VertexArray ores(sf::Quads);
    for (auto [e, pos, ore] : g_->reg.view<GridPos, OreDeposit>().each()) {
        if (std::abs(pos.x - cx) > halfTiles || std::abs(pos.y - cy) > halfTiles) continue;
        const float px = (pos.x - cx + halfTiles) * pxPerTile;
        const float py = (pos.y - cy + halfTiles) * pxPerTile;
        const sf::Color c = ItemSystem::color(ore.type);
        const float s = 2.6f;
        ores.append(sf::Vertex({px, py}, c));
        ores.append(sf::Vertex({px + s, py}, c));
        ores.append(sf::Vertex({px + s, py + s}, c));
        ores.append(sf::Vertex({px, py + s}, c));
    }
    minimap_.draw(ores);

    // 敌人（红点）
    sf::VertexArray foes(sf::Quads);
    for (auto [e, en] : g_->reg.view<Enemy>().each()) {
        const int tx = static_cast<int>(std::floor(en.pos.x / cfg::TILE_SIZE));
        const int ty = static_cast<int>(std::floor(en.pos.y / cfg::TILE_SIZE));
        if (std::abs(tx - cx) > halfTiles || std::abs(ty - cy) > halfTiles) continue;
        const float px = (tx - cx + halfTiles) * pxPerTile;
        const float py = (ty - cy + halfTiles) * pxPerTile;
        const float s = 2.4f;
        foes.append(sf::Vertex({px, py}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({px + s, py}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({px + s, py + s}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({px, py + s}, sf::Color(230, 60, 60)));
    }
    minimap_.draw(foes);
    minimap_.display();
}

void GameUI::drawMinimap(sf::RenderTarget& rt) {
    const float MS = 160.0f;
    const float x = 10.0f, y = winH_ - MS - 10.0f;
    // 底框
    sf::RectangleShape bg({MS + 8.0f, MS + 24.0f});
    bg.setPosition(x - 4.0f, y - 22.0f);
    bg.setFillColor(sf::Color(20, 22, 26, 200));
    bg.setOutlineColor(UI_BORDER);
    bg.setOutlineThickness(1.0f);
    rt.draw(bg);
    // 地图
    sf::Sprite sp(minimap_.getTexture());
    sp.setPosition(x, y);
    rt.draw(sp);
    // 当前视野框（摄像机可见范围）
    const auto winSize = g_->window.getSize();
    const auto tl = g_->camera.screenToWorld(0.0f, 0.0f);
    const auto br = g_->camera.screenToWorld(static_cast<float>(winSize.x),
                                             static_cast<float>(winSize.y));
    const sf::Vector2f camCenter(g_->camera.x, g_->camera.y);
    const int cx = static_cast<int>(std::floor(camCenter.x / cfg::TILE_SIZE));
    const int cy = static_cast<int>(std::floor(camCenter.y / cfg::TILE_SIZE));
    const float pxPerTile = MS / 100.0f;
    const float x0 = x + (tl.x / cfg::TILE_SIZE - cx + 50) * pxPerTile;
    const float y0 = y + (tl.y / cfg::TILE_SIZE - cy + 50) * pxPerTile;
    const float x1 = x + (br.x / cfg::TILE_SIZE - cx + 50) * pxPerTile;
    const float y1 = y + (br.y / cfg::TILE_SIZE - cy + 50) * pxPerTile;
    sf::RectangleShape frame({x1 - x0, y1 - y0});
    frame.setPosition(x0, y0);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineColor(sf::Color(255, 255, 255, 220));
    frame.setOutlineThickness(1.0f);
    rt.draw(frame);
    drawText(rt, "小地图", 10, {x + MS / 2.0f, y - 18.0f}, UI_TEXT_LIGHT, true);
}

// ---------------------------------------------------------------------
// 世界地图（Xaero's World Map 式：全屏总览，M键切换）
// ---------------------------------------------------------------------
void GameUI::toggleWorldMap() {
    worldMapOpen_ = !worldMapOpen_;
    if (worldMapOpen_) rebuildWorldMap();
}

void GameUI::rebuildWorldMap() {
    worldmap_.clear(sf::Color(24, 26, 30));
    sf::VertexArray tiles(sf::Quads);
    for (int ty = 0; ty < g_->grid.h; ++ty) {
        for (int tx = 0; tx < g_->grid.w; ++tx) {
            sf::Color c = (g_->terrain[static_cast<size_t>(ty) * g_->grid.w + tx] == 1)
                              ? sf::Color(146, 120, 90)
                              : sf::Color(56, 104, 50);
            const entt::entity b = g_->grid.at(tx, ty).building;
            if (b != entt::null && g_->reg.valid(b)) c = sf::Color(190, 190, 200);
            tiles.append(sf::Vertex({static_cast<float>(tx), static_cast<float>(ty)}, c));
            tiles.append(sf::Vertex({static_cast<float>(tx + 1), static_cast<float>(ty)}, c));
            tiles.append(sf::Vertex({static_cast<float>(tx + 1), static_cast<float>(ty + 1)}, c));
            tiles.append(sf::Vertex({static_cast<float>(tx), static_cast<float>(ty + 1)}, c));
        }
    }
    worldmap_.draw(tiles);
    // 矿点
    sf::VertexArray ores(sf::Quads);
    for (auto [e, pos, ore] : g_->reg.view<GridPos, OreDeposit>().each()) {
        const sf::Color c = ItemSystem::color(ore.type);
        const float s = 1.6f;
        ores.append(sf::Vertex({pos.x, pos.y}, c));
        ores.append(sf::Vertex({pos.x + s, pos.y}, c));
        ores.append(sf::Vertex({pos.x + s, pos.y + s}, c));
        ores.append(sf::Vertex({pos.x, pos.y + s}, c));
    }
    worldmap_.draw(ores);
    // 敌人
    sf::VertexArray foes(sf::Quads);
    for (auto [e, en] : g_->reg.view<Enemy>().each()) {
        const float x = en.pos.x / cfg::TILE_SIZE;
        const float y = en.pos.y / cfg::TILE_SIZE;
        const float s = 1.4f;
        foes.append(sf::Vertex({x, y}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({x + s, y}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({x + s, y + s}, sf::Color(230, 60, 60)));
        foes.append(sf::Vertex({x, y + s}, sf::Color(230, 60, 60)));
    }
    worldmap_.draw(foes);
    worldmap_.display();
}

void GameUI::drawWorldMap(sf::RenderTarget& rt) {
    if (!worldMapOpen_) return;
    // 全屏暗底
    sf::RectangleShape overlay({winW_, winH_});
    overlay.setFillColor(sf::Color(12, 14, 18, 235));
    rt.draw(overlay);
    // 地图按窗口适配居中（最近邻放大，保持像素感）
    const float margin = 40.0f;
    const float scale = std::min((winW_ - margin * 2) / cfg::GRID_WIDTH,
                                 (winH_ - margin * 2) / cfg::GRID_HEIGHT);
    const float w = cfg::GRID_WIDTH * scale, h = cfg::GRID_HEIGHT * scale;
    const float ox = (winW_ - w) / 2.0f, oy = (winH_ - h) / 2.0f;
    sf::Sprite sp(worldmap_.getTexture());
    sp.setScale(scale, scale);
    sp.setPosition(ox, oy);
    rt.draw(sp);
    // 边框
    sf::RectangleShape border({w, h});
    border.setPosition(ox, oy);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(UI_ACCENT);
    border.setOutlineThickness(2.0f);
    rt.draw(border);
    // 当前视野框
    const auto winSize = g_->window.getSize();
    const auto tl = g_->camera.screenToWorld(0.0f, 0.0f);
    const auto br = g_->camera.screenToWorld(static_cast<float>(winSize.x),
                                             static_cast<float>(winSize.y));
    sf::RectangleShape frame({(br.x - tl.x) / cfg::TILE_SIZE * scale,
                              (br.y - tl.y) / cfg::TILE_SIZE * scale});
    frame.setPosition(ox + tl.x / cfg::TILE_SIZE * scale,
                      oy + tl.y / cfg::TILE_SIZE * scale);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineColor(sf::Color(255, 255, 255, 200));
    frame.setOutlineThickness(1.0f);
    rt.draw(frame);
    drawText(rt, "世界地图 · M/ESC/点击 关闭", 16,
             {winW_ / 2.0f, oy - 26.0f}, UI_TEXT_LIGHT, true);
}

// ---------------------------------------------------------------------
// ME网络物品清单（AE2终端式：右键任意ME设备打开，全网物品只读）
// ---------------------------------------------------------------------
void GameUI::showMePanel(entt::entity e) {
    mePanelActive_ = true;
    mePanelEntity_ = e;
    const int nid = MeSystem::networkIdOf(*g_, e);
    size_t rows = 0;
    if (nid >= 0 && nid < static_cast<int>(MeSystem::networks().size()))
        rows = MeSystem::networks()[static_cast<size_t>(nid)].items.size();
    const float w = 420.0f;
    const float h = 64.0f + std::max<size_t>(rows, 1) * 24.0f;
    mePanelRect_ = {(winW_ - w) / 2.0f, 90.0f, w, h};
}

void GameUI::drawMePanel(sf::RenderTarget& rt) {
    if (!mePanelActive_) return;
    const auto& panel = mePanelRect_;
    // 背景
    sf::RectangleShape bg({panel.width, panel.height});
    bg.setPosition(panel.left, panel.top);
    bg.setFillColor(sf::Color(24, 34, 40, 245));
    bg.setOutlineColor(sf::Color(0, 200, 220));
    bg.setOutlineThickness(2.0f);
    rt.draw(bg);
    // 头部条（青色，AE2式）
    sf::RectangleShape head({panel.width, 32.0f});
    head.setPosition(panel.left, panel.top + 2.0f);
    head.setFillColor(sf::Color(0, 140, 165));
    rt.draw(head);
    drawText(rt, "ME 网络终端", 16, {panel.left + 14.0f, panel.top + 8.0f}, UI_TEXT_LIGHT);

    const int nid = MeSystem::networkIdOf(*g_, mePanelEntity_);
    if (nid < 0 || nid >= static_cast<int>(MeSystem::networks().size())) {
        drawText(rt, "未连接到网络", 13, {panel.left + 20.0f, panel.top + 40.0f}, UI_TEXT);
        return;
    }
    const auto& net = MeSystem::networks()[static_cast<size_t>(nid)];
    drawText(rt, "物品 " + std::to_string(net.totalItems) + " / 容量 " +
                     std::to_string(net.capacity),
             13, {panel.left + 20.0f, panel.top + 40.0f}, UI_ACCENT_GREEN);

    // 物品行（类型排序稳定显示）
    std::vector<std::pair<cfg::ItemType, int>> rows(net.items.begin(), net.items.end());
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    float y = panel.top + 62.0f;
    for (const auto& [t, n] : rows) {
        sf::RectangleShape block({14.0f, 14.0f});
        block.setPosition(panel.left + 22.0f, y + 1.0f);
        block.setFillColor(ItemSystem::color(t));
        rt.draw(block);
        drawText(rt, std::string(ItemSystem::nameZh(t)) + " × " + std::to_string(n), 13,
                 {panel.left + 44.0f, y}, UI_TEXT);
        y += 24.0f;
    }
    drawText(rt, "网络内物品全局共享：接口自动入网/出网 · ESC/点击外部关闭", 10,
             {panel.left + panel.width / 2.0f, panel.top + panel.height - 14.0f},
             UI_TEXT, true);
}

// ---------------------------------------------------------------------
// ME接口过滤面板（点选锁定输出物品，白名单）
// ---------------------------------------------------------------------
void GameUI::showFilterPanel(entt::entity e) {
    filterPanelActive_ = true;
    filterPanelEntity_ = e;
    const float w = 440.0f, cellW = 205.0f, cellH = 22.0f;
    const float px = (winW_ - w) / 2.0f;
    const float py = 90.0f;
    const float top = py + 40.0f;
    for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
        const int row = i / 2, col = i % 2;
        filterRects_[static_cast<size_t>(i)] =
            {px + 10.0f + col * (cellW + 10.0f), top + row * cellH, cellW, cellH};
    }
    const float h = 40.0f + (cfg::ITEM_COUNT / 2) * cellH + 26.0f;
    filterPanelRect_ = {px, py, w, h};
}

void GameUI::handleFilterPanelClick(sf::Vector2f pos) {
    if (!filterPanelRect_.contains(pos)) { hideFilterPanel(); return; }
    for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
        if (!filterRects_[static_cast<size_t>(i)].contains(pos)) continue;
        const auto t = static_cast<cfg::ItemType>(i);
        if (g_->reg.valid(filterPanelEntity_) &&
            g_->reg.all_of<MeInterface>(filterPanelEntity_)) {
            auto& f = g_->reg.get<MeInterface>(filterPanelEntity_).filter;
            auto it = std::find(f.begin(), f.end(), t);
            if (it == f.end()) f.push_back(t);
            else f.erase(it);
        }
        return;   // 点中行不关闭，可连续勾选
    }
}

void GameUI::drawFilterPanel(sf::RenderTarget& rt) {
    if (!filterPanelActive_) return;
    const auto& panel = filterPanelRect_;
    sf::RectangleShape bg({panel.width, panel.height});
    bg.setPosition(panel.left, panel.top);
    bg.setFillColor(sf::Color(24, 34, 40, 245));
    bg.setOutlineColor(sf::Color(0, 200, 220));
    bg.setOutlineThickness(2.0f);
    rt.draw(bg);
    sf::RectangleShape head({panel.width, 32.0f});
    head.setPosition(panel.left, panel.top + 2.0f);
    head.setFillColor(sf::Color(0, 140, 165));
    rt.draw(head);
    drawText(rt, "ME 接口过滤", 16, {panel.left + 14.0f, panel.top + 9.0f}, UI_TEXT_LIGHT);

    const bool valid = g_->reg.valid(filterPanelEntity_) &&
                       g_->reg.all_of<MeInterface>(filterPanelEntity_);
    std::vector<cfg::ItemType> flt;
    if (valid) flt = g_->reg.get<MeInterface>(filterPanelEntity_).filter;

    for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
        const auto t = static_cast<cfg::ItemType>(i);
        const auto& rect = filterRects_[static_cast<size_t>(i)];
        const bool locked = std::find(flt.begin(), flt.end(), t) != flt.end();
        sf::RectangleShape row({rect.width, rect.height});
        row.setPosition(rect.left, rect.top);
        row.setFillColor(locked ? sf::Color(0, 110, 135) : sf::Color(30, 40, 48));
        row.setOutlineColor(locked ? sf::Color(0, 220, 255) : sf::Color(60, 70, 80));
        row.setOutlineThickness(locked ? 1.5f : 1.0f);
        rt.draw(row);
        sf::RectangleShape icon({14.0f, 14.0f});
        icon.setPosition(rect.left + 6.0f, rect.top + 4.0f);
        icon.setFillColor(ItemSystem::color(t));
        rt.draw(icon);
        drawText(rt, ItemSystem::nameZh(t), 12, {rect.left + 26.0f, rect.top + 5.0f},
                 locked ? UI_TEXT_LIGHT : UI_TEXT);
        if (locked)
            drawText(rt, "✓", 13, {rect.left + rect.width - 16.0f, rect.top + 4.0f},
                     sf::Color(0, 240, 255));
    }
    drawText(rt, "锁定物品后仅输出这些物品；全部解锁=输出全部 · 点击外部/ESC关闭", 10,
             {panel.left + panel.width / 2.0f, panel.top + panel.height - 14.0f}, UI_TEXT, true);
}

void GameUI::drawResourceBar(sf::RenderTarget& rt) {
    const float barW = winW_ - cfg::ui::SIDE_PANEL_WIDTH;
    const float barH = static_cast<float>(cfg::ui::RESOURCE_BAR_HEIGHT);

    // 背景 + 底部分隔线 + 顶部高光
    sf::RectangleShape bar({barW, barH});
    bar.setFillColor(UI_PANEL);
    rt.draw(bar);
    sf::RectangleShape line({barW, 2.0f});
    line.setPosition(0, barH - 2.0f);
    line.setFillColor(UI_BORDER);
    rt.draw(line);
    sf::RectangleShape topHi({barW, 1.0f});
    topHi.setFillColor(sf::Color(255, 255, 255, 16));
    rt.draw(topHi);

    // ---- 左上角商店按钮 ----
    sf::RectangleShape shopBtn({shopBtn_.width, shopBtn_.height});
    shopBtn.setPosition(shopBtn_.left, shopBtn_.top);
    shopBtn.setFillColor(UI_ACCENT);
    shopBtn.setOutlineColor(UI_BORDER_LIGHT);
    shopBtn.setOutlineThickness(1.5f);
    rt.draw(shopBtn);
    drawText(rt, "商店", 14,
             {shopBtn_.left + shopBtn_.width / 2.0f, shopBtn_.top + 8.0f}, UI_TEXT_LIGHT, true);

    // ---- 左上角帮助按钮（说明书 / 新手引导） ----
    sf::RectangleShape helpBtn({helpBtn_.width, helpBtn_.height});
    helpBtn.setPosition(helpBtn_.left, helpBtn_.top);
    helpBtn.setFillColor(helpOpen_ ? UI_ACCENT_GREEN : UI_PANEL_LIGHT);
    helpBtn.setOutlineColor(UI_BORDER_LIGHT);
    helpBtn.setOutlineThickness(1.5f);
    rt.draw(helpBtn);
    drawText(rt, "帮助 H", 14,
             {helpBtn_.left + helpBtn_.width / 2.0f, helpBtn_.top + 8.0f}, UI_TEXT_LIGHT, true);

    // ---- 右侧信息（右对齐，自适应窗口宽度） ----
    // 金币（金币圆盘 + 数字）
    auto goldDigits = std::to_string(g_->gold);
    const float goldW = 26.0f + goldDigits.size() * 10.0f;
    const float waveW = 74.0f, livesW = 82.0f, modeW = 150.0f;

    // 从右往左依次：模式标签 → 生命 → 波次 → 金币
    const float modeX = barW - 12.0f - modeW;
    const float livesX = modeX - 12.0f - livesW;
    const float waveX = livesX - 12.0f - waveW;
    const float goldX = waveX - 12.0f - goldW;

    // 金币
    sf::CircleShape coin(9.0f);
    coin.setPosition(goldX, 16.0f);
    coin.setFillColor(sf::Color(225, 185, 55));
    coin.setOutlineColor(sf::Color(160, 120, 20));
    coin.setOutlineThickness(1.5f);
    rt.draw(coin);
    drawText(rt, goldDigits, 17, {goldX + 24.0f, 12.0f}, sf::Color(235, 200, 80));

    // 波次
    const sf::Color waveColor = g_->lives <= 3 ? UI_ACCENT_RED : UI_ACCENT;
    drawText(rt, "第 " + std::to_string(g_->currentWave) + " 波", 15, {waveX, 15.0f}, waveColor);

    // 生命
    const sf::Color livesColor = g_->lives <= 3 ? UI_ACCENT_RED : UI_ACCENT_GREEN;
    drawText(rt, "生命 " + std::to_string(g_->lives), 15, {livesX, 15.0f}, livesColor);

    // 模式提示（测试模式 / 倒计时 / 等待）
    if (!cfg::WAVE_AUTO_SPAWN) {
        drawText(rt, "测试模式  Z/X/C 刷怪", 13, {modeX, 16.0f}, UI_ACCENT_ORANGE);
    } else if (g_->waveState == WaveState::Countdown) {
        const int mins = static_cast<int>(g_->countdown) / 60;
        const int secs = static_cast<int>(g_->countdown) % 60;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "倒计时: %02d:%02d", mins, secs);
        drawText(rt, buf, 15, {modeX, 15.0f}, UI_ACCENT_ORANGE);
    } else {
        drawText(rt, "等待开始", 15, {modeX, 15.0f}, UI_TEXT);
    }
}

void GameUI::drawSidePanel(sf::RenderTarget& rt) {
    const float px = winW_ - cfg::ui::SIDE_PANEL_WIDTH;
    sf::RectangleShape panel({static_cast<float>(cfg::ui::SIDE_PANEL_WIDTH), winH_});
    panel.setPosition(px, 0);
    panel.setFillColor(UI_PANEL);
    rt.draw(panel);

    // 标题
    sf::RectangleShape titleBar({static_cast<float>(cfg::ui::SIDE_PANEL_WIDTH), 40.0f});
    titleBar.setPosition(px, 0);
    titleBar.setFillColor(UI_ACCENT);
    rt.draw(titleBar);
    drawText(rt, "背包", 14, {px + cfg::ui::SIDE_PANEL_WIDTH / 2.0f, 10.0f}, UI_TEXT_LIGHT, true);

    // 数量缩写（10 万 → 100k，千 → x k）
    auto fmt = [](int n) -> std::string {
        if (n >= 100000) return std::to_string(n / 1000) + "k";
        if (n >= 1000) return std::to_string(n / 1000) + "k";
        return std::to_string(n);
    };

    // ---- 机器区（可点击放置） ----
    drawText(rt, "机器", 12, {px + 8.0f, 46.0f}, UI_ACCENT_ORANGE);
    for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
        const auto& rect = machineRects_[static_cast<size_t>(i)];
        const auto bt = static_cast<cfg::BuildingType>(i);
        const bool active = g_->hasSelection && g_->selected == bt;
        sf::RectangleShape r({rect.width, rect.height});
        r.setPosition(rect.left, rect.top);
        r.setFillColor(buildingCategoryColor(bt));
        r.setOutlineColor(active ? sf::Color(255, 255, 255) : UI_BORDER_LIGHT);
        r.setOutlineThickness(active ? 2.0f : 1.0f);
        rt.draw(r);
        drawText(rt, fmt(g_->backpackMachines[static_cast<size_t>(i)]), 9,
                 {rect.left + rect.width / 2.0f, rect.top + rect.height - 12.0f}, UI_TEXT_LIGHT, true);
    }

    // ---- 物品区（仅展示数量） ----
    drawText(rt, "物品", 12, {px + 8.0f, itemRects_[0].top - 20.0f}, UI_ACCENT_GREEN);
    for (int i = 0; i < cfg::ITEM_COUNT; ++i) {
        const auto& rect = itemRects_[static_cast<size_t>(i)];
        const auto t = static_cast<cfg::ItemType>(i);
        sf::RectangleShape r({rect.width, rect.height});
        r.setPosition(rect.left, rect.top);
        r.setFillColor(ItemSystem::color(t));
        r.setOutlineColor(UI_BORDER_LIGHT);
        r.setOutlineThickness(1.0f);
        rt.draw(r);
        drawText(rt, fmt(g_->playerInv[t]), 9,
                 {rect.left + rect.width / 2.0f, rect.top + rect.height - 12.0f}, UI_TEXT_LIGHT, true);
    }
}

void GameUI::drawToast(sf::RenderTarget& rt) {
    // 最后0.5秒淡出（Python行为）
    float alpha = 1.0f;
    if (toastTimer_ < 0.5f) alpha = toastTimer_ / 0.5f;
    const float w = 300.0f, h = 60.0f;
    const float x = (winW_ - cfg::ui::SIDE_PANEL_WIDTH - w) / 2.0f;
    sf::RectangleShape bg({w, h});
    bg.setPosition(x, 80.0f);
    bg.setFillColor(sf::Color(0, 0, 0, static_cast<uint8_t>(180.0f * alpha)));
    rt.draw(bg);
    sf::RectangleShape border({w, h});
    border.setPosition(x, 80.0f);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(UI_ACCENT_RED);
    border.setOutlineThickness(2.0f);
    rt.draw(border);
    drawText(rt, toast_, 18, {x + w / 2.0f, 80.0f + h / 2.0f - 10.0f},
             sf::Color(255, 255, 255, static_cast<uint8_t>(255.0f * alpha)), true);
}

void GameUI::drawDirectionPopup(sf::RenderTarget& rt) {
    const bool is8 = popupCount_ == 8;
    const float pw = is8 ? 200.0f : 180.0f;
    const float ph = is8 ? 200.0f : 180.0f;
    const float cx = popupScreen_.x, cy = popupScreen_.y;

    sf::RectangleShape bg({pw, ph});
    bg.setPosition(cx - pw / 2.0f, cy - ph / 2.0f);
    bg.setFillColor(UI_PANEL);
    bg.setOutlineColor(UI_ACCENT);
    bg.setOutlineThickness(3.0f);
    rt.draw(bg);
    drawText(rt, "选择方向", 14, {cx, cy - ph / 2.0f + 4.0f}, UI_TEXT, true);

    // 方向按钮
    const char* symbols4[4] = {"↑", "→", "↓", "←"};
    const char* symbols8[8] = {"↑", "↗", "→", "↘", "↓", "↙", "←", "↖"};
    for (int i = 0; i < popupCount_; ++i) {
        const auto& rect = popupRects_[static_cast<size_t>(i)];
        sf::RectangleShape btn({rect.width, rect.height});
        btn.setPosition(rect.left, rect.top);
        btn.setFillColor(UI_ACCENT);
        btn.setOutlineColor(UI_BORDER_LIGHT);
        btn.setOutlineThickness(1.0f);
        rt.draw(btn);
        drawText(rt, is8 ? symbols8[i] : symbols4[i], 16,
                 {rect.left + rect.width / 2.0f, rect.top + 12.0f}, UI_TEXT_LIGHT, true);
    }
}

void GameUI::drawTooltip(sf::RenderTarget& rt) {
    if (!tooltipActive_) return;
    const sf::Vector2i m = sf::Mouse::getPosition(g_->window);
    // 高度随行数自适应
    const float w = 190.0f;
    const float titleH = 26.0f;
    const float lineH = 17.0f;
    const float pad = 10.0f;
    const float h = titleH + static_cast<float>(tooltipLines_.size()) * lineH + pad;
    float x = static_cast<float>(m.x) + 15.0f;
    float y = static_cast<float>(m.y) + 15.0f;
    if (x + w > winW_) x = static_cast<float>(m.x) - w - 15.0f;
    if (y + h > winH_) y = static_cast<float>(m.y) - h - 15.0f;

    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(UI_PANEL);
    bg.setOutlineColor(UI_ACCENT);
    bg.setOutlineThickness(2.0f);
    rt.draw(bg);
    // 标题（强调色）+ 分隔线
    drawText(rt, tooltipTitle_, 14, {x + pad, y + 5.0f}, UI_ACCENT_ORANGE);
    sf::RectangleShape divider({w - pad * 2.0f, 1.0f});
    divider.setPosition(x + pad, y + titleH);
    divider.setFillColor(UI_DIVIDER);
    rt.draw(divider);
    float ly = y + titleH + 6.0f;
    for (const auto& line : tooltipLines_) {
        drawText(rt, line, 12, {x + pad, ly}, UI_TEXT);
        ly += lineH;
    }
}

void GameUI::drawFaceEditor(sf::RenderTarget& rt) {
    const entt::entity target = g_->faceEditTarget;
    if (target == entt::null || !g_->reg.valid(target)) return;
    const auto& b = g_->reg.get<Building>(target);
    const bool isWire = b.type == cfg::BuildingType::PowerWire;

    // 半透明覆盖层（Python行为）
    sf::RectangleShape overlay({winW_, winH_});
    overlay.setFillColor(sf::Color(0, 0, 0, 120));
    rt.draw(overlay);

    // 中心高亮框
    const auto center = g_->buildingCenter(b);
    const auto sc = g_->worldToScreen(center);
    const float size = cfg::TILE_SIZE * g_->camera.zoom;
    sf::RectangleShape hl({size, size});
    hl.setPosition(sc.x - size / 2.0f, sc.y - size / 2.0f);
    hl.setFillColor(isWire ? sf::Color(255, 255, 0, 80) : sf::Color(0, 255, 255, 80));
    rt.draw(hl);
    sf::RectangleShape hlBorder({size, size});
    hlBorder.setPosition(sc.x - size / 2.0f, sc.y - size / 2.0f);
    hlBorder.setFillColor(sf::Color::Transparent);
    hlBorder.setOutlineColor(isWire ? sf::Color(255, 255, 0) : sf::Color(0, 255, 255));
    hlBorder.setOutlineThickness(3.0f);
    rt.draw(hlBorder);

    // 面模式颜色与标签
    const auto& fc = g_->reg.get<FaceConfig>(target);
    const auto rects = faceEditorRects();
    // 箭头方向：INPUT朝向机器中心(向内)，OUTPUT背离中心(向外)，一目了然
    const char* arrowIn[4]  = {"↓", "←", "↑", "→"};   // 输入箭头(指向中心)
    const char* arrowOut[4] = {"↑", "→", "↓", "←"};   // 输出箭头(指向外侧)
    for (int d = 0; d < 4; ++d) {
        const auto mode = fc.get(d);
        sf::Color color(70, 70, 75);
        const char* arrow = "✕";
        const char* label = "无";
        if (isWire) {
            const sf::Color colors[4] = {sf::Color(70, 70, 75), sf::Color(0, 140, 255),
                                         sf::Color(180, 180, 40), sf::Color(255, 140, 0)};
            const char* labels[4] = {"无", "输入", "传输", "输出"};
            const char* arrows[4] = {"✕", arrowIn[d], "⇄", arrowOut[d]};
            color = colors[static_cast<uint8_t>(mode)];
            label = labels[static_cast<uint8_t>(mode)];
            arrow = arrows[static_cast<uint8_t>(mode)];
        } else {
            const sf::Color colors[3] = {sf::Color(70, 70, 75), sf::Color(0, 180, 80),
                                         sf::Color(220, 80, 40)};
            const uint8_t m = std::min<uint8_t>(static_cast<uint8_t>(mode), 2);
            const char* labels[3] = {"无", "输入", "输出"};
            color = colors[m];
            label = labels[m];
            arrow = (mode == cfg::FaceMode::INPUT) ? arrowIn[d]
                  : (mode == cfg::FaceMode::OUTPUT) ? arrowOut[d] : "✕";
        }
        const auto& rect = rects[static_cast<size_t>(d)];
        sf::RectangleShape btn({rect.width, rect.height});
        btn.setPosition(rect.left, rect.top);
        btn.setFillColor(color);
        btn.setOutlineColor(sf::Color(180, 180, 180));
        btn.setOutlineThickness(1.0f);
        rt.draw(btn);
        drawText(rt, arrow, 12, {rect.left + rect.width / 2.0f, rect.top + 6.0f},
                 sf::Color(230, 230, 240), true);
        drawText(rt, label, 10, {rect.left + rect.width / 2.0f, rect.top + rect.height + 4.0f},
                 sf::Color(230, 230, 240), true);
    }

    // 标题提示 + 颜色图例
    drawText(rt, isWire ? "电线方向编辑 - 点击切换 无/输入/传输/输出"
                        : "面配置编辑 - 点击切换 无/输入/输出",
             12, {sc.x, sc.y - size / 2.0f - 24.0f * g_->camera.zoom - 14.0f},
             sf::Color(255, 220, 50), true);
    const char* legend = isWire
        ? "图例: 灰=无  蓝=输入  黄=传输  橙=输出"
        : "图例: 灰=无  绿=输入  红=输出";
    drawText(rt, legend, 11, {sc.x, sc.y + size / 2.0f + 34.0f * g_->camera.zoom + 6.0f},
             sf::Color(220, 220, 220), true);
    drawText(rt, "[ESC]或点击外部关闭", 9,
             {sc.x, sc.y + size / 2.0f + 34.0f * g_->camera.zoom + 24.0f},
             sf::Color(180, 180, 180), true);
}

void GameUI::drawOverlays(sf::RenderTarget& rt) {
    // 暂停面板打开时由面板自身呈现，不再叠一层大字"暂停"
    if (g_->paused && !pausePanelOpen_)
        drawText(rt, "暂停", 72, {winW_ / 2.0f, winH_ / 2.0f - 40.0f},
                 sf::Color::White, true);
    if (g_->gameOver)
        drawText(rt, "游戏结束", 72, {winW_ / 2.0f, winH_ / 2.0f - 40.0f},
                 sf::Color::Red, true);
}

// =====================================================================
// 暂停面板（ESC 打开）
//
// 参考《我的世界》等游戏的暂停菜单：全屏变暗 + 居中纵向按钮列。
//   继续游戏 / 保存存档 / 设置 / 返回主界面
// 「设置」子页与启动菜单的「设置」界面共用 gset 里的同一份设置项定义
// （标签/取值/切换/激活），因此行为完全一致。
// =====================================================================
void GameUI::openPausePanel() {
    pausePanelOpen_ = true;
    pauseSettingsOpen_ = false;
    pauseFocus_ = 0;          // 默认落在「继续游戏」
    pauseSettingRow_ = 0;
    g_->paused = true;        // 冻结世界
    // 关掉可能同时打开的面板，避免叠层（面板内不再操作世界）
    closePanels();
    hideRecipePopup();
    g_->faceEditTarget = entt::null;
}

void GameUI::closePausePanel() {
    pausePanelOpen_ = false;
    pauseSettingsOpen_ = false;
    g_->paused = false;       // 恢复游戏
}

std::array<sf::FloatRect, 4> GameUI::pauseMenuRects() const {
    const float bw = 340.0f, bh = 54.0f, gap = 12.0f;
    const float x = (winW_ - bw) / 2.0f;
    const float total = 4 * bh + 3 * gap;
    const float y0 = winH_ * 0.5f - total / 2.0f + 34.0f;
    std::array<sf::FloatRect, 4> r{};
    for (int i = 0; i < 4; ++i)
        r[static_cast<size_t>(i)] = {x, y0 + static_cast<float>(i) * (bh + gap), bw, bh};
    return r;
}

std::array<sf::FloatRect, gset::SETTING_ROW_COUNT> GameUI::pauseSettingRects() const {
    constexpr int N = gset::SETTING_ROW_COUNT;
    const float bw = std::min(620.0f, winW_ - 120.0f);
    const float bh = 50.0f, gap = 10.0f;
    const float total = N * bh + (N - 1) * gap;
    const float x0 = (winW_ - bw) / 2.0f;
    const float y0 = std::max(winH_ * 0.26f, (winH_ - total) * 0.5f);
    std::array<sf::FloatRect, N> r{};
    for (int i = 0; i < N; ++i)
        r[static_cast<size_t>(i)] = {x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh};
    return r;
}

void GameUI::activatePauseItem(int i) {
    switch (i) {
        case 0: closePausePanel(); break;                       // 继续游戏
        case 1: g_->saveGame(); break;                          // 保存存档（Toast 反馈，面板不关）
        case 2: pauseSettingsOpen_ = true; pauseSettingRow_ = 0; break;  // 设置
        case 3:                                                 // 返回主界面（先自动保存）
            g_->saveGame();
            g_->returnToMenu = true;   // 主循环据此退出，main.cpp 重新进入入口系统
            break;
        default: break;
    }
}

void GameUI::handlePauseKey(const sf::Event::KeyEvent& k) {
    using K = sf::Keyboard;
    if (pauseSettingsOpen_) {
        constexpr int N = gset::SETTING_ROW_COUNT;
        switch (k.code) {
            case K::Up: case K::W:
                pauseSettingRow_ = (pauseSettingRow_ + N - 1) % N; break;
            case K::Down: case K::S:
                pauseSettingRow_ = (pauseSettingRow_ + 1) % N; break;
            case K::Left: case K::A:
                if (gset::cycleSettingRow(pauseSettingRow_, -1)) pauseDisplayApply_ = true; break;
            case K::Right: case K::D:
                if (gset::cycleSettingRow(pauseSettingRow_, 1)) pauseDisplayApply_ = true; break;
            case K::Enter: case K::Space: {
                const auto r = gset::activateSettingRow(pauseSettingRow_);
                if (r == gset::SettingActivate::DisplayChanged) pauseDisplayApply_ = true;
                else if (r == gset::SettingActivate::Back) pauseSettingsOpen_ = false;
                break;
            }
            case K::Escape: pauseSettingsOpen_ = false; break;   // 设置 → 返回暂停面板
            default: break;
        }
        return;
    }
    switch (k.code) {
        case K::Up: case K::W:    pauseFocus_ = (pauseFocus_ + 3) % 4; break;
        case K::Down: case K::S:  pauseFocus_ = (pauseFocus_ + 1) % 4; break;
        case K::Enter: case K::Space: activatePauseItem(pauseFocus_); break;
        case K::Escape: closePausePanel(); break;
        default: break;
    }
}

void GameUI::handlePauseClick(sf::Vector2f pos) {
    if (pauseSettingsOpen_) {
        // 与启动菜单一致：先点选中，再点一次 = 激活该行
        const auto rects = pauseSettingRects();
        for (int i = 0; i < gset::SETTING_ROW_COUNT; ++i) {
            if (!rects[static_cast<size_t>(i)].contains(pos)) continue;
            if (pauseSettingRow_ == i) {
                const auto r = gset::activateSettingRow(i);
                if (r == gset::SettingActivate::DisplayChanged) pauseDisplayApply_ = true;
                else if (r == gset::SettingActivate::Back) pauseSettingsOpen_ = false;
            } else {
                pauseSettingRow_ = i;
            }
            return;
        }
        return;
    }
    const auto rects = pauseMenuRects();
    for (int i = 0; i < 4; ++i) {
        if (rects[static_cast<size_t>(i)].contains(pos)) {
            pauseFocus_ = i;
            activatePauseItem(i);
            return;
        }
    }
}

void GameUI::drawPausePanel(sf::RenderTarget& rt) {
    sf::RectangleShape dim({winW_, winH_});
    dim.setFillColor(sf::Color(0, 0, 0, 175));   // 全屏变暗
    rt.draw(dim);
    if (pauseSettingsOpen_) drawPauseSettings(rt);
    else drawPauseMenu(rt);
}

void GameUI::drawPauseMenu(sf::RenderTarget& rt) {
    drawText(rt, "游戏已暂停", 46, {winW_ / 2.0f, winH_ * 0.15f}, UI_TEXT_LIGHT, true);
    drawText(rt, "P A U S E D", 15, {winW_ / 2.0f, winH_ * 0.15f + 54.0f},
             UI_ACCENT_ORANGE, true);

    static const char* const kItems[4] = {"继续游戏", "保存存档", "设置", "返回主界面"};
    const auto rects = pauseMenuRects();
    for (int i = 0; i < 4; ++i) {
        const sf::FloatRect& r = rects[static_cast<size_t>(i)];
        const bool active = (i == pauseFocus_);
        sf::RectangleShape body({r.width, r.height});
        body.setPosition(r.left, r.top);
        body.setFillColor(active ? sf::Color(58, 62, 70, 245) : sf::Color(40, 43, 50, 235));
        body.setOutlineThickness(active ? 2.0f : 1.0f);
        body.setOutlineColor(active ? UI_ACCENT : UI_BORDER);
        rt.draw(body);
        drawText(rt, kItems[i], 22,
                 {r.left + r.width / 2.0f, r.top + r.height / 2.0f - 16.0f},
                 active ? UI_TEXT_LIGHT : UI_TEXT, true);
    }
    drawText(rt, "↑↓ 选择    Enter 确认    Esc 继续游戏", 13,
             {winW_ / 2.0f, winH_ - 30.0f}, UI_TEXT, true);
}

void GameUI::drawPauseSettings(sf::RenderTarget& rt) {
    drawText(rt, "设置", 40, {winW_ / 2.0f, winH_ * 0.10f}, UI_TEXT_LIGHT, true);
    drawText(rt, "O P T I O N S", 14, {winW_ / 2.0f, winH_ * 0.10f + 36.0f},
             UI_ACCENT_ORANGE, true);

    // 右对齐小工具（GameUI::drawText 只支持居中，这里取值需要右对齐）
    auto valueRight = [&](const std::string& s, float rightX, float y, sf::Color c) {
        sf::Text t;
        t.setFont(g_->assets.font());
        t.setCharacterSize(19);
        t.setString(sf::String::fromUtf8(s.begin(), s.end()));
        t.setFillColor(c);
        t.setPosition(rightX - t.getLocalBounds().width, y);
        rt.draw(t);
    };

    const auto rects = pauseSettingRects();
    for (int i = 0; i < gset::SETTING_ROW_COUNT; ++i) {
        const sf::FloatRect& r = rects[static_cast<size_t>(i)];
        const bool active = (i == pauseSettingRow_);
        sf::RectangleShape body({r.width, r.height});
        body.setPosition(r.left, r.top);
        body.setFillColor(active ? sf::Color(58, 62, 70, 245) : sf::Color(40, 43, 50, 235));
        body.setOutlineThickness(active ? 2.0f : 1.0f);
        body.setOutlineColor(active ? UI_ACCENT : UI_BORDER);
        rt.draw(body);

        const float ty = r.top + r.height / 2.0f - 14.0f;
        drawText(rt, gset::settingRowLabel(i), 19, {r.left + 18.0f, ty},
                 active ? UI_TEXT_LIGHT : UI_TEXT);
        if (!gset::settingRowIsAction(i))
            valueRight(gset::settingRowValue(i), r.left + r.width - 18.0f, ty,
                       active ? UI_ACCENT : UI_TEXT);
    }
    drawText(rt, "↑↓ 选择    ←→ 调整    Enter 确认    Esc 返回", 13,
             {winW_ / 2.0f, winH_ - 30.0f}, UI_TEXT, true);
}

// =====================================================================
// 游戏说明书 / 新手引导（H 或 F1 打开；新游戏首次进入自动弹出）
//
// TODO.md 诉求："README 没人看，做一个 UI 版说明书"。
// 面板分 6 个标签页，内容随 config.json 数值变化保持定性描述（不写死数值）。
// =====================================================================
namespace {

/// 说明页文本行（style: 0=正文 1=小标题 2=绿色强调）
struct HelpLine {
    std::string text;
    int style = 0;
};

/// 说明书标签页名称（与 helpTab_ 对应）
const char* const HELP_TAB_NAMES[6] = {"新手引导", "操作按键", "建筑一览",
                                       "生产与物流", "电力系统", "常见问题"};
constexpr int HELP_TAB_COUNT = 6;

/// 建筑用途说明（按 cfg::BuildingType 枚举序）
const char* buildingHelp(cfg::BuildingType t) {
    switch (t) {
        case cfg::BuildingType::TowerBasic:    return "基础炮塔，消耗弹药自动开火，前期主力";
        case cfg::BuildingType::TowerRapid:    return "速射塔，射速最快，耗弹量大，适合弹药充足时";
        case cfg::BuildingType::TowerSniper:   return "狙击塔，射程最远、单发伤害最高，专打坦克";
        case cfg::BuildingType::TowerElectric: return "电力塔，不需弹药但必须接入电网供电";
        case cfg::BuildingType::Miner:         return "采矿场1级：采集5×5范围内所有矿石，每秒4个";
        case cfg::BuildingType::MinerL2:       return "采矿场2级：采集9×9范围，每秒16个";
        case cfg::BuildingType::MinerL3:       return "采矿场3级：采集13×13范围，每秒256个";
        case cfg::BuildingType::MinerVoid:     return "虚空采矿场：无需矿点，全类型矿石每秒4096个";
        case cfg::BuildingType::Furnace:       return "熔炉：矿石→锭（铁/铜/金/镍/银/铅），放置时选输出面";
        case cfg::BuildingType::Assembler:     return "组装机：右键选定配方后量产（弹药 / 电路板）";
        case cfg::BuildingType::Generator:     return "旧版大功率发电机(2×2)：1块煤→3000EU，爆发式供电";
        case cfg::BuildingType::PowerPole:     return "电线杆：150px半径内恒导通，用于跨距离连电网";
        case cfg::BuildingType::PowerGenerator:return "燃煤发电机：烧煤稳定发电32EU/秒，需持续供煤";
        case cfg::BuildingType::Capacitor:     return "电容库：储存50000EU，平滑发电与用电的波动";
        case cfg::BuildingType::PowerWire:     return "电力线缆：四面独立配置(无/输入/传输/输出)，右键编辑";
        case cfg::BuildingType::Pipe:          return "物品管道：自动链接四邻，把物品送往最近的可接收端点";
        case cfg::BuildingType::Bucket:        return "储物桶：容量极大，每0.5秒按输出面吐出一个物品";
        case cfg::BuildingType::Splitter:      return "分流器：把物品轮询均分给多个出口，均分产线";
        case cfg::BuildingType::AlloyFurnace:  return "合金炉：右键选定合金配方，锭→合金，无需电力";
        case cfg::BuildingType::MeInterface:   return "ME接口：吸入物品入网/导出物品出网，右键可锁定输出物品";
        case cfg::BuildingType::MeDrive:       return "ME存储单元：每块为网络提供20000容量，叠加生效";
        case cfg::BuildingType::MeTerminal:    return "ME终端：右键查看整个网络的物品清单";
        default:                               return "";
    }
}

/// 生成指定标签页的说明内容
std::vector<HelpLine> buildHelpPage(int tab) {
    std::vector<HelpLine> out;
    auto h = [&](const char* s) { out.push_back({s, 1}); };      // 小标题
    auto p = [&](const char* s) { out.push_back({s, 0}); };      // 正文

    switch (tab) {
        case 0:  // 新手引导
            h("◆ 游戏目标");
            p("敌人沿土黄色路径走向终点，漏过去一个扣 1 点生命（初始 10），归零即结束。");
            p("击杀敌人获得金币，金币可在商店（B）兑换材料与机器。");
            h("◆ 第 1 步 · 采矿");
            p("按 3 选采矿场，放在矿点附近即可——范围内自动采集所有矿石，不用压在矿上。");
            p("1/2/3 级分别覆盖 5×5 / 9×9 / 13×13，每秒 4 / 16 / 256 个；右键旋转输出面。");
            h("◆ 第 2 步 · 把矿运出来");
            p("按 4 铺物品管道：自动链接四邻的机器、桶和管道，不用配方向。");
            p("也可以把储物桶（5）直接贴到机器的输出面上，机器会自动往里塞产物。");
            h("◆ 第 3 步 · 冶炼");
            p("按 6 放熔炉，矿石→锭。机器会自动从相邻管道拉取自己配方需要的原料。");
            h("◆ 第 4 步 · 加工");
            p("按 7 放组装机，右键选配方（弹药 / 电路板）；合金炉右键选合金配方，无需电力。");
            h("◆ 第 5 步 · 防御");
            p("按 1 / 2 放炮塔。基础/速射/狙击塔需要弹药，电力塔需要电网供电。");
            p("弹药由组装机量产，前期也可用 V 随身工作台手工合成。");
            h("◆ 第 6 步 · 供电");
            p("按 0 放燃煤发电机（烧煤）→ 用 9 电线杆或 = 电力线缆把电送出去。");
            p("线缆四面可独立配置，右键点线缆逐面切换 无/输入/传输/输出。");
            h("◆ 第 7 步 · 进阶物流（中后期）");
            p("用电路板 + 钢锭造 ME 接口 / 存储单元 / 终端：物品数字化入网，全网共享。");
            p("建成后物流瓶颈基本消失，可以放心大规模扩产。");
            h("◆ 常用按键");
            p("B 商店 · V 工作台 · M 世界地图 · H 本说明 · F5 保存 / F9 读取 · 空格暂停 · DEL 拆除");
            break;

        case 1:  // 操作按键
            h("◆ 镜头");
            p("WASD 移动镜头 · 鼠标滚轮缩放（0.5x ~ 2.0x） · 空格暂停");
            h("◆ 选择与放置");
            p("数字键 1~0 与 - = \\ 直接选建筑 · TAB 循环切换全部建筑");
            p("左键放置（带方向的建筑会弹出方向选择窗） · DEL 拆除并返还材料");
            h("◆ 右键能做什么");
            p("采矿场：旋转输出面 · 组装机/合金炉：打开配方菜单");
            p("电线 / 熔炉 / 储物桶 / 发电机：打开面配置编辑器");
            p("ME接口：输出过滤 · ME存储单元 / ME终端：查看网络物品清单");
            p("管道 / 分流器：自动链接四邻，无需任何操作");
            h("◆ 面板");
            p("B 商店 · V 随身工作台 · M 世界地图 · H / F1 说明书 · F5 保存 · F9 读取 · ESC 关闭");
            h("◆ 测试用");
            p("Z 普通敌人 · X 快速敌人 · C 坦克敌人（击杀只加金币，不掉落物品）");
            break;

        case 2:  // 建筑一览
            h("◆ 全部建筑（快捷键 + 用途）");
            for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
                const auto& info = cfg::BUILDING_INFOS[static_cast<size_t>(i)];
                const auto bt = static_cast<cfg::BuildingType>(i);
                std::string line = "[";
                line += (info.hotkey && info.hotkey[0]) ? info.hotkey : "-";
                line += "] " + std::string(info.nameZh) + " — ";
                line += buildingHelp(bt);
                // 成本
                if (!info.cost.empty()) {
                    line += "（造价: ";
                    for (size_t k = 0; k < info.cost.size(); ++k) {
                        if (k) line += " + ";
                        line += std::string(ItemSystem::nameZh(info.cost[k].first)) + "×" +
                                std::to_string(info.cost[k].second);
                    }
                    line += "）";
                }
                out.push_back({line, 0});
            }
            break;

        case 3:  // 生产与物流
            h("◆ 完整生产链");
            p("矿石（采矿场）→ 锭（熔炉）→ 合金（合金炉）→ 电路板 / 弹药（组装机或随身工作台）");
            h("◆ 熔炉配方（矿石 → 锭）");
            p("铁 / 铜 约 2 秒一个；金 / 镍 / 银 / 铅 约 3 秒一个（数值见 config.json）");
            h("◆ 合金炉配方（右键选一种，无需电力）");
            p("钢锭 = 2铁锭 + 2煤 · 琥珀金锭 = 1金锭 + 1银锭 → 2");
            p("因瓦锭 = 2铁锭 + 1镍锭 → 3 · 康铜锭 = 1铜锭 + 1镍锭 → 2");
            h("◆ 组装机 / 随身工作台（V）");
            p("弹药 = 2铁锭 + 1铜锭 · 电路板 = 1铁锭 + 1铜锭");
            p("前期用 V 手工合成起步，后期交给组装机自动量产。");
            h("◆ 前期物流：物品管道");
            p("自动链接四邻，每隔一小段时间把缓冲里的物品送到最近的可接收端点。");
            p("送不出去就留在管道里（背压，不会丢失）；机器只拉自己配方需要的原料。");
            h("◆ 后期物流：ME 网络");
            p("ME接口吸入/导出物品，ME存储单元提供容量，ME终端右键查看全网物品。");
            p("接口 / 存储单元 / 终端任意一台都能作为入网口，全网库存共享、机器直接取料。");
            break;

        case 4:  // 电力系统
            h("◆ 单位");
            p("全部电力以 EU/秒 计（与帧率无关）。电网无损耗、无过载——这是设计如此。");
            h("◆ 发电");
            p("燃煤发电机：烧煤稳定输出 32EU/秒，需要持续供煤（用管道送煤进去）。");
            p("旧版大功率发电机（2×2）：1 块煤 → 3000EU，爆发式供电，适合应急。");
            h("◆ 输电");
            p("电线杆：150px 半径内恒导通，用来跨越长距离。");
            p("电力线缆：四面独立配置，可设为 无 / 输入 / 传输 / 输出，右键逐面切换。");
            p("导通规则：本面为 输出或传输 且 对面为 输入或传输，才通电。");
            h("◆ 储能与用电");
            p("电容库：容量 50000EU，充放速率 64EU/秒，用来平滑波动。");
            p("电力塔：8EU/秒，断电即停火。熔炉、组装机、合金炉、采矿场（测试期）不需要电力。");
            break;

        default:  // 常见问题
            h("◆ 物品卡在管道里不动？");
            p("说明没有可接收的端点：目标机器只收自己配方要的原料、桶已满，");
            p("或者你把管道顶在了机器的输出面上（产物面不收原料，要接输入面）。");
            h("◆ 机器不工作？");
            p("熔炉 / 组装机靠相邻管道供料；合金炉要先右键选配方；电力塔需要电网供电。");
            h("◆ 采矿场不产矿？");
            p("范围内必须有矿点（虚空采矿场除外）。按 M 看世界地图找矿点，");
            p("每个矿点储量 1000，采完会消失。");
            h("◆ 电网不通？");
            p("发电机要有煤；线缆面要配成 输入/输出；电线杆之间不超过 150px。");
            p("电容库只储能，不会自己发电。");
            h("◆ ME 网络存不进去？");
            p("网络容量来自 ME 存储单元（每块 20000），没有存储单元就存不进物品。");
            h("◆ 商店买的机器在哪？");
            p("进右侧背包上方的机器格，点格子选中即可放置（数字键同样可切建筑）。");
            h("◆ 想改数值？");
            p("改 assets/config.json（游戏实际读取 build/assets/config.json），");
            p("保存后重启游戏即可生效，不需要重新编译。");
            break;
    }
    return out;
}
} // namespace

void GameUI::toggleHelp() {
    if (helpOpen_) { helpOpen_ = false; return; }
    openHelp(0);
}

void GameUI::openHelp(int tab) {
    helpOpen_ = true;
    helpTab_ = std::clamp(tab, 0, HELP_TAB_COUNT - 1);
    helpScroll_ = 0.0f;
    // 与其它全屏面板互斥，避免叠加
    shopOpen_ = false;
    workbenchOpen_ = false;
    worldMapOpen_ = false;
    layoutHelp();
}

void GameUI::layoutHelp() {
    const float pw = std::min(winW_ - 200.0f, 780.0f);
    const float px = (winW_ - pw) / 2.0f;
    const float py = 152.0f;
    helpPanel_ = {px, py, pw, std::max(180.0f, winH_ - py - 100.0f)};

    const float tw = 120.0f, th = 30.0f, gap = 6.0f;
    const float totalW = HELP_TAB_COUNT * tw + (HELP_TAB_COUNT - 1) * gap;
    const float tx = (winW_ - totalW) / 2.0f;
    for (int i = 0; i < HELP_TAB_COUNT; ++i)
        helpTabRects_[static_cast<size_t>(i)] = {tx + i * (tw + gap), 110.0f, tw, th};
}

void GameUI::handleHelpClick(sf::Vector2f pos) {
    for (int i = 0; i < HELP_TAB_COUNT; ++i) {
        if (helpTabRects_[static_cast<size_t>(i)].contains(pos)) {
            if (helpTab_ != i) { helpTab_ = i; helpScroll_ = 0.0f; }
            return;
        }
    }
    // 点击面板外部 → 关闭
    if (!helpPanel_.contains(pos)) helpOpen_ = false;
}

void GameUI::drawHelp(sf::RenderTarget& rt) {
    if (!helpOpen_) return;
    const auto& p = helpPanel_;

    // 全屏遮罩
    sf::RectangleShape bg({winW_, winH_});
    bg.setFillColor(sf::Color(14, 16, 20, 246));
    rt.draw(bg);

    drawText(rt, "游戏说明书", 30, {winW_ / 2.0f, 30.0f}, UI_TEXT_LIGHT, true);
    drawText(rt, "随时按 H 或 F1 重新打开 · 滚轮滚动内容 · ESC 或点击外部关闭", 12,
             {winW_ / 2.0f, 76.0f}, UI_ACCENT_ORANGE, true);

    // 标签按钮
    for (int i = 0; i < HELP_TAB_COUNT; ++i) {
        const auto& rect = helpTabRects_[static_cast<size_t>(i)];
        const bool active = (i == helpTab_);
        sf::RectangleShape btn({rect.width, rect.height});
        btn.setPosition(rect.left, rect.top);
        btn.setFillColor(active ? UI_ACCENT : UI_PANEL_LIGHT);
        btn.setOutlineColor(UI_BORDER_LIGHT);
        btn.setOutlineThickness(1.0f);
        rt.draw(btn);
        drawText(rt, HELP_TAB_NAMES[i], 13,
                 {rect.left + rect.width / 2.0f, rect.top + 7.0f},
                 active ? UI_TEXT_LIGHT : UI_TEXT, true);
    }

    // 内容面板
    sf::RectangleShape panel({p.width, p.height});
    panel.setPosition(p.left, p.top);
    panel.setFillColor(UI_PANEL);
    panel.setOutlineColor(UI_BORDER_LIGHT);
    panel.setOutlineThickness(1.5f);
    rt.draw(panel);

    const auto lines = buildHelpPage(helpTab_);
    float y = p.top + 12.0f - helpScroll_;
    for (const auto& ln : lines) {
        const float lh = (ln.style == 1) ? 28.0f : 21.0f;
        // 视口外跳过（简易裁剪）
        if (y + lh > p.top && y < p.top + p.height - 6.0f) {
            if (ln.style == 1)
                drawText(rt, ln.text, 15, {p.left + 14.0f, y}, UI_ACCENT);
            else if (ln.style == 2)
                drawText(rt, ln.text, 13, {p.left + 22.0f, y}, UI_ACCENT_GREEN);
            else
                drawText(rt, ln.text, 13, {p.left + 22.0f, y}, UI_TEXT);
        }
        y += lh;
    }

    // 滚动到底部时钳制偏移（内容高度 = y + helpScroll_ - 面板顶）
    const float contentH = (y + helpScroll_) - p.top;
    const float maxScroll = std::max(0.0f, contentH - (p.height - 18.0f));
    if (helpScroll_ > maxScroll) helpScroll_ = maxScroll;

    drawText(rt, "H / F1 或 ESC 关闭", 11, {winW_ / 2.0f, winH_ - 44.0f}, UI_TEXT, true);
}
