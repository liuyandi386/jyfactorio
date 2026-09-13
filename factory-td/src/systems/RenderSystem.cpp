// =====================================================================
// RenderSystem.cpp —— 渲染系统实现
//
// 优化手段：
//   1. 地形/传送带/物品用 sf::VertexArray 批量渲染（各一次draw call）
//   2. 只渲染摄像机视野内的内容（视锥剔除）
//   3. 电线/分流器按面配置直接绘制色块（Python程序化贴图的等价实现）
// =====================================================================
#include "systems/RenderSystem.h"
#include <algorithm>
#include <cmath>
#include "Game.h"
#include "systems/ItemSystem.h"
#include "systems/MeSystem.h"
#include "utils/Profiler.h"

namespace {

/// 视野内可见的瓦片范围（含1格余量）
struct VisibleRange {
    int x0, y0, x1, y1;
};
VisibleRange visibleTiles(const Game& g) {
    // 使用窗口实际尺寸（最大化/缩放后视野随之变化）
    const auto winSize = g.window.getSize();
    const auto tl = g.screenToWorld({0.0f, 0.0f});
    const auto br = g.screenToWorld({static_cast<float>(winSize.x),
                                     static_cast<float>(winSize.y)});
    VisibleRange r;
    // 用floor/ceil避免负坐标向零截断导致漏画
    r.x0 = std::max(0, static_cast<int>(std::floor(tl.x / cfg::TILE_SIZE)) - 1);
    r.y0 = std::max(0, static_cast<int>(std::floor(tl.y / cfg::TILE_SIZE)) - 1);
    r.x1 = std::min(g.grid.w - 1, static_cast<int>(std::ceil(br.x / cfg::TILE_SIZE)) + 1);
    r.y1 = std::min(g.grid.h - 1, static_cast<int>(std::ceil(br.y / cfg::TILE_SIZE)) + 1);
    return r;
}

/// 屏幕坐标是否可见（简单AABB剔除，窗口实际尺寸）
bool onScreen(const Game& g, sf::Vector2f p, float margin = cfg::TILE_SIZE) {
    const auto winSize = g.window.getSize();
    return p.x > -margin && p.x < winSize.x + margin &&
           p.y > -margin && p.y < winSize.y + margin;
}

/// 把贴图按"一格大小"绘制（纹理分辨率无关：32/64/90px贴图都归一化到
/// TILE_SIZE×zoom；wMul/hMul 用于2×2占地建筑）
void drawGridSprite(sf::RenderTarget& rt, const sf::Texture& tex, sf::Vector2f topLeft,
                    float zoom, int wMul = 1, int hMul = 1) {
    const auto ts = tex.getSize();
    sf::Sprite sp(tex);
    sp.setScale(cfg::TILE_SIZE * zoom * wMul / static_cast<float>(ts.x),
                cfg::TILE_SIZE * zoom * hMul / static_cast<float>(ts.y));
    sp.setPosition(topLeft);
    rt.draw(sp);
}

/// 面模式颜色（电线4态 / 其他3态）
sf::Color faceColor(cfg::FaceMode m, bool wire) {
    if (wire) {
        const sf::Color c[4] = {sf::Color(70, 70, 75), sf::Color(0, 140, 255),
                                sf::Color(180, 180, 40), sf::Color(255, 140, 0)};
        return c[static_cast<uint8_t>(m)];
    }
    const sf::Color c[3] = {sf::Color(70, 70, 75), sf::Color(0, 180, 80),
                            sf::Color(220, 80, 40)};
    return c[std::min<uint8_t>(static_cast<uint8_t>(m), 2)];
}

} // namespace

