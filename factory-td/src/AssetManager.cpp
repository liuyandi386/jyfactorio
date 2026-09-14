// =====================================================================
// AssetManager.cpp —— 资源管理实现
// 移植自 Python sprites.py：PNG加载优先，缺失时程序化生成回退。
// =====================================================================
#include "AssetManager.h"
#include <cmath>
#include <filesystem>
#include <random>
#include <utility>

namespace fs = std::filesystem;

namespace {
// 8方向名称（tower_basic_up.png 等）
constexpr const char* DIR_NAMES_8[8] = {"up", "up_right", "right", "down_right",
                                        "down", "down_left", "left", "up_left"};
// 4方向名称
constexpr const char* DIR_NAMES_4[4] = {"up", "right", "down", "left"};
// 塔类型文件名前缀
const char* towerPrefix(cfg::TurretType t) {
    switch (t) {
        case cfg::TurretType::Basic:    return "basic";
        case cfg::TurretType::Rapid:    return "rapid";
        case cfg::TurretType::Sniper:   return "sniper";
        case cfg::TurretType::Electric: return "electric";
    }
    return "basic";
}

// ---- 程序化贴图辅助：立体倒角面板 + 垂直渐变 ----

/// 倒角面板：填充色 + 上/左高光 + 下/右阴影（工业风立体感）
void drawBevelPanel(sf::RenderTarget& rt, float x, float y, float w, float h,
                    sf::Color fill, sf::Color light, sf::Color dark, float t = 1.5f) {
    sf::RectangleShape body({w, h});
    body.setPosition(x, y);
    body.setFillColor(fill);
    rt.draw(body);
    sf::RectangleShape ht({w, t}); ht.setPosition(x, y); ht.setFillColor(light); rt.draw(ht);
    sf::RectangleShape hl({t, h}); hl.setPosition(x, y); hl.setFillColor(light); rt.draw(hl);
    sf::RectangleShape hb({w, t}); hb.setPosition(x, y + h - t); hb.setFillColor(dark); rt.draw(hb);
    sf::RectangleShape hr({t, h}); hr.setPosition(x + w - t, y); hr.setFillColor(dark); rt.draw(hr);
}

/// 垂直渐变矩形
void drawGradient(sf::RenderTarget& rt, float x, float y, float w, float h,
                  sf::Color top, sf::Color bottom) {
    sf::VertexArray va(sf::Quads, 4);
    va[0] = sf::Vertex({x, y}, top);
    va[1] = sf::Vertex({x + w, y}, top);
    va[2] = sf::Vertex({x + w, y + h}, bottom);
    va[3] = sf::Vertex({x, y + h}, bottom);
    rt.draw(va);
}

/// 颜色明暗调节（+delta提亮 / -delta压暗，自动钳制）
sf::Color shade(sf::Color c, int delta) {
    auto cl = [delta](sf::Uint8 v) {
        const int n = static_cast<int>(v) + delta;
        return static_cast<sf::Uint8>(n < 0 ? 0 : (n > 255 ? 255 : n));
    };
    return sf::Color(cl(c.r), cl(c.g), cl(c.b), c.a);
}

/// 四角铆钉（工业金属拼接感）
void drawRivets(sf::RenderTarget& rt, float x0, float y0, float x1, float y1,
                float r, sf::Color c) {
    sf::CircleShape v(r);
    v.setFillColor(c);
    v.setPosition(x0 - r, y0 - r); rt.draw(v);
    v.setPosition(x1 - r, y0 - r); rt.draw(v);
    v.setPosition(x0 - r, y1 - r); rt.draw(v);
    v.setPosition(x1 - r, y1 - r); rt.draw(v);
}

/// 散热栅栏（一组竖条通风口）
void drawVents(sf::RenderTarget& rt, float x, float y, float w, float h,
               int n, sf::Color c) {
    const float gap = w / static_cast<float>(n * 2 - 1);
    for (int i = 0; i < n; ++i) {
        sf::RectangleShape v({gap, h});
        v.setPosition(x + i * gap * 2.0f, y);
        v.setFillColor(c);
        rt.draw(v);
    }
}

/// 指示灯（外圈光晕 + 高亮灯芯）
void drawLamp(sf::RenderTarget& rt, float cx, float cy, float r, sf::Color c) {
    sf::CircleShape halo(r * 1.8f);
    halo.setPosition(cx - r * 1.8f, cy - r * 1.8f);
    halo.setFillColor(sf::Color(c.r, c.g, c.b, 70));
    rt.draw(halo);
    sf::CircleShape body(r);
    body.setPosition(cx - r, cy - r);
    body.setFillColor(c);
    body.setOutlineColor(sf::Color(20, 20, 22));
    body.setOutlineThickness(1.0f);
    rt.draw(body);
    sf::CircleShape core(r * 0.5f);
    core.setPosition(cx - r * 0.5f, cy - r * 0.5f);
    core.setFillColor(sf::Color(255, 255, 255, 220));
    rt.draw(core);
}

/// 发光核心（外光晕 + 内亮核，径向发光感）
void drawGlowCore(sf::RenderTarget& rt, float cx, float cy, float r,
                  sf::Color halo, sf::Color core) {
    sf::CircleShape h(r * 1.6f);
    h.setPosition(cx - r * 1.6f, cy - r * 1.6f);
    h.setFillColor(sf::Color(halo.r, halo.g, halo.b, 90));
    rt.draw(h);
    sf::CircleShape m(r);
    m.setPosition(cx - r, cy - r);
    m.setFillColor(halo);
    rt.draw(m);
    sf::CircleShape c2(r * 0.55f);
    c2.setPosition(cx - r * 0.55f, cy - r * 0.55f);
    c2.setFillColor(core);
    rt.draw(c2);
}

/// 黄黑斜纹警告条（工业危险标识）
void drawHazard(sf::RenderTarget& rt, float x, float y, float w, float h,
                float stripe = 4.0f) {
    sf::RectangleShape bg({w, h});
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(232, 190, 40));
    rt.draw(bg);
    for (float sx = x - h; sx < x + w; sx += stripe * 2.0f) {
        sf::ConvexShape d;
        d.setPointCount(4);
        d.setPoint(0, {sx, y});
        d.setPoint(1, {sx + stripe, y});
        d.setPoint(2, {sx + stripe + h, y + h});
        d.setPoint(3, {sx + h, y + h});
        d.setFillColor(sf::Color(28, 28, 30));
        rt.draw(d);
    }
}

