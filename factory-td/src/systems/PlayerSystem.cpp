// =====================================================================
// PlayerSystem.cpp —— 玩家系统实现
// 移植 GameScene 的交互：建筑放置/拆除、传送带两点式放置、
// 右键旋转与面配置编辑、WASD摄像机、滚轮缩放。
// =====================================================================
#include "systems/PlayerSystem.h"
#include <algorithm>
#include "Game.h"
#include "ui/GameUI.h"
#include "systems/EnemySystem.h"
#include "utils/Profiler.h"

namespace {
/// 世界坐标是否落在建筑矩形内（含2×2占地）
bool hitBuilding(const Game& g, entt::entity e, sf::Vector2f world) {
    const auto& b = g.reg.get<Building>(e);
    const float x0 = b.pos.x * cfg::TILE_SIZE;
    const float y0 = b.pos.y * cfg::TILE_SIZE;
    return world.x >= x0 && world.x < x0 + b.w * cfg::TILE_SIZE &&
           world.y >= y0 && world.y < y0 + b.h * cfg::TILE_SIZE;
}

/// 获取鼠标下的建筑（按Python从实体到基础设施的顺序）
entt::entity buildingAtWorld(const Game& g, sf::Vector2f world) {
    // 塔 → 机器 → 物流 → 电网
    const std::array<cfg::BuildingType, 19> order = {
        cfg::BuildingType::TowerBasic, cfg::BuildingType::TowerRapid,
        cfg::BuildingType::TowerSniper, cfg::BuildingType::TowerElectric,
        cfg::BuildingType::Miner, cfg::BuildingType::MinerL2,
        cfg::BuildingType::MinerL3, cfg::BuildingType::MinerVoid,
        cfg::BuildingType::Furnace, cfg::BuildingType::AlloyFurnace,
        cfg::BuildingType::Assembler, cfg::BuildingType::Generator,
        cfg::BuildingType::Bucket, cfg::BuildingType::Pipe,
        cfg::BuildingType::PowerGenerator, cfg::BuildingType::Capacitor,
        cfg::BuildingType::MeInterface, cfg::BuildingType::MeDrive,
        cfg::BuildingType::MeTerminal,
    };
    for (auto t : order) {
        for (auto e : g.reg.view<Building>()) {
            if (g.reg.get<Building>(e).type == t && hitBuilding(g, e, world)) return e;
        }
    }
    for (auto t : {cfg::BuildingType::PowerWire, cfg::BuildingType::Splitter,
                   cfg::BuildingType::PowerPole}) {
        for (auto e : g.reg.view<Building>()) {
            if (g.reg.get<Building>(e).type == t && hitBuilding(g, e, world)) return e;
        }
    }
    return entt::null;
}
} // namespace

