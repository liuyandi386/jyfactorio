// =====================================================================
// main.cpp —— 程序入口
//
// 创建游戏主控并进入主循环（对应 Python main.py 的 main()）。
// 帧时间由 sf::Clock 计算 dt 驱动（与帧率无关）；
// 帧率/垂直同步策略统一在 gset::applyFrameMode（见 Settings.h），
// 默认垂直同步 → 跟随显示器刷新率（144Hz 屏即 144 帧）。
//
// 附加命令行参数：
//   --selftest-save  运行存档往返自检（保存→清空→读档→逐项核对，
//                    自动备份/恢复玩家存档），退出码 0=全部通过，1=存在失败项
//   --selftest-ime   运行输入法抑制回归自检（菜单窗口销毁后游戏窗口是否仍被接管；
//                    HWND 复用是这条路径的历史坑），退出码 0=通过
//   --selftest-pipe  运行管道长距离传输自检（直线 5~150 格 / 主干+死胡同支路 /
//                    蛇形 380 格 / 末端无容器时的背压负例），退出码 0=全部通过
//   --safe-mode      黑屏应急启动：强制 1280×720 窗口化 + 垂直同步
//                    （不写回 saves/settings.json，下次正常启动仍用原设置）
//
// 启动顺序：开启高 DPI 感知 → 读取用户设置 → 入口系统（启动动画/标题菜单/设置/加载）
// → 按入口选择创建 Game（新游戏 or 读档继续）→ 游戏主循环。
// 游戏内暂停面板选「返回主界面」时回到入口系统（循环重进，见下）。
// =====================================================================
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Game.h"
#include "SaveSystem.h"
#include "Settings.h"
#include "ui/EntrySystem.h"
#include "systems/EnemySystem.h"
#include "systems/PipeSystem.h"
#include "systems/MeSystem.h"
#include "components/Me.h"

