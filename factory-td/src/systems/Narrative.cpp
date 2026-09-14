#include "systems/Narrative.h"

#include <array>
#include <cstddef>
#include <cstdio>

#include "Game.h"
#include "ui/GameUI.h"

namespace narrative {
namespace {

// ---------------------------------------------------------------------
// 台词池
//
// 语气基准（写作规范，新增台词请遵守）：
//   · 干净、专业、轻松。织女星是同事，不是上司，也不是捧场的啦啦队。
//   · 用"工程师"称呼玩家，不用"指挥官"（你不是军人，是外派工程师）。
//   · 吐槽只针对公司流程与邮件，从不针对玩家、不针对难度。
//   · 单条不超过 30 个汉字，保证通讯条一行放得下；不用感叹号堆情绪。
// ---------------------------------------------------------------------

// 着陆：抵达新星球（星球简报的载体，短一句即可）
constexpr const char* kLanding[] = {
    "着陆完成。织星工业外派任务，交接给你了，工程师。",
    "我们到了。大气、重力、设备自检，全部正常。",
    "星球档案已同步到你的终端，慢慢看，不急。",
    "轨道船卸完货就返航了。接下来几个月，这里归你管。",
};

// 首座建筑落成
constexpr const char* kFirstBuild[] = {
    "第一座建筑落地。这颗星球开始有产线了。",
    "矿石在动，就说明我们在动。",
    "产线雏形确认。进度我先替你报上去了。",
    "很好。从这一步开始，剩下的都是重复劳动。",
};

// 首次遇敌
constexpr const char* kFirstCombat[] = {
    "雷达有热源。本地生物对我们挖矿有点意见。",
    "检测到敌对个体。炮塔交给你，报告交给我。",
    "老规矩：不主动清巢，但别让它们碰到产线。",
};

// 首次击杀
constexpr const char* kFirstKill[] = {
    "首次击杀确认。炮塔状态良好，弹药我再补一批。",
    "干净利落。这次我写进当班记录了。",
    "威胁解除。产线可以继续往外扩了。",
};

// 一波清空
constexpr const char* kWaveCleared[] = {
    "本波清空。产线没停过，这是最好的结果。",
    "波次结束。有几处弹药见底，建议补一下产线。",
    "清理完毕。数据我记下了，你继续。",
    "这波比上一波多。数据在涨，产线最好也在涨。",
};

// 防护告急
constexpr const char* kLowLives[] = {
    "剩余防护不多了。补两座炮塔，或者先补弹药。",
    "我建议认真看待这个数字——它在往下走。",
    "防线有缺口。需要我把重点路径标出来吗？",
};

// 开发指标达成（关卡指标系统落地后由关卡流程调用）
constexpr const char* kDelivered[] = {
    "开发指标达成。交付报告已经上传了。",
    "配额清空。轨道船正在降落，准备回收产线。",
    "干得漂亮。下一颗星球的档案我已经提前下好了。",
};

// 吐槽公司（只吐槽流程，不吐槽玩家）
constexpr const char* kRoast[] = {
    "总部说「资源充足」。这是他们今年第 12 次这么说。",
    "人力资源部问你需不需要心理支持。我说你在造工厂，应该没事。",
    "报销流程我帮你走了，大概三个月后有回音。",
    "总部又群发了一封「降本增效」。我替你删了，不用谢。",
    "安全手册第 4 条：不要在矿脉上睡觉。这就是第 4 条。",
    "季度考核是「可持续交付」。你已经交付了，所以可持续。",
    "他们说这颗星球「条件优越」。你现在应该有自己的判断了。",
};

// ---------------------------------------------------------------------
// 状态（每局重置；用静态量是因为叙事不参与存档，也不需要回放）
// ---------------------------------------------------------------------
std::array<int, static_cast<size_t>(Event::Count)> g_cursor{};      // 每事件台词轮换指针
std::array<bool, static_cast<size_t>(Event::Count)> g_fired{};      // announceOnce 用

float g_roastTimer = 0.0f;                       // 距离下一次吐槽公司
constexpr float ROAST_INTERVAL = 240.0f;         // 约 4 分钟一条，不刷屏

template <size_t N>
void speakPool(Game& g, Event ev, const char* const (&pool)[N]) {
    const size_t i = static_cast<size_t>(ev);
    g_cursor[i] = (g_cursor[i] + 1) % static_cast<int>(N);
    if (g.ui) g.ui->showComms(kAiName, pool[g_cursor[i]]);
}

} // namespace

void announce(Game& g, Event ev) {
    switch (ev) {
        case Event::Landing:     speakPool(g, ev, kLanding);     break;
        case Event::FirstBuild:  speakPool(g, ev, kFirstBuild);  break;
        case Event::FirstCombat: speakPool(g, ev, kFirstCombat); break;
        case Event::FirstKill:   speakPool(g, ev, kFirstKill);   break;
        case Event::WaveCleared: speakPool(g, ev, kWaveCleared); break;
        case Event::LowLives:    speakPool(g, ev, kLowLives);    break;
        case Event::Delivered:   speakPool(g, ev, kDelivered);   break;
        case Event::Roast:       speakPool(g, ev, kRoast);       break;
        case Event::Count:       break;
    }
}

void announceOnce(Game& g, Event ev) {
    const size_t i = static_cast<size_t>(ev);
    if (g_fired[i]) return;
    g_fired[i] = true;
    announce(g, ev);
}

void waveLog(Game& g, int wave, int eliminated, int lives) {
    // 每 3 波换成织女星的一句话，其余波次给标准进度报告。
    // 两者共用同一条通讯条，分波次交替就不会互相顶掉。
    if (wave % 3 == 0) {
        speakPool(g, Event::WaveCleared, kWaveCleared);
        return;
    }
    char buf[160];
    // 模板：抬头 · 结果 · 产线状态 · 剩余防护
    std::snprintf(buf, sizeof(buf), "开发日志 #%02d · 清空 %d 个目标 · 产线正常 · 防护 %d",
                  wave, eliminated, lives);
    if (g.ui) g.ui->showComms("开发日志", buf);
}

void update(Game& g, float dt) {
    // 新手教程有自己的完整旁白，不叠加通讯条
    if (g.isTutorial()) return;

    g_roastTimer -= dt;
    if (g_roastTimer <= 0.0f) {
        g_roastTimer = ROAST_INTERVAL;
        announce(g, Event::Roast);
    }
}

void reset() {
    g_cursor.fill(-1);   // 首次取用时 +1 变 0，从头播
    g_fired.fill(false);
    g_roastTimer = ROAST_INTERVAL;
}

} // namespace narrative
