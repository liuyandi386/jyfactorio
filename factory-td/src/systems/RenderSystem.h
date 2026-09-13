#pragma once
// =====================================================================
// RenderSystem.h —— 渲染系统
//
// 用 SFML 顶点数组批量渲染减少draw call（传送带+物品一张图一批），
// 只绘制摄像机视野内的内容（视锥剔除）。
// 绘制顺序与 Python GameScene.render 一致。
// =====================================================================
#include <SFML/Graphics.hpp>

class Game;

/// 渲染系统
class RenderSystem {
public:
    /// 渲染世界层（UI由GameUI负责）
    static void renderWorld(Game& g, sf::RenderTarget& rt);
};
