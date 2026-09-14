// =====================================================================
// TutorialSystem.cpp —— 新手引导系统实现
// 详细设计见 TutorialSystem.h 顶部注释。
// =====================================================================
#include "systems/TutorialSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Game.h"
#include "ui/GameUI.h"
#include "components/Building.h"

using nlohmann::json;

namespace tutorial {

namespace {

// ---------------- 视觉常量（与 GameUI 暗色工业风一致） ----------------
const sf::Color C_ACCENT(230, 150, 60);        // 主强调（橙）
const sf::Color C_GREEN(120, 190, 80);         // 完成（绿）
const sf::Color C_RED(210, 70, 70);            // 误操作（红）
const sf::Color C_PANEL(22, 24, 28, 232);      // 横幅底色
const sf::Color C_PANEL2(16, 18, 21, 236);     // 叙事框底色
const sf::Color C_BORDER(96, 100, 110);
const sf::Color C_TEXT(230, 231, 236);
const sf::Color C_DIM(155, 159, 168);

/// 移动步骤的累计位移阈值（像素）
constexpr float MOVE_THRESHOLD = 240.0f;
/// 误操作纠正提示的冷却（秒），避免刷屏
constexpr float MISTAKE_COOLDOWN = 2.5f;

// ---------------- 文件级动画计时器（进程内单例，无需持久化） ----------------
float g_time = 0.0f;             // 累计时间（脉冲动画）
float g_advanceFlash = 0.0f;     // 完成打勾动画剩余时间
float g_mistakeFlash = 0.0f;     // 误操作红框剩余时间
float g_mistakeCooldown = 0.0f;  // 误操作提示冷却
float g_lastCamX = 0.0f, g_lastCamY = 0.0f;
bool g_hasLastCam = false;

// ---------------- 绘制小工具 ----------------
void drawStr(sf::RenderTarget& rt, const sf::Font& font, const std::string& s,
             unsigned size, sf::Vector2f pos, sf::Color color,
             bool centered = false, bool bold = false) {
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(size);
    text.setString(sf::String::fromUtf8(s.begin(), s.end()));
    text.setFillColor(color);
    if (bold) text.setStyle(sf::Text::Bold);
    text.setPosition(pos);
    if (centered) {
        const auto b = text.getLocalBounds();
        text.setPosition(pos.x - b.width / 2.0f, pos.y);
    }
    rt.draw(text);
}

/// 当前步骤脚本
const Step& cur(const State& s) { return stepAt(s, s.step); }

/// 世界内查找第一座指定类型的建筑
entt::entity findFirst(Game& g, cfg::BuildingType t) {
    for (auto [e, b] : g.reg.view<Building>().each())
        if (b.type == t) return e;
    return entt::null;
}

/// 建筑在屏幕上的矩形
sf::FloatRect buildingScreenRect(Game& g, entt::entity e) {
    const auto& b = g.reg.get<Building>(e);
    const sf::Vector2f tl =
        g.worldToScreen(g.tileWorld(sf::Vector2i{b.pos.x, b.pos.y}));
    const float z = g.camera.zoom;
    return {tl.x, tl.y, b.w * cfg::TILE_SIZE * z, b.h * cfg::TILE_SIZE * z};
}

/// 世界区域（右边界为右侧面板左边缘）
float worldAreaWidth(Game& g) {
    return static_cast<float>(g.window.getSize().x) - cfg::ui::SIDE_PANEL_WIDTH;
}

/// 横幅矩形
sf::FloatRect bannerRect(Game& g) {
    const float area = worldAreaWidth(g);
    const float w = std::min(660.0f, area - 40.0f);
    return {(area - w) / 2.0f, static_cast<float>(cfg::ui::RESOURCE_BAR_HEIGHT) + 12.0f, w, 84.0f};
}

/// 横幅右侧「跳过引导」按钮矩形
sf::FloatRect skipButtonRect(Game& g) {
    const auto b = bannerRect(g);
    return {b.left + b.width - 96.0f, b.top + 10.0f, 86.0f, 26.0f};
}

/// 叙事对话框矩形
sf::FloatRect narrationRect(Game& g) {
    const float area = worldAreaWidth(g);
    const float w = std::min(620.0f, area - 40.0f);
    return {(area - w) / 2.0f, static_cast<float>(g.window.getSize().y) - 92.0f, w, 74.0f};
}

// ---------------- 反馈 ----------------
void toast(Game& g, const std::string& msg) {
    if (g.ui) g.ui->showToast(msg);
}

/// 记录一次误操作并给出纠正提示（带冷却）
void recordMistake(Game& g, const char* msg) {
    if (!msg || !*msg) return;
    if (g_mistakeCooldown > 0.0f) return;
    g.tutorial.mistakes++;
    g_mistakeCooldown = MISTAKE_COOLDOWN;
    g_mistakeFlash = 1.6f;
    toast(g, std::string("✗ ") + msg);
}

/// 完成当前步骤并推进
void completeStep(Game& g) {
    State& s = g.tutorial;
    const Step& st = cur(s);
    if (s.step >= 0 && s.step < static_cast<int>(s.done.size())) {
        s.done[static_cast<size_t>(s.step)] = 1;
        s.stepTimes[static_cast<size_t>(s.step)] = s.stepElapsed;
    }
    if (st.hint && *st.hint) toast(g, std::string("✓ ") + st.hint);
    g_advanceFlash = 2.0f;

    s.step++;
    s.progressCounter = 0;
    s.stepElapsed = 0.0f;
    s.moveAccum = 0.0f;
    s.justAdvanced = true;

    if (s.step >= stepCount()) {
        finish(g);
        return;
    }
    // 阅读型步骤无需操作，仅提示
    if (stepAt(s, s.step).narration && *stepAt(s, s.step).narration)
        toast(g, stepAt(s, s.step).narration);
}

} // namespace

// ---------------------------------------------------------------------
// 章节名
// ---------------------------------------------------------------------
const char* chapterName(Chapter c) {
    switch (c) {
        case Chapter::Basics:     return "第一章 · 观察与移动";
        case Chapter::Production: return "第二章 · 建造与生产";
        case Chapter::Logistics:  return "第三章 · 物流与冶炼";
        case Chapter::Defense:    return "第四章 · 防御与战斗";
        case Chapter::Power:      return "第五章 · 电力网络";
        case Chapter::Automation: return "第六章 · 自动化与进阶";
        default:                  return "";
    }
}

// ---------------------------------------------------------------------
// 教学脚本（数据驱动：改文案/顺序只需动这张表）
//   难度曲线：纯阅读 → 单键操作 → 选建筑 → 单次放置 → 多建筑协作
//            → 右键进阶操作 → 战斗验证 → 电力网络 → 自主查阅资料
// ---------------------------------------------------------------------
const std::vector<Step>& script() {
    using C = Chapter;
    using T = Task;
    using B = cfg::BuildingType;
    static const std::vector<Step> kScript = {
        // ---------- 第一章 · 观察与移动 ----------
        {C::Basics, "建立通讯", "——",
         "织女星：……通讯建立。我是织女星，织星工业配给你的随船 AI。"
         "这颗星球的开发任务已经交接完毕。先用几分钟熟悉环境，工程师。",
         "通讯已建立", "", T::ReadNarration, B::TowerBasic, 1, 6.0f},

        {C::Basics, "熟悉视角", "W A S D 移动镜头 · 滚轮缩放",
         "织女星：先熟悉视角。W A S D 移动镜头，滚轮拉近拉远。矿脉就在这片区域里。",
         "视角操作已掌握", "请用 W / A / S / D 移动镜头。", T::MoveCamera, B::TowerBasic, 1, 0.0f},

        {C::Basics, "选中采矿场", "点击右侧面板『采矿场』· 或按 3",
         "织女星：第一步，把『采矿场』拿到手里。它负责把整片矿脉变成我们的原料。",
         "已选中采矿场", "现在还不需要别的建筑——先选中『采矿场』。",
         T::SelectBuilding, B::Miner, 1, 0.0f},

        // ---------- 第二章 · 建造与生产 ----------
        {C::Production, "放下第一座采矿场", "鼠标左键 放置",
         "织女星：矿脉就在脚下。放下采矿场，它会自动开采范围内的所有矿石。",
         "采矿场已就位，矿石开始流入", "这一步只需要『采矿场』。",
         T::PlaceBuilding, B::Miner, 1, 0.0f},

        {C::Production, "炼制第一块铁锭", "按 6 选熔炉 → 左键放置",
         "织女星：原矿不能直接交付。再放一台熔炉，把矿石炼成铁锭。",
         "熔炉已就位", "这一步需要『熔炉』（快捷键 6）。",
         T::PlaceBuilding, B::Furnace, 1, 0.0f},

        // ---------- 第三章 · 物流与冶炼 ----------
        {C::Logistics, "接通物流", "按 4 选管道 → 放在采矿场与熔炉之间",
         "织女星：用管道把采矿场和熔炉连起来，矿石就会自己走进熔炉——"
         "这就是产线的第一口气。",
         "物流已打通", "这一步需要『管道』（快捷键 4），放在采矿场与熔炉之间。",
         T::PlaceBuilding, B::Pipe, 1, 0.0f},

        {C::Logistics, "调整输出面", "右键点击你放下的采矿场",
         "织女星：右键点击采矿场会打开设置面板——可以旋转输出面，也能切换采集模式。"
         "对准管道，物流会更顺。",
         "采矿场设置已打开", "请右键点击你已经放下的那座采矿场。",
         T::RightClickBuilding, B::Miner, 1, 0.0f},

        // ---------- 第四章 · 防御与战斗 ----------
        {C::Defense, "架起防线", "按 1 选基础炮塔 → 放在路径旁",
         "织女星：雷达上出现热源。本地生物对我们挖矿有点意见——"
         "在它们的必经之路旁架一座炮塔。",
         "炮塔已架设", "这一步需要『基础炮塔』（快捷键 1），放在敌人路径旁边。",
         T::PlaceBuilding, B::TowerBasic, 1, 0.0f},

        {C::Defense, "召唤一次演练", "按 Z 生成一个普通敌人",
         "织女星：不用干等真正的敌人。按 Z 放一个靶子，先看看炮塔的成色。",
         "靶子已生成", "按 Z 生成一个普通敌人。", T::SpawnEnemy, B::TowerBasic, 1, 0.0f},

        {C::Defense, "首次击杀", "让炮塔开火 · 按 X / C 可加大考验",
         "织女星：弹药会自动补给，炮塔会自动索敌。这条防线，就是我们安心扩张的前提。",
         "首次击杀完成！", "先让炮塔把敌人打掉。", T::KillEnemy, B::TowerBasic, 1, 0.0f},

        // ---------- 第五章 · 电力网络 ----------
        {C::Power, "接入电力", "按 0 选燃煤发电机 → 左键放置",
         "织女星：炮塔靠弹药，工厂靠电。放一台燃煤发电机——记得喂它煤。",
         "发电机已就位", "这一步需要『燃煤发电机』（快捷键 0）。",
         T::PlaceBuilding, B::PowerGenerator, 1, 0.0f},

        {C::Power, "架设电网", "按 9 选电线杆 → 放在发电机与炮塔之间",
         "织女星：电不会自己长脚。用电线杆把电送到炮塔那里。",
         "电网已连通", "这一步需要『电线杆』（快捷键 9）。",
         T::PlaceBuilding, B::PowerPole, 1, 0.0f},

        // ---------- 第六章 · 自动化与进阶 ----------
        {C::Automation, "打开说明书", "按 H 或 F1",
         "织女星：基础课程到此为止。分流器、通物网络、合金炉——"
         "进阶图纸都在这本说明书里，随时可以翻开。",
         "说明书已打开，随时查阅", "按 H 或 F1 打开说明书。", T::OpenHelp, B::TowerBasic, 1, 0.0f},

        {C::Automation, "结业通讯", "——",
         "织女星：接下来波次会一次比一次密。放心扩张，数据我替你盯着——"
         "对了，总部那边的汇报我会替你写。通讯结束。",
         "新手引导已完成", "", T::ReadNarration, B::TowerBasic, 1, 6.5f},
    };
    return kScript;
}

int stepCount() { return static_cast<int>(script().size()); }

const Step& stepAt(const State& s, int index) {
    const auto& v = script();
    if (v.empty()) {
        static const Step kDummy{};
        return kDummy;
    }
    const int i = std::clamp(index, 0, static_cast<int>(v.size()) - 1);
    return v[static_cast<size_t>(i)];
}

// ---------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------
void begin(Game& g) {
    State& s = g.tutorial;
    const int n = stepCount();
    s.active = true;
    s.skipped = false;
    s.finished = false;
    s.step = 0;
    s.mistakes = 0;
    s.progressCounter = 0;
    s.stepElapsed = 0.0f;
    s.totalElapsed = 0.0f;
    s.moveAccum = 0.0f;
    s.justAdvanced = false;
    s.done.assign(static_cast<size_t>(n), 0);
    s.stepTimes.assign(static_cast<size_t>(n), 0.0f);
    g_hasLastCam = false;
    g_advanceFlash = 0.0f;
    g_mistakeFlash = 0.0f;
    // 引导只在"没有存档"的新档自动开启；手动重开时提示一下
    toast(g, "新手引导已开启 · F2 可跳过");
}

void finish(Game& g) {
    State& s = g.tutorial;
    s.active = false;
    s.finished = true;
    s.step = stepCount();
    for (auto& d : s.done) d = 1;
    saveProgressFile(s);
    logProgress(s);
    toast(g, "新手引导已全部完成 · F2 可重新查看");
}

void skip(Game& g) {
    State& s = g.tutorial;
    s.active = false;
    s.skipped = true;
    saveProgressFile(s);
    logProgress(s);
    toast(g, "已跳过新手引导 · 按 F2 可重新开启");
}

// ---------------------------------------------------------------------
// 每帧推进
// ---------------------------------------------------------------------
void update(Game& g, float dt) {
    g_time += dt;
    if (g_advanceFlash > 0.0f) g_advanceFlash -= dt;
    if (g_mistakeFlash > 0.0f) g_mistakeFlash -= dt;
    if (g_mistakeCooldown > 0.0f) g_mistakeCooldown -= dt;

    State& s = g.tutorial;
    s.justAdvanced = false;

    // 摄像机位移累计（MoveCamera 步骤判定）——放在最前，跳过时也能复位
    const float dx = g.camera.x - g_lastCamX;
    const float dy = g.camera.y - g_lastCamY;
    g_lastCamX = g.camera.x;
    g_lastCamY = g.camera.y;
    const float moved = std::sqrt(dx * dx + dy * dy);
    if (g_hasLastCam && moved > 0.0f) onCameraMoved(g, moved);
    g_hasLastCam = true;

    if (!s.active) return;

    s.stepElapsed += dt;
    s.totalElapsed += dt;

    const Step& st = cur(s);
    // 阅读型步骤：计时到点自动前进
    if (st.task == Task::ReadNarration && st.autoSeconds > 0.0f &&
        s.stepElapsed >= st.autoSeconds) {
        completeStep(g);
    }
}

// ---------------------------------------------------------------------
// 事件钩子
// ---------------------------------------------------------------------
void onKey(Game& g, sf::Keyboard::Key k) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);

    if (k == sf::Keyboard::F2) {   // 跳过 / 重开（由 PlayerSystem 触发流程）
        return;
    }
    switch (st.task) {
        case Task::OpenHelp:
            if (k == sf::Keyboard::H || k == sf::Keyboard::F1) completeStep(g);
            break;
        case Task::SpawnEnemy:
            if (k == sf::Keyboard::Z || k == sf::Keyboard::X || k == sf::Keyboard::C ||
                k == sf::Keyboard::U)
                completeStep(g);
            break;
        default:
            break;
    }
}