#ifdef _WIN32
// 放在 SFML 头之后包含，避免 windows.h 的宏污染 SFML（与 Settings.cpp 一致）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// ---------------------------------------------------------------------
// 高 DPI 感知（必须在创建任何窗口之前调用）
// ---------------------------------------------------------------------
// 不做这一步时进程是"DPI 不感知"的：在 125%/150% 缩放的桌面上（2K/4K 高刷屏很常见）
// 系统会把桌面虚拟化成更小的逻辑尺寸，sf::VideoMode::getDesktopMode() 拿到的尺寸
// 与实际屏幕不一致 —— 无边框全屏窗口于是只占画面一角/露出桌面，
// 部分显卡驱动下还会因为后台缓冲尺寸与窗口客户区不匹配而整屏黑掉。
//
// 用 GetProcAddress 动态取地址，避免依赖新版本 Windows SDK 的符号：
//   -4 = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2（Win10 1703+）
//   退化路径 SetProcessDPIAware（Vista+）也聊胜于无。
void enableHighDpiAwareness() {
#ifdef _WIN32
    const HMODULE user32 = ::LoadLibraryW(L"user32.dll");
    if (!user32) return;
    using SetCtxFn   = BOOL(WINAPI*)(void*);
    using SetAwareFn = BOOL(WINAPI*)(void);
    if (auto setCtx = reinterpret_cast<SetCtxFn>(
            ::GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
        if (setCtx(reinterpret_cast<void*>(static_cast<std::intptr_t>(-4)))) return;
    }
    if (auto setAware = reinterpret_cast<SetAwareFn>(
            ::GetProcAddress(user32, "SetProcessDPIAware")))
        setAware();
#endif
}


struct Check {
    bool ok = false;
    std::string name;
};
std::vector<Check> checks;

void check(const std::string& name, bool ok) { checks.push_back({ok, name}); }

/// 找指定类型建筑的实体
entt::entity findBuilding(Game& g, cfg::BuildingType t) {
    for (auto [e, b] : g.reg.view<Building>().each())
        if (b.type == t) return e;
    return entt::null;
}

/// 建筑实体计数
int buildingCount(Game& g, cfg::BuildingType t) {
    int n = 0;
    for (auto [e, b] : g.reg.view<Building>().each())
        if (b.type == t) n++;
    return n;
}

int runSelftestSave() {
    namespace fs = std::filesystem;
    // ---- 自检写入独立的临时文件，绝不触碰玩家的 10 个槽位存档 ----
    const std::string save = "saves/_selftest_slot.json";
    std::error_code ec;
    fs::remove(save, ec);   // 清掉上次自检可能残留的文件

    Game game;   // 初始化窗口/资源/地形/矿点

    // ---- 布置场景：覆盖全部建筑类型与状态 ----
    entt::entity miner = game.placeBuilding(50, 50, cfg::BuildingType::Miner, cfg::Dir::RIGHT, false);
    entt::entity furnace = game.placeBuilding(52, 50, cfg::BuildingType::Furnace, cfg::Dir::DOWN, false);
    entt::entity pipe = game.placeBuilding(51, 50, cfg::BuildingType::Pipe, 0, false);
    entt::entity bucket = game.placeBuilding(54, 50, cfg::BuildingType::Bucket, cfg::Dir::LEFT, false);
    entt::entity wire = game.placeBuilding(56, 50, cfg::BuildingType::PowerWire, 0, false);
    entt::entity tower = game.placeBuilding(58, 50, cfg::BuildingType::TowerBasic, 5, false);
    entt::entity assembler = game.placeBuilding(60, 50, cfg::BuildingType::Assembler, cfg::Dir::UP, false);
    entt::entity splitter = game.placeBuilding(62, 50, cfg::BuildingType::Splitter, 0, false);
    entt::entity alloy = game.placeBuilding(64, 50, cfg::BuildingType::AlloyFurnace, cfg::Dir::RIGHT, false);
    entt::entity gen = game.placeBuilding(66, 50, cfg::BuildingType::PowerGenerator, cfg::Dir::DOWN, false);
    entt::entity meI = game.placeBuilding(68, 50, cfg::BuildingType::MeInterface, 0, false);
    entt::entity meD = game.placeBuilding(69, 50, cfg::BuildingType::MeDrive, 0, false);
    entt::entity meT = game.placeBuilding(70, 50, cfg::BuildingType::MeTerminal, 0, false);
    check("放置13种建筑", miner != entt::null && furnace != entt::null && pipe != entt::null &&
                            bucket != entt::null && wire != entt::null && tower != entt::null &&
                            assembler != entt::null && splitter != entt::null && alloy != entt::null &&
                            gen != entt::null && meI != entt::null && meD != entt::null && meT != entt::null);

    // ---- 占地尺寸规格审计（唯一数据源 cfg::BUILDING_INFOS / cfg::buildingSize）----
    // ① 静态：全部建筑统一 1×1，不存在任何多格建筑（2×2 发电机已废止）
    {
        int multiCell = 0;
        const char* multiName = "";
        for (int i = 0; i < cfg::BUILDING_COUNT; ++i) {
            const auto sz = cfg::buildingSize(static_cast<cfg::BuildingType>(i));
            if (sz.w != 1 || sz.h != 1) {
                ++multiCell;
                multiName = cfg::BUILDING_INFOS[static_cast<size_t>(i)].nameZh;
            }
        }
        (void)multiName;
        check("占地规格：全部建筑均为1×1", multiCell == 0);
    }
    // ② 实地：找一块空地放置旧版发电机，验证 canPlace 逐格扫描、
    //     占地 1×1 仅登记 1 格、且该格被阻挡
    int gx = -1, gy = -1;
    for (int ty = 40; ty < 90 && gx < 0; ++ty)
        for (int tx = 40; tx < 110; ++tx)
            if (game.canPlace(tx, ty, cfg::BuildingType::Generator)) { gx = tx; gy = ty; break; }
    entt::entity legacyGen = (gx >= 0)
        ? game.placeBuilding(gx, gy, cfg::BuildingType::Generator, cfg::Dir::DOWN, false)
        : entt::null;
    check("发电机：占地1×1且单格登记", legacyGen != entt::null &&
        game.reg.get<Building>(legacyGen).w == 1 && game.reg.get<Building>(legacyGen).h == 1 &&
        game.isOccupied(gx, gy) && !game.isOccupied(gx + 1, gy) && !game.isOccupied(gx, gy + 1));
    check("占用格阻挡新建筑", legacyGen != entt::null &&
        !game.canPlace(gx, gy, cfg::BuildingType::TowerBasic));

    // 机器库存 / 任务状态
    game.reg.get<Inventory>(miner).add(cfg::ItemType::CopperOre, 123);
    game.reg.get<Machine>(miner).acc = 1.5f;
    game.reg.get<Inventory>(furnace).add(cfg::ItemType::IronOre, 7);
    game.reg.get<Machine>(furnace).hasJob = true;
    game.reg.get<Machine>(furnace).recipeId = 1;
    game.reg.get<Machine>(furnace).jobTime = 0.8f;
    game.reg.get<Machine>(furnace).jobTotal = 3.0f;
    game.reg.get<Machine>(assembler).recipeId = 1;
    game.reg.get<Inventory>(assembler).add(cfg::ItemType::IronIngot, 5);
    game.reg.get<Inventory>(alloy).add(cfg::ItemType::SteelIngot, 3);
    // 管道缓冲
    auto& pbuf = game.reg.get<Pipe>(pipe).buffer;
    pbuf.push_back(cfg::ItemType::GoldOre);
    pbuf.push_back(cfg::ItemType::LeadOre);
    pbuf.push_back(cfg::ItemType::NickelOre);
    // 储物桶
    auto& bk = game.reg.get<Bucket>(bucket);
    bk.items.push_back(cfg::ItemType::DiamondOre);
    bk.items.push_back(cfg::ItemType::SilverOre);
    bk.outputTimer = 0.42f;
    // 电线面配置（旋转过的面必须保留）
    game.reg.get<FaceConfig>(wire).set(cfg::Dir::UP, cfg::FaceMode::OUTPUT);
    game.reg.get<FaceConfig>(wire).set(cfg::Dir::LEFT, cfg::FaceMode::INPUT);
    // 塔
    game.reg.get<Turret>(tower).ammo = 42;
    game.reg.get<Turret>(tower).barrelDir = 5;
    // 分流器
    auto& sp = game.reg.get<SplitterQueue>(splitter);
    sp.queue.push_back(cfg::ItemType::Coal);
    sp.queue.push_back(cfg::ItemType::Ammo);
    sp.outputIndex = 2;
    // 发电机
    game.reg.get<Inventory>(gen).add(cfg::ItemType::Coal, 9);
    game.reg.get<PowerGeneratorNode>(gen).fuelTime = 1.5f;
    // 通物网络
    MeSystem::rebuildNetworks(game);
    check("通物网络已重建", !MeSystem::networks().empty());
    if (!MeSystem::networks().empty()) {
        MeSystem::addItem(MeSystem::networksMutable().front(), cfg::ItemType::IronIngot, 10);
        MeSystem::addItem(MeSystem::networksMutable().front(), cfg::ItemType::CopperIngot, 6);
    }
    // 全局状态
    game.gold = 777;
    game.lives = 5;
    game.camera.x = 1234.5f;
    game.camera.y = 678.25f;
    EnemySystem::spawnEnemy(game, cfg::EnemyType::Fast);

    const size_t oreCountBefore = static_cast<size_t>(std::distance(
        game.reg.view<GridPos, OreDeposit>().begin(), game.reg.view<GridPos, OreDeposit>().end()));

    // ---- 保存 ----
    check("saveGameToFile()成功", ::saveGameToFile(game, save));

    // ---- 手动清空整个世界（模拟重新进游戏） ----
    game.reg.clear();
    for (auto& cell : game.grid.cells) cell.building = entt::null;

    // ---- 读档 ----
    check("loadGameFromFile()成功", ::loadGameFromFile(game, save));

    // ---- 逐项核对 ----
    check("建筑总数14", buildingCount(game, cfg::BuildingType::TowerBasic) == 1 &&
                        buildingCount(game, cfg::BuildingType::Miner) == 1 &&
                        buildingCount(game, cfg::BuildingType::Furnace) == 1 &&
                        buildingCount(game, cfg::BuildingType::Pipe) == 1 &&
                        buildingCount(game, cfg::BuildingType::Bucket) == 1 &&
                        buildingCount(game, cfg::BuildingType::PowerWire) == 1 &&
                        buildingCount(game, cfg::BuildingType::Assembler) == 1 &&
                        buildingCount(game, cfg::BuildingType::Splitter) == 1 &&
                        buildingCount(game, cfg::BuildingType::AlloyFurnace) == 1 &&
                        buildingCount(game, cfg::BuildingType::PowerGenerator) == 1 &&
                        buildingCount(game, cfg::BuildingType::Generator) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeInterface) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeDrive) == 1 &&
                        buildingCount(game, cfg::BuildingType::MeTerminal) == 1);

    // 读档后占地必须完整恢复（尺寸由类型派生，统一 1×1）
    {
        const entt::entity lg = findBuilding(game, cfg::BuildingType::Generator);
        check("读档后发电机占地1×1", lg != entt::null &&
            game.reg.get<Building>(lg).w == 1 && game.reg.get<Building>(lg).h == 1 &&
            game.isOccupied(game.reg.get<Building>(lg).pos.x,
                            game.reg.get<Building>(lg).pos.y));
    }

    entt::entity f2 = findBuilding(game, cfg::BuildingType::Furnace);
    check("熔炉库存(铁矿石×7)", f2 != entt::null &&
        game.reg.get<Inventory>(f2).count(cfg::ItemType::IronOre) == 7);
    check("熔炉任务状态", f2 != entt::null && game.reg.get<Machine>(f2).hasJob &&
        game.reg.get<Machine>(f2).recipeId == 1 &&
        std::abs(game.reg.get<Machine>(f2).jobTime - 0.8f) < 0.001f);

    entt::entity m2 = findBuilding(game, cfg::BuildingType::Miner);
    check("采矿机库存(铜矿石×123)", m2 != entt::null &&
        game.reg.get<Inventory>(m2).count(cfg::ItemType::CopperOre) == 123);

    entt::entity p2 = findBuilding(game, cfg::BuildingType::Pipe);
    check("管道缓冲3件", p2 != entt::null && game.reg.get<Pipe>(p2).buffer.size() == 3);

    entt::entity b2 = findBuilding(game, cfg::BuildingType::Bucket);
    check("储物桶2件+计时器", b2 != entt::null && game.reg.get<Bucket>(b2).items.size() == 2 &&
        std::abs(game.reg.get<Bucket>(b2).outputTimer - 0.42f) < 0.001f);

    entt::entity w2 = findBuilding(game, cfg::BuildingType::PowerWire);
    check("电线面配置保留", w2 != entt::null &&
        game.reg.get<FaceConfig>(w2).get(cfg::Dir::UP) == cfg::FaceMode::OUTPUT &&
        game.reg.get<FaceConfig>(w2).get(cfg::Dir::LEFT) == cfg::FaceMode::INPUT);

    entt::entity t2 = findBuilding(game, cfg::BuildingType::TowerBasic);
    check("塔弹药+炮管朝向", t2 != entt::null && game.reg.get<Turret>(t2).ammo == 42 &&
        game.reg.get<Turret>(t2).barrelDir == 5);

    entt::entity a2 = findBuilding(game, cfg::BuildingType::Assembler);
    check("组装机配方+库存", a2 != entt::null && game.reg.get<Machine>(a2).recipeId == 1 &&
        game.reg.get<Inventory>(a2).count(cfg::ItemType::IronIngot) == 5);

    entt::entity s2 = findBuilding(game, cfg::BuildingType::Splitter);
    check("分流器队列2件+轮询索引", s2 != entt::null &&
        game.reg.get<SplitterQueue>(s2).queue.size() == 2 &&
        game.reg.get<SplitterQueue>(s2).outputIndex == 2);

    entt::entity g2 = findBuilding(game, cfg::BuildingType::PowerGenerator);
    check("发电机煤×9+燃料", g2 != entt::null &&
        game.reg.get<Inventory>(g2).count(cfg::ItemType::Coal) == 9 &&
        std::abs(game.reg.get<PowerGeneratorNode>(g2).fuelTime - 1.5f) < 0.001f);

    check("通物网络物品(铁锭10+铜锭6)", !MeSystem::networks().empty() &&
        MeSystem::countItem(MeSystem::networks().front(), cfg::ItemType::IronIngot) == 10 &&
        MeSystem::countItem(MeSystem::networks().front(), cfg::ItemType::CopperIngot) == 6);

    check("矿点数量一致", static_cast<size_t>(std::distance(
        game.reg.view<GridPos, OreDeposit>().begin(), game.reg.view<GridPos, OreDeposit>().end())) == oreCountBefore);
    check("金币777", game.gold == 777);
    check("生命5", game.lives == 5);
    check("摄像机位置", std::abs(game.camera.x - 1234.5f) < 0.01f &&
        std::abs(game.camera.y - 678.25f) < 0.01f);
    check("敌人1个(快速)", [&] {
        int n = 0; bool fast = false;
        for (auto [e, en] : game.reg.view<Enemy>().each()) { n++; fast = en.type == cfg::EnemyType::Fast; }
        return n == 1 && fast;
    }());

    // ---- 清理自检产生的临时文件 ----
    fs::remove(save, ec);

    // ---- 汇总 ----
    int fails = 0;
    for (const auto& c : checks) {
        std::printf("[%s] %s\n", c.ok ? "PASS" : "FAIL", c.name.c_str());
        if (!c.ok) fails++;
    }
    std::printf("存档往返自检: %zu/%zu 通过\n", checks.size() - static_cast<size_t>(fails),
                checks.size());
    return fails == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------
