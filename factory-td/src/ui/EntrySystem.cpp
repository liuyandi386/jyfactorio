// =====================================================================
// EntrySystem.cpp —— 游戏入口系统实现
//
// 流程：启动动画 → 标题/主菜单 → (设置) → 加载界面 → 交回 main.cpp
// 说明：加载界面的进度是"真实"的——每一步都会实际执行工作
//       （读取 config.json / 校验存档 / 展开敌人路径 / 生成矿点分布），
//       并把真实计算出的路径与矿点绘制在迷你地图上作为直观反馈。
// =====================================================================
#include "ui/EntrySystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>

#include "ConfigLoader.h"
#include "GameConfig.h"
#include "SaveSystem.h"
#include "Settings.h"
#include "utils/Pathfinder.h"

namespace {

// ---------------- 文案 ----------------
constexpr const char* kTitleMain = "织星计划";
constexpr const char* kTitleEn   = "W E A V E S T A R";
constexpr const char* kVersion   = "v1.3.4  ALPHA BUILD";
constexpr const char* kStudio    = "JYGame 工作室";
constexpr const char* kCompany   = "织星工业";                                   // 玩家所属公司
constexpr const char* kTagline   = "建厂 · 清障 · 交付 · 前往下一颗星球";

// ---------------- 调色板（工业暗色 + 警示橙） ----------------
const sf::Color kBgTop(9, 13, 18);
const sf::Color kBgBottom(28, 21, 17);
const sf::Color kAccent(255, 152, 0);
const sf::Color kAccentSoft(255, 152, 0, 70);
const sf::Color kPanel(20, 25, 31, 240);
const sf::Color kPanelEdge(78, 92, 108);
const sf::Color kText(226, 232, 238);
const sf::Color kTextDim(138, 150, 163);
const sf::Color kDisabled(92, 100, 108);

// 设置行数（与游戏内暂停面板共用同一份定义）
constexpr int kSettingRows = gset::SETTING_ROW_COUNT;

// ---------------- 小工具 ----------------
sf::String u8(const std::string& s) { return sf::String::fromUtf8(s.begin(), s.end()); }

float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

/// 淡入淡出曲线（0→1→0，带线性缓动）
float fadeEnvelope(float t, float in, float hold, float out, float total) {
    if (t < in) return clamp01(t / std::max(0.001f, in));
    if (t < in + hold) return 1.f;
    if (t < total) return clamp01((total - t) / std::max(0.001f, out));
    return 0.f;
}

} // namespace

// =====================================================================
// 构造
// =====================================================================
EntrySystem::EntrySystem() {
    createWindow();
    loadFont();

    // ---- 存档：先迁移旧版单文件存档，再扫描 10 个槽位 ----
    migrateLegacySave();

    // 主菜单：新手教程与普通关卡是两个完全独立的模式入口，平级并列、互不嵌套。
    // 「载入存档」进入 10 槽位界面（读取 / 删除都在那里完成，且都有二次确认）；
    // 新开一局不再弹"会覆盖存档"的确认框——手动保存模式下新局不会碰任何存档。
    menu = {
        {"新手教程", "独立的引导关卡 · 织女星带你分步上手（不影响存档）", true},
        {"普通关卡", "新开一局 · 手动保存（F5），不会自动覆盖任何存档", true},
        {"载入存档", "", true},
        {"设置", "调整显示模式与交互选项", true},
        {"关于本作", "版本 / 技术栈 / 制作信息", true},
        {"退出游戏", "返回桌面", true},
    };
    refreshSlots();   // 扫描槽位并填好主菜单第 3 项的摘要
    for (size_t i = 0; i < menu.size(); ++i)
        if (menu[i].enabled) { menuFocus = static_cast<int>(i); break; }

    initBackdrop();
}

// ---------------------------------------------------------------------
// 存档槽位（载入存档）
//
// 手动保存 + 10 槽位：入口处只能"读取 / 删除"，创建与覆盖在游戏内完成
// （暂停面板「保存存档」或 F5），因此这里不会主动写盘。
// ---------------------------------------------------------------------
void EntrySystem::refreshSlots() {
    for (int i = 0; i < SAVE_SLOT_COUNT; ++i)
        slotInfos[static_cast<size_t>(i)] = querySlot(i);
    const int newest = newestSlot();
    hasSaveFile = (newest >= 0);
    if (hasSaveFile) {
        const SaveSlotInfo& s = slotInfos[static_cast<size_t>(newest)];
        saveSummary = "槽位 " + std::to_string(newest + 1) + " · " + s.savedAt;
    } else {
        saveSummary.clear();
    }
    if (menu.size() > 2) {   // 主菜单「载入存档」的提示随存档变化
        menu[2].hint = hasSaveFile ? ("10 个独立槽位 · 最近：" + saveSummary)
                                   : "10 个槽位都还是空的（游戏内按 F5 保存）";
    }
}

void EntrySystem::openSlots() {
    refreshSlots();
    slotFocus = 0;
    slotHover = -1;
    // 默认把焦点放在最近一次保存的槽位上，方便直接载入
    const int newest = newestSlot();
    if (newest >= 0) slotFocus = newest;
    slotNotice.clear();
    slotNoticeTimer = 0.f;
    state = State::Slots;
    stateTime = 0.f;
}

void EntrySystem::closeSlots() {
    state = State::Title;
    stateTime = 0.f;
}

std::array<sf::FloatRect, SAVE_SLOT_COUNT> EntrySystem::slotTileRects() const {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float gap = 12.f;
    const float cw = std::min(400.f, (w - 120.f - gap) / 2.f);
    const float ch = 74.f;
    const float x0 = (w - (2.f * cw + gap)) / 2.f;
    const float y0 = h * 0.24f;
    std::array<sf::FloatRect, SAVE_SLOT_COUNT> r{};
    for (int i = 0; i < SAVE_SLOT_COUNT; ++i) {
        const float col = static_cast<float>(i % 2);
        const float row = static_cast<float>(i / 2);
        r[static_cast<size_t>(i)] = {x0 + col * (cw + gap), y0 + row * (ch + gap), cw, ch};
    }
    return r;
}

std::array<sf::FloatRect, 3> EntrySystem::slotButtonRects() const {
    const auto tiles = slotTileRects();
    const sf::FloatRect& lastRow = tiles[static_cast<size_t>(SAVE_SLOT_COUNT - 2)];
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float bw = 200.f, bh = 42.f, gap = 18.f;
    const float x0 = (w - (3.f * bw + 2.f * gap)) / 2.f;
    const float y0 = std::min(lastRow.top + lastRow.height + 20.f, h - 88.f);
    std::array<sf::FloatRect, 3> r{};
    for (int i = 0; i < 3; ++i)
        r[static_cast<size_t>(i)] = {x0 + static_cast<float>(i) * (bw + gap), y0, bw, bh};
    return r;
}

void EntrySystem::activateFocusedSlot() {
    const SaveSlotInfo& s = slotInfos[static_cast<size_t>(slotFocus)];
    if (!s.used) {                       // 空槽位没有可读内容，给个明确提示
        slotNotice = "槽位 " + std::string(slotFocus + 1 < 10 ? "0" : "") +
                     std::to_string(slotFocus + 1) + " 是空的 · 在游戏内按 F5 可以保存到这里";
        slotNoticeTimer = 3.f;
        return;
    }
    chosenSlotIndex = slotFocus;
    dialog = Dialog::ConfirmLoad;        // 载入前先确认（防止误点覆盖当前进度）
}