/// 输出方向箭头（指向输出面）
void drawDirArrow(sf::RenderTarget& rt, int d, int S, sf::Color c) {
    const float cx = S / 2.0f, cy = S / 2.0f;
    sf::ConvexShape arrow;
    arrow.setPointCount(3);
    switch (d) {
        case cfg::Dir::UP:
            arrow.setPoint(0, {cx, 3.0f});
            arrow.setPoint(1, {cx - 4.0f, 9.0f});
            arrow.setPoint(2, {cx + 4.0f, 9.0f});
            break;
        case cfg::Dir::RIGHT:
            arrow.setPoint(0, {S - 3.0f, cy});
            arrow.setPoint(1, {S - 9.0f, cy - 4.0f});
            arrow.setPoint(2, {S - 9.0f, cy + 4.0f});
            break;
        case cfg::Dir::DOWN:
            arrow.setPoint(0, {cx, S - 3.0f});
            arrow.setPoint(1, {cx - 4.0f, S - 9.0f});
            arrow.setPoint(2, {cx + 4.0f, S - 9.0f});
            break;
        default:
            arrow.setPoint(0, {3.0f, cy});
            arrow.setPoint(1, {9.0f, cy - 4.0f});
            arrow.setPoint(2, {9.0f, cy + 4.0f});
            break;
    }
    arrow.setFillColor(c);
    rt.draw(arrow);
}
} // namespace

// ---------------------------------------------------------------------
// 贴图键生成
// ---------------------------------------------------------------------
std::string AssetManager::towerKey(cfg::TurretType t, int dir8) {
    return std::string("tower_") + towerPrefix(t) + "_" + DIR_NAMES_8[dir8 % 8];
}

std::string AssetManager::machineKey(const char* name, int dir, bool rotated) {
    // 贴图后缀 == 逻辑朝向：PNG 文件由 organize_sprites.py 按"顺时针 angle=dir*90°"
    // 生成并命名为 DIR_NAMES_4[dir]，所以 machine_miner_right.png 里的箭头确实指向右。
    // 早期沿用 Python 的 DIR_NAMES_4_ROTATED 逆时针90°映射（(dir+3)%4），
    // 会让贴图比逻辑方向多偏 90°——这就是"机器贴图向左旋转90°"的根因。
    (void)rotated;   // 参数保留以兼容既有调用点，不再参与映射
    const int d = ((dir % 4) + 4) % 4;
    return std::string("machine_") + name + "_" + DIR_NAMES_4[d];
}

std::string AssetManager::oreKey(cfg::ItemType t) {
    switch (t) {
        case cfg::ItemType::IronOre:    return "ore_iron";
        case cfg::ItemType::CopperOre:  return "ore_copper";
        case cfg::ItemType::Coal:       return "ore_coal";
        case cfg::ItemType::GoldOre:    return "ore_gold";
        case cfg::ItemType::DiamondOre: return "ore_diamond";
        case cfg::ItemType::NickelOre:  return "ore_nickel";
        case cfg::ItemType::SilverOre:  return "ore_silver";
        case cfg::ItemType::LeadOre:    return "ore_lead";
        default:                        return "ore_iron";
    }
}

std::string AssetManager::enemyKey(cfg::EnemyType t) {
    switch (t) {
        case cfg::EnemyType::Basic: return "enemy_basic";
        case cfg::EnemyType::Fast:  return "enemy_fast";
        default:                    return "enemy_tank";
    }
}

// ---------------------------------------------------------------------
// 资源加载
// ---------------------------------------------------------------------
bool AssetManager::load(const std::string& assetDir) {
    const std::string sprites = assetDir + "/sprites";

    // ---- 地形 / 矿石 / 敌人 / 物品 ----
    loadTexture("grass", sprites + "/terrain/terrain_grass.png");
    loadTexture("path", sprites + "/terrain/terrain_path.png");
    loadTexture("ore_iron", sprites + "/ores/ore_iron.png");
    loadTexture("ore_copper", sprites + "/ores/ore_copper.png");
    loadTexture("ore_coal", sprites + "/ores/ore_coal.png");
    // 5种新矿石：PNG缺失时不插紫红回退（generateStaticTextures 会程序化生成，
    // 避免"已有贴图"导致程序化矿石被跳过而显示纯紫块）
    tryLoadTexture("ore_gold", sprites + "/ores/ore_gold.png");
    tryLoadTexture("ore_diamond", sprites + "/ores/ore_diamond.png");
    tryLoadTexture("ore_nickel", sprites + "/ores/ore_nickel.png");
    tryLoadTexture("ore_silver", sprites + "/ores/ore_silver.png");
    tryLoadTexture("ore_lead", sprites + "/ores/ore_lead.png");
    loadTexture("enemy_basic", sprites + "/enemies/enemy_basic.png");
    loadTexture("enemy_fast", sprites + "/enemies/enemy_fast.png");
    loadTexture("enemy_tank", sprites + "/enemies/enemy_tank.png");
    loadTexture("item_ammo", sprites + "/items/item_ammo.png");

    // ---- 塔（4种 × 8方向） ----
    for (int t = 0; t < 4; ++t)
        for (int d = 0; d < 8; ++d) {
            const auto type = static_cast<cfg::TurretType>(t);
            loadTexture(towerKey(type, d),
                        sprites + "/towers/tower_" + towerPrefix(type) + "_" + DIR_NAMES_8[d] + ".png");
        }

    // ---- 机器（4方向，贴图后缀 == 逻辑朝向） ----
    for (int d = 0; d < 4; ++d) {
        loadTexture(machineKey("miner", d, true), sprites + "/machines/machine_miner_" +
                       DIR_NAMES_4[d] + ".png");
        // 组装机复用原"弹药制造机"贴图
        loadTexture(machineKey("assembler", d, true), sprites + "/machines/machine_ammo_factory_" +
                       DIR_NAMES_4[d] + ".png");
        loadTexture(machineKey("generator", d, true), sprites + "/machines/machine_generator_" +
                       DIR_NAMES_4[d] + ".png");
    }
    loadTexture("machine_power_pole", sprites + "/machines/machine_power_pole.png");

    // ---- 储物桶（4方向直接映射） ----
    for (int d = 0; d < 4; ++d)
        loadTexture(machineKey("bucket", d, false), sprites + "/containers/container_bucket_" +
                       DIR_NAMES_4[d] + ".png");

    // ---- 程序化生成贴图（Python中原本就是动态绘制的） ----
    generateStaticTextures();
    std::array<cfg::FaceMode, 4> defFaces{};
    defFaces.fill(cfg::FaceMode::TRANSFER);
    rebuildWireTexture(defFaces);

    // ---- 字体（中文优先：打包字体 → 系统字体） ----
    // 打包字体：assets/fonts/simsun.ttc（宋体，随游戏分发，不依赖系统）
    const std::string bundledFonts[] = {
        assetDir + "/fonts/simsun.ttc",
        assetDir + "/fonts/msyh.ttc",
        assetDir + "/fonts/simhei.ttf",
    };
    for (const auto& f : bundledFonts)
        if (fs::exists(f) && font_.loadFromFile(f)) return true;
    const char* fontCandidates[] = {
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
    };
    for (const char* f : fontCandidates)
        if (font_.loadFromFile(f)) return true;
    // 全部失败时使用SFML内置字体（中文将显示为方块，不影响游戏逻辑）
    return false;
}