// 输入法抑制回归自检
// ---------------------------------------------------------------------
// 现场：**无边框全屏 + HWND_TOPMOST** 窗口若不禁用输入法，输入法候选窗/语言栏会与它
// 互抢 Z 序与前台焦点 → 持续闪屏/黑屏 + 鼠标漂移，整个系统无法操作（v1.3.4 补丁修过一次）。
// 坑：**HWND 会被系统复用**。主菜单窗口销毁、或玩家在主菜单里切换显示模式让窗口重建后，
// 游戏窗口很可能拿到同一个句柄值；早期实现用"HWND 值没变"判重，于是游戏窗口被整段跳过、
// 完全没有防护 → 闪屏复发（v1.3.5 全屏进教程事故）。
// 本自检按真实顺序走一遍：菜单建窗 → 切换显示模式（重建）→ 销毁 → 游戏建窗，断言游戏
// 窗口确实已被接管，并打印各窗口句柄（能看出系统是否复用了句柄）。
int runSelftestIme() {
    const auto hwndOf = [](sf::RenderWindow& w) {
        return reinterpret_cast<std::uintptr_t>(w.getSystemHandle());
    };
    std::vector<std::uintptr_t> handles;
    {
        sf::RenderWindow menu;                     // 等价于 EntrySystem 的窗口
        gset::applyToWindow(menu, "selftest-menu");
        handles.push_back(hwndOf(menu));
        gset::applyToWindow(menu, "selftest-menu");   // 模拟"切换显示模式"→ 同一窗口重建
        handles.push_back(hwndOf(menu));
    }                                              // 作用域结束 = 销毁窗口，HWND 归还系统

    Game game(GameMode::Tutorial);                 // 本次事故现场：教程模式全屏窗口
    handles.push_back(hwndOf(game.window));

    const bool reused = handles[0] == handles[1] || handles[0] == handles[2] ||
                        handles[1] == handles[2];
    const bool suppressed = gset::isImeSuppressed(game.window);
    for (size_t i = 0; i < handles.size(); ++i) {
        static const char* const kWho[3] = {"菜单窗口", "菜单重建后", "游戏窗口"};
        std::printf("%s HWND = %llu\n", kWho[i], static_cast<unsigned long long>(handles[i]));
    }
    std::printf("HWND 复用：%s\n", reused ? "发生（正是历史事故场景）" : "未发生");
    std::printf("[%s] 游戏窗口已接管输入法\n", suppressed ? "PASS" : "FAIL");
    return suppressed ? 0 : 1;
}