void EntrySystem::deleteFocusedSlot() {
    if (!slotInfos[static_cast<size_t>(slotFocus)].used) {
        slotNotice = "槽位 " + std::string(slotFocus + 1 < 10 ? "0" : "") +
                     std::to_string(slotFocus + 1) + " 是空的，无需删除";
        slotNoticeTimer = 3.f;
        return;
    }
    dialog = Dialog::ConfirmDelete;      // 删除不可逆 → 必须确认
}

void EntrySystem::handleSlotsKey(const sf::Event::KeyEvent& key) {
    using K = sf::Keyboard;
    switch (key.code) {
        case K::Up: case K::W:
            slotFocus = (slotFocus + SAVE_SLOT_COUNT - 2) % SAVE_SLOT_COUNT;
            break;
        case K::Down: case K::S:
            slotFocus = (slotFocus + 2) % SAVE_SLOT_COUNT;
            break;
        case K::Left: case K::A:
            slotFocus = (slotFocus + SAVE_SLOT_COUNT - 1) % SAVE_SLOT_COUNT;
            break;
        case K::Right: case K::D:
            slotFocus = (slotFocus + 1) % SAVE_SLOT_COUNT;
            break;
        case K::Enter: case K::Space: activateFocusedSlot(); break;
        case K::Delete: deleteFocusedSlot(); break;
        case K::Escape: closeSlots(); break;      // 返回主菜单
        default: break;
    }
}

void EntrySystem::handleSlotsClick(float x, float y) {
    const sf::Vector2f m(x, y);
    const auto tiles = slotTileRects();
    for (int i = 0; i < SAVE_SLOT_COUNT; ++i) {
        if (!tiles[static_cast<size_t>(i)].contains(m)) continue;
        if (slotFocus == i) activateFocusedSlot();   // 先点选中，再点一次 = 执行
        else slotFocus = i;
        return;
    }
    const auto btns = slotButtonRects();
    if (btns[0].contains(m)) { activateFocusedSlot(); return; }   // 读取该槽位
    if (btns[1].contains(m)) { deleteFocusedSlot(); return; }     // 删除该槽位
    if (btns[2].contains(m)) { closeSlots(); return; }            // 返回
}

// =====================================================================
// 窗口与字体
// =====================================================================
void EntrySystem::createWindow() {
    // 显示模式策略统一在 Settings 层（全屏=无边框窗口，不用独占全屏，避免闪屏黑屏/鼠标漂移）
    gset::applyToWindow(window, "织星计划 · Project Weavestar");
    gset::applyFrameMode(window);   // 帧率/垂直同步统一策略（不再硬编码 60 帧）
    window.setKeyRepeatEnabled(false);
    window.setMouseCursorVisible(true);
    window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                                          static_cast<float>(window.getSize().x),
                                          static_cast<float>(window.getSize().y))));
}

void EntrySystem::applyDisplayMode() {
    createWindow();   // 切换显示模式即重建窗口，玩家可立即看到效果
}

bool EntrySystem::loadFont() {
    const char* candidates[] = {
        "assets/fonts/msyh.ttc",           "assets/fonts/simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",  "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",  "C:\\Windows\\Fonts\\simsun.ttf",
    };
    for (const char* f : candidates)
        if (font.loadFromFile(f)) { fontLoaded = true; return true; }
    return false;
}

// =====================================================================
// 背景（工业氛围：渐变 + 滚动网格 + 齿轮 + 厂房剪影 + 上升火星）
// =====================================================================
void EntrySystem::initBackdrop() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    // ---- 火星粒子 ----
    std::uniform_real_distribution<float> dx(0.f, w), dy(0.f, h);
    std::uniform_real_distribution<float> ds(14.f, 52.f);   // 上升速度(px/s)
    std::uniform_real_distribution<float> dr(0.8f, 2.4f);   // 半径
    embers.clear(); emberSpeed.clear(); emberRadius.clear();
    for (int i = 0; i < 80; ++i) {
        embers.push_back({dx(rng), dy(rng)});
        emberSpeed.push_back(ds(rng));
        emberRadius.push_back(dr(rng));
    }

    // ---- 远景厂房剪影（生成两倍宽度，便于无缝横向滚动） ----
    skylineWidth = w * 2.f;
    skyline.clear();
    std::uniform_real_distribution<float> bw(46.f, 130.f);
    std::uniform_real_distribution<float> bh(38.f, 150.f);
    std::uniform_int_distribution<int> bc(0, 3);
    for (float x = 0.f; x < skylineWidth;) {
        Silo s;
        s.w = bw(rng);
        s.h = bh(rng);
        s.chimneys = bc(rng);
        s.x = x;
        skyline.push_back(s);
        x += s.w + 10.f;
    }
}

void EntrySystem::drawGear(float cx, float cy, float radius, int teeth, float rotation,
                           sf::Color color, float thickness) {
    sf::CircleShape ring(radius);
    ring.setOrigin(radius, radius);
    ring.setPosition(cx, cy);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(thickness);
    ring.setOutlineColor(color);
    window.draw(ring);

    sf::RectangleShape tooth({thickness * 1.8f, radius * 0.26f});
    tooth.setOrigin(tooth.getSize().x / 2.f, -radius + thickness * 0.4f);
    tooth.setFillColor(color);
    for (int i = 0; i < teeth; ++i) {
        tooth.setPosition(cx, cy);
        tooth.setRotation(rotation + 360.f * static_cast<float>(i) / static_cast<float>(teeth));
        window.draw(tooth);
    }

    const float hr = radius * 0.24f;
    sf::CircleShape hub(hr);
    hub.setOrigin(hr, hr);
    hub.setPosition(cx, cy);
    hub.setFillColor(sf::Color::Transparent);
    hub.setOutlineThickness(thickness);
    hub.setOutlineColor(color);
    window.draw(hub);
}