bool AssetManager::loadTexture(const std::string& key, const std::string& file) {
    if (tryLoadTexture(key, file)) return true;
    generateFallback(key);
    return false;
}

bool AssetManager::tryLoadTexture(const std::string& key, const std::string& file) {
    sf::Texture tex;
    if (fs::exists(file) && tex.loadFromFile(file)) {
        tex.setSmooth(false);
        textures_[key] = std::move(tex);
        return true;
    }
    return false;
}

void AssetManager::generateFallback(const std::string& key) {
    // Python sprites.py 同名回退：紫红色块
    sf::RenderTexture rt;
    rt.create(cfg::TILE_SIZE, cfg::TILE_SIZE);
    rt.clear(sf::Color(255, 0, 255));
    rt.display();
    textures_[key] = rt.getTexture();
}

void AssetManager::generateStaticTextures() {
    const int S = cfg::TILE_SIZE;

    // ---- 熔炉（4方向：金属外壳 + 铆钉 + 警告条 + 发光炉口） ----
    for (int d = 0; d < 4; ++d) {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        // 金属外壳（倒角 + 渐变）
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(58, 58, 64), sf::Color(92, 92, 100), sf::Color(30, 30, 36));
        drawGradient(rt, 5, 5, S - 10.0f, S - 10.0f,
                     sf::Color(102, 102, 110), sf::Color(70, 70, 78));
        // 四角铆钉
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.4f, sf::Color(150, 150, 158));
        // 顶部烟囱
        sf::RectangleShape chimney({6.0f, 8.0f});
        chimney.setPosition(cx - 3.0f, 2.0f);
        chimney.setFillColor(sf::Color(48, 48, 54));
        chimney.setOutlineColor(sf::Color(28, 28, 32));
        chimney.setOutlineThickness(1.0f);
        rt.draw(chimney);
        // 底部黄黑警告条纹
        drawHazard(rt, 7.0f, S - 8.0f, S - 14.0f, 4.0f);
        // 中央炉门 + 发光炉口
        sf::RectangleShape door({S - 12.0f, 9.0f});
        door.setPosition(6.0f, cy - 3.0f);
        door.setFillColor(sf::Color(40, 40, 46));
        door.setOutlineColor(sf::Color(26, 26, 30));
        door.setOutlineThickness(1.0f);
        rt.draw(door);
        drawGlowCore(rt, cx, cy + 1.0f, 3.2f, sf::Color(255, 140, 0), sf::Color(255, 240, 160));
        // 输出方向箭头
        drawDirArrow(rt, d, S, sf::Color(255, 152, 0));
        // 外框
        sf::RectangleShape border({static_cast<float>(S), static_cast<float>(S)});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(24, 24, 28));
        border.setOutlineThickness(1.0f);
        rt.draw(border);
        rt.display();
        textures_["furnace_d" + std::to_string(d)] = rt.getTexture();
    }

    // ---- 合金炉（4方向：青紫配色 + 电极 + 发光核心 + 警告条） ----
    for (int d = 0; d < 4; ++d) {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(48, 42, 58), sf::Color(74, 66, 92), sf::Color(24, 20, 32));
        drawGradient(rt, 5, 5, S - 10.0f, S - 10.0f,
                     sf::Color(80, 70, 100), sf::Color(54, 46, 72));
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.4f, sf::Color(130, 118, 160));
        // 顶部电极
        sf::RectangleShape rod({4.0f, 9.0f});
        rod.setPosition(cx - 2.0f, 1.0f);
        rod.setFillColor(sf::Color(170, 130, 255));
        rt.draw(rod);
        // 底部警告条
        drawHazard(rt, 7.0f, S - 8.0f, S - 14.0f, 4.0f);
        // 中央观察窗 + 高温发光核心
        sf::CircleShape window(5.0f, 8);
        window.setPosition(cx - 5.0f, cy - 4.0f);
        window.setFillColor(sf::Color(22, 26, 34));
        window.setOutlineColor(sf::Color(180, 140, 255));
        window.setOutlineThickness(1.5f);
        rt.draw(window);
        drawGlowCore(rt, cx, cy + 1.0f, 2.6f, sf::Color(150, 90, 255), sf::Color(230, 255, 255));
        // 输出方向箭头
        drawDirArrow(rt, d, S, sf::Color(0, 220, 255));
        sf::RectangleShape border({static_cast<float>(S), static_cast<float>(S)});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(24, 24, 28));
        border.setOutlineThickness(1.0f);
        rt.draw(border);
        rt.display();
        textures_["alloy_furnace_d" + std::to_string(d)] = rt.getTexture();
    }

    // ---- 电容库（电池单元组 + 指示灯 + 正负极） ----
    {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(42, 48, 58), sf::Color(68, 74, 86), sf::Color(20, 24, 32));
        drawGradient(rt, 4, 4, S - 8.0f, S - 8.0f,
                     sf::Color(74, 80, 92), sf::Color(48, 53, 64));
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.3f, sf::Color(140, 146, 158));
        // 顶部电量指示灯（发光）
        drawLamp(rt, S / 2.0f, 7.0f, 2.6f, sf::Color(0, 200, 255));
        // 三节电池单元（蓝色金属 + 高光）
        for (int i = 0; i < 3; ++i) {
            const float bx = 6.0f + i * (S - 22.0f) / 3.0f + i * 2.0f;
            const float bw = (S - 22.0f) / 3.0f;
            sf::RectangleShape cell({bw, S - 22.0f});
            cell.setPosition(bx, 12.0f);
            cell.setFillColor(sf::Color(0, 120, 210));
            cell.setOutlineColor(sf::Color(0, 60, 120));
            cell.setOutlineThickness(1.0f);
            rt.draw(cell);
            sf::RectangleShape cap({bw, 3.0f});
            cap.setPosition(bx, 12.0f);
            cap.setFillColor(sf::Color(120, 190, 255));
            rt.draw(cap);
        }
        // 正负极标识
        sf::CircleShape plus(2.0f);
        plus.setPosition(S / 2.0f - 8.0f, S - 6.0f);
        plus.setFillColor(sf::Color(255, 70, 70));
        rt.draw(plus);
        sf::CircleShape minus(2.0f);
        minus.setPosition(S / 2.0f + 6.0f, S - 6.0f);
        minus.setFillColor(sf::Color(40, 60, 220));
        rt.draw(minus);
        rt.display();
        textures_["capacitor"] = rt.getTexture();
    }

    // ---- 燃煤发电机（涡轮 + 烟囱 + 火焰 + 警告条） ----
    {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(52, 52, 58), sf::Color(80, 80, 88), sf::Color(26, 26, 32));
        drawGradient(rt, 4, 4, S - 8.0f, S - 8.0f,
                     sf::Color(84, 84, 90), sf::Color(58, 58, 64));
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.3f, sf::Color(150, 150, 158));
        // 顶部烟囱
        sf::RectangleShape chimney({6.0f, 9.0f});
        chimney.setPosition(S / 2.0f + 3.0f, 1.0f);
        chimney.setFillColor(sf::Color(78, 78, 84));
        chimney.setOutlineColor(sf::Color(40, 40, 44));
        chimney.setOutlineThickness(1.0f);
        rt.draw(chimney);
        // 左侧排气栅栏
        drawVents(rt, 6.0f, 12.0f, S - 22.0f, 4.0f, 4, sf::Color(30, 30, 34));
        // 底部警告条
        drawHazard(rt, 7.0f, S - 8.0f, S - 14.0f, 4.0f);
        // 涡轮（旋转叶片感：圆 + 十字叶片）
        const float cx = S / 2.0f - 1.0f, cy = S / 2.0f + 2.0f;
        sf::CircleShape turbine(5.0f, 8);
        turbine.setPosition(cx - 5.0f, cy - 5.0f);
        turbine.setFillColor(sf::Color(40, 40, 46));
        turbine.setOutlineColor(sf::Color(140, 140, 150));
        turbine.setOutlineThickness(1.2f);
        rt.draw(turbine);
        sf::RectangleShape blade1({9.0f, 2.0f});
        blade1.setPosition(cx - 4.5f, cy - 1.0f);
        blade1.setFillColor(sf::Color(160, 160, 168));
        rt.draw(blade1);
        sf::RectangleShape blade2({2.0f, 9.0f});
        blade2.setPosition(cx - 1.0f, cy - 4.5f);
        blade2.setFillColor(sf::Color(160, 160, 168));
        rt.draw(blade2);
        // 燃烧室火焰（发光）
        drawGlowCore(rt, cx - 4.0f, S - 5.0f, 2.4f, sf::Color(255, 170, 0), sf::Color(255, 250, 190));
        // 运行指示灯
        drawLamp(rt, S - 7.0f, 6.0f, 1.8f, sf::Color(0, 230, 120));
        rt.display();
        textures_["power_generator"] = rt.getTexture();
    }

    // ---- 采矿场（L1/L2/L3/虚空，4方向：钻头 + 履带 + 发光核心 + 等级标识） ----
    struct MinerStyle {
        const char* name;
        sf::Color body, accent, arrow;
        int level;   // 0=L1, 2=L2, 3=L3, 4=虚空
    };
    const MinerStyle styles[4] = {
        {"miner",      sf::Color(50, 64, 70),   sf::Color(0, 190, 150),  sf::Color(255, 152, 0), 0},
        {"miner_l2",   sf::Color(40, 70, 80),   sf::Color(0, 180, 200),  sf::Color(255, 152, 0), 2},
        {"miner_l3",   sf::Color(50, 40, 80),   sf::Color(150, 80, 255), sf::Color(255, 152, 0), 3},
        {"miner_void", sf::Color(46, 20, 56),   sf::Color(200, 60, 255), sf::Color(120, 255, 160), 4},
    };
    for (const auto& st : styles) {
        for (int d = 0; d < 4; ++d) {
            const int texDir = d;   // 箭头朝向 == 逻辑方向 == 贴图键后缀
            sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
            const float cx = S / 2.0f, cy = S / 2.0f;
            // 金属外壳（倒角 + 渐变）
            drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                           st.body, shade(st.body, 34), shade(st.body, -30));
            drawGradient(rt, 5, 5, S - 10.0f, S - 10.0f,
                         shade(st.body, 22), shade(st.body, -12));
            drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.3f, shade(st.body, 55));
            // 底部履带
            sf::RectangleShape track({S - 8.0f, 4.0f});
            track.setPosition(4.0f, S - 7.0f);
            track.setFillColor(sf::Color(24, 24, 28));
            rt.draw(track);
            for (int i = 0; i < 6; ++i) {
                sf::RectangleShape tooth({2.0f, 2.0f});
                tooth.setPosition(5.0f + i * 4.0f, S - 9.0f);
                tooth.setFillColor(sf::Color(70, 70, 76));
                rt.draw(tooth);
            }
            // 底部钻头（朝下三角锥）
            sf::ConvexShape drill;
            drill.setPointCount(3);
            drill.setPoint(0, {cx, 14.0f});
            drill.setPoint(1, {cx - 3.5f, 9.0f});
            drill.setPoint(2, {cx + 3.5f, 9.0f});
            drill.setFillColor(sf::Color(150, 150, 158));
            rt.draw(drill);
            // 中心发光核心
            if (st.level == 4) {
                sf::CircleShape ring(S / 3.4f);
                ring.setPosition(cx - S / 3.4f, cy - S / 3.4f - 1.0f);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(st.accent);
                ring.setOutlineThickness(1.5f);
                rt.draw(ring);
            }
            drawGlowCore(rt, cx, cy - 2.0f, 3.0f, st.accent, sf::Color(240, 255, 255));
            // 等级标识（右上角点数，虚空不画）
            if (st.level != 4) {
                const int marks = st.level == 0 ? 1 : st.level;
                for (int i = 0; i < marks; ++i) {
                    sf::RectangleShape m({3.0f, 3.0f});
                    m.setPosition(S - 8.0f, 5.0f + i * 6.0f);
                    m.setFillColor(st.accent);
                    rt.draw(m);
                }
            }
            // 输出方向箭头
            drawDirArrow(rt, texDir, S, st.arrow);
            // 外框
            sf::RectangleShape border({static_cast<float>(S), static_cast<float>(S)});
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineColor(sf::Color(24, 24, 28));
            border.setOutlineThickness(1.0f);
            rt.draw(border);
            rt.display();
            textures_[machineKey(st.name, d, true)] = rt.getTexture();
        }
    }

    // ---- 8 种矿石（程序化统一重画，覆盖旧 PNG：岩石基底 + 晶体矿团） ----
    struct OreStyle { const char* key; sf::Color main; };
    const OreStyle ores[8] = {
        {"ore_iron",    sf::Color(130, 128, 146)},
        {"ore_copper",  sf::Color(196, 116, 58)},
        {"ore_coal",    sf::Color(54, 54, 58)},
        {"ore_gold",    sf::Color(255, 210, 40)},
        {"ore_diamond", sf::Color(0, 225, 245)},
        {"ore_nickel",  sf::Color(150, 180, 150)},
        {"ore_silver",  sf::Color(208, 210, 220)},
        {"ore_lead",    sf::Color(104, 104, 122)},
    };
    for (const auto& o : ores) {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float c = S / 2.0f;
        // 岩石基底
        sf::CircleShape rock(S / 3.0f, 7);
        rock.setPosition(c - S / 3.0f, c - S / 3.5f);
        rock.setFillColor(sf::Color(60, 56, 52));
        rock.setOutlineColor(sf::Color(40, 36, 34));
        rock.setOutlineThickness(1.0f);
        rt.draw(rock);
        // 主矿团（晶体）
        sf::CircleShape blob(S / 4.0f, 6);
        blob.setPosition(c - S / 4.0f, c - S / 4.0f);
        blob.setFillColor(o.main);
        blob.setOutlineColor(shade(o.main, -55));
        blob.setOutlineThickness(1.5f);
        rt.draw(blob);
        // 次亮分面
        sf::CircleShape facet(S / 7.0f, 5);
        facet.setPosition(c - S / 5.0f, c - S / 4.4f);
        facet.setFillColor(shade(o.main, 45));
        rt.draw(facet);
        // 高光
        sf::CircleShape shine(S / 11.0f);
        shine.setPosition(c - S / 7.0f, c - S / 5.0f);
        shine.setFillColor(sf::Color(255, 255, 255, 210));
        rt.draw(shine);
        // 暗色碎块（矿石质感）
        sf::CircleShape chip(S / 9.0f, 5);
        chip.setPosition(c + S / 8.0f, c + S / 8.0f);
        chip.setFillColor(shade(o.main, -45));
        rt.draw(chip);
        rt.display();
        textures_[o.key] = rt.getTexture();
    }
    // ---- 地形（草地/路径：多层次写实） ----
    {
        std::mt19937 rng(20260815);
        // 草地：深绿基底 + 大块明暗草斑 + 细草点
        {
            sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color(56, 112, 52));
            for (int i = 0; i < 26; ++i) {
                const float x = static_cast<float>(rng() % S);
                const float y = static_cast<float>(rng() % S);
                const int r = 4 + static_cast<int>(rng() % 6);
                sf::CircleShape patch(static_cast<float>(r));
                patch.setPosition(x - r, y - r);
                patch.setFillColor((i % 2) ? sf::Color(66, 128, 60)
                                           : sf::Color(46, 92, 44));
                rt.draw(patch);
            }
            for (int i = 0; i < 180; ++i) {
                const float x = static_cast<float>(rng() % S);
                const float y = static_cast<float>(rng() % S);
                const float w = 1.0f + static_cast<float>(rng() % 2);
                sf::RectangleShape sp({w, w});
                sp.setPosition(x, y);
                sp.setFillColor((i % 2) ? sf::Color(86, 150, 72) : sf::Color(40, 84, 40));
                rt.draw(sp);
            }
            rt.display();
            textures_["grass"] = rt.getTexture();
        }
        // 路径：泥土基底 + 大块明暗 + 碎石 + 车辙 + 边缘压暗
        {
            sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color(148, 122, 92));
            for (int i = 0; i < 20; ++i) {
                const float x = static_cast<float>(rng() % S);
                const float y = static_cast<float>(rng() % S);
                const int r = 4 + static_cast<int>(rng() % 5);
                sf::CircleShape patch(static_cast<float>(r));
                patch.setPosition(x - r, y - r);
                patch.setFillColor((i % 2) ? sf::Color(160, 134, 100)
                                           : sf::Color(128, 104, 78));
                rt.draw(patch);
            }
            for (int i = 0; i < 90; ++i) {
                const float x = static_cast<float>(rng() % S);
                const float y = static_cast<float>(rng() % S);
                const float w = 1.0f + static_cast<float>(rng() % 2);
                sf::RectangleShape sp({w, w});
                sp.setPosition(x, y);
                sp.setFillColor((i % 3 == 0) ? sf::Color(110, 96, 78)
                                             : sf::Color(176, 148, 112));
                rt.draw(sp);
            }
            sf::RectangleShape rut({static_cast<float>(S), 3.0f});
            rut.setFillColor(sf::Color(96, 80, 60, 110));
            rut.setPosition(0, S / 3.0f);
            rt.draw(rut);
            rut.setPosition(0, S * 2.0f / 3.0f);
            rt.draw(rut);
            sf::RectangleShape edge({static_cast<float>(S), 1.0f});
            edge.setFillColor(sf::Color(52, 42, 32, 130));
            edge.setPosition(0, 0);
            rt.draw(edge);
            edge.setPosition(0, S - 1.0f);
            rt.draw(edge);
            sf::RectangleShape edgeV({1.0f, static_cast<float>(S)});
            edgeV.setFillColor(sf::Color(52, 42, 32, 130));
            edgeV.setPosition(0, 0);
            rt.draw(edgeV);
            edgeV.setPosition(S - 1.0f, 0);
            rt.draw(edgeV);
            rt.display();
            textures_["path"] = rt.getTexture();
        }
    }

    // ---- 储物桶（4方向：金属桶 + 箍环 + 顶盖，覆盖旧 PNG） ----
    for (int d = 0; d < 4; ++d) {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        sf::CircleShape body(S / 3.1f, 14);
        body.setPosition(cx - S / 3.1f, cy - S / 3.1f + 3.0f);
        body.setFillColor(sf::Color(156, 100, 54));
        body.setOutlineColor(sf::Color(96, 60, 30));
        body.setOutlineThickness(1.5f);
        rt.draw(body);
        sf::CircleShape lid(S / 3.6f, 14);
        lid.setPosition(cx - S / 3.6f, cy - S / 3.6f);
        lid.setFillColor(sf::Color(190, 126, 68));
        lid.setOutlineColor(sf::Color(120, 76, 40));
        lid.setOutlineThickness(1.0f);
        rt.draw(lid);
        sf::RectangleShape band1({static_cast<float>(S - 4.0f), 3.0f});
        band1.setPosition(2.0f, cy + 1.0f);
        band1.setFillColor(sf::Color(90, 56, 28));
        rt.draw(band1);
        sf::RectangleShape band2({static_cast<float>(S - 4.0f), 3.0f});
        band2.setPosition(2.0f, cy + 7.0f);
        band2.setFillColor(sf::Color(90, 56, 28));
        rt.draw(band2);
        sf::CircleShape shine(S / 12.0f);
        shine.setPosition(cx - S / 8.0f, cy - S / 6.0f);
        shine.setFillColor(sf::Color(255, 240, 210, 180));
        rt.draw(shine);
        rt.display();
        textures_[machineKey("bucket", d, false)] = rt.getTexture();
    }

    // ---- 组装机（4方向：机械外壳 + 齿轮 + 指示灯，覆盖旧 PNG） ----
    for (int d = 0; d < 4; ++d) {
        const int texDir = d;   // 箭头朝向 == 逻辑方向 == 贴图键后缀
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(56, 74, 60), sf::Color(82, 104, 88), sf::Color(30, 40, 32));
        drawGradient(rt, 5, 5, S - 10.0f, S - 10.0f,
                     sf::Color(82, 106, 88), sf::Color(56, 76, 62));
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.3f, sf::Color(120, 150, 128));
        // 中心齿轮
        sf::CircleShape gear(5.0f);
        gear.setPosition(cx - 5.0f, cy - 5.0f);
        gear.setFillColor(sf::Color(120, 130, 120));
        gear.setOutlineColor(sf::Color(60, 66, 60));
        gear.setOutlineThickness(1.2f);
        rt.draw(gear);
        sf::CircleShape hub(2.0f);
        hub.setPosition(cx - 2.0f, cy - 2.0f);
        hub.setFillColor(sf::Color(50, 54, 50));
        rt.draw(hub);
        const float tx[4] = {cx - 2.0f, cx + 5.0f, cx - 2.0f, cx - 7.0f};
        const float ty[4] = {cy - 9.0f, cy - 2.0f, cy + 6.0f, cy - 2.0f};
        for (int i = 0; i < 4; ++i) {
            sf::RectangleShape tooth({4.0f, 3.0f});
            tooth.setPosition(tx[i], ty[i]);
            tooth.setFillColor(sf::Color(120, 130, 120));
            rt.draw(tooth);
        }
        drawLamp(rt, cx, 6.0f, 1.8f, sf::Color(0, 230, 120));
        drawDirArrow(rt, texDir, S, sf::Color(0, 210, 120));
        sf::RectangleShape border({static_cast<float>(S), static_cast<float>(S)});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(24, 26, 24));
        border.setOutlineThickness(1.0f);
        rt.draw(border);
        rt.display();
        textures_[machineKey("assembler", d, true)] = rt.getTexture();
    }

    // ---- 旧版发电机（2×2 大功率，4方向，覆盖旧 PNG） ----
    for (int d = 0; d < 4; ++d) {
        const int texDir = d;   // 箭头朝向 == 逻辑方向 == 贴图键后缀
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        drawBevelPanel(rt, 2, 2, S - 4.0f, S - 4.0f,
                       sf::Color(60, 56, 50), sf::Color(90, 84, 74), sf::Color(32, 30, 26));
        drawGradient(rt, 5, 5, S - 10.0f, S - 10.0f,
                     sf::Color(90, 84, 74), sf::Color(62, 58, 52));
        drawRivets(rt, 5.0f, 5.0f, S - 5.0f, S - 5.0f, 1.4f, sf::Color(140, 130, 116));
        // 高压闪电标识
        sf::ConvexShape bolt;
        bolt.setPointCount(3);
        bolt.setPoint(0, {cx, 4.0f});
        bolt.setPoint(1, {cx - 4.0f, 11.0f});
        bolt.setPoint(2, {cx + 4.0f, 11.0f});
        bolt.setFillColor(sf::Color(255, 210, 0));
        rt.draw(bolt);
        // 涡轮
        sf::CircleShape turbine(5.0f, 8);
        turbine.setPosition(cx - 5.0f, cy - 4.0f);
        turbine.setFillColor(sf::Color(40, 40, 44));
        turbine.setOutlineColor(sf::Color(150, 150, 158));
        turbine.setOutlineThickness(1.2f);
        rt.draw(turbine);
        // 底部火焰
        drawGlowCore(rt, cx, S - 6.0f, 2.6f, sf::Color(255, 160, 0), sf::Color(255, 250, 190));
        drawDirArrow(rt, texDir, S, sf::Color(255, 200, 40));
        sf::RectangleShape border({static_cast<float>(S), static_cast<float>(S)});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(24, 22, 20));
        border.setOutlineThickness(1.0f);
        rt.draw(border);
        rt.display();
        textures_[machineKey("generator", d, true)] = rt.getTexture();
    }

    // ---- 电线杆（金属杆 + 横担 + 瓷绝缘子，覆盖旧 PNG） ----
    {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f;
        sf::RectangleShape pole({4.0f, static_cast<float>(S - 2.0f)});
        pole.setPosition(cx - 2.0f, 1.0f);
        pole.setFillColor(sf::Color(66, 60, 52));
        pole.setOutlineColor(sf::Color(40, 36, 32));
        pole.setOutlineThickness(1.0f);
        rt.draw(pole);
        sf::RectangleShape arm({static_cast<float>(S - 6.0f), 3.0f});
        arm.setPosition(3.0f, 8.0f);
        arm.setFillColor(sf::Color(76, 70, 60));
        rt.draw(arm);
        sf::CircleShape ins1(2.5f);
        ins1.setPosition(5.0f, 11.0f);
        ins1.setFillColor(sf::Color(200, 200, 200));
        rt.draw(ins1);
        sf::CircleShape ins2(2.5f);
        ins2.setPosition(S - 8.0f, 11.0f);
        ins2.setFillColor(sf::Color(200, 200, 200));
        rt.draw(ins2);
        sf::RectangleShape foot({8.0f, 4.0f});
        foot.setPosition(cx - 4.0f, S - 5.0f);
        foot.setFillColor(sf::Color(50, 46, 40));
        rt.draw(foot);
        rt.display();
        textures_["machine_power_pole"] = rt.getTexture();
    }

    // ---- 炮塔（4种 × 8方向：程序化重画，覆盖旧 PNG） ----
    struct TowerStyle { cfg::TurretType type; sf::Color base, gun; };
    const TowerStyle towers[4] = {
        {cfg::TurretType::Basic,    sf::Color(86, 92, 102), sf::Color(210, 82, 70)},
        {cfg::TurretType::Rapid,    sf::Color(64, 88, 94),  sf::Color(0, 198, 214)},
        {cfg::TurretType::Sniper,   sf::Color(82, 74, 100), sf::Color(172, 92, 220)},
        {cfg::TurretType::Electric, sf::Color(56, 80, 108), sf::Color(70, 150, 255)},
    };
    for (const auto& ts : towers) {
        sf::RenderTexture base; base.create(S, S); base.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        // 八角底座 + 内环 + 铆钉 + 炮座
        sf::CircleShape outer(12.5f, 8);
        outer.setPosition(cx - 12.5f, cy - 12.5f);
        outer.setFillColor(ts.base);
        outer.setOutlineColor(shade(ts.base, -40));
        outer.setOutlineThickness(1.5f);
        base.draw(outer);
        sf::CircleShape inner(8.0f, 8);
        inner.setPosition(cx - 8.0f, cy - 8.0f);
        inner.setFillColor(shade(ts.base, 24));
        inner.setOutlineColor(shade(ts.base, -28));
        inner.setOutlineThickness(1.0f);
        base.draw(inner);
        drawRivets(base, cx - 8.0f, cy - 8.0f, cx + 8.0f, cy + 8.0f, 1.2f, sf::Color(205, 205, 210));
        sf::CircleShape turret(5.5f, 8);
        turret.setPosition(cx - 5.5f, cy - 5.5f);
        turret.setFillColor(shade(ts.base, 8));
        turret.setOutlineColor(shade(ts.base, -32));
        turret.setOutlineThickness(1.2f);
        base.draw(turret);
        // 炮管（按塔类型）
        if (ts.type == cfg::TurretType::Rapid) {
            sf::RectangleShape b1({3.0f, 14.0f}); b1.setPosition(cx - 6.0f, cy - 16.0f); b1.setFillColor(ts.gun); base.draw(b1);
            sf::RectangleShape b2({3.0f, 14.0f}); b2.setPosition(cx + 3.0f, cy - 16.0f); b2.setFillColor(ts.gun); base.draw(b2);
            sf::CircleShape m1(2.0f); m1.setPosition(cx - 5.0f, cy - 18.0f); m1.setFillColor(sf::Color(255, 255, 240)); base.draw(m1);
            sf::CircleShape m2(2.0f); m2.setPosition(cx + 4.0f, cy - 18.0f); m2.setFillColor(sf::Color(255, 255, 240)); base.draw(m2);
        } else {
            const float len = (ts.type == cfg::TurretType::Sniper) ? 18.0f : 14.0f;
            sf::RectangleShape bl({4.0f, len});
            bl.setPosition(cx - 2.0f, cy - len);
            bl.setFillColor(ts.gun);
            base.draw(bl);
            if (ts.type == cfg::TurretType::Sniper) {
                sf::RectangleShape scope({3.0f, 7.0f});
                scope.setPosition(cx + 3.0f, cy - 13.0f);
                scope.setFillColor(sf::Color(60, 60, 66));
                base.draw(scope);
            }
            if (ts.type == cfg::TurretType::Electric) {
                for (float ey = cy - 13.0f; ey <= cy - 6.0f; ey += 3.5f) {
                    sf::CircleShape coil(2.6f);
                    coil.setPosition(cx - 2.6f, ey - 1.0f);
                    coil.setFillColor(sf::Color::Transparent);
                    coil.setOutlineColor(sf::Color(0, 220, 255));
                    coil.setOutlineThickness(1.1f);
                    base.draw(coil);
                }
            }
            sf::CircleShape muzzle(2.0f);
            muzzle.setPosition(cx - 2.0f, cy - len - 1.0f);
            muzzle.setFillColor(sf::Color(255, 255, 240));
            base.draw(muzzle);
        }
        base.display();
        // 8 方向旋转
        for (int d = 0; d < 8; ++d) {
            sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
            sf::Sprite sp(base.getTexture());
            sp.setOrigin(cx, cy);
            sp.setPosition(cx, cy);
            sp.setRotation(45.0f * d);
            rt.draw(sp);
            rt.display();
            textures_[towerKey(ts.type, d)] = rt.getTexture();
        }
    }

    // ---- 敌人（机械机甲，3种，覆盖旧 PNG） ----
    struct EnemyStyle { const char* key; sf::Color body, eye; bool heavy, fast; };
    const EnemyStyle enemies[3] = {
        {"enemy_basic", sf::Color(108, 114, 126), sf::Color(235, 70, 60),  false, false},
        {"enemy_fast",  sf::Color(0, 178, 190),   sf::Color(255, 210, 40), false, true},
        {"enemy_tank",  sf::Color(88, 92, 104),   sf::Color(255, 140, 40), true,  false},
    };
    for (const auto& en : enemies) {
        sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);
        const float cx = S / 2.0f, cy = S / 2.0f;
        const float bodyR = en.heavy ? S / 3.0f : S / 3.6f;
        // 两侧履带
        const float trW = 4.5f;
        sf::RectangleShape tl({trW, S - 6.0f});
        tl.setPosition(cx - bodyR - trW - 1.0f, 3.0f);
        tl.setFillColor(sf::Color(34, 34, 40));
        tl.setOutlineColor(sf::Color(60, 60, 68));
        tl.setOutlineThickness(1.0f);
        rt.draw(tl);
        sf::RectangleShape tr({trW, S - 6.0f});
        tr.setPosition(cx + bodyR + 1.0f, 3.0f);
        tr.setFillColor(sf::Color(34, 34, 40));
        tr.setOutlineColor(sf::Color(60, 60, 68));
        tr.setOutlineThickness(1.0f);
        rt.draw(tr);
        // 履带轮齿
        for (int i = 0; i < 4; ++i) {
            sf::RectangleShape tooth({3.0f, 3.0f});
            tooth.setPosition(cx - bodyR - trW - 2.5f, 6.0f + i * 6.0f);
            tooth.setFillColor(sf::Color(90, 90, 98));
            rt.draw(tooth);
            tooth.setPosition(cx + bodyR + 4.0f, 6.0f + i * 6.0f);
            rt.draw(tooth);
        }
        // 机身（圆机甲）
        sf::CircleShape body(bodyR, 12);
        body.setPosition(cx - bodyR, cy - bodyR);
        body.setFillColor(en.body);
        body.setOutlineColor(shade(en.body, -50));
        body.setOutlineThickness(1.5f);
        rt.draw(body);
        // 机身铆钉
        drawRivets(rt, cx - bodyR + 3.0f, cy - bodyR + 3.0f,
                   cx + bodyR - 3.0f, cy + bodyR - 3.0f, 1.2f, shade(en.body, 50));
        // 发光单眼
        drawGlowCore(rt, cx, cy - bodyR * 0.35f, 3.0f, en.eye, sf::Color(255, 255, 255));
        // fast：顶部速度尖角
        if (en.fast) {
            sf::ConvexShape spike;
            spike.setPointCount(3);
            spike.setPoint(0, {cx, cy - bodyR - 5.0f});
            spike.setPoint(1, {cx - 3.0f, cy - bodyR + 2.0f});
            spike.setPoint(2, {cx + 3.0f, cy - bodyR + 2.0f});
            spike.setFillColor(en.eye);
            rt.draw(spike);
        }
        // tank：底部黄黑警告条纹
        if (en.heavy) {
            drawHazard(rt, cx - bodyR + 2.0f, cy + bodyR - 5.0f,
                       bodyR * 2.0f - 4.0f, 3.0f);
        }
        rt.display();
        textures_[en.key] = rt.getTexture();
    }
}

