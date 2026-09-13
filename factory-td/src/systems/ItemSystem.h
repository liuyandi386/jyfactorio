#pragma once
// =====================================================================
// ItemSystem.h —— 物品系统（物品类型注册表 + 堆叠规则）
//
// 提供物品中文名/缩写/颜色/可运输性查询（数据源 GameConfig.h），
// 以及"违禁品"检查（电力类物品不可上传送带/入桶，Python规则）。
// 另提供 物品键名(JSON用) ↔ 枚举 的转换助手。
// =====================================================================
#include <optional>
#include <string>
#include <SFML/Graphics/Color.hpp>
#include "GameConfig.h"

class Game;

/// 物品系统（无状态工具类）
class ItemSystem {
public:
    /// 物品中文名
    static const char* nameZh(cfg::ItemType t);
    /// 物品缩写符号（网格显示）
    static const char* symbol(cfg::ItemType t);
    /// 物品显示颜色
    static sf::Color color(cfg::ItemType t);
    /// 是否可由传送带运输/入桶（Python can_transport / can_accept 违禁品检查）
    static bool transportable(cfg::ItemType t);
    /// 物品键名（JSON存档/配置使用，如 "iron_ore"）
    static const char* key(cfg::ItemType t);
    /// 键名 → 物品枚举（未知返回 std::nullopt）
    static std::optional<cfg::ItemType> parse(const std::string& key);
};