// ---------------------------------------------------------------------
// 管道长距离传输自检（--selftest-pipe）
// ---------------------------------------------------------------------
// 起因（v1.3.5 补丁 P）：BFS 路由把"最大跳数"当成"BFS 总展开次数"用，
// 直线管道上 BFS 每前进一格就吃掉一次预算 → 100 格左右的长走线末端永远收不到货，
// 支路/死胡同还会额外吃预算、使实际可达距离更短。本自检把这个静默截断钉死：
//   ① 直线：5 / 50 / 90 / 99 / 100 / 101 / 110 / 120 / 150 格，全部必须送达；
//   ② 主干 + 死胡同支路：主干 60 格、3 条 20 格死胡同，末端容器必须照样收到（支路不吃预算）；
//   ③ 蛇形长链：约 380 格连续管道（远超一屏宽度），必须送达；
//   ④ 负例：末端没有任何容器时，物品必须**留在管道里**（背压），不许凭空消失。
// 退出码 0=全部通过，1=存在失败项。
int runSelftestPipe() {
    Game game;   // 初始化窗口/资源/地形/矿点
    int fails = 0;
    auto report = [&](bool ok, const std::string& name, size_t got, size_t left) {
        if (!ok) ++fails;
        std::printf("[%s] %s (收到=%zu 残留=%zu)\n", ok ? "PASS" : "FAIL", name.c_str(), got, left);
    };
    // 在 (x,y) 往右铺 len 格管道，末端 (x+len,y) 放储物桶
    auto buildLine = [&](int x, int y, int len, entt::entity& src, entt::entity& dst) {
        for (int k = 0; k <= len; ++k)
            if (!game.canPlace(x + k, y, k == len ? cfg::BuildingType::Bucket
                                                  : cfg::BuildingType::Pipe)) return false;
        src = game.placeBuilding(x, y, cfg::BuildingType::Pipe, 0, false);
        for (int k = 1; k < len; ++k)
            game.placeBuilding(x + k, y, cfg::BuildingType::Pipe, 0, false);
        dst = game.placeBuilding(x + len, y, cfg::BuildingType::Bucket, 0, false);
        return src != entt::null && dst != entt::null;
    };
    // 往 src 缓冲塞 1 件铁矿石，跑 steps 个路由周期，返回(桶收到, 管道残留)
    auto tryRoute = [&](entt::entity src, entt::entity dst, int steps) {
        game.reg.get<Pipe>(src).buffer.push_back(cfg::ItemType::IronOre);
        for (int i = 0; i < steps; ++i) PipeSystem::updatePipes(game, 0.3f);
        const size_t got  = (dst != entt::null && game.reg.valid(dst))
                                ? game.reg.get<Bucket>(dst).items.size() : 0;
        const size_t left = game.reg.get<Pipe>(src).buffer.size();
        return std::pair<size_t, size_t>{got, left};
    };

    // ---- ① 直线：不同长度（放置失败就换下一行，避开矿点/路径）----
    const int lens[] = {5, 50, 90, 99, 100, 101, 110, 120, 150};
    int yCur = 12;
    for (int len : lens) {
        entt::entity src = entt::null, dst = entt::null;
        bool built = false;
        for (int t = 0; t < 12 && !built; ++t, yCur += 3) {
            if (yCur >= 66) break;
            built = buildLine(6, yCur, len, src, dst);
        }
        const std::string name = "直线管道 " + std::to_string(len) + " 格";
        if (!built) { std::printf("[SKIP] %s（没找到足够空地）\n", name.c_str()); continue; }
        const auto [got, left] = tryRoute(src, dst, 40);
        report(got > 0, name, got, left);
    }

    // ---- ② 主干 + 死胡同支路：支路不得吃掉可达距离 ----
    {
        const int y = 72, x = 6, trunk = 60, spur = 20;
        bool free = true;
        for (int k = 0; k <= trunk && free; ++k)
            if (!game.canPlace(x + k, y, cfg::BuildingType::Pipe)) free = false;
        for (int k = 0; k < trunk && free; k += 6)
            for (int s = 1; s <= spur && free; ++s)
                if (!game.canPlace(x + k, y + s, cfg::BuildingType::Pipe)) free = false;
        if (free && !game.canPlace(x + trunk, y, cfg::BuildingType::Bucket)) free = false;
        if (!free) {
            std::printf("[SKIP] 主干+支路（没找到足够空地）\n");
        } else {
            entt::entity src = game.placeBuilding(x, y, cfg::BuildingType::Pipe, 0, false);
            for (int k = 1; k < trunk; ++k)
                game.placeBuilding(x + k, y, cfg::BuildingType::Pipe, 0, false);
            for (int k = 0; k < trunk; k += 6)
                for (int s = 1; s <= spur; ++s)
                    game.placeBuilding(x + k, y + s, cfg::BuildingType::Pipe, 0, false);
            entt::entity dst = game.placeBuilding(x + trunk, y, cfg::BuildingType::Bucket, 0, false);
            const auto [got, left] = tryRoute(src, dst, 40);
            report(got > 0, "主干 60 格 + 3 条 20 格死胡同支路", got, left);
        }
    }

    // ---- ③ 蛇形长链：远超一屏宽度的连续管道 ----
    {
        std::vector<sf::Vector2i> cells;
        const int xLo = 6, xHi = 190, yLo = 120, yHi = 178, want = 380;
        for (int y = yLo, dir = 1; y <= yHi && static_cast<int>(cells.size()) < want; ++y, dir = -dir)
            for (int x = (dir > 0 ? xLo : xHi);
                 (dir > 0 ? x <= xHi : x >= xLo) && static_cast<int>(cells.size()) < want; x += dir)
                cells.emplace_back(x, y);
        bool free = !cells.empty();
        for (const auto& c : cells)
            if (!game.canPlace(c.x, c.y, cfg::BuildingType::Pipe)) free = false;
        const sf::Vector2i tail{cells.back().x, cells.back().y + 1};
        if (free && !game.canPlace(tail.x, tail.y, cfg::BuildingType::Bucket)) free = false;
        if (!free) {
            std::printf("[SKIP] 蛇形长链（没找到足够空地）\n");
        } else {
            entt::entity src = entt::null;
            for (const auto& c : cells) {
                const entt::entity e = game.placeBuilding(c.x, c.y, cfg::BuildingType::Pipe, 0, false);
                if (src == entt::null) src = e;
            }
            entt::entity dst = game.placeBuilding(tail.x, tail.y, cfg::BuildingType::Bucket, 0, false);
            const auto [got, left] = tryRoute(src, dst, 60);
            report(got > 0, "蛇形连续管道 " + std::to_string(cells.size()) + " 格", got, left);
        }
    }

    // ---- ④ 负例：末端没有容器 → 物品必须留下（背压，不许消失）----
    {
        const int x = 6, len = 8;
        entt::entity src = entt::null, dst = entt::null;
        bool built = false;
        for (int t = 0; t < 12 && !built; ++t) {
            const int y = 42 + t * 2;
            if (y >= 62) break;
            bool rowFree = true;
            for (int k = 0; k <= len && rowFree; ++k)
                if (!game.canPlace(x + k, y, cfg::BuildingType::Pipe)) rowFree = false;
            if (!rowFree) continue;
            src = game.placeBuilding(x, y, cfg::BuildingType::Pipe, 0, false);
            for (int k = 1; k <= len; ++k)
                game.placeBuilding(x + k, y, cfg::BuildingType::Pipe, 0, false);
            built = (src != entt::null);
        }
        if (!built) {
            std::printf("[SKIP] 背压负例（没找到空地）\n");
        } else {
            const auto [got, left] = tryRoute(src, dst, 20);
            report(got == 0 && left == 1, "末端无容器时物品留在管道内（背压）", got, left);
        }
    }

    std::printf("管道长传输自检: %s\n", fails == 0 ? "全部通过" : "存在失败项");
    return fails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    // ⚠ 必须在创建任何窗口之前：开启高 DPI 感知（高刷屏常配 125%/150% 缩放）
    enableHighDpiAwareness();

    // ⚠ 同样必须在建窗之前：进程级禁用输入法。游戏无任何文本输入，而「无边框全屏 +
    // HWND_TOPMOST」窗口一旦让输入法弹候选窗/语言栏，两者会互抢 Z 序与前台焦点 →
    // 持续闪屏黑屏、整个系统无法操作（只能切虚拟桌面结束进程）。
    gset::disableImeForProcess();

    // 最先读取用户设置：入口系统与游戏窗口都据此决定显示模式
    // 首次运行（或配置文件损坏）时写入一份默认设置，方便玩家直接编辑
    if (!gset::load("saves/settings.json")) gset::save("saves/settings.json");

    // 黑屏应急：强制窗口化 + 垂直同步，绕开"无边框全屏 + 置顶窗口"这条路径。
    // 刻意不 save()：只在本次运行生效，玩家的 saves/settings.json 保持原样。
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--safe-mode" || a == "--windowed") {
            gset::get().displayMode = gset::DisplayMode::Window1280x720;
            gset::get().frameMode   = gset::FrameMode::Vsync;
        } else if (a == "--selftest-save") {
            return runSelftestSave();
        } else if (a == "--selftest-ime") {
            return runSelftestIme();
        } else if (a == "--selftest-pipe") {
            return runSelftestPipe();
        }
    }

    // ---- 游戏入口：启动动画 → 标题/主菜单 → 设置 → 加载 → 游戏 ----
    // 循环：游戏内暂停面板选「返回主界面」后重新进入入口系统（等价于回到上一级菜单）
    while (true) {
        EntryAction action = EntryAction::NewGame;
        int slot = -1;                      // Continue 时要载入的槽位（0..9）
        {
            EntrySystem entry;              // 自带窗口；离开作用域即销毁
            action = entry.run();
            slot = entry.chosenSlot();      // 必须在销毁前取出
        }
        if (action == EntryAction::Quit) return 0;

        {
            // 两种模式完全独立、互不嵌套：由主菜单各自的入口按钮决定走哪条流程
            const GameMode mode = (action == EntryAction::Tutorial) ? GameMode::Tutorial
                                                                    : GameMode::Normal;
            Game game(mode);                // 初始化窗口/资源/地形/矿点
            if (action == EntryAction::Continue) game.loadFromSlot(slot);   // 普通关卡 · 读取选中槽位
            game.run();                     // 游戏主循环
            if (!game.returnToMenu) return 0;   // 关窗 = 正常退出
        }
        // returnToMenu = true → 回到主菜单，再走一遍入口系统
    }
}