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
#include "components/Power.h"

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
/// 教学脚本版本：新增/重排步骤后必须 +1。
/// 旧进度文件按 step 索引续接，脚本一改索引就错位（会跳到不相干的步骤）——
/// 版本不匹配时丢弃旧进度从头教起（新手教程的进度没有保留价值，重置无副作用）。
constexpr int SCRIPT_VERSION = 5;
/// PowerOn 步骤的轮询/诊断间隔（秒）：太密会刷屏，太疏玩家不知道卡在哪
constexpr float POWER_POLL_INTERVAL = 4.0f;

// ---------------- 文件级动画计时器（进程内单例，无需持久化） ----------------
float g_time = 0.0f;             // 累计时间（脉冲动画）
float g_advanceFlash = 0.0f;     // 完成打勾动画剩余时间
float g_mistakeFlash = 0.0f;     // 误操作红框剩余时间
float g_mistakeCooldown = 0.0f;  // 误操作提示冷却
float g_lastCamX = 0.0f, g_lastCamY = 0.0f;
bool g_hasLastCam = false;
float g_powerPoll = 0.0f;        // PowerOn 步骤的下一轮诊断倒计时

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

/// 某一格（含占地尺寸）在屏幕上的矩形 —— 用于高亮"固定建造位"（Step::targetTile）。
/// 世界里的空地和已建成的建筑都要能高亮，所以这里不能走 findFirst。
sf::FloatRect tileScreenRect(Game& g, sf::Vector2i t, cfg::BuildingSize sz) {
    const sf::Vector2f tl = g.worldToScreen(g.tileWorld(t));
    const float z = g.camera.zoom;
    return {tl.x, tl.y, sz.w * cfg::TILE_SIZE * z, sz.h * cfg::TILE_SIZE * z};
}

/// 「可跳过」判定（Step::optional）：该步的"目的"是否已经被玩家顺手达成了。
/// 目前只对放置类步骤有意义，按建筑分派：
///   · 物品管道 —— 上游机器（采矿场/熔炉）的输出面已经直接贴着可接收的建筑，
///                  物料根本不经过管道就能交接，这一步自然不必再做。
///   · 其它建筑 —— 世界里已经有同种建筑（重复放置没有意义）。
/// 判定是"宽松"的：宁可少拦一次，也不要让玩家卡在一个已经做完的步骤上。
/// 续接前的世界自检（实现）。
/// 教程进度是"世界状态"的映射：第 4 步放下采矿场、第 5 步放下熔炉……每完成一步，
/// 世界里就多一件东西。可教程模式的世界不落盘（只写 tutorial.json），重启客户端后
/// 建筑全部消失、进度文件却仍写着 step=N —— 照旧续接的话，玩家会被卡在一个永远做不完
/// 的步骤上（第 5 步要求放熔炉，可第 4 步的采矿场已经没了）。
/// 这里按脚本表逐条核对：已完成步骤里要求"放置"的每一种建筑，现在是否还在世界里。
bool canResumeInWorldImpl(Game& g) {
    const State& s = g.tutorial;
    if (s.step <= 0) return true;   // 一步都没走过，没什么要对账的
    const int upto = std::min(s.step, stepCount());
    for (int i = 0; i < upto; ++i) {
        const Step& st = script()[static_cast<size_t>(i)];
        if (st.task != Task::PlaceBuilding) continue;
        if (findFirst(g, st.building) == entt::null) return false;
    }
    return true;
}

bool goalAlreadyMet(Game& g, const Step& st) {
    using B = cfg::BuildingType;
    if (st.building == B::Pipe) {
        // 管道步骤一律不判"目的已达成"。
        // 旧实现把"某台采矿场/熔炉的输出面旁边有任意建筑"当作物流已通，直接跳过铺管道这一步；
        // 但引擎里机器之间不会隔着格子直接交接物品——产物只能顺着物品管道走。
        // 于是玩家照教程"贴着放"之后，矿石堆在采矿场里出不来，一路卡到第 11 步「首次击杀」。
        // 现在管道步骤必须真的铺下一格，判定交给 onBuildingPlaced。
        (void)g;
        return false;
    }
    return findFirst(g, st.building) != entt::null;
}