// ---------------------------------------------------------------------
// 键盘事件
// ---------------------------------------------------------------------
static void handleKey(Game& g, const sf::Event::KeyEvent& key) {
    using K = sf::Keyboard;
    auto select = [&](cfg::BuildingType t) { g.selectBuilding(t); };
    switch (key.code) {
        case K::W: g.keys[0] = true; break;
        case K::A: g.keys[1] = true; break;
        case K::S: g.keys[2] = true; break;
        case K::D: g.keys[3] = true; break;
        case K::Space: g.paused = !g.paused; break;            // 空格暂停
        case K::R: g.rotateSelected(); break;                  // R旋转(Python空实现)
        case K::Num1: select(cfg::BuildingType::TowerBasic); break;
        case K::Num2: select(cfg::BuildingType::TowerElectric); break;
        case K::Num3: select(cfg::BuildingType::Miner); break;
        case K::Num4: select(cfg::BuildingType::Pipe); break;
        case K::Num5: select(cfg::BuildingType::Bucket); break;
        case K::Num6: select(cfg::BuildingType::Furnace); break;      // 新增熔炉
        case K::Num7: select(cfg::BuildingType::Assembler); break;    // 组装机(替代弹药机)
        case K::Num8: select(cfg::BuildingType::Generator); break;
        case K::Num9: select(cfg::BuildingType::PowerPole); break;
        case K::Num0: select(cfg::BuildingType::PowerGenerator); break;
        case K::Hyphen: select(cfg::BuildingType::Capacitor); break;  // '-'电容库
        case K::Equal: select(cfg::BuildingType::PowerWire); break;   // '='电力线缆
        case K::BackSlash: select(cfg::BuildingType::Splitter); break;// '\'分流器
        case K::Tab: g.cycleBuilding(); break;              // TAB循环切换
        case K::Z: EnemySystem::spawnEnemy(g, cfg::EnemyType::Basic); break;  // Z普通敌人
        case K::X: EnemySystem::spawnEnemy(g, cfg::EnemyType::Fast); break;   // X快速敌人
        case K::C: EnemySystem::spawnEnemy(g, cfg::EnemyType::Tank); break;   // C坦克敌人
        case K::U: EnemySystem::spawnEnemy(g); break;       // U同样可生成(兼容)
        case K::B: g.ui->toggleShop(); break;               // B打开/关闭商店
        case K::V: g.ui->toggleWorkbench(); break;          // V打开/关闭随身工作台
        case K::M: g.ui->toggleWorldMap(); break;           // M打开/关闭世界地图
        case K::H: g.ui->toggleHelp(); break;               // H打开/关闭说明书(新手引导)
        case K::F1: g.ui->toggleHelp(); break;              // F1同H（帮助键惯例）
        case K::F5: g.saveGame(); break;                    // F5保存
        case K::F9: g.loadGame(); break;                    // F9读档
        case K::Delete: g.deleteAtCursor(); break;          // DEL拆除并返还材料
        case K::Escape: {
            // 有面板/编辑器打开 → 先关掉（MC 习惯）；什么都没有 → 打开暂停面板
            const bool hadPanel = g.faceEditTarget != entt::null || g.ui->anyPanelOpen();
            g.faceEditTarget = entt::null;                  // ESC关闭面编辑器
            g.ui->hideRecipePopup();                        // ESC关闭配方菜单
            g.ui->closePanels();                            // ESC关闭商店/工作台/说明书等
            if (!hadPanel) g.ui->openPausePanel();          // 否则打开暂停面板
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------
// 事件分发
// ---------------------------------------------------------------------
void PlayerSystem::handleEvent(Game& g, const sf::Event& e) {
    FT_PROFILE;
    if (e.type == sf::Event::KeyPressed) {
        handleKey(g, e.key);
    } else if (e.type == sf::Event::KeyReleased) {
        using K = sf::Keyboard;
        if (e.key.code == K::W) g.keys[0] = false;
        else if (e.key.code == K::A) g.keys[1] = false;
        else if (e.key.code == K::S) g.keys[2] = false;
        else if (e.key.code == K::D) g.keys[3] = false;
    } else if (e.type == sf::Event::MouseButtonPressed) {
        const sf::Vector2f screen(static_cast<float>(e.mouseButton.x),
                                  static_cast<float>(e.mouseButton.y));
        const sf::Vector2f world = g.screenToWorld(screen);
        if (e.mouseButton.button == sf::Mouse::Left) {
            handleLeftClick(g, world, screen);
        } else if (e.mouseButton.button == sf::Mouse::Right) {
            handleRightClick(g, world, screen);
        }
    } else if (e.type == sf::Event::MouseWheelScrolled) {
        // 滚轮缩放（Python: button 4/5）
        if (e.mouseWheelScroll.delta > 0) g.camera.zoomIn();
        else g.camera.zoomOut();
    }
}

// ---------------------------------------------------------------------
// 摄像机
// ---------------------------------------------------------------------
void PlayerSystem::updateCamera(Game& g) {
    // Python _update_camera: WASD → move → 平滑更新
    float dx = 0.0f, dy = 0.0f;
    if (g.keys[0]) dy -= 1.0f;   // W 上
    if (g.keys[2]) dy += 1.0f;   // S 下
    if (g.keys[1]) dx -= 1.0f;   // A 左
    if (g.keys[3]) dx += 1.0f;   // D 右
    if (dx != 0.0f || dy != 0.0f) g.camera.move(dx, dy);
    g.camera.update();
}

// ---------------------------------------------------------------------
// 左键：放置建筑
// ---------------------------------------------------------------------
void PlayerSystem::handleLeftClick(Game& g, sf::Vector2f world, sf::Vector2f screen) {
    if (!g.hasSelection) return;
    // floor取格，与Python的 // 一致（负坐标安全）
    const sf::Vector2i tile = g.tileAt(world);
    const int tx = tile.x, ty = tile.y;

    const auto& info = cfg::BUILDING_INFOS[static_cast<size_t>(g.selected)];
    // 商店兑换的机器：一次免费放置（材料已在商店扣除）
    const bool freePlace = g.hasFreePlace && g.selected == g.freePlaceType;
    // 位置与资源检查（Python: 先查可放置，再查资源）
    if (!g.canPlace(tx, ty, g.selected)) return;
    if (!freePlace && !g.canAfford(g.selected)) {
        g.ui->showToast("资源不足!");
        return;
    }

    if (info.needDirection) {
        // 需要方向的建筑：弹出方向选择悬浮窗
        g.ui->showDirectionPopup(g.selected, {tx, ty}, screen);
    } else {
        // 无需方向的建筑：直接放置
        const entt::entity e = g.placeBuilding(tx, ty, g.selected, cfg::Dir::DOWN);
        if (e != entt::null) {
            if (freePlace) g.hasFreePlace = false;
            else g.deductCost(g.selected);
        }
    }
}

// ---------------------------------------------------------------------
// 右键：采矿机旋转输出面、面配置编辑器、组装机配方菜单
// （物品管道/分流器自动链接，无需右键交互）
// ---------------------------------------------------------------------
void PlayerSystem::handleRightClick(Game& g, sf::Vector2f world, sf::Vector2f screen) {
    const entt::entity e = buildingAtWorld(g, world);
    if (e == entt::null) return;
    auto& b = g.reg.get<Building>(e);

    switch (b.type) {
        case cfg::BuildingType::Miner:
        case cfg::BuildingType::MinerL2:
        case cfg::BuildingType::MinerL3:
        case cfg::BuildingType::MinerVoid: {
            // 采矿机旋转输出面（Python rotate_direction）
            g.reg.get<FaceConfig>(e).rotate();
            break;
        }
        case cfg::BuildingType::Assembler:
        case cfg::BuildingType::AlloyFurnace: {
            // 组装机/合金炉：打开配方选择菜单（选定后只按该配方合成，可重复选择）
            g.ui->showRecipePopup(e, screen);
            break;
        }
        case cfg::BuildingType::MeInterface: {
            // ME接口：打开输出过滤面板（锁定输出物品）
            g.ui->showFilterPanel(e);
            break;
        }
        case cfg::BuildingType::MeDrive:
        case cfg::BuildingType::MeTerminal: {
            // ME存储/终端：打开网络物品清单（AE2终端式）
            g.ui->showMePanel(e);
            break;
        }
        case cfg::BuildingType::PowerWire:
        case cfg::BuildingType::Furnace:
        case cfg::BuildingType::Bucket:
        case cfg::BuildingType::Generator:
        case cfg::BuildingType::PowerGenerator: {
            // 格雷科技式面配置编辑（管道/分流器自动链接，无面配置）
            g.faceEditTarget = e;
            break;
        }
        default:
            break;   // 塔/电线杆/电容库/管道/分流器无右键交互
    }
}

// ---------------------------------------------------------------------
// 放置预览
// ---------------------------------------------------------------------
void PlayerSystem::updatePreview(Game& g) {
    g.previewActive = g.hasSelection;
    if (!g.hasSelection) return;
    const auto mouse = sf::Mouse::getPosition(g.window);
    const sf::Vector2f world = g.screenToWorld({static_cast<float>(mouse.x),
                                                static_cast<float>(mouse.y)});
    g.previewTile = g.tileAt(world);   // floor取格，负坐标安全

    // 可放置性 + 资源（Python _draw_placement_preview）
    // 采矿场不再要求脚下有矿点：改为范围内采集（虚空采矿场连范围都不需要）
    g.previewCanBuild = g.canPlace(g.previewTile.x, g.previewTile.y, g.selected) &&
                        g.canAfford(g.selected);
}