void EntrySystem::drawBackdrop() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    // ---- 底色渐变 ----
    sf::VertexArray grad(sf::Quads, 4);
    grad[0] = sf::Vertex({0.f, 0.f}, kBgTop);
    grad[1] = sf::Vertex({w, 0.f}, kBgTop);
    grad[2] = sf::Vertex({w, h}, kBgBottom);
    grad[3] = sf::Vertex({0.f, h}, kBgBottom);
    window.draw(grad);

    // ---- 中央暖色光晕 ----
    for (int i = 5; i >= 1; --i) {
        const float r = 90.f + static_cast<float>(i) * 60.f;
        sf::CircleShape glow(r);
        glow.setOrigin(r, r);
        glow.setPosition(w * 0.5f, h * 0.42f);
        glow.setFillColor(sf::Color(255, 140, 40, static_cast<sf::Uint8>(4 + i)));
        window.draw(glow);
    }

    // ---- 滚动蓝图网格 ----
    const float cell = 56.f;
    const float ox = std::fmod(globalTime * 7.f, cell);
    const float oy = std::fmod(globalTime * 3.5f, cell);
    sf::VertexArray grid(sf::Lines);
    const sf::Color gridCol(96, 130, 160, 20);
    for (float x = -cell + ox; x < w; x += cell) {
        grid.append(sf::Vertex({x, 0.f}, gridCol));
        grid.append(sf::Vertex({x, h}, gridCol));
    }
    for (float y = -cell + oy; y < h; y += cell) {
        grid.append(sf::Vertex({0.f, y}, gridCol));
        grid.append(sf::Vertex({w, y}, gridCol));
    }
    window.draw(grid);

    // ---- 缓慢旋转的齿轮（工业氛围） ----
    drawGear(w * 0.13f, h * 0.24f, 74.f, 12, globalTime * 9.f, sf::Color(120, 150, 180, 26), 3.f);
    drawGear(w * 0.88f, h * 0.72f, 108.f, 16, -globalTime * 6.f, sf::Color(120, 150, 180, 20), 4.f);

    // ---- 远景厂房剪影 + 烟雾 ----
    const float off = std::fmod(globalTime * 5.f, skylineWidth);
    for (int pass = 0; pass < 2; ++pass) {
        const float shift = (pass == 0 ? -off : skylineWidth - off);
        for (const auto& s : skyline) {
            const float x = s.x + shift;
            if (x + s.w < -20.f || x > w + 20.f) continue;
            sf::RectangleShape body({s.w, s.h});
            body.setPosition(x, h - s.h);
            body.setFillColor(sf::Color(10, 14, 20));
            window.draw(body);
            for (int c = 0; c < s.chimneys; ++c) {
                const float cw = s.w * 0.13f;
                const float cx = x + s.w * (0.25f + 0.22f * static_cast<float>(c));
                sf::RectangleShape ch({cw, s.h * 0.45f});
                ch.setPosition(cx, h - s.h - s.h * 0.45f);
                ch.setFillColor(sf::Color(10, 14, 20));
                window.draw(ch);
                for (int k = 0; k < 3; ++k) {
                    const float t = std::fmod(globalTime * 0.30f +
                                                  static_cast<float>(k) / 3.f +
                                                  static_cast<float>(c) * 0.13f, 1.f);
                    const float rr = 5.f + t * 16.f;
                    sf::CircleShape smoke(rr);
                    smoke.setOrigin(rr, rr);
                    smoke.setPosition(cx + cw * 0.5f + t * 10.f, h - s.h * 1.45f - t * 70.f);
                    smoke.setFillColor(sf::Color(150, 160, 175,
                                                 static_cast<sf::Uint8>((1.f - t) * 26.f)));
                    window.draw(smoke);
                }
            }
        }
    }

    // ---- 上升火星（位置在 update() 中推进） ----
    for (size_t i = 0; i < embers.size(); ++i) {
        const float r = emberRadius[i];
        sf::CircleShape e(r);
        e.setOrigin(r, r);
        e.setPosition(embers[i]);
        const float life = clamp01(1.f - embers[i].y / h);
        e.setFillColor(sf::Color(255, 172, 64, static_cast<sf::Uint8>(40.f + life * 130.f)));
        window.draw(e);
    }
}

// =====================================================================
// 文本 / 按钮绘制
// =====================================================================
void EntrySystem::drawText(const std::string& s, float x, float y, unsigned size,
                           sf::Color color, int align, bool bold, float) {
    if (!fontLoaded) return;
    sf::Text t(u8(s), font, size);
    t.setFillColor(color);
    if (bold) t.setStyle(sf::Text::Bold);
    const sf::FloatRect b = t.getLocalBounds();
    float ox = 0.f;
    if (align == 1) ox = -b.left - b.width / 2.f;        // 水平居中
    else if (align == 2) ox = -b.left - b.width;         // 右对齐
    t.setPosition(std::floor(x + ox), std::floor(y - b.top - b.height / 2.f)); // y=垂直中心
    window.draw(t);
}

void EntrySystem::drawButton(const sf::FloatRect& r, const std::string& text,
                             const std::string& hint, bool active, bool enabled) {
    sf::RectangleShape body({r.width, r.height});
    body.setPosition(r.left, r.top);
    if (!enabled)    body.setFillColor(sf::Color(22, 26, 31, 200));
    else if (active) body.setFillColor(sf::Color(48, 37, 24, 240));
    else             body.setFillColor(sf::Color(26, 32, 39, 225));
    window.draw(body);

    const sf::Color edge = !enabled ? sf::Color(58, 64, 70)
                                    : (active ? kAccent : sf::Color(70, 82, 96));
    sf::RectangleShape border({r.width, r.height});
    border.setPosition(r.left, r.top);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(active ? 2.f : 1.f);
    border.setOutlineColor(edge);
    window.draw(border);

    if (enabled) {   // 左侧强调条
        sf::RectangleShape bar({3.f, r.height - 12.f});
        bar.setPosition(r.left + 6.f, r.top + 6.f);
        bar.setFillColor(active ? kAccent : sf::Color(70, 82, 96));
        window.draw(bar);
    }

    const sf::Color textCol = !enabled ? kDisabled
                                       : (active ? sf::Color(255, 226, 178) : kText);
    drawText(text, r.left + 26.f, r.top + r.height / 2.f, 22, textCol, 0);

    if (active && !hint.empty())   // 提示文字（仅当前聚焦项显示）
        drawText(hint, r.left + 26.f, r.top + r.height + 16.f, 14,
                 enabled ? kAccentSoft : sf::Color(150, 110, 90), 0);
}

void EntrySystem::drawSettingRow(const sf::FloatRect& r, const std::string& label,
                                 const std::string& value, bool active) {
    sf::RectangleShape body({r.width, r.height});
    body.setPosition(r.left, r.top);
    body.setFillColor(active ? sf::Color(42, 35, 24, 235) : sf::Color(24, 29, 35, 210));
    window.draw(body);
    sf::RectangleShape border({r.width, r.height});
    border.setPosition(r.left, r.top);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(1.f);
    border.setOutlineColor(active ? kAccent : sf::Color(64, 74, 86));
    window.draw(border);

    drawText(label, r.left + 22.f, r.top + r.height / 2.f, 19,
             active ? sf::Color(255, 226, 178) : kText, 0);
    drawText(value, r.left + r.width - 56.f, r.top + r.height / 2.f, 19, kAccent, 2);
    if (active) {
        drawText("<", r.left + r.width - 66.f, r.top + r.height / 2.f - 1.f, 20,
                 sf::Color(200, 210, 220), 2);
        drawText(">", r.left + r.width - 20.f, r.top + r.height / 2.f - 1.f, 20,
                 sf::Color(200, 210, 220), 0);
    }
}

sf::Vector2f EntrySystem::mousePos() const {
    const auto p = sf::Mouse::getPosition(window);
    return {static_cast<float>(p.x), static_cast<float>(p.y)};
}

// =====================================================================
// 主循环
// =====================================================================
EntryAction EntrySystem::run() {
    sf::Clock clock;
    while (window.isOpen() && state != State::Finished) {
        const float dt = std::min(clock.restart().asSeconds(), 0.05f);
        globalTime += dt;
        stateTime += dt;
        handleEvents();
        if (!window.isOpen()) { result = EntryAction::Quit; break; }
        update(dt);
        render();
    }
    if (window.isOpen()) window.close();
    return result;
}

// =====================================================================
// 事件
// =====================================================================
void EntrySystem::handleEvents() {
    sf::Event e;
    while (window.pollEvent(e)) {
        if (e.type == sf::Event::Closed) { window.close(); return; }
        if (e.type == sf::Event::Resized && e.size.width > 0 && e.size.height > 0) {
            window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                                                  static_cast<float>(e.size.width),
                                                  static_cast<float>(e.size.height))));
            initBackdrop();
            continue;
        }
        if (e.type == sf::Event::KeyPressed) {
            onKeyPressed(e.key);
        } else if (e.type == sf::Event::MouseButtonPressed &&
                   e.mouseButton.button == sf::Mouse::Left) {
            onMouseClick(static_cast<float>(e.mouseButton.x),
                         static_cast<float>(e.mouseButton.y));
        }
    }
}