/// 世界里是否已有"真的通上电"的指定用电设备（Task::PowerOn 的唯一判定条件）
bool poweredConsumer(Game& g, cfg::BuildingType t) {
    for (auto [e, b, pc] : g.reg.view<Building, PowerConsumer>().each())
        if (b.type == t && pc.powered) return true;
    return false;
}

/// 诊断"电为什么没通"：返回一句指向具体断点的纠正提示。
/// 第五章要的是真闭环，所以提示必须落到 发电 / 送煤 / 架线 三个可操作的断点上，
/// 而不是笼统地说"电力塔没通电"。
std::string powerDiagnosis(Game& g) {
    using B = cfg::BuildingType;
    bool hasGen = false, genRunning = false;
    for (auto e : g.reg.view<PowerGeneratorNode>()) {
        hasGen = true;
        if (g.reg.get<PowerGeneratorNode>(e).running) genRunning = true;
    }
    if (!hasGen) return "还没有『燃煤发电机』——先按 0 放一台。";
    if (!genRunning) return "燃煤发电机没有在发电：用『物品管道』把煤送进发电机。";

    for (auto [e, b] : g.reg.view<Building>().each()) {
        if (b.type != B::TowerElectric || !g.reg.all_of<PowerConsumer>(e)) continue;
        for (const auto& net : g.power.networks) {
            bool inNet = false;
            for (auto c : net.consumers) if (c == e) inNet = true;
            if (!inNet) continue;
            if (net.generators.empty())
                return "『电力线缆』没把发电机接到电力塔上：按 = 铺线，一格接一格不能断。";
            return "发电量不足，电网供电不够：多放一台发电机或减少用电设备。";
        }
        return "电力塔还不在任何电网上：按 = 用『电力线缆』从发电机一路铺到塔脚下。";
    }
    return "还没有『电力塔』——先按 2 放一座。";
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
void recordMistake(Game& g, const std::string& msg) {
    if (msg.empty()) return;
    if (g_mistakeCooldown > 0.0f) return;
    g.tutorial.mistakes++;
    g_mistakeCooldown = MISTAKE_COOLDOWN;
    g_mistakeFlash = 1.6f;
    toast(g, std::string("✗ ") + msg);
}
void recordMistake(Game& g, const char* msg) {
    if (!msg || !*msg) return;
    recordMistake(g, std::string(msg));
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
    g_powerPoll = 0.0f;   // 新步骤的"电为什么没通"诊断重新计时

    if (s.step >= stepCount()) {
        finish(g);
        return;
    }
    // 每完成一步就落盘：教程中途关掉游戏也不会丢掉已经走完的进度。
    // （旧实现只在 skip()/finish() 时写文件，TutorialSystem.h 里"每步写入"的承诺没有兑现，
    //   玩家在第 11 步退出后重进会被迫从第 1 步重来。）
    saveProgressFile(s);
    // 这里以前会把"下一步"的整段叙事再 toast 一遍。toast 是单行、不换行的，
    // 长句会横向铺满整个视口、正好压在教程横幅上（玩家看到的是一团叠字），
    // 而叙事本来就由底部的对话框常驻显示 —— 不再重复弹一次。
}

} // namespace