// ---------------------------------------------------------------------
// 主渲染入口
// ---------------------------------------------------------------------
void RenderSystem::renderWorld(Game& g, sf::RenderTarget& rt) {
    FT_PROFILE;
    rt.clear(cfg::color::BACKGROUND);
    const auto vr = visibleTiles(g);
    const float z = g.camera.zoom;

    // ================= 1. 地形（顶点数组批量渲染） =================
    {
        // 第一遍：只画草地格；路径格在第二遍用路径纹理单独绘制
        const auto& grass = g.assets.get("grass");
        const float tw = static_cast<float>(grass.getSize().x);   // 贴图实际宽(64px等)
        const float th = static_cast<float>(grass.getSize().y);
        sf::VertexArray va(sf::Quads);
        for (int ty = vr.y0; ty <= vr.y1; ++ty) {
            for (int tx = vr.x0; tx <= vr.x1; ++tx) {
                if (g.terrain[static_cast<size_t>(ty) * g.grid.w + tx] == 1) continue;
                const sf::Vector2f sc = g.worldToScreen(tx * cfg::TILE_SIZE, ty * cfg::TILE_SIZE);
                const float s = cfg::TILE_SIZE * z;
                // 逐格轻微明暗变化（确定性哈希），打破同一贴图重复感
                const uint32_t h = (static_cast<uint32_t>(tx) * 2654435761u) ^
                                   (static_cast<uint32_t>(ty) * 40503u);
                const uint8_t v = static_cast<uint8_t>(228u + (h % 28u));
                const sf::Color tint(v, v, v);
                va.append(sf::Vertex({sc.x, sc.y}, tint, {0, 0}));
                va.append(sf::Vertex({sc.x + s, sc.y}, tint, {tw, 0}));
                va.append(sf::Vertex({sc.x + s, sc.y + s}, tint, {tw, th}));
                va.append(sf::Vertex({sc.x, sc.y + s}, tint, {0, th}));
            }
        }
        rt.draw(va, &grass);
    }
    // 路径单独一遍（避免纹理绑定切换错误）
    {
        const auto& path = g.assets.get("path");
        const float tw = static_cast<float>(path.getSize().x);
        const float th = static_cast<float>(path.getSize().y);
        sf::VertexArray va(sf::Quads);
        for (int ty = vr.y0; ty <= vr.y1; ++ty) {
            for (int tx = vr.x0; tx <= vr.x1; ++tx) {
                if (g.terrain[static_cast<size_t>(ty) * g.grid.w + tx] != 1) continue;
                const sf::Vector2f sc = g.worldToScreen(tx * cfg::TILE_SIZE, ty * cfg::TILE_SIZE);
                const float s = cfg::TILE_SIZE * z;
                const uint32_t h = (static_cast<uint32_t>(tx) * 40503u) ^
                                   (static_cast<uint32_t>(ty) * 2654435761u);
                const uint8_t v = static_cast<uint8_t>(228u + (h % 28u));
                const sf::Color tint(v, v, v);
                va.append({sf::Vertex({sc.x, sc.y}, tint, {0, 0})});
                va.append({sf::Vertex({sc.x + s, sc.y}, tint, {tw, 0})});
                va.append({sf::Vertex({sc.x + s, sc.y + s}, tint, {tw, th})});
                va.append({sf::Vertex({sc.x, sc.y + s}, tint, {0, th})});
            }
        }
        rt.draw(va, &path);
    }

    // ================= 2. 矿点 =================
    for (auto [e, pos, ore] : g.reg.view<GridPos, OreDeposit>().each()) {
        if (pos.x < vr.x0 || pos.x > vr.x1 || pos.y < vr.y0 || pos.y > vr.y1) continue;
        drawGridSprite(rt, g.assets.get(AssetManager::oreKey(ore.type)),
                       g.worldToScreen(pos.x * cfg::TILE_SIZE, pos.y * cfg::TILE_SIZE), z);
    }

    // ================= 3. 建筑精灵层（桶/发电机/组装机/熔炉/采矿机/电线杆/电容） =================
    for (auto [e, b] : g.reg.view<Building>().each()) {
        const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE, b.pos.y * cfg::TILE_SIZE);
        if (!onScreen(g, sc, b.w * cfg::TILE_SIZE * z)) continue;
        const sf::Texture* tex = nullptr;
        switch (b.type) {
            case cfg::BuildingType::Bucket:
                tex = &g.assets.get(AssetManager::machineKey("bucket", b.dir, false));
                break;
            case cfg::BuildingType::Generator:
                tex = &g.assets.get(AssetManager::machineKey("generator", b.dir, true));
                break;
            case cfg::BuildingType::Assembler:
                tex = &g.assets.get(AssetManager::machineKey("assembler", b.dir, true));
                break;
            case cfg::BuildingType::Furnace:
                tex = &g.assets.get("furnace_d" + std::to_string(b.dir % 4));
                break;
            case cfg::BuildingType::AlloyFurnace:
                tex = &g.assets.get("alloy_furnace_d" + std::to_string(b.dir % 4));
                break;
            case cfg::BuildingType::Miner:
                tex = &g.assets.get(AssetManager::machineKey("miner", b.dir, true));
                break;
            case cfg::BuildingType::MinerL2:
                tex = &g.assets.get(AssetManager::machineKey("miner_l2", b.dir, true));
                break;
            case cfg::BuildingType::MinerL3:
                tex = &g.assets.get(AssetManager::machineKey("miner_l3", b.dir, true));
                break;
            case cfg::BuildingType::MinerVoid:
                tex = &g.assets.get(AssetManager::machineKey("miner_void", b.dir, true));
                break;
            case cfg::BuildingType::PowerPole:
                tex = &g.assets.get("machine_power_pole");
                break;
            case cfg::BuildingType::PowerGenerator:
                tex = &g.assets.get("power_generator");
                break;
            case cfg::BuildingType::Capacitor:
                tex = &g.assets.get("capacitor");
                break;
            default:
                continue;   // 其他类型在其他层绘制
        }
        // 按实际纹理尺寸归一化到占地大小（2×2发电机 → wMul/hMul=2）
        drawGridSprite(rt, *tex, sc, z, b.w, b.h);

        // ---- 附加指示 ----
        if (b.type == cfg::BuildingType::Bucket) {
            const auto& bucket = g.reg.get<Bucket>(e);
            if (!bucket.items.empty()) {
                sf::Text t;
                t.setFont(g.assets.font());
                t.setCharacterSize(std::max(6u, static_cast<unsigned>(8 * z)));
                t.setString(std::to_string(bucket.items.size()));
                t.setFillColor(sf::Color::White);
                const auto tb = t.getLocalBounds();
                t.setPosition(sc.x + cfg::TILE_SIZE * z / 2.0f - tb.width / 2.0f,
                              sc.y + cfg::TILE_SIZE * z - 14.0f * z);
                rt.draw(t);
            }
        } else if (b.type == cfg::BuildingType::Assembler) {
            const auto& inv = g.reg.get<Inventory>(e);
            // 配方标签（显示当前选定配方名，左上角）
            const auto& mach = g.reg.get<Machine>(e);
            const std::string& rname = cfg::ASSEMBLER_RECIPES[static_cast<size_t>(mach.recipeId)].nameZh;
            sf::Text rt2;
            rt2.setFont(g.assets.font());
            rt2.setCharacterSize(std::max(8u, static_cast<unsigned>(10 * z)));
            rt2.setString(sf::String::fromUtf8(rname.begin(), rname.end()));
            rt2.setFillColor(sf::Color(255, 200, 80));
            rt2.setPosition(sc.x + 2.0f * z, sc.y + 1.0f * z);
            rt.draw(rt2);
            // 产物数量（右下角）
            int produced = 0;
            for (auto [t, n] : cfg::ASSEMBLER_RECIPES[static_cast<size_t>(mach.recipeId)].outputs)
                produced += inv.count(t);
            if (produced > 0) {
                sf::Text t;
                t.setFont(g.assets.font());
                t.setCharacterSize(std::max(8u, static_cast<unsigned>(12 * z)));
                t.setString(std::to_string(produced));
                t.setFillColor(sf::Color(255, 255, 0));
                const auto tb = t.getLocalBounds();
                t.setPosition(sc.x + cfg::TILE_SIZE * z - 4.0f - tb.width,
                              sc.y + cfg::TILE_SIZE * z - 22.0f * z);
                rt.draw(t);
            }
        } else if (b.type == cfg::BuildingType::Miner ||
                   b.type == cfg::BuildingType::MinerL2 ||
                   b.type == cfg::BuildingType::MinerL3 ||
                   b.type == cfg::BuildingType::MinerVoid) {
            const auto& m = g.reg.get<Machine>(e);
            // 断电红点（Python行为）
            if (!m.powered) {
                sf::CircleShape dot(4.0f * z);
                dot.setPosition(sc.x, sc.y - 8.0f * z);
                dot.setFillColor(sf::Color(255, 0, 0));
                rt.draw(dot);
            }
            // 生产进度条
            if (m.producing) {
                const float bw = cfg::TILE_SIZE * z, bh = 3.0f * z;
                sf::RectangleShape bg({bw, bh});
                bg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z + 2.0f * z);
                bg.setFillColor(sf::Color::Black);
                rt.draw(bg);
                sf::RectangleShape fg({bw * m.progress, bh});
                fg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z + 2.0f * z);
                fg.setFillColor(sf::Color(0, 255, 0));
                rt.draw(fg);
            }
        } else if (b.type == cfg::BuildingType::Furnace ||
                   b.type == cfg::BuildingType::AlloyFurnace) {
            const auto& m = g.reg.get<Machine>(e);
            if (m.producing) {
                const float bw = cfg::TILE_SIZE * z, bh = 3.0f * z;
                sf::RectangleShape bg({bw, bh});
                bg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z + 2.0f * z);
                bg.setFillColor(sf::Color::Black);
                rt.draw(bg);
                sf::RectangleShape fg({bw * m.progress, bh});
                fg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z + 2.0f * z);
                fg.setFillColor(sf::Color(255, 140, 0));
                rt.draw(fg);
            }
        } else if (b.type == cfg::BuildingType::PowerGenerator) {
            const auto& gen = g.reg.get<PowerGeneratorNode>(e);
            if (gen.running) {
                sf::CircleShape dot(3.0f * z);
                dot.setPosition(sc.x + cfg::TILE_SIZE * z / 2.0f, sc.y + 6.0f * z);
                dot.setFillColor(sf::Color(0, 255, 0));
                rt.draw(dot);
            }
        } else if (b.type == cfg::BuildingType::Generator) {
            const auto& gen = g.reg.get<PowerGeneratorNode>(e);
            sf::CircleShape dot(4.0f * z);
            dot.setPosition(sc.x + 4.0f * z, sc.y + 4.0f * z);
            dot.setFillColor(gen.running ? sf::Color(255, 255, 0) : sf::Color(255, 0, 0));
            rt.draw(dot);
        } else if (b.type == cfg::BuildingType::Capacitor) {
            // 电量条（Python capacitor.draw）
            const auto& cap = g.reg.get<PowerCapacitor>(e);
            const float ratio = cap.capacity > 0 ? std::min(1.0f, cap.energy / cap.capacity) : 0.0f;
            const float bw = cfg::TILE_SIZE * z, bh = std::max(2.0f, 3.0f * z);
            sf::RectangleShape bg({bw, bh});
            bg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z - 4.0f * z);
            bg.setFillColor(sf::Color(30, 30, 30));
            rt.draw(bg);
            sf::RectangleShape fg({bw * ratio, bh});
            fg.setPosition(sc.x, sc.y + cfg::TILE_SIZE * z - 4.0f * z);
            fg.setFillColor(ratio > 0.3f ? sf::Color(0, 150, 255) : sf::Color(200, 50, 50));
            rt.draw(fg);
        }
    }

    // ================= 4. 物品管道 + 分流器（AE2式，无动画） =================
    {
        auto appendQuad = [](sf::VertexArray& va, sf::Vector2f p, sf::Vector2f s,
                             sf::Color c) {
            va.append(sf::Vertex({p.x, p.y}, c));
            va.append(sf::Vertex({p.x + s.x, p.y}, c));
            va.append(sf::Vertex({p.x + s.x, p.y + s.y}, c));
            va.append(sf::Vertex({p.x, p.y + s.y}, c));
        };
        sf::VertexArray casing(sf::Quads);      // 外层金属
        sf::VertexArray casingInner(sf::Quads); // 内层盖板
        sf::VertexArray links(sf::Quads);       // 管道/端口连接

        // 节点判定：管道0 / 分流器1 / ME接口2 / ME存储单元3 / ME终端4
        auto nodeKind = [&](entt::entity e) -> int {
            if (e == entt::null || !g.reg.valid(e)) return -1;
            if (g.reg.all_of<Pipe>(e)) return 0;
            if (g.reg.all_of<SplitterQueue>(e)) return 1;
            if (g.reg.all_of<MeInterface>(e)) return 2;
            if (g.reg.all_of<MeDrive>(e)) return 3;
            if (g.reg.all_of<MeTerminal>(e)) return 4;
            return -1;
        };
        // 端点判定（自动链接的容器/机器/塔）
        auto isEndpoint = [&](entt::entity e) {
            return e != entt::null && g.reg.valid(e) &&
                   (g.reg.all_of<Bucket>(e) || g.reg.all_of<Machine>(e) ||
                    g.reg.all_of<Turret>(e) || g.reg.all_of<PowerGeneratorNode>(e));
        };
        // 节点连接掩码
        auto maskOf = [&](int kind, entt::entity e) -> uint8_t {
            switch (kind) {
                case 0: return g.reg.get<Pipe>(e).connMask;
                case 1: return g.reg.get<SplitterQueue>(e).connMask;
                case 2: return g.reg.get<MeInterface>(e).connMask;
                case 3: return g.reg.get<MeDrive>(e).connMask;
                default: return g.reg.get<MeTerminal>(e).connMask;
            }
        };

        for (int ty = vr.y0; ty <= vr.y1; ++ty) {
            for (int tx = vr.x0; tx <= vr.x1; ++tx) {
                const entt::entity e = g.grid.at(tx, ty).building;
                const int kind = nodeKind(e);
                if (kind < 0) continue;
                const uint8_t mask = maskOf(kind, e);
                const bool isMe = kind >= 2;
                const sf::Vector2f sc = g.worldToScreen(tx * cfg::TILE_SIZE,
                                                        ty * cfg::TILE_SIZE);
                const float S = cfg::TILE_SIZE * z, half = S / 2.0f;
                const float lw = 7.0f * z;   // 连接条宽

                // 外壳两层（金属外框 + 内层盖板）
                const sf::Color shell = kind == 1 ? sf::Color(46, 50, 66)
                                      : isMe   ? sf::Color(20, 42, 50)
                                               : sf::Color(50, 54, 60);
                const sf::Color innerCol = kind == 1 ? sf::Color(58, 64, 82)
                                         : isMe   ? sf::Color(28, 56, 66)
                                                  : sf::Color(66, 72, 80);
                appendQuad(casing, {sc.x + 2.0f * z, sc.y + 2.0f * z},
                           {S - 4.0f * z, S - 4.0f * z}, shell);
                appendQuad(casingInner, {sc.x + 6.0f * z, sc.y + 6.0f * z},
                           {S - 12.0f * z, S - 12.0f * z}, innerCol);

                for (int d = 0; d < 4; ++d) {
                    const int nx = tx + cfg::Dir::OFFSETS[d][0];
                    const int ny = ty + cfg::Dir::OFFSETS[d][1];
                    const entt::entity nb = g.grid.inBounds(nx, ny)
                                                ? g.grid.at(nx, ny).building
                                                : entt::null;
                    sf::Color link;
                    sf::Vector2f p, s;
                    switch (d) {
                        case cfg::Dir::UP:
                            p = {sc.x + half - lw / 2, sc.y};
                            s = {lw, half};
                            break;
                        case cfg::Dir::RIGHT:
                            p = {sc.x + half, sc.y + half - lw / 2};
                            s = {half, lw};
                            break;
                        case cfg::Dir::DOWN:
                            p = {sc.x + half - lw / 2, sc.y + half};
                            s = {lw, half};
                            break;
                        default:
                            p = {sc.x, sc.y + half - lw / 2};
                            s = {half, lw};
                            break;
                    }
                    if (mask & (1u << d)) {
                        // 连接到同类设备：亮色传输线（ME亮青）
                        link = isMe    ? sf::Color(0, 220, 255)
                              : kind == 1 ? sf::Color(0, 200, 220)
                                          : sf::Color(150, 190, 210);
                        appendQuad(links, p, s, link);
                    } else if (isEndpoint(nb)) {
                        // 连接到容器/机器：暗色端口
                        appendQuad(links, p, s, sf::Color(90, 100, 110));
                    }
                }
            }
        }
        rt.draw(casing);
        rt.draw(casingInner);
        rt.draw(links);

        // ---- 管道截面孔（圆形，工业管道感） ----
        for (int ty = vr.y0; ty <= vr.y1; ++ty) {
            for (int tx = vr.x0; tx <= vr.x1; ++tx) {
                const entt::entity e = g.grid.at(tx, ty).building;
                if (e == entt::null || !g.reg.valid(e) || !g.reg.all_of<Pipe>(e)) continue;
                const sf::Vector2f sc = g.worldToScreen(tx * cfg::TILE_SIZE, ty * cfg::TILE_SIZE);
                if (!onScreen(g, sc)) continue;
                const float S = cfg::TILE_SIZE * z, half = S / 2.0f;
                sf::CircleShape hole(6.0f * z);
                hole.setPosition(sc.x + half - 6.0f * z, sc.y + half - 6.0f * z);
                hole.setFillColor(sf::Color(16, 20, 24));
                hole.setOutlineColor(sf::Color(108, 116, 124));
                hole.setOutlineThickness(2.0f * z);
                rt.draw(hole);
            }
        }

        // ---- 分流器核心与队列计数 ----
        auto sview = g.reg.view<Building, SplitterQueue>();
        for (auto [e, b, sp] : sview.each()) {
            const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE,
                                                    b.pos.y * cfg::TILE_SIZE);
            if (!onScreen(g, sc)) continue;
            const float S = cfg::TILE_SIZE * z, half = S / 2.0f;
            // 中央分流盘（外环 + 叉臂 + 中心孔）
            sf::CircleShape outer(10.0f * z);
            outer.setPosition(sc.x + half - 10.0f * z, sc.y + half - 10.0f * z);
            outer.setFillColor(sf::Color::Transparent);
            outer.setOutlineColor(sf::Color(0, 210, 230));
            outer.setOutlineThickness(2.0f * z);
            rt.draw(outer);
            sf::RectangleShape arm({16.0f * z, 3.0f * z});
            arm.setOrigin(8.0f * z, 1.5f * z);
            arm.setPosition(sc.x + half, sc.y + half);
            arm.setFillColor(sf::Color(0, 210, 230));
            arm.setRotation(45.0f);
            rt.draw(arm);
            arm.setRotation(-45.0f);
            rt.draw(arm);
            sf::CircleShape center(4.5f * z);
            center.setPosition(sc.x + half - 4.5f * z, sc.y + half - 4.5f * z);
            center.setFillColor(sf::Color(24, 26, 32));
            center.setOutlineColor(sf::Color(0, 170, 190));
            center.setOutlineThickness(1.5f * z);
            rt.draw(center);
            if (!sp.queue.empty()) {
                sf::Text t;
                t.setFont(g.assets.font());
                t.setCharacterSize(std::max(8u, static_cast<unsigned>(10 * z)));
                t.setString(sp.queue.size() < 10 ? std::to_string(sp.queue.size()) : "+");
                t.setFillColor(sf::Color(255, 255, 0));
                t.setPosition(sc.x + S - 16.0f * z, sc.y + 2.0f * z);
                rt.draw(t);
            }
        }

        // ---- ME设备核心图标（AE2式：接口菱形 / 存储单元容量条 / 终端屏幕） ----
        for (auto [e, b] : g.reg.view<Building>().each()) {
            const bool isMe = b.type == cfg::BuildingType::MeInterface ||
                              b.type == cfg::BuildingType::MeDrive ||
                              b.type == cfg::BuildingType::MeTerminal;
            if (!isMe) continue;
            const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE,
                                                    b.pos.y * cfg::TILE_SIZE);
            if (!onScreen(g, sc)) continue;
            const float S = cfg::TILE_SIZE * z, half = S / 2.0f;
            if (b.type == cfg::BuildingType::MeInterface) {
                // 菱形核心 + 外发光环 + 中心亮点
                sf::ConvexShape halo;
                halo.setPointCount(4);
                halo.setPoint(0, {half, half - 11.0f * z});
                halo.setPoint(1, {half + 11.0f * z, half});
                halo.setPoint(2, {half, half + 11.0f * z});
                halo.setPoint(3, {half - 11.0f * z, half});
                halo.setPosition(sc.x, sc.y);
                halo.setFillColor(sf::Color(0, 220, 255, 40));
                rt.draw(halo);
                sf::ConvexShape diamond;
                diamond.setPointCount(4);
                diamond.setPoint(0, {half, half - 7.0f * z});
                diamond.setPoint(1, {half + 7.0f * z, half});
                diamond.setPoint(2, {half, half + 7.0f * z});
                diamond.setPoint(3, {half - 7.0f * z, half});
                diamond.setPosition(sc.x, sc.y);
                diamond.setFillColor(sf::Color(0, 220, 255));
                diamond.setOutlineColor(sf::Color(180, 255, 255));
                diamond.setOutlineThickness(1.2f * z);
                rt.draw(diamond);
                sf::CircleShape core(2.5f * z);
                core.setPosition(sc.x + half - 2.5f * z, sc.y + half - 2.5f * z);
                core.setFillColor(sf::Color(255, 255, 255));
                rt.draw(core);
            } else if (b.type == cfg::BuildingType::MeDrive) {
                // 网络容量条 + 上下硬盘格栅
                const int nid = MeSystem::networkIdOf(g, e);
                float ratio = 0.0f;
                if (nid >= 0 && nid < static_cast<int>(MeSystem::networks().size())) {
                    const auto& net = MeSystem::networks()[static_cast<size_t>(nid)];
                    ratio = net.capacity > 0
                                ? std::min(1.0f, static_cast<float>(net.totalItems) / net.capacity)
                                : 0.0f;
                }
                const float bw = S - 12.0f * z, bh = 6.0f * z;
                sf::RectangleShape bg({bw, bh});
                bg.setPosition(sc.x + 6.0f * z, sc.y + half - bh / 2.0f);
                bg.setFillColor(sf::Color(8, 16, 20));
                bg.setOutlineColor(sf::Color(0, 140, 165));
                bg.setOutlineThickness(1.0f * z);
                rt.draw(bg);
                sf::RectangleShape fg({bw * ratio, bh});
                fg.setPosition(sc.x + 6.0f * z, sc.y + half - bh / 2.0f);
                fg.setFillColor(sf::Color(0, 220, 255));
                rt.draw(fg);
                for (int i = 0; i < 2; ++i) {
                    sf::RectangleShape grid({bw, 2.0f * z});
                    grid.setPosition(sc.x + 6.0f * z, sc.y + (i == 0 ? 7.0f : S - 9.0f) * z);
                    grid.setFillColor(sf::Color(0, 150, 175, 180));
                    rt.draw(grid);
                }
            } else {
                // 终端：屏幕 + 发光边框 + 数据线 + 键盘格
                sf::RectangleShape screen({S - 12.0f * z, S - 18.0f * z});
                screen.setPosition(sc.x + 6.0f * z, sc.y + 5.0f * z);
                screen.setFillColor(sf::Color(8, 14, 18));
                screen.setOutlineColor(sf::Color(0, 200, 220));
                screen.setOutlineThickness(1.5f * z);
                rt.draw(screen);
                for (int i = 0; i < 3; ++i) {
                    sf::RectangleShape line({S - 20.0f * z, 2.0f * z});
                    line.setPosition(sc.x + 10.0f * z, sc.y + 11.0f * z + i * 5.0f * z);
                    line.setFillColor(sf::Color(0, 220, 255, 220));
                    rt.draw(line);
                }
                for (int i = 0; i < 4; ++i) {
                    sf::RectangleShape key({(S - 22.0f) / 4.0f * z, 3.0f * z});
                    key.setPosition(sc.x + 8.0f * z + i * ((S - 18.0f) / 4.0f * z),
                                    sc.y + S - 9.0f * z);
                    key.setFillColor(sf::Color(0, 150, 175));
                    rt.draw(key);
                }
            }
        }

        // ---- 管道缓冲物品（静态小色块，最多显示前8件，无动画） ----
        sf::VertexArray items(sf::Quads);
        auto pview = g.reg.view<Building, Pipe>();
        for (auto [e, b, p] : pview.each()) {
            if (p.buffer.empty()) continue;
            const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE,
                                                    b.pos.y * cfg::TILE_SIZE);
            if (!onScreen(g, sc)) continue;
            const float dot = 6.0f * z, gap = 7.5f * z;
            const float ox = sc.x + 5.0f * z, oy = sc.y + 5.0f * z;
            int n = 0;
            for (auto it = p.buffer.begin();
                 it != p.buffer.end() && n < 8; ++it, ++n) {
                const float px = ox + (n % 4) * gap;
                const float py = oy + (n / 4) * gap;
                const sf::Color c = ItemSystem::color(*it);
                items.append(sf::Vertex({px, py}, c));
                items.append(sf::Vertex({px + dot, py}, c));
                items.append(sf::Vertex({px + dot, py + dot}, c));
                items.append(sf::Vertex({px, py + dot}, c));
            }
            if (p.buffer.size() > 8) {   // 缓冲更多时显示计数
                sf::Text t;
                t.setFont(g.assets.font());
                t.setCharacterSize(std::max(8u, static_cast<unsigned>(9 * z)));
                t.setString(std::to_string(p.buffer.size()));
                t.setFillColor(sf::Color(255, 220, 120));
                t.setPosition(sc.x + cfg::TILE_SIZE * z - 22.0f * z,
                              sc.y + cfg::TILE_SIZE * z - 16.0f * z);
                rt.draw(t);
            }
        }
        rt.draw(items);
    }

    // ================= 5. 电线杆连线 =================
    {
        std::vector<entt::entity> poles;
        for (auto [e, b, pole] : g.reg.view<Building, PowerPole>().each()) poles.push_back(e);
        for (size_t i = 0; i < poles.size(); ++i) {
            for (size_t j = i + 1; j < poles.size(); ++j) {
                const auto& b1 = g.reg.get<Building>(poles[i]);
                const auto& b2 = g.reg.get<Building>(poles[j]);
                const auto c1 = g.buildingCenter(b1), c2 = g.buildingCenter(b2);
                const float dx = c2.x - c1.x, dy = c2.y - c1.y;
                if (dx * dx + dy * dy > cfg::POWER_POLE_RADIUS * cfg::POWER_POLE_RADIUS) continue;
                sf::VertexArray line(sf::Lines, 2);
                line[0] = sf::Vertex(g.worldToScreen(c1.x, c1.y), sf::Color(150, 150, 150));
                line[1] = sf::Vertex(g.worldToScreen(c2.x, c2.y), sf::Color(150, 150, 150));
                rt.draw(line);
            }
        }
    }

    // ================= 6. 电线（面配置色块 + 连接线） =================
    for (auto [e, b, fc] : g.reg.view<Building, FaceConfig>().each()) {
        if (b.type != cfg::BuildingType::PowerWire) continue;
        const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE, b.pos.y * cfg::TILE_SIZE);
        if (!onScreen(g, sc)) continue;
        const float S = cfg::TILE_SIZE * z;
        const float half = S / 2.0f, third = S / 3.0f, lt = std::max(2.0f * z, 1.0f);

        sf::VertexArray va(sf::Quads);
        // 4面色条 + 指示块
        const sf::Vector2f stripPos[4] = {
            {sc.x + half - lt, sc.y}, {sc.x + S - third, sc.y + half - lt},
            {sc.x + half - lt, sc.y + S - third}, {sc.x, sc.y + half - lt}};
        const sf::Vector2f stripSize[4] = {
            {lt * 2, third}, {third, lt * 2}, {lt * 2, third}, {third, lt * 2}};
        for (int d = 0; d < 4; ++d) {
            const sf::Color c = faceColor(fc.get(d), true);
            const sf::Vector2f p = stripPos[d], s2 = stripSize[d];
            va.append(sf::Vertex({p.x, p.y}, c));
            va.append(sf::Vertex({p.x + s2.x, p.y}, c));
            va.append(sf::Vertex({p.x + s2.x, p.y + s2.y}, c));
            va.append(sf::Vertex({p.x, p.y + s2.y}, c));
        }
        rt.draw(va);

        // 中心端子
        sf::CircleShape term(6.0f * z);
        term.setPosition(sc.x + half - 6.0f * z, sc.y + half - 6.0f * z);
        term.setFillColor(sf::Color(160, 160, 170));
        rt.draw(term);
        sf::CircleShape hole(4.0f * z);
        hole.setPosition(sc.x + half - 4.0f * z, sc.y + half - 4.0f * z);
        hole.setFillColor(sf::Color(30, 30, 35));
        rt.draw(hole);

        // 连接线到相邻电网设备（Python wire.draw）
        for (int d = 0; d < 4; ++d) {
            const int nx = b.pos.x + cfg::Dir::OFFSETS[d][0];
            const int ny = b.pos.y + cfg::Dir::OFFSETS[d][1];
            if (!g.grid.inBounds(nx, ny)) continue;
            const entt::entity nb = g.grid.at(nx, ny).building;
            if (nb == entt::null) continue;
            const bool powerDevice = g.reg.all_of<PowerGeneratorNode>(nb) ||
                                     g.reg.all_of<PowerCapacitor>(nb) ||
                                     g.reg.all_of<PowerConsumer>(nb) ||
                                     g.reg.all_of<PowerPole>(nb) ||
                                     (g.reg.all_of<Building>(nb) &&
                                      g.reg.get<Building>(nb).type == cfg::BuildingType::PowerWire);
            if (!powerDevice) continue;
            const auto c1 = g.buildingCenter(b);
            const auto c2 = g.buildingCenter(g.reg.get<Building>(nb));
            sf::VertexArray line(sf::Lines, 2);
            line[0] = sf::Vertex(g.worldToScreen(c1.x, c1.y), sf::Color(80, 80, 90));
            line[1] = sf::Vertex(g.worldToScreen(c2.x, c2.y), sf::Color(80, 80, 90));
            rt.draw(line);
        }
    }

    // ================= 8. 炮塔 =================
    {
        for (auto [e, b, t] : g.reg.view<Building, Turret>().each()) {
            const sf::Vector2f sc = g.worldToScreen(b.pos.x * cfg::TILE_SIZE, b.pos.y * cfg::TILE_SIZE);
            if (!onScreen(g, sc)) continue;
            const bool hovered = (g.hoveredEntity == e);

            // 攻击范围圈（悬停时显示，Python行为）
            if (hovered) {
                const auto center = g.worldToScreen(g.buildingCenter(b));
                const float r = t.range * z;
                sf::CircleShape range(r);
                range.setPosition(center.x - r, center.y - r);
                range.setFillColor(sf::Color(100, 100, 255, 50));
                range.setOutlineColor(sf::Color(100, 100, 255, 150));
                range.setOutlineThickness(2.0f);
                rt.draw(range);
            }

            // 塔体（8方向贴图，按实际纹理尺寸归一化）
            drawGridSprite(rt, g.assets.get(AssetManager::towerKey(t.type, t.barrelDir)), sc, z);

            // 弹药指示（每5发一个小黄块，最多3个）
            if (!t.isElectric && t.ammo > 0) {
                const float bx = sc.x + cfg::TILE_SIZE * z - 10.0f * z;
                const float by = sc.y + 2.0f * z;
                const float bs = 3.0f * z;
                for (int i = 0; i < std::min(3, t.ammo / 5); ++i) {
                    sf::RectangleShape rect({bs, bs});
                    rect.setPosition(bx, by + i * bs * 1.5f);
                    rect.setFillColor(sf::Color(255, 255, 0));
                    rt.draw(rect);
                }
            }
            // 电力塔：电力条
            if (t.isElectric) {
                const float bx = sc.x + cfg::TILE_SIZE * z - 10.0f * z;
                const float by = sc.y + 2.0f * z;
                const float ratio = t.maxPower > 0 ? t.power / t.maxPower : 0.0f;
                sf::RectangleShape bg({9.0f * z, 3.0f * z});
                bg.setPosition(bx, by);
                bg.setFillColor(sf::Color(30, 30, 30));
                rt.draw(bg);
                sf::RectangleShape fg({9.0f * z * ratio, 3.0f * z});
                fg.setPosition(bx, by);
                fg.setFillColor(sf::Color(0, 150, 255));
                rt.draw(fg);
            }
        }
    }

    // ================= 9. 敌人 + 血条 =================
    for (auto [e, en] : g.reg.view<Enemy>().each()) {
        const sf::Vector2f sc = g.worldToScreen(en.pos.x, en.pos.y);
        if (!onScreen(g, sc)) continue;
        drawGridSprite(rt, g.assets.get(AssetManager::enemyKey(en.type)), sc, z);
        // 血条（受伤时显示，Python行为）
        if (en.health < en.maxHealth) {
            const float bw = cfg::TILE_SIZE * z, bh = 4.0f * z;
            const float ratio = static_cast<float>(en.health) / en.maxHealth;
            sf::RectangleShape bg({bw, bh});
            bg.setPosition(sc.x, sc.y - 8.0f * z);
            bg.setFillColor(sf::Color::Black);
            rt.draw(bg);
            const sf::Color c = ratio > 0.5f ? sf::Color(0, 255, 0)
                : ratio > 0.25f ? sf::Color(255, 255, 0) : sf::Color(255, 0, 0);
            sf::RectangleShape fg({bw * ratio, bh});
            fg.setPosition(sc.x, sc.y - 8.0f * z);
            fg.setFillColor(c);
            rt.draw(fg);
        }
    }

    // ================= 10. 子弹（圆+拖尾） =================
    for (auto [e, b] : g.reg.view<Bullet>().each()) {
        const sf::Color colors[4] = {sf::Color(255, 255, 0), sf::Color(0, 255, 255),
                                     sf::Color(255, 0, 255), sf::Color(255, 255, 0)};
        const sf::Color c = colors[static_cast<size_t>(b.towerType)];
        // 拖尾（Python: 3段逐渐缩小）
        for (int i = 0; i < cfg::BULLET_TRAIL_LENGTH; ++i) {
            const float alpha = 255.0f - i * 85.0f;
            if (alpha <= 0) break;
            const float dx = b.pos.x - b.startX, dy = b.pos.y - b.startY;
            const sf::Vector2f tp = g.worldToScreen(b.pos.x - dx * i * 0.15f,
                                                    b.pos.y - dy * i * 0.15f);
            const float size = std::max(0.0f, (4.0f - i * 1.0f) * z);
            if (size <= 0) continue;
            sf::CircleShape trail(size);
            trail.setPosition(tp.x - size, tp.y - size);
            trail.setFillColor(sf::Color(c.r, c.g, c.b, static_cast<uint8_t>(alpha)));
            rt.draw(trail);
        }
        // 弹体
        const sf::Vector2f p = g.worldToScreen(b.pos.x, b.pos.y);
        const float r = 4.0f * z;
        sf::CircleShape body(r);
        body.setPosition(p.x - r, p.y - r);
        body.setFillColor(c);
        rt.draw(body);
        sf::CircleShape glow(r / 2.0f);
        glow.setPosition(p.x - r / 2.0f, p.y - r / 2.0f);
        glow.setFillColor(sf::Color::White);
        rt.draw(glow);
    }

    // ================= 11. 放置预览 =================
    if (g.hasSelection) {
        const auto& info = cfg::BUILDING_INFOS[static_cast<size_t>(g.selected)];
        (void)info;
        const sf::Vector2f sc = g.worldToScreen(g.previewTile.x * cfg::TILE_SIZE,
                                                g.previewTile.y * cfg::TILE_SIZE);
        const float s = cfg::TILE_SIZE * z;
        sf::RectangleShape rect({s, s});
        rect.setPosition(sc.x, sc.y);
        rect.setFillColor(g.previewCanBuild ? sf::Color(0, 255, 0, 128)
                                            : sf::Color(255, 0, 0, 128));
        rt.draw(rect);
        sf::RectangleShape border({s, s});
        border.setPosition(sc.x, sc.y);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color::White);
        border.setOutlineThickness(2.0f);
        rt.draw(border);
    }

    // ================= 13. 敌人悬停血量（Python _draw_enemy_health_on_hover） =================
    {
        const entt::entity he = g.hoveredEnemy();
        if (he != entt::null && g.reg.valid(he)) {
            const auto& en = g.reg.get<Enemy>(he);
            const sf::Vector2f sc = g.worldToScreen(en.pos.x, en.pos.y);
            const float ratio = static_cast<float>(en.health) / en.maxHealth;
            const float bw = 60.0f, bh = 6.0f;
            sf::RectangleShape bg({bw, bh});
            bg.setPosition(sc.x - bw / 2.0f, sc.y - 20.0f);
            bg.setFillColor(sf::Color(30, 30, 30));
            rt.draw(bg);
            const sf::Color c = ratio > 0.5f ? sf::Color(0, 255, 0)
                : ratio > 0.25f ? sf::Color(255, 255, 0) : sf::Color(255, 0, 0);
            sf::RectangleShape fg({bw * ratio, bh});
            fg.setPosition(sc.x - bw / 2.0f, sc.y - 20.0f);
            fg.setFillColor(c);
            rt.draw(fg);
            sf::RectangleShape border({bw, bh});
            border.setPosition(sc.x - bw / 2.0f, sc.y - 20.0f);
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineColor(sf::Color(200, 200, 200));
            border.setOutlineThickness(1.0f);
            rt.draw(border);
            sf::Text t;
            t.setFont(g.assets.font());
            t.setCharacterSize(12);
            t.setString(std::to_string(en.health) + "/" + std::to_string(en.maxHealth));
            t.setFillColor(sf::Color::White);
            const auto tb = t.getLocalBounds();
            t.setPosition(sc.x - tb.width / 2.0f, sc.y - 40.0f);
            rt.draw(t);
        }
    }
}