void EntrySystem::onKeyPressed(const sf::Event::KeyEvent& key) {
    using K = sf::Keyboard;

    // ---- 对话框优先 ----
    if (dialog != Dialog::None) {
        if (key.code == K::Escape) {
            dialog = Dialog::None;
        } else if (dialog == Dialog::About) {
            if (key.code == K::Enter || key.code == K::Space) dialog = Dialog::None;
        } else if (key.code == K::Enter || key.code == K::Space) {
            activateDialogOption(0);
        } else if (key.code == K::Right || key.code == K::Left || key.code == K::Tab) {
            activateDialogOption(1);
        }
        return;
    }

    switch (state) {
        case State::Splash:                       // 任意键跳过启动动画
            state = State::Title;
            stateTime = 0.f;
            return;
        case State::Loading:
            if (loadReady) loadReadyTimer = 100.f;  // 加载完成 → 立即进场
            return;
        case State::Title:
            switch (key.code) {
                case K::Up: case K::W:
                    menuFocus = (menuFocus + static_cast<int>(menu.size()) - 1) %
                                static_cast<int>(menu.size());
                    break;
                case K::Down: case K::S:
                    menuFocus = (menuFocus + 1) % static_cast<int>(menu.size());
                    break;
                case K::Enter: case K::Space: activateMenu(menuFocus); break;
                case K::T: activateMenu(0); break;   // 新手教程
                case K::N: activateMenu(1); break;   // 普通关卡
                case K::L: case K::C: activateMenu(2); break;   // 载入存档（C 兼容旧习惯）
                case K::Escape: dialog = Dialog::ConfirmQuit; break;
                default: break;
            }
            return;
        case State::Slots:
            handleSlotsKey(key);
            return;
        case State::Settings:
            switch (key.code) {
                case K::Up: case K::W:
                    settingRow = (settingRow + kSettingRows - 1) % kSettingRows;
                    break;
                case K::Down: case K::S:
                    settingRow = (settingRow + 1) % kSettingRows;
                    break;
                case K::Left: case K::A: cycleSettingRow(settingRow, -1); break;
                case K::Right: case K::D: cycleSettingRow(settingRow, 1); break;
                case K::Enter: case K::Space: activateSettingRow(settingRow); break;
                case K::Escape: closeSettings(); break;
                default: break;
            }
            return;
        default: return;
    }
}

void EntrySystem::onMouseClick(float x, float y) {
    const sf::Vector2f m(x, y);

    // ---- 对话框优先 ----
    if (dialog != Dialog::None) {
        const int n = dialogOptionCount();
        for (int i = 0; i < n; ++i)
            if (dialogOptionRect(i).contains(m)) { activateDialogOption(i); return; }
        return;
    }

    if (state == State::Splash) { state = State::Title; stateTime = 0.f; return; }
    if (state == State::Loading) {
        if (loadReady) loadReadyTimer = 100.f;
        return;
    }

    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    if (state == State::Slots) {
        handleSlotsClick(x, y);
        return;
    }

    if (state == State::Title) {
        const float bw = std::min(460.f, w - 120.f);
        const float bh = 56.f, gap = 14.f;
        const float total = static_cast<float>(menu.size()) * bh +
                            static_cast<float>(menu.size() - 1) * gap;
        const float x0 = (w - bw) / 2.f;
        const float y0 = std::max(h * 0.44f, (h - total) * 0.52f);
        for (size_t i = 0; i < menu.size(); ++i) {
            const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
            if (r.contains(m)) {
                menuFocus = static_cast<int>(i);
                activateMenu(static_cast<int>(i));
                return;
            }
        }
        return;
    }

    if (state == State::Settings) {
        const float bw = std::min(620.f, w - 120.f);
        const float bh = 50.f, gap = 10.f;
        const float total = kSettingRows * bh + (kSettingRows - 1) * gap;
        const float x0 = (w - bw) / 2.f;
        const float y0 = std::max(h * 0.30f, (h - total) * 0.5f);
        for (int i = 0; i < kSettingRows; ++i) {
            const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
            if (r.contains(m)) {
                if (settingRow == i) activateSettingRow(i);  // 再次点击 = 切换该行数值
                else settingRow = i;
                return;
            }
        }
        return;
    }
}

// =====================================================================
// 更新
// =====================================================================
void EntrySystem::update(float dt) {
    // ---- 背景火星上升 ----
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    for (size_t i = 0; i < embers.size(); ++i) {
        embers[i].y -= emberSpeed[i] * dt;
        embers[i].x += std::sin(globalTime * 1.4f + static_cast<float>(i)) * 12.f * dt;
        if (embers[i].y < -4.f) {
            embers[i].y = h + 4.f;
            embers[i].x = std::uniform_real_distribution<float>(0.f, std::max(1.f, w))(rng);
        }
    }

    if (state == State::Splash && stateTime > 2.6f) {
        state = State::Title;
        stateTime = 0.f;
    }

    if (slotNoticeTimer > 0.f) slotNoticeTimer -= dt;   // 槽位界面内提示自动消失

    if (state == State::Loading) {
        advanceLoading(dt);
        if (loadReady) {
            loadReadyTimer += dt;
            if (loadReadyTimer > 1.4f) fadeOut = std::min(1.f, fadeOut + dt * 2.2f);
            if (fadeOut >= 1.f) {
                result = pendingAction;
                state = State::Finished;
            }
        }
    }
}

// =====================================================================
// 渲染
// =====================================================================
void EntrySystem::render() {
    // 显示模式变更在事件循环之外生效，避免在 pollEvent 过程中重建窗口
    if (pendingDisplayApply) {
        pendingDisplayApply = false;
        applyDisplayMode();
    }
    window.clear(kBgTop);
    switch (state) {
        case State::Splash:   drawSplash(); break;
        case State::Title:    drawBackdrop(); drawTitle(); break;
        case State::Settings: drawBackdrop(); drawSettings(); break;
        case State::Slots:    drawBackdrop(); drawSlots(); break;
        case State::Loading:  drawBackdrop(); drawLoading(); break;
        case State::Finished: break;
    }
    if (dialog != Dialog::None) drawDialog();
    drawFadeOverlay();
    window.display();
}

void EntrySystem::drawFadeOverlay() {
    if (fadeOut <= 0.f) return;
    sf::RectangleShape f({static_cast<float>(window.getSize().x),
                          static_cast<float>(window.getSize().y)});
    f.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(clamp01(fadeOut) * 255.f)));
    window.draw(f);
}

// ---------------------------------------------------------------------
// 启动动画（工作室 logo + 淡入淡出）
// ---------------------------------------------------------------------
void EntrySystem::drawSplash() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float a = fadeEnvelope(stateTime, 0.7f, 1.2f, 0.7f, 2.6f);

    sf::RectangleShape bg({w, h});
    bg.setFillColor(sf::Color(6, 8, 11));
    window.draw(bg);

    auto col = [a](int r, int g, int b) {
        return sf::Color(static_cast<sf::Uint8>(r), static_cast<sf::Uint8>(g),
                         static_cast<sf::Uint8>(b), static_cast<sf::Uint8>(a * 255.f));
    };
    drawGear(w / 2.f, h * 0.42f, 46.f, 10, globalTime * 40.f, col(255, 152, 0), 3.f);
    drawText(kStudio, w / 2.f, h * 0.57f, 34, col(232, 238, 244), 1);
    drawText("P R E S E N T S", w / 2.f, h * 0.64f, 15, col(150, 162, 175), 1);

    const float lineW = 220.f * a;
    sf::RectangleShape line({lineW, 2.f});
    line.setPosition(w / 2.f - lineW / 2.f, h * 0.73f);
    line.setFillColor(col(255, 152, 0));
    window.draw(line);

    drawText("按任意键跳过", w / 2.f, h - 40.f, 13, col(96, 106, 116), 1);
}