// ---------------------------------------------------------------------
// 章节名
// ---------------------------------------------------------------------
const char* chapterName(Chapter c) {
    switch (c) {
        case Chapter::Basics:     return "第一章 · 观察与移动";
        case Chapter::Production: return "第二章 · 建造与冶炼";
        case Chapter::Logistics:  return "第三章 · 物流与加工";
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
    // 教程地图在教学区强制预置了 铁/铜/煤 三种矿点（见 GameConfig.h「新手教程固定布局」），
    // 所以采矿场必须落在这一格——放偏了就退回"随机矿点缺铜 → 组装机造不出弹药"的老坑。
    static const sf::Vector2i kMinerTile{cfg::TUT_MINER_X, cfg::TUT_MINER_Y};

    static const std::vector<Step> kScript = {
        // ---------- 第一章 · 观察与移动 ----------
        {C::Basics, "建立通讯", "——",
         "织女星：……通讯建立。我是织女星，织星工业配给你的随船 AI。"
         "这颗星球的开发任务已经交接完毕。先用几分钟熟悉环境，工程师。",
         "通讯已建立", "", T::ReadNarration, B::TowerBasic, 1, 6.0f},

        {C::Basics, "熟悉视角", "W A S D 移动镜头 · 滚轮缩放",
         "织女星：先熟悉视角。W A S D 移动镜头，滚轮拉近拉远。"
         "镜头正对着我选定的矿脉，闪烁标记的那一格就是我们的起家点。",
         "视角操作已掌握", "请用 W / A / S / D 移动镜头。", T::MoveCamera, B::TowerBasic, 1, 0.0f},

        // ---------- 第二章 · 建造与冶炼 ----------
        {C::Production, "选中采矿场1级", "点击右侧面板『采矿场1级』· 或按 3",
         "织女星：第一步，把『采矿场1级』拿到手里。它负责把整片矿脉变成我们的原料。",
         "已选中采矿场1级", "现在还不需要别的建筑——先选中『采矿场1级』（快捷键 3）。",
         T::SelectBuilding, B::Miner, 1, 0.0f},

        {C::Production, "放下第一座采矿场1级", "左键放在闪烁标记的矿点上 · 弹窗里选输出面",
         "织女星：矿脉就在脚下。放在我标出来的那一格——那一格周围我探明了铁、铜、煤三种矿，"
         "一条产线要用的原料就齐了。",
         "采矿场1级已就位，矿石开始流入",
         "请把『采矿场1级』放在闪烁标记的那一格上，那里才是三种矿的交汇点。",
         T::PlaceBuilding, B::Miner, 1, 0.0f, false, kMinerTile},

        {C::Production, "矿石分类暂存", "按 5 选储物桶 → 左键放在采矿场1级的输出面这一侧",
         "织女星：等一下——采矿场挖的是整片矿脉，铁、铜、煤三种矿石会一起出来，"
         "可熔炉只认自己那条配方，收到不认的矿就会把管道堵死。"
         "先放一个储物桶当暂存站：矿石先统统倒进桶里，再由桶分类送出去。"
         "记住它的定位：桶只负责『上一级交给下一级』这一小段的分选，它是个中转站，不是中央仓库——"
         "将来要建大规模的集中仓储，靠的是通物存储网络。",
         "储物桶已就位 · 矿石有了中转站",
         "这一步需要『储物桶』（快捷键 5），放在采矿场1级的输出面那一边。",
         T::PlaceBuilding, B::Bucket, 1, 0.0f},

        {C::Logistics, "接通物流", "按 4 选物品管道 → 从采矿场1级的输出面铺向储物桶",
         "织女星：用物品管道把采矿场1级和储物桶连起来，矿石就会自己走进桶里——"
         "这就是产线的第一口气。物品管道不用配方向，贴上四邻就自动连通。"
         "记住一件事：机器之间隔着格子不会自己递东西，矿石只能顺着管道走。",
         "物流已打通",
         "这一步需要『物品管道』（快捷键 4）。从采矿场1级的输出面那一格起、往储物桶方向铺："
         "机器不会隔着空气交接，必须由物品管道接上。",
         T::PlaceBuilding, B::Pipe, 1, 0.0f},

        {C::Logistics, "给储物桶分选矿种", "右键储物桶 → 点『过滤设置』→ 先选面，再勾矿种",
         "织女星：现在告诉桶怎么分。右键储物桶打开面配置，把朝熔炉的那两个面都设成『输出』，"
         "再点『过滤设置』——一个面只勾『铁矿石』，另一个面只勾『铜矿石』。"
         "某个面一个都不勾时它不过滤、什么都能出；勾上以后就只放行勾中的那几种，"
         "过滤槽里可以放多种物品。没人认领的煤就老实待在桶里，不会再去堵熔炉。",
         "分选规则已设定",
         "右键『储物桶』→ 点『过滤设置』：先点一个输出面，再勾选该面允许输出的矿种"
         "（一个面勾铁矿石、另一个面勾铜矿石）。",
         T::RightClickBuilding, B::Bucket, 1, 0.0f},

        {C::Production, "炼制第一块铁锭", "按 6 选熔炉 → 放在储物桶两侧、各隔一格 · 弹窗里选输出面",
         "织女星：一台熔炉一次只烧一种配方，两种锭就得两台炉子。"
         "在储物桶两侧各放一台：左边那台接铁矿石，右边那台接铜矿石。"
         "注意别让炉子贴着桶——中间各留一格给物品管道，让桶把矿石递进管道、炉子再从管道里取，"
         "这才是一级一级往下走的走法；炉子直接贴着桶，等多配方的时候就会互相串料。"
         "放下时弹出的方向窗口就是『输出面』——产物从那一面吐出来，"
         "所以输出面要朝着你接下来要接的物品管道。",
         "两台熔炉已就位",
         "这一步需要『熔炉』（快捷键 6），在储物桶两侧各放一台（共两台），"
         "并且每台都与储物桶隔开一格——空出来的那一格是留给物品管道的。",
         T::PlaceBuilding, B::Furnace, 2, 0.0f},

        {C::Logistics, "分送两种矿", "按 4 选物品管道 → 从储物桶两侧的输出面各铺一段到熔炉",
         "织女星：把储物桶左右两个输出面分别接上熔炉——左边一路走铁矿石，右边一路走铜矿石。"
         "矿石顺着管子各回各家，两边都不会串味。",
         "两种矿石各就各位",
         "这一步需要『物品管道』（快捷键 4），从储物桶两侧的输出面各铺一段到对应熔炉（共两段）。",
         T::PlaceBuilding, B::Pipe, 2, 0.0f},

        {C::Logistics, "造出第一批弹药", "按 7 选组装机 → 放在熔炉的输出侧，避开储物桶正下方",
         "织女星：铁锭打不死东西，炮塔吃的是弹药。组装机把 2 份铁锭 + 1 份铜锭压成 1 发弹药——"
         "把它放在熔炉输出面的那一侧、贴着管道，输出面按同样的规矩对准产线。"
         "切记别把它摆在储物桶的正下方：桶对递过来的东西来者不拒，"
         "锭一进去就出不来了，组装机只能干等。",
         "组装机已就位 · 默认配方就是弹药",
         "这一步需要『组装机』（快捷键 7），放在熔炉输出面的那一侧（管道旁边），"
         "不要放在储物桶的正下方。",
         T::PlaceBuilding, B::Assembler, 1, 0.0f},

        {C::Logistics, "给组装机供料", "按 4 选物品管道 → 把两台熔炉出的锭都接进组装机（绕开储物桶）",
         "织女星：最后把两台炉子的产物都汇进组装机：铁锭一路、铜锭一路，"
         "缺了哪一路，弹药都压不出来。铺这段管道时要绕开储物桶——"
         "别让成品管道挨着桶，桶是分选站，不是仓库，成品进去就出不来了。",
         "组装机已接入两种原料",
         "这一步需要『物品管道』（快捷键 4），把两台熔炉的输出面都接到组装机（共两段），"
         "路上绕开储物桶那一格；若组装机迟迟没有原料、而桶里却堆着锭，"
         "就是成品管道贴到了桶旁边——把管道挪开一格。",
         T::PlaceBuilding, B::Pipe, 2, 0.0f},

        // ---------- 第四章 · 防御与战斗 ----------
        {C::Defense, "架起防线", "按 1 选基础塔 → 放在路径旁 · 弹窗里选炮口朝向",
         "织女星：雷达上出现热源。本地生物对我们挖矿有点意见——在它们的必经之路旁架一座基础塔。"
         "路径就是北边那条深色大道，塔要贴着路边放才够得着。"
         "注意：炮塔出厂时弹仓是空的，它只认弹药。",
         "基础塔已架设 · 弹仓待供弹", "这一步需要『基础塔』（快捷键 1），放在敌人路径旁边。",
         T::PlaceBuilding, B::TowerBasic, 1, 0.0f},

        {C::Defense, "打通弹药线", "按 4 选物品管道 → 一路铺到基础塔（可连点铺多格）",
         "织女星：最后一段物品管道，别怕长——管道没有长度限制，从组装机那一格起顺着方向"
         "一路点过去，想铺多格就铺多格，拐弯、跨过敌人路径都不成问题；只有矿点那几格铺不了"
         "（矿点不能被任何建筑盖住），绕开一格就行。"
         "弹药会顺着它直接进炮塔的弹仓，这条防线就不再缺弹。",
         "弹药产线已贯通",
         "这一步需要『物品管道』（快捷键 4），从组装机一路连到基础塔；"
         "管道可以一格一格连点、铺成任意长度，也能直接铺在敌人路径上，不必绕路；"
         "但矿点那一格放不下，绕开一格即可。"
         "若弹药送不过去，说明组装机的输出面没朝产线——拆掉重放一次最省事。",
         T::PlaceBuilding, B::Pipe, 1, 0.0f},

        {C::Defense, "召唤一次演练", "按 Z 生成一个普通敌人",
         "织女星：不用干等真正的敌人。按 Z 放一个靶子，先看看炮塔的成色。",
         "靶子已生成", "按 Z 生成一个普通敌人。", T::SpawnEnemy, B::TowerBasic, 1, 0.0f},

        {C::Defense, "首次击杀", "让炮塔开火 · 按 X / C 可加大考验",
         "织女星：看好了——弹药从产线自己送上去，炮塔自己索敌。这条防线，就是我们安心扩张的前提。",
         "首次击杀完成！",
         "炮塔弹仓还是空的：确认组装机在造弹药、物品管道接到了炮塔。",
         T::KillEnemy, B::TowerBasic, 1, 0.0f},

        // ---------- 第五章 · 电力网络 ----------
        // 这一章是"真闭环"：最后一步不是"放下线缆就算过"，而是轮询电力塔是否真的
        // 通上了电（发电 → 线缆 → 设备 全链路），所以步骤之间必须把三件事都教到。
        {C::Power, "接入电力", "按 0 选燃煤发电机 → 左键放置",
         "织女星：炮塔靠弹药，工厂靠电。放一台燃煤发电机——它烧煤发电，32EU/秒，"
         "是整套电力网络的心脏。",
         "燃煤发电机已就位 · 尚未点火",
         "这一步需要『燃煤发电机』（快捷键 0）。",
         T::PlaceBuilding, B::PowerGenerator, 1, 0.0f},

        {C::Power, "架起电力塔", "按 2 选电力塔 → 放在敌人路径旁",
         "织女星：电力塔不用弹药，通上电就能一直开火，比基础塔省心，但也更费电。"
         "把它架在路径旁边，和基础塔形成交叉火力。",
         "电力塔已架设 · 等待供电",
         "这一步需要『电力塔』（快捷键 2），放在敌人路径旁边。",
         T::PlaceBuilding, B::TowerElectric, 1, 0.0f},

        // 注意这两步的顺序：线缆与管道都要铺很多格，把"要铺一串"的步骤依次排在
        // 只认本步建筑的进度计数之后，玩家多铺几格时就落在同一步里（不算误操作）；
        // 最后一步 PowerOn 完全不检查建筑放置，所以整章不会因为"多铺了一格"而报错。
        {C::Power, "架设电网", "按 = 选电力线缆 → 一格一格铺，连起发电机与电力塔",
         "织女星：电不会隔着空气跳过去。用电力线缆把燃煤发电机和电力塔连成一串——"
         "一格接一格，中间不能断开。线缆四面默认全通，不需要配方向。",
         "电网已铺设",
         "这一步需要『电力线缆』（快捷键 =），中途不能改用别的建筑接线。",
         T::PlaceBuilding, B::PowerWire, 1, 0.0f},

        {C::Power, "给发电机运煤", "按 4 选物品管道 → 先让储物桶空着的那一面『输出』并只勾煤，再从那一面铺到燃煤发电机",
         "织女星：发电机认煤不认别的。注意储物桶的输出面是**白名单**——前面给铁矿石、"
         "铜矿石各占了一面，采出来的煤虽然进了桶，却一件都出不去。"
         "右键储物桶，把还空着的那一面设成『输出』、过滤里只勾『煤』，再铺一段管道过去，"
         "煤就会自己流进炉膛。",
         "煤已上路 · 发电机即将点火",
         "这一步需要『物品管道』（快捷键 4）：先右键储物桶，把没用过的那一面设成『输出』、"
         "过滤里勾上『煤』，再从那一面接管道到燃煤发电机。",
         T::PlaceBuilding, B::Pipe, 1, 0.0f},

        {C::Power, "接通电源", "——",
         "织女星：最后一步，等电接通。发电机烧煤 → 线缆送电 → 电力塔上线。"
         "这条链上任何一环断了，塔就只是一根铁柱子；断在哪一环，我会看着告诉你。",
         "电力塔已通电 · 电力网络正式上线！",
         "",
         T::PowerOn, B::TowerElectric, 1, 0.0f},

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
bool canResumeInWorld(Game& g) { return canResumeInWorldImpl(g); }

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
    g_powerPoll = 0.0f;
    // 教程地图是固定布局（矿点钉在 TUT_MINER_X/Y，见 GameConfig.h），所以开局就把镜头
    // 摆到教学区上方一点：目标格与北边的敌人路径（y=80）一屏之内都能看到，玩家不必先找地方。
    g.camera.x = g.camera.targetX =
        (static_cast<float>(cfg::TUT_MINER_X) + 0.5f) * cfg::TILE_SIZE;
    g.camera.y = g.camera.targetY =
        (static_cast<float>(cfg::TUT_MINER_Y) - 2.0f) * cfg::TILE_SIZE;
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
    // （每条分支 completeStep 后立即 return：st 还指着旧步骤，继续往下跑会重复推进）
    if (st.task == Task::ReadNarration && st.autoSeconds > 0.0f &&
        s.stepElapsed >= st.autoSeconds) {
        completeStep(g);
        return;
    }

    // 可跳过步骤：目的已被玩家顺手达成 → 直接放行，不逼玩家重复一遍
    if (st.task == Task::PlaceBuilding && st.optional && goalAlreadyMet(g, st)) {
        completeStep(g);
        return;
    }

    // 通电步骤：轮询"用电设备是否真的通上了电"（真闭环，而不是"放下线缆就算过"）。
    // 没通电就按 POWER_POLL_INTERVAL 给一次诊断，指出断在 发电/送煤/架线 的哪一环。
    if (st.task == Task::PowerOn) {
        if (poweredConsumer(g, st.building)) {
            completeStep(g);
            return;
        }
        g_powerPoll += dt;
        if (g_powerPoll >= POWER_POLL_INTERVAL) {
            g_powerPoll = 0.0f;
            recordMistake(g, powerDiagnosis(g));
        }
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

void onBuildingPlaced(Game& g, cfg::BuildingType t, sf::Vector2i tile) {
    State& s = g.tutorial;
    if (!s.active) return;
    const Step& st = cur(s);
    if (st.task != Task::PlaceBuilding) return;
    if (t != st.building) {
        // 玩家可能还在补上一步的同类建筑：线缆/管道往往要铺一长串，本步只按"放下第一段"
        // 计数通过，剩下的格子必然落在下一步里。这种情况不算误操作，直接忽略，
        // 否则玩家每多铺一格就被纠正一次。
        if (s.step > 0 && script()[static_cast<size_t>(s.step - 1)].building == t) return;
        recordMistake(g, st.mistake);
        return;
    }
    // 固定建造位：只认这一格。教程的矿点是钉死的，采矿场放偏了这一格就是白放
    // （放偏的地方没有铁/铜/煤，玩家会以为"教程让我挖矿却挖不出东西"）。
    if (st.targetTile.x >= 0 && tile.x >= 0 && tile != st.targetTile) {
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

    // 固定建造位（Step::targetTile）优先级最高：直接标世界里的那一格。
    // 教程的矿点是钉死的，玩家必须看见"放哪一格"，而不是右侧面板。
    if (st.targetTile.x >= 0)
        return tileScreenRect(g, st.targetTile, cfg::buildingSize(st.building));

    // 世界里已有目标建筑 → 高亮世界中的它；否则高亮右侧面板的按钮
    // 注意：只有"右键"类步骤才高亮世界里的旧建筑——"放置"类步骤一律高亮右侧面板按钮，
    //       否则当世界已存在同类建筑（如第 2、3 次铺管道）时会误导玩家去点那一栋。
    const entt::entity e = findFirst(g, st.building);
    if (e != entt::null && st.task == Task::RightClickBuilding) {
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
    // 可跳过步骤给个明示，玩家才知道"熔炉贴脸时这步会自己过"
    const std::string keysLine =
        std::string(st.keys ? st.keys : "") + (st.optional ? "　（此步可跳过）" : "");
    drawStr(rt, font, keysLine, 15, {tx, b.top + 54.0f}, C_ACCENT);
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
        j["script_v"] = SCRIPT_VERSION;
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
        // 教学脚本改版（步骤增删/重排）后，旧档里的 step 索引会指向错位的步骤：
        // 丢弃旧进度、从头教起（返回 true 但 active=false → 调用方走 begin）。
        if (j.value("script_v", 1) != SCRIPT_VERSION) {
            out = State{};
            return true;
        }
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
