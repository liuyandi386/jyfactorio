// =====================================================================
// ItemSystem.cpp —— 物品系统实现
// =====================================================================
#include "systems/ItemSystem.h"
#include <array>
#include "GameConfig.h"

namespace {
// 物品键名表（与 ItemType 枚举顺序一致，JSON存档/配置使用）
constexpr std::array<const char*, cfg::ITEM_COUNT> ITEM_KEYS = {
    "iron_ore",      // 铁矿石
    "copper_ore",    // 铜矿石
    "coal",          // 煤矿
    "gold_ore",      // 金矿石
    "diamond_ore",   // 钻石矿石
    "nickel_ore",    // 镍矿石
    "silver_ore",    // 银矿石
    "lead_ore",      // 铅矿石
    "iron_ingot",    // 铁锭
    "copper_ingot",  // 铜锭
    "gold_ingot",    // 金锭
    "nickel_ingot",  // 镍锭
    "silver_ingot",  // 银锭
    "lead_ingot",    // 铅锭
    "circuit_board", // 电路板
    "ammo",          // 弹药
    "steel_ingot",   // 钢锭
    "electrum_ingot",// 琥珀金锭
    "invar_ingot",   // 因瓦锭
    "constantan_ingot", // 康铜锭
};
} // namespace

const char* ItemSystem::nameZh(cfg::ItemType t) {
    return cfg::ITEM_INFOS[static_cast<size_t>(t)].nameZh;
}

const char* ItemSystem::symbol(cfg::ItemType t) {
    return cfg::ITEM_INFOS[static_cast<size_t>(t)].symbol;
}

sf::Color ItemSystem::color(cfg::ItemType t) {
    return cfg::ITEM_INFOS[static_cast<size_t>(t)].color;
}

bool ItemSystem::transportable(cfg::ItemType t) {
    return cfg::ITEM_INFOS[static_cast<size_t>(t)].transportable;
}

const char* ItemSystem::key(cfg::ItemType t) {
    return ITEM_KEYS[static_cast<size_t>(t)];
}

std::optional<cfg::ItemType> ItemSystem::parse(const std::string& key) {
    for (int i = 0; i < cfg::ITEM_COUNT; ++i)
        if (key == ITEM_KEYS[static_cast<size_t>(i)])
            return static_cast<cfg::ItemType>(i);
    return std::nullopt;
}