// ---------------------------------------------------------------------
// 标题界面 / 主菜单
// ---------------------------------------------------------------------
void EntrySystem::drawTitle() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float intro = clamp01(stateTime / 0.6f);   // 入场淡入

    // ---- 标题 ----
    const float titleY = h * 0.22f;
    drawText(kTitleEn, w / 2.f, titleY - 46.f, 20,
             sf::Color(255, 152, 0, static_cast<sf::Uint8>(200 * intro)), 1);

    for (int dx = -2; dx <= 2; dx += 2)              // 描边阴影（厚重工业感）
        for (int dy = -2; dy <= 2; dy += 2)
            drawText(kTitleMain, w / 2.f + static_cast<float>(dx),
                     titleY + static_cast<float>(dy) + 6.f, 62,
                     sf::Color(0, 0, 0, static_cast<sf::Uint8>(150 * intro)), 1, true);
    drawText(kTitleMain, w / 2.f, titleY + 6.f, 62,
             sf::Color(240, 246, 252, static_cast<sf::Uint8>(255 * intro)), 1, true);

    const float lineW = 420.f * intro;
    sf::RectangleShape line({lineW, 3.f});
    line.setPosition(w / 2.f - lineW / 2.f, titleY + 48.f);
    line.setFillColor(sf::Color(255, 152, 0, static_cast<sf::Uint8>(230 * intro)));
    window.draw(line);
    drawText(kTagline, w / 2.f, titleY + 74.f, 17,
             sf::Color(150, 162, 175, static_cast<sf::Uint8>(255 * intro)), 1);

    // ---- 菜单（键盘焦点为主，鼠标悬停跟随） ----
    const float bw = std::min(460.f, w - 120.f);
    const float bh = 56.f, gap = 14.f;
    const float total = static_cast<float>(menu.size()) * bh +
                        static_cast<float>(menu.size() - 1) * gap;
    const float x0 = (w - bw) / 2.f;
    const float y0 = std::max(h * 0.44f, (h - total) * 0.52f);

    const sf::Vector2f mp = mousePos();
    const bool mouseMoved = !mousePosValid ||
                            mp.x != lastMousePos.x || mp.y != lastMousePos.y;
    lastMousePos = mp;
    mousePosValid = true;
    menuHover = -1;
    for (size_t i = 0; i < menu.size(); ++i) {
        const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
        if (r.contains(mp)) {
            menuHover = static_cast<int>(i);
            // 只有鼠标真正移动时才接管焦点（否则指针停住会每帧抢走键盘焦点）
            if (mouseMoved && menu[i].enabled) menuFocus = static_cast<int>(i);
        }
    }

    for (size_t i = 0; i < menu.size(); ++i) {
        const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
        const bool active = (static_cast<int>(i) == menuFocus) || (static_cast<int>(i) == menuHover);
        drawButton(r, menu[i].label, menu[i].hint, active, menu[i].enabled);
    }

    // ---- 底部信息栏 ----
    sf::RectangleShape bar({w, 34.f});
    bar.setPosition(0.f, h - 34.f);
    bar.setFillColor(sf::Color(10, 13, 18, 215));
    window.draw(bar);
    drawText(std::string(kVersion) + "   |   " +
                 (hasSaveFile ? ("最近存档 " + saveSummary) : "10 个槽位均为空"),
             16.f, h - 17.f, 13, kTextDim, 0);
    drawText("↑↓ 选择    Enter 确认    T 教程    N 普通关卡    L 载入存档    Esc 退出",
             w - 16.f, h - 17.f, 13, kTextDim, 2);
}

// ---------------------------------------------------------------------
// 存档槽位界面（WorldBox 式 10 个独立槽位）
// ---------------------------------------------------------------------
void EntrySystem::drawSlots() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float intro = clamp01(stateTime / 0.35f);

    drawText("载入存档", w / 2.f, h * 0.135f, 40,
             sf::Color(240, 246, 252, static_cast<sf::Uint8>(255 * intro)), 1, true);
    drawText("L O A D   G A M E", w / 2.f, h * 0.135f + 34.f, 14,
             sf::Color(255, 152, 0, static_cast<sf::Uint8>(200 * intro)), 1);
    drawText("共 10 个独立槽位 · 手动保存：只有主动保存过的进度才会留在这里",
             w / 2.f, h * 0.135f + 58.f, 13, kTextDim, 1);

    const sf::Vector2f mp = mousePos();
    const bool mouseMoved = !mousePosValid || mp.x != lastMousePos.x || mp.y != lastMousePos.y;
    lastMousePos = mp;
    mousePosValid = true;

    const auto tiles = slotTileRects();
    slotHover = -1;
    for (int i = 0; i < SAVE_SLOT_COUNT; ++i) {
        const sf::FloatRect& r = tiles[static_cast<size_t>(i)];
        const SaveSlotInfo& s = slotInfos[static_cast<size_t>(i)];
        if (r.contains(mp)) {
            slotHover = i;
            if (mouseMoved) slotFocus = i;   // 悬停即选中（停住不抢键盘焦点）
        }
        const bool lit = (i == slotFocus) || (i == slotHover);

        sf::RectangleShape body({r.width, r.height});
        body.setPosition(r.left, r.top);
        if (s.used) body.setFillColor(lit ? sf::Color(38, 44, 52, 246) : kPanel);
        else        body.setFillColor(lit ? sf::Color(30, 35, 42, 236)
                                          : sf::Color(18, 22, 28, 220));
        body.setOutlineThickness(lit ? 2.f : 1.f);
        body.setOutlineColor(lit ? kAccent : (s.used ? kPanelEdge : sf::Color(50, 58, 68)));
        window.draw(body);

        const std::string num = "槽位 " + std::string(i + 1 < 10 ? "0" : "") +
                                std::to_string(i + 1);
        drawText(num, r.left + 16.f, r.top + 12.f, 20, s.used ? kText : kTextDim, 0);
        const float rightX = r.left + r.width - 16.f;
        auto valueRight = [&](const std::string& s2, float y, unsigned size, sf::Color c) {
            sf::Text t;
            t.setFont(font);
            t.setCharacterSize(size);
            t.setString(u8(s2));
            t.setFillColor(c);
            t.setPosition(rightX - t.getLocalBounds().width, y);
            window.draw(t);
        };
        if (!s.used) {
            valueRight("空槽位", r.top + 16.f, 14, kDisabled);
            drawText("暂无进度 · 游戏内按 F5 可保存到这里", r.left + 16.f, r.top + 44.f,
                     12, kTextDim, 0);
        } else if (s.corrupt) {
            valueRight("损坏", r.top + 16.f, 14, sf::Color(214, 110, 90));
            drawText("存档文件无法解析 · 可删除后重新保存", r.left + 16.f, r.top + 44.f,
                     12, sf::Color(200, 120, 100), 0);
        } else {
            valueRight("已占用", r.top + 16.f, 14, sf::Color(120, 200, 140));
            drawText(s.savedAt, r.left + 16.f, r.top + 40.f, 13, kText, 0);
            drawText(s.summary, r.left + 16.f, r.top + 58.f, 12, kTextDim, 0);
        }
    }

    // ---- 底部按钮：读取 / 删除 / 返回 ----
    const bool used = slotInfos[static_cast<size_t>(slotFocus)].used;
    static const char* const kLabels[3] = {"读取该槽位", "删除该槽位", "返回"};
    const bool enabled[3] = {used, used, true};
    const auto btns = slotButtonRects();
    for (int i = 0; i < 3; ++i) {
        const sf::FloatRect& b = btns[static_cast<size_t>(i)];
        const bool on = enabled[i] && b.contains(mp);
        sf::RectangleShape body({b.width, b.height});
        body.setPosition(b.left, b.top);
        body.setFillColor(!enabled[i] ? sf::Color(20, 24, 30, 200)
                                      : (on ? sf::Color(38, 44, 52, 246) : kPanel));
        body.setOutlineThickness(on ? 2.f : 1.f);
        body.setOutlineColor(on ? kAccent : kPanelEdge);
        window.draw(body);
        drawText(kLabels[i], b.left + b.width / 2.f, b.top + b.height / 2.f - 11.f, 16,
                 !enabled[i] ? kDisabled : (on ? kAccent : kText), 1);
    }

    // ---- 提示行 ----
    if (slotNoticeTimer > 0.f && !slotNotice.empty()) {
        drawText(slotNotice, w / 2.f, h - 74.f, 14,
                 sf::Color(255, 152, 0, static_cast<sf::Uint8>(200 * clamp01(slotNoticeTimer))),
                 1);
    }
    drawText("↑↓←→ 选择槽位    Enter 读取    Delete 删除    Esc 返回",
             w / 2.f, h - 34.f, 13, kTextDim, 1);
}

