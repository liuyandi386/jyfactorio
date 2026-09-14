#pragma once
// =====================================================================
// AssetManager.h —— 资源管理（贴图 + 字体）
//
// 移植自 Python sprites.py：
//   1. 优先从 assets/sprites/ 加载PNG贴图（带缓存）
//   2. 文件缺失时回退到程序化生成（电线/分流器/电容库/燃煤发电机/
//      熔炉等Python原本就是程序化绘制的贴图）
// 电线与分流器的贴图会随"面配置"变化，提供动态重绘接口。
// =====================================================================
#include <array>
#include <string>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "GameConfig.h"

class AssetManager {
public:
    /// 加载全部资源（assetDir 为 assets 目录路径）
    bool load(const std::string& assetDir);

    /// 获取贴图（不存在时返回fallback）
    const sf::Texture& get(const std::string& key) const;

    /// 动态面配置贴图：电线(4态)
    void rebuildWireTexture(const std::array<cfg::FaceMode, 4>& faces);
    const sf::Texture& wireTexture() const { return wireTex_; }

    /// 字体（中文优先: simsun.ttc → msyh.ttc → simhei.ttf）
    const sf::Font& font() const { return font_; }

    // ---- 贴图键生成辅助 ----
    static std::string towerKey(cfg::TurretType t, int dir8);   // 塔8方向
    /// 机器4方向键（dir 即逻辑朝向；rotated 为历史遗留参数，已不影响映射）
    static std::string machineKey(const char* name, int dir, bool rotated = true);
    static std::string oreKey(cfg::ItemType t);                 // 矿石
    static std::string enemyKey(cfg::EnemyType t);              // 敌人

private:
    /// 从文件加载一张贴图（失败返回false）
    bool loadTexture(const std::string& key, const std::string& file);
    /// 仅尝试加载（失败不插入回退贴图，供程序化生成兜底的贴图使用）
    bool tryLoadTexture(const std::string& key, const std::string& file);
    /// 程序化生成静态贴图（熔炉/电容库/燃煤发电机）
    void generateStaticTextures();
    /// 生成通用回退贴图（紫红色块，Python同款）
    void generateFallback(const std::string& key);

    std::unordered_map<std::string, sf::Texture> textures_; // 贴图缓存
    sf::Texture wireTex_;                                     // 动态面配置贴图（电线）
    sf::Font font_;                                         // UI字体
    sf::Texture fallback_;                                  // 回退贴图
};