// ---------------------------------------------------------------------
// 动态面配置贴图（面编辑后重绘）
// ---------------------------------------------------------------------
void AssetManager::rebuildWireTexture(const std::array<cfg::FaceMode, 4>& faces) {
    // 移植 power_wire.py refresh_appearance
    const int S = cfg::TILE_SIZE;
    const float half = S / 2.0f, third = S / 3.0f;
    const float lt = 2.0f, ind = 3.0f;
    sf::RenderTexture rt; rt.create(S, S); rt.clear(sf::Color::Transparent);

    const sf::Color faceColors[4] = {
        sf::Color(70, 70, 75),     // NONE
        sf::Color(0, 140, 255),    // INPUT
        sf::Color(180, 180, 40),   // TRANSFER
        sf::Color(255, 140, 0),    // OUTPUT
    };
    // 4个方向的面色条
    const sf::Vector2f stripPos[4] = {
        {half - lt, 0}, {S - third, half - lt}, {half - lt, S - third}, {0, half - lt}};
    const sf::Vector2f stripSize[4] = {
        {lt * 2, third}, {third, lt * 2}, {lt * 2, third}, {third, lt * 2}};
    // 指示方块位置
    const sf::Vector2f indPos[4] = {
        {half - ind, third - ind}, {S - third - ind, half - ind},
        {half - ind, S - third - ind}, {third - ind, half - ind}};

    for (int d = 0; d < 4; ++d) {
        const sf::Color c = faceColors[static_cast<uint8_t>(faces[d])];
        sf::RectangleShape strip(stripSize[d]);
        strip.setPosition(stripPos[d]);
        strip.setFillColor(c);
        rt.draw(strip);
        sf::RectangleShape box({ind * 2, ind * 2});
        box.setPosition(indPos[d]);
        box.setFillColor(c);
        rt.draw(box);
    }
    // 中心端子
    sf::CircleShape term(S / 5.0f);
    term.setPosition(half - S / 5.0f, half - S / 5.0f);
    term.setFillColor(sf::Color(160, 160, 170));
    rt.draw(term);
    sf::CircleShape hole(S / 5.0f - 2.0f);
    hole.setPosition(half - (S / 5.0f - 2.0f), half - (S / 5.0f - 2.0f));
    hole.setFillColor(sf::Color(30, 30, 35));
    rt.draw(hole);
    rt.display();
    wireTex_ = rt.getTexture();
}

const sf::Texture& AssetManager::get(const std::string& key) const {
    auto it = textures_.find(key);
    if (it != textures_.end()) return it->second;
    static sf::Texture empty;
    return empty;
}