void onBuildingSelected(Game& g, cfg::BuildingType t) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task == Task::SelectBuilding) {
        if (t == st.building) completeStep(g);
        else recordMistake(g, st.mistake);
    } else if (st.task == Task::PlaceBuilding && t != st.building) {
        // 这一步要造别的建筑：给出方向性纠正（不阻断操作）
        recordMistake(g, st.mistake);
    }
}

void onBuildingPlaced(Game& g, cfg::BuildingType t) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task != Task::PlaceBuilding) return;
    if (t != st.building) {
        recordMistake(g, st.mistake);
        return;
    }
    if (++s.progressCounter >= std::max(1, st.count)) completeStep(g);
}

void onRightClickBuilding(Game& g, cfg::BuildingType t) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task == Task::RightClickBuilding) {
        if (t == st.building) completeStep(g);
    }
}

void onEnemyKilled(Game& g, int n) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task != Task::KillEnemy) return;
    s.progressCounter += std::max(1, n);
    if (s.progressCounter >= std::max(1, st.count)) completeStep(g);
}

void onCameraMoved(Game& g, float distance) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task != Task::MoveCamera) return;
    s.moveAccum += distance;
    if (s.moveAccum >= MOVE_THRESHOLD) completeStep(g);
}

// ---------------------------------------------------------------------
// 引导层事件（横幅上的「跳过引导」按钮）
// ---------------------------------------------------------------------
bool handleEvent(Game& g, const sf::Event& e) {
    if (!g.tutorial.active) return false;
    if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
        const sf::Vector2f p(static_cast<float>(e.mouseButton.x),
                             static_cast<float>(e.mouseButton.y));
        if (skipButtonRect(g).contains(p)) {
            skip(g);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------
// 高亮
// ---------------------------------------------------------------------
bool hasHighlight(Game& g) {
    if (!g.tutorial.active) return false;
    const Step& st = cur(g.tutorial);
    return st.task == Task::SelectBuilding || st.task == Task::PlaceBuilding ||
           st.task == Task::RightClickBuilding;
}

sf::FloatRect highlightRect(Game& g) {
    if (!hasHighlight(g)) return {};
    const Step& st = cur(g.tutorial);

    // 世界里已有目标建筑 → 高亮世界中的它；否则高亮右侧面板的按钮
    const entt::entity e = findFirst(g, st.building);
    if (e != entt::null && (st.task == Task::RightClickBuilding ||
                            st.task == Task::PlaceBuilding)) {
        return buildingScreenRect(g, e);
    }
    if (g.ui) {
        const sf::FloatRect r = g.ui->machineButtonRect(st.building);
        if (r.width > 0.0f && r.height > 0.0f) return r;
    }
    return {};
}

// ---------------------------------------------------------------------
// 绘制引导层
// ---------------------------------------------------------------------
void drawOverlay(Game& g, sf::RenderTarget& rt) {
    // 暂停/结束时让位给模态面板
    if (g.paused || g.gameOver) return;

    const sf::Font& font = g.assets.font();
    const float winH = static_cast<float>(g.window.getSize().y);

    // ---- 目标高亮的脉冲环（世界/按钮通用） ----
    if (g.tutorial.active) {
        const sf::FloatRect hr = highlightRect(g);
        if (hr.width > 0.0f && hr.height > 0.0f) {
            const float pulse = 0.5f + 0.5f * std::sin(g_time * 4.0f);
            const float pad = 6.0f + 4.0f * pulse;

            sf::RectangleShape fill({hr.width + pad * 2.0f, hr.height + pad * 2.0f});
            fill.setPosition(hr.left - pad, hr.top - pad);
            fill.setFillColor(sf::Color(230, 150, 60, static_cast<uint8_t>(28 + 26 * pulse)));
            rt.draw(fill);

            sf::RectangleShape ring({hr.width + pad * 2.0f, hr.height + pad * 2.0f});
            ring.setPosition(hr.left - pad, hr.top - pad);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(2.0f);
            ring.setOutlineColor(sf::Color(230, 150, 60,
                                            static_cast<uint8_t>(150 + 100 * pulse)));
            rt.draw(ring);
        }
    }

    if (!g.tutorial.active) {
        // 步骤刚完成的一瞬间仍显示打勾
        if (g_advanceFlash > 0.0f) {
            const auto b = bannerRect(g);
            drawStr(rt, font, "✓ 目标完成", 22,
                    {b.left + b.width / 2.0f, b.top + 26.0f}, C_GREEN, true, true);
        }
        return;
    }

    const State& s = g.tutorial;
    const Step& st = cur(s);

    // ---- 目标横幅 ----
    const auto b = bannerRect(g);
    sf::RectangleShape bg({b.width, b.height});
    bg.setPosition(b.left, b.top);
    bg.setFillColor(C_PANEL);
    rt.draw(bg);
    sf::RectangleShape border({b.width, b.height});
    border.setPosition(b.left, b.top);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(2.0f);
    border.setOutlineColor(g_mistakeFlash > 0.0f ? C_RED : C_ACCENT);
    rt.draw(border);
    // 左侧章节色条
    sf::RectangleShape bar({4.0f, b.height});
    bar.setPosition(b.left, b.top);
    bar.setFillColor(C_ACCENT);
    rt.draw(bar);

    const float tx = b.left + 16.0f;
    drawStr(rt, font, chapterName(st.chapter), 13, {tx, b.top + 8.0f}, C_DIM);
    drawStr(rt, font, st.title, 19, {tx, b.top + 26.0f}, C_TEXT, false, true);
    drawStr(rt, font, st.keys, 15, {tx, b.top + 54.0f}, C_ACCENT);
    // 进度
    drawStr(rt, font, "进度 " + std::to_string(s.step + 1) + " / " + std::to_string(stepCount()),
            13, {b.left + b.width - 16.0f, b.top + 8.0f}, C_DIM, false);
    // 跳过按钮
    const auto sk = skipButtonRect(g);
    sf::RectangleShape sbtn({sk.width, sk.height});
    sbtn.setPosition(sk.left, sk.top);
    sbtn.setFillColor(sf::Color(52, 55, 62));
    rt.draw(sbtn);
    sf::RectangleShape sbd({sk.width, sk.height});
    sbd.setPosition(sk.left, sk.top);
    sbd.setFillColor(sf::Color::Transparent);
    sbd.setOutlineThickness(1.0f);
    sbd.setOutlineColor(C_BORDER);
    rt.draw(sbd);
    drawStr(rt, font, "跳过引导 F2", 13, {sk.left + sk.width / 2.0f, sk.top + 5.0f}, C_TEXT, true);

    // ---- 完成打勾 ----
    if (g_advanceFlash > 0.0f) {
        drawStr(rt, font, "✓ 目标完成", 18, {b.left + b.width / 2.0f, b.top + b.height + 6.0f},
                C_GREEN, true, true);
    }

    // ---- 叙事对话框（织女星通讯） ----
    if (st.narration && *st.narration) {
        const auto n = narrationRect(g);
        sf::RectangleShape nbg({n.width, n.height});
        nbg.setPosition(n.left, n.top);
        nbg.setFillColor(C_PANEL2);
        rt.draw(nbg);
        sf::RectangleShape nb({n.width, n.height});
        nb.setPosition(n.left, n.top);
        nb.setFillColor(sf::Color::Transparent);
        nb.setOutlineThickness(1.0f);
        nb.setOutlineColor(sf::Color(70, 120, 150));
        rt.draw(nb);
        // 头像方块（织女星）
        sf::RectangleShape av({34.0f, 34.0f});
        av.setPosition(n.left + 10.0f, n.top + 10.0f);
        av.setFillColor(sf::Color(30, 60, 80));
        av.setOutlineThickness(1.0f);
        av.setOutlineColor(sf::Color(90, 170, 210));
        rt.draw(av);
        drawStr(rt, font, "AI", 15, {n.left + 27.0f, n.top + 17.0f}, sf::Color(140, 210, 245), true);
        drawStr(rt, font, "织女星 · 织星工业AI", 12, {n.left + 54.0f, n.top + 6.0f},
                sf::Color(140, 200, 230));
        // 旁白按宽度手工折行（UTF-8 逐字符，中文按 2 列宽计）
        const std::string text(st.narration);
        std::vector<std::string> lines;
        std::string line;
        int col = 0;
        size_t i = 0;
        while (i < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            const size_t len = (c < 0x80) ? 1 : ((c >> 5) == 0x6 ? 2 : ((c >> 4) == 0xE ? 3 : 4));
            line += text.substr(i, len);
            col += (c >= 0x80) ? 2 : 1;
            if (col >= 52) {
                lines.push_back(line);
                line.clear();
                col = 0;
            }
            i += len;
        }
        if (!line.empty()) lines.push_back(line);
        for (size_t k = 0; k < lines.size() && k < 3; ++k)
            drawStr(rt, font, lines[k], 14, {n.left + 54.0f, n.top + 24.0f + k * 18.0f}, C_TEXT);
    }
}

// ---------------------------------------------------------------------
// 持久化：saves/tutorial.json（学习进度档案）
// ---------------------------------------------------------------------
void saveProgressFile(const State& s) {
    try {
        std::error_code ec;
        std::filesystem::create_directories("saves", ec);
        json j;
        j["v"] = 1;
        j["active"] = s.active;
        j["skipped"] = s.skipped;
        j["finished"] = s.finished;
        j["step"] = s.step;
        j["total_steps"] = stepCount();
        j["mistakes"] = s.mistakes;
        j["total_elapsed"] = s.totalElapsed;
        j["updated"] = static_cast<double>(g_time);
        // 每步：是否完成 + 耗时（供后续分析玩家在哪一步卡住）
        json steps = json::array();
        for (int i = 0; i < stepCount(); ++i) {
            const bool done = i < static_cast<int>(s.done.size()) && s.done[static_cast<size_t>(i)];
            json sj;
            sj["i"] = i;
            sj["chapter"] = chapterName(stepAt(s, i).chapter);
            sj["title"] = stepAt(s, i).title;
            sj["done"] = done;
            sj["time"] = (i < static_cast<int>(s.stepTimes.size()))
                             ? s.stepTimes[static_cast<size_t>(i)]
                             : 0.0f;
            steps.push_back(std::move(sj));
        }
        j["steps"] = std::move(steps);
        std::ofstream f("saves/tutorial.json");
        if (f.good()) f << j.dump(2);
    } catch (...) {
        // 进度记录失败不应影响游戏
    }
}

bool loadProgressFile(State& out) {
    try {
        std::ifstream f("saves/tutorial.json");
        if (!f.good()) return false;
        json j;
        f >> j;
        out.skipped = j.value("skipped", false);
        out.finished = j.value("finished", false);
        out.mistakes = j.value("mistakes", 0);
        out.totalElapsed = j.value("total_elapsed", 0.0f);
        const int n = stepCount();
        out.done.assign(static_cast<size_t>(n), 0);
        out.stepTimes.assign(static_cast<size_t>(n), 0.0f);
        if (j.contains("steps")) {
            for (const auto& sj : j["steps"]) {
                const int i = sj.value("i", -1);
                if (i < 0 || i >= n) continue;
                out.done[static_cast<size_t>(i)] = sj.value("done", false) ? 1 : 0;
                out.stepTimes[static_cast<size_t>(i)] = sj.value("time", 0.0f);
            }
        }
        out.step = std::clamp(j.value("step", 0), 0, n);
        // 未完成且未跳过 → 继续引导（放在上次那一步）
        out.active = !out.finished && !out.skipped;
        return true;
    } catch (...) {
        return false;
    }
}

void logProgress(const State& s) {
    std::printf("[tutorial] step=%d/%d finished=%d skipped=%d mistakes=%d total=%.1fs\n",
                s.step, stepCount(), s.finished ? 1 : 0, s.skipped ? 1 : 0, s.mistakes,
                static_cast<double>(s.totalElapsed));
    std::fflush(stdout);
}

} // namespace tutorial