// ---------------------------------------------------------------------
// 设置界面
// ---------------------------------------------------------------------
void EntrySystem::drawSettings() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    drawText("设置", w / 2.f, h * 0.13f, 40, kText, 1, true);
    drawText("O P T I O N S", w / 2.f, h * 0.13f + 34.f, 14, kAccentSoft, 1);

    const float bw = std::min(620.f, w - 120.f);
    const float bh = 50.f, gap = 10.f;
    const float total = kSettingRows * bh + (kSettingRows - 1) * gap;
    const float x0 = (w - bw) / 2.f;
    const float y0 = std::max(h * 0.30f, (h - total) * 0.5f);

    const sf::Vector2f mp = mousePos();
    const bool mouseMoved = !mousePosValid ||
                            mp.x != lastMousePos.x || mp.y != lastMousePos.y;
    lastMousePos = mp;
    mousePosValid = true;
    settingHover = -1;
    for (int i = 0; i < kSettingRows; ++i) {
        const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
        if (r.contains(mp)) {
            settingHover = i;
            // 只有鼠标真正移动时才改键盘焦点：指针停住时 ←→/↑↓ 不再被悬停覆盖
            if (mouseMoved) settingRow = i;
        }
    }

    for (int i = 0; i < kSettingRows; ++i) {
        const sf::FloatRect r(x0, y0 + static_cast<float>(i) * (bh + gap), bw, bh);
        drawSettingRow(r, gset::settingRowLabel(i),
                       gset::settingRowIsAction(i) ? std::string()
                                                   : gset::settingRowValue(i),
                       i == settingRow);
    }

    drawText("↑↓ 选择    ←→ 调整    Enter 确认    Esc 返回", w / 2.f, h - 26.f, 13,
             kTextDim, 1);
}

// ---------------------------------------------------------------------
// 加载界面（含真实进度、步骤清单与迷你地图预览）
// ---------------------------------------------------------------------
void EntrySystem::drawLoading() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    const float contentW = std::min(900.f, w - 80.f);
    const float x0 = (w - contentW) / 2.f;
    const float topY = h * 0.30f;
    const float leftW = contentW * 0.58f;

    // ---- 标题 ----
    drawText("正在生成星球地表", w / 2.f, h * 0.17f, 36, kText, 1, true);
    drawText("PREPARING PLANET  ·  " + std::string(kVersion), w / 2.f, h * 0.17f + 32.f, 14,
             kAccentSoft, 1);

    // ---- 进度条 ----
    const float barH = 16.f;
    sf::RectangleShape barBg({leftW, barH});
    barBg.setPosition(x0, topY);
    barBg.setFillColor(sf::Color(30, 36, 43, 230));
    window.draw(barBg);
    sf::RectangleShape barBorder({leftW, barH});
    barBorder.setPosition(x0, topY);
    barBorder.setFillColor(sf::Color::Transparent);
    barBorder.setOutlineThickness(1.f);
    barBorder.setOutlineColor(kPanelEdge);
    window.draw(barBorder);

    const float fillW = std::max(0.f, leftW - 4.f) * clamp01(loadProgress);
    if (fillW > 0.f) {
        sf::RectangleShape fill({fillW, barH - 4.f});
        fill.setPosition(x0 + 2.f, topY + 2.f);
        fill.setFillColor(kAccent);
        window.draw(fill);
    }
    // 进度条高光线（扫过动画）
    const float sweep = std::fmod(globalTime * 0.7f, 1.f);
    sf::RectangleShape sheen({26.f, barH - 4.f});
    sheen.setPosition(x0 + 2.f + (leftW - 30.f) * sweep, topY + 2.f);
    sheen.setFillColor(sf::Color(255, 220, 160, 40));
    window.draw(sheen);

    char pct[32];
    std::snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(clamp01(loadProgress) * 100.f + 0.5f));
    drawText(pct, x0 + leftW + 16.f, topY + barH / 2.f, 22,
             loadReady ? sf::Color(120, 220, 140) : kAccent, 0, true);

    // ---- 步骤清单 ----
    const float stepY = topY + 50.f;
    for (size_t i = 0; i < loadSteps.size(); ++i) {
        const float y = stepY + static_cast<float>(i) * 30.f;
        const bool done = loadDone[i];
        const bool doing = (!done && static_cast<int>(i) == loadIndex);
        drawText(done ? "[+]" : (doing ? "[>]" : "[ ]"), x0, y, 17,
                 done ? sf::Color(120, 220, 140) : (doing ? kAccent : kDisabled), 0, true);
        drawText(loadSteps[i], x0 + 40.f, y, 17,
                 done ? kText : (doing ? sf::Color(255, 226, 178) : kDisabled), 0);
    }

    // ---- 迷你地图预览（真实路径 + 矿点） ----
    const float mapSize = std::min(contentW * 0.34f, h * 0.42f);
    const float mapX = x0 + contentW - mapSize;
    const float mapY = topY - 6.f;
    sf::RectangleShape mapBg({mapSize, mapSize});
    mapBg.setPosition(mapX, mapY);
    mapBg.setFillColor(sf::Color(14, 20, 16, 235));
    window.draw(mapBg);
    sf::RectangleShape mapBorder({mapSize, mapSize});
    mapBorder.setPosition(mapX, mapY);
    mapBorder.setFillColor(sf::Color::Transparent);
    mapBorder.setOutlineThickness(1.f);
    mapBorder.setOutlineColor(kPanelEdge);
    window.draw(mapBorder);

    const float cell = mapSize / static_cast<float>(cfg::GRID_WIDTH);
    // 网格
    sf::VertexArray gridLines(sf::Lines);
    for (int i = 1; i < 10; ++i) {
        const float gx = mapX + mapSize * static_cast<float>(i) / 10.f;
        const float gy = mapY + mapSize * static_cast<float>(i) / 10.f;
        const sf::Color gc(90, 120, 100, 26);
        gridLines.append(sf::Vertex({gx, mapY}, gc));
        gridLines.append(sf::Vertex({gx, mapY + mapSize}, gc));
        gridLines.append(sf::Vertex({mapX, gy}, gc));
        gridLines.append(sf::Vertex({mapX + mapSize, gy}, gc));
    }
    window.draw(gridLines);

    auto cellQuad = [&](sf::VertexArray& va, int tx, int ty, float size, sf::Color c) {
        const float cx = mapX + (static_cast<float>(tx) + 0.5f) * cell;
        const float cy = mapY + (static_cast<float>(ty) + 0.5f) * cell;
        sf::Vertex v({cx - size / 2.f, cy - size / 2.f}, c);
        va.append(v);
        va.append(sf::Vertex({cx + size / 2.f, cy - size / 2.f}, c));
        va.append(sf::Vertex({cx + size / 2.f, cy + size / 2.f}, c));
        va.append(sf::Vertex({cx - size / 2.f, cy + size / 2.f}, c));
    };

    sf::VertexArray ores(sf::Quads);
    for (const auto& t : previewOres) cellQuad(ores, t.x, t.y, 2.4f, sf::Color(90, 200, 190, 190));
    window.draw(ores);

    sf::VertexArray paths(sf::Quads);
    for (const auto& t : previewPath) cellQuad(paths, t.x, t.y, 1.8f, sf::Color(255, 152, 0, 210));
    window.draw(paths);

    if (!cfg::PATH_POINTS.empty()) {   // 起点/终点标记
        auto marker = [&](sf::Vector2i p, sf::Color c) {
            sf::CircleShape m(3.5f);
            m.setOrigin(3.5f, 3.5f);
            m.setPosition(mapX + (static_cast<float>(p.x) + 0.5f) * cell,
                          mapY + (static_cast<float>(p.y) + 0.5f) * cell);
            m.setFillColor(c);
            window.draw(m);
        };
        marker(cfg::PATH_POINTS.front(), sf::Color(120, 220, 140));
        marker(cfg::PATH_POINTS.back(), sf::Color(230, 80, 80));
    }
    drawText("敌人路径 / 矿点分布", mapX + mapSize / 2.f, mapY + mapSize + 16.f, 13, kTextDim, 1);

    // ---- 完成提示 ----
    if (loadReady) {
        const float a = 0.55f + 0.45f * std::sin(globalTime * 5.f);
        drawText("按任意键 / 点击鼠标  进入游戏", w / 2.f, h - 60.f, 20,
                 sf::Color(120, 220, 140, static_cast<sf::Uint8>(140 + a * 115)), 1, true);
    } else {
        drawText("正在载入，请稍候…", w / 2.f, h - 60.f, 16, kTextDim, 1);
    }
}

// ---------------------------------------------------------------------
// 对话框（确认新游戏 / 确认退出 / 关于本作）
// ---------------------------------------------------------------------
namespace {
/// 对话框面板矩形（绘制与点击命中共用同一套布局参数）
sf::FloatRect dialogPanel(const sf::Vector2u& size, bool about) {
    const float w = static_cast<float>(size.x), h = static_cast<float>(size.y);
    const float pw = std::min(about ? 640.f : 560.f, w - 80.f);
    const float ph = about ? 306.f : 236.f;
    return sf::FloatRect((w - pw) / 2.f, (h - ph) / 2.f, pw, ph);
}
} // namespace

int EntrySystem::dialogOptionCount() const {
    return dialog == Dialog::About ? 1 : 2;
}

std::string EntrySystem::dialogOptionLabel(int i) const {
    switch (dialog) {
        case Dialog::About:         return "关闭";
        case Dialog::ConfirmLoad:   return i == 0 ? "读取并进入" : "取消";
        case Dialog::ConfirmDelete: return i == 0 ? "删除存档" : "取消";
        case Dialog::ConfirmQuit:   return i == 0 ? "退出游戏" : "取消";
        default:                    return "";
    }
}

sf::FloatRect EntrySystem::dialogOptionRect(int i) const {
    const sf::FloatRect p = dialogPanel(window.getSize(), dialog == Dialog::About);
    const int n = std::max(1, dialogOptionCount());
    const float bw = 176.f, bh = 42.f, gap = 24.f;
    const float total = static_cast<float>(n) * bw + static_cast<float>(n - 1) * gap;
    const float bx = p.left + (p.width - total) / 2.f;
    return sf::FloatRect(bx + static_cast<float>(i) * (bw + gap),
                         p.top + p.height - bh - 24.f, bw, bh);
}

void EntrySystem::activateDialogOption(int i) {
    if (dialog == Dialog::About) { dialog = Dialog::None; return; }
    if (dialog == Dialog::ConfirmLoad) {
        dialog = Dialog::None;
        // 选中槽位在 activateFocusedSlot() 里已记入 chosenSlotIndex
        if (i == 0 && chosenSlotIndex >= 0) beginLoading(EntryAction::Continue);
        return;
    }
    if (dialog == Dialog::ConfirmDelete) {
        dialog = Dialog::None;
        if (i == 0) {
            const int slot = slotFocus;
            if (deleteSlot(slot)) {
                slotNotice = "已删除槽位 " + std::string(slot + 1 < 10 ? "0" : "") +
                             std::to_string(slot + 1) + " 号存档";
                slotNoticeTimer = 3.f;
            }
            refreshSlots();
        }
        return;
    }
    if (dialog == Dialog::ConfirmQuit) {
        dialog = Dialog::None;
        if (i == 0) { result = EntryAction::Quit; state = State::Finished; }
    }
}

void EntrySystem::drawDialog() {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 175));
    window.draw(dim);

    const bool about = (dialog == Dialog::About);
    const sf::FloatRect p = dialogPanel(window.getSize(), about);
    sf::RectangleShape panel({p.width, p.height});
    panel.setPosition(p.left, p.top);
    panel.setFillColor(kPanel);
    window.draw(panel);
    sf::RectangleShape border({p.width, p.height});
    border.setPosition(p.left, p.top);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(2.f);
    border.setOutlineColor(kAccent);
    window.draw(border);

    std::string title;
    std::vector<std::string> lines;
    const int slot = std::max(0, std::min(slotFocus, SAVE_SLOT_COUNT - 1));
    const SaveSlotInfo& info = slotInfos[static_cast<size_t>(slot)];
    const std::string num = "槽位 " + std::string(slot + 1 < 10 ? "0" : "") +
                            std::to_string(slot + 1);
    if (dialog == Dialog::ConfirmLoad) {
        title = "载入存档";
        lines = {"将读取 " + num + "（" + info.savedAt + " · " + info.summary + "）。",
                 "该槽位存档不会被修改，之后仍可继续保存。",
                 "确定要载入并进入游戏吗？"};
    } else if (dialog == Dialog::ConfirmDelete) {
        title = "删除存档";
        lines = {num + " 的存档将被永久删除（" + info.savedAt + "）。",
                 "删除后无法找回。",
                 "确定要删除吗？"};
    } else if (dialog == Dialog::ConfirmQuit) {
        title = "退出游戏";
        lines = {"确定要退出《织星计划》吗？",
                 "本作采用手动保存：未主动保存的进度不会留下。",
                 "游戏内按 F5 可选择槽位保存进度。"};
    } else {
        title = "关于本作";
        lines = {"织星计划  ·  " + std::string(kVersion),
                 std::string("身份: ") + kCompany + "外派工程师    随船 AI: 织女星",
                 "玩法: 建厂 → 清障 → 交付指标 → 前往下一颗星球",
                 "技术栈: C++20 / SFML 2.6 / EnTT (ECS 架构)",
                 "存档: saves/slot_01..10.json（手动保存 · 10 槽位）",
                 "制作: " + std::string(kStudio)};
    }

    float y = p.top + 32.f;
    drawText(title, p.left + p.width / 2.f, y, 26, kText, 1, true);
    y += 44.f;
    for (const auto& l : lines) {
        drawText(l, p.left + p.width / 2.f, y, 16, kTextDim, 1);
        y += 26.f;
    }

    const sf::Vector2f mp = mousePos();
    for (int i = 0; i < dialogOptionCount(); ++i) {
        const sf::FloatRect r = dialogOptionRect(i);
        const bool hov = r.contains(mp);
        sf::RectangleShape b({r.width, r.height});
        b.setPosition(r.left, r.top);
        b.setFillColor(hov ? sf::Color(58, 44, 26, 245) : sf::Color(30, 36, 44, 240));
        window.draw(b);
        sf::RectangleShape bd({r.width, r.height});
        bd.setPosition(r.left, r.top);
        bd.setFillColor(sf::Color::Transparent);
        bd.setOutlineThickness(1.f);
        bd.setOutlineColor(hov ? kAccent : kPanelEdge);
        window.draw(bd);
        drawText(dialogOptionLabel(i), r.left + r.width / 2.f, r.top + r.height / 2.f, 18,
                 hov ? sf::Color(255, 226, 178) : kText, 1);
    }
}

// =====================================================================
// 子流程
// =====================================================================
void EntrySystem::activateMenu(int index) {
    if (index < 0 || index >= static_cast<int>(menu.size())) return;
    if (!menu[index].enabled) return;
    switch (index) {
        case 0: requestStart(EntryAction::Tutorial); break;   // 新手教程（独立模式）
        case 1: requestStart(EntryAction::NewGame); break;    // 普通关卡（独立模式）
        case 2: openSlots(); break;                           // 载入存档 → 10 槽位界面
        case 3: openSettings(); break;
        case 4: dialog = Dialog::About; break;
        case 5: dialog = Dialog::ConfirmQuit; break;
        default: break;
    }
}

void EntrySystem::openSettings() {
    state = State::Settings;
    stateTime = 0.f;
    settingRow = 0;
    // 记录当前鼠标位置并标记为"已同步"：进入界面时焦点固定在第 0 行，
    // 只有玩家真正移动鼠标后才让悬停行接管焦点。
    lastMousePos = mousePos();
    mousePosValid = true;
}

void EntrySystem::closeSettings() {
    gset::save();          // 离开设置界面时统一落盘
    state = State::Title;
    stateTime = 0.f;
}

void EntrySystem::cycleSettingRow(int row, int dir) {
    // 取值切换逻辑在 gset 里（与游戏内暂停面板共用同一份，保证行为一致）
    if (gset::cycleSettingRow(row, dir)) {
        pendingDisplayApply = true;   // 显示模式变化：事件循环结束后再重建窗口
        return;
    }
    // 帧率/垂直同步不需要重建窗口，就地套用（否则要等下次重建窗口才生效）
    if (row == gset::SETTING_ROW_FRAME_MODE) gset::applyFrameMode(window);
}

void EntrySystem::activateSettingRow(int row) {
    switch (gset::activateSettingRow(row)) {
        case gset::SettingActivate::DisplayChanged: pendingDisplayApply = true; break;
        case gset::SettingActivate::Changed:
            if (row == gset::SETTING_ROW_FRAME_MODE) gset::applyFrameMode(window);
            break;
        case gset::SettingActivate::Back:           closeSettings(); break;
        default:                                    break;
    }
}

void EntrySystem::requestStart(EntryAction action) {
    // 载入存档：必须先在槽位界面选中一个有效槽位（chosenSlotIndex 由 activateFocusedSlot 记录）
    if (action == EntryAction::Continue) {
        if (chosenSlotIndex < 0 ||
            !slotInfos[static_cast<size_t>(chosenSlotIndex)].used) return;
        beginLoading(action);
        return;
    }
    // 新手教程是完全独立的模式：直接进入，不校验、不触碰任何存档
    // 普通关卡 · 新游戏：手动保存模式下新局不会写盘/覆盖任何槽位，因此无需确认
    beginLoading(action);
}

void EntrySystem::beginLoading(EntryAction action) {
    pendingAction = action;
    loadSteps = {
        "读取引擎配置 assets/config.json",
        "校验本地存档数据",
        "预生成敌人路径 (200 x 200)",
        "生成矿点分布 (预览)",
        "初始化界面与渲染管线",
    };
    loadDone.assign(loadSteps.size(), false);
    loadIndex = 0;
    loadStepTimer = 0.f;
    loadProgress = 0.f;
    loadReady = false;
    loadReadyTimer = 0.f;
    fadeOut = 0.f;
    previewPath.clear();
    previewOres.clear();
    dialog = Dialog::None;
    state = State::Loading;
    stateTime = 0.f;
}

/// 加载进度推进：每一步都真正执行对应的工作（不是假进度）
void EntrySystem::advanceLoading(float dt) {
    const float stepDur = 0.42f;
    if (loadIndex < static_cast<int>(loadSteps.size())) {
        loadStepTimer += dt;
        if (loadStepTimer >= stepDur) {
            switch (loadIndex) {
                case 0:   // 读取数值配置（覆盖塔/敌人/配方/成本）
                    cfg::loadConfig("assets/config.json");
                    break;
                case 1:   // 扫描 10 个槽位（只读取状态，不写盘），供主菜单展示
                    refreshSlots();
                    break;
                case 2:   // 展开敌人折线路径（与 Game::generateTerrain 同一算法）
                    previewPath = buildPathTiles(cfg::PATH_POINTS, cfg::GRID_WIDTH,
                                                 cfg::GRID_HEIGHT);
                    break;
                case 3: { // 复现 Game::generateOreDeposits 的矿点分布（同种子42）
                    std::set<std::pair<int, int>> placed;
                    for (const auto& p : previewPath) placed.insert({p.x, p.y});
                    std::mt19937 oreRng(cfg::ORE_RANDOM_SEED);
                    std::uniform_int_distribution<int> rx(cfg::ORE_X_MIN, cfg::ORE_X_MAX);
                    std::uniform_int_distribution<int> ry(cfg::ORE_Y_MIN, cfg::ORE_Y_MAX);
                    auto gen = [&](int count) {
                        int ok = 0, attempts = 0;
                        while (ok < count && attempts < count * 100) {
                            ++attempts;
                            const int x = rx(oreRng), y = ry(oreRng);
                            if (placed.count({x, y})) continue;
                            placed.insert({x, y});
                            previewOres.push_back({x, y});
                            ++ok;
                        }
                    };
                    gen(cfg::ORE_IRON_COUNT);
                    gen(cfg::ORE_COPPER_COUNT);
                    gen(cfg::ORE_COAL_COUNT);
                    gen(cfg::ORE_GOLD_COUNT);
                    gen(cfg::ORE_DIAMOND_COUNT);
                    gen(cfg::ORE_NICKEL_COUNT);
                    gen(cfg::ORE_SILVER_COUNT);
                    gen(cfg::ORE_LEAD_COUNT);
                    break;
                }
                case 4:   // 界面与视图初始化（窗口/字体/视图已在构造与创建时完成）
                    window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                        static_cast<float>(window.getSize().x),
                        static_cast<float>(window.getSize().y))));
                    break;
                default: break;
            }
            loadDone[loadIndex] = true;
            ++loadIndex;
            loadStepTimer = 0.f;
        }
    } else {
        loadReady = true;
    }

    // 按当前实际进度平滑逼近目标值
    float target = 1.f;
    if (loadIndex < static_cast<int>(loadSteps.size())) {
        target = (static_cast<float>(loadIndex) + clamp01(loadStepTimer / stepDur)) /
                 static_cast<float>(loadSteps.size());
    }
    loadProgress += (target - loadProgress) * std::min(1.f, dt * 9.f);
    loadProgress = clamp01(loadProgress);
}

