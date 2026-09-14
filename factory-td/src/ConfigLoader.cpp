// =====================================================================
// ConfigLoader.cpp —— JSON 配置加载器实现
// 使用 nlohmann/json 单头库（deps/local2/nlohmann/json.hpp）。
// 只做"存在才覆盖"：JSON 缺哪个字段就保留内置默认值。
// =====================================================================
#include "ConfigLoader.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "GameConfig.h"
#include "systems/ItemSystem.h"

using nlohmann::json;

namespace cfg {

namespace {

/// 若 JSON 含 key 则覆盖目标变量（模板：数值/字符串）
template <class T>
void setIf(T& target, const json& j, const char* key) {
    if (j.contains(key)) target = j.at(key).get<T>();
}

/// 从 JSON 数组解析 物品:数量 列表（"iron_ore": 2 形式）
std::vector<std::pair<ItemType, int>> parseItems(const json& j) {
    std::vector<std::pair<ItemType, int>> out;
    if (!j.is_object()) return out;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (auto t = ItemSystem::parse(it.key()))
            out.emplace_back(*t, it.value().get<int>());
    }
    return out;
}

/// 建筑键名 → 枚举（与 BUILDING_INFOS 顺序一致）
std::optional<BuildingType> parseBuilding(const std::string& key) {
    static const std::array<const char*, BUILDING_COUNT> NAMES = {
        "basic_tower", "rapid_tower", "sniper_tower", "electric_tower",
        "miner", "miner_l2", "miner_l3", "miner_void",
        "furnace", "assembler", "generator",
        "power_pole", "power_generator", "capacitor", "power_wire",
        "pipe", "bucket", "splitter", "alloy_furnace",
        "me_interface", "me_drive", "me_terminal"};
    for (int i = 0; i < BUILDING_COUNT; ++i)
        if (key == NAMES[static_cast<size_t>(i)])
            return static_cast<BuildingType>(i);
    return std::nullopt;
}

} // namespace

bool loadConfig(const std::string& path) {
    json j;
    try {
        std::ifstream f(path);
        if (!f) return false;
        f >> j;
    } catch (...) {
        return false;   // 解析失败：保持内置默认值
    }

    // ---- 平铺数值 ----
    setIf(INITIAL_GOLD, j, "initial_gold");
    setIf(INITIAL_LIVES, j, "initial_lives");
    setIf(WAVE_INTERVAL, j, "wave_interval");
    setIf(WAVE_ENEMIES, j, "wave_enemies");
    setIf(INITIAL_COUNTDOWN, j, "initial_countdown");
    setIf(SPAWN_INTERVAL, j, "spawn_interval");
    setIf(WAVE_AUTO_SPAWN, j, "wave_auto_spawn");
    setIf(INFINITE_RESOURCE, j, "infinite_resource");
    setIf(RESOURCE_INFINITE, j, "resource_infinite");

    // ---- 矿点（8种矿石独立随机分布 + 无限/有限储量） ----
    if (j.contains("ore_counts")) {
        const auto& o = j.at("ore_counts");
        setIf(ORE_IRON_COUNT, o, "iron");
        setIf(ORE_COPPER_COUNT, o, "copper");
        setIf(ORE_COAL_COUNT, o, "coal");
        setIf(ORE_GOLD_COUNT, o, "gold");
        setIf(ORE_DIAMOND_COUNT, o, "diamond");
        setIf(ORE_NICKEL_COUNT, o, "nickel");
        setIf(ORE_SILVER_COUNT, o, "silver");
        setIf(ORE_LEAD_COUNT, o, "lead");
    }
    setIf(ORE_INFINITE, j, "ore_infinite");
    setIf(ORE_DEPOSIT_AMOUNT, j, "ore_deposit_amount");

    // ---- 采矿场参数（范围/速率，个每秒） ----
    setIf(MINER_RADIUS_L1, j, "miner_radius_l1");
    setIf(MINER_RADIUS_L2, j, "miner_radius_l2");
    setIf(MINER_RADIUS_L3, j, "miner_radius_l3");
    setIf(MINER_RATE_L1, j, "miner_rate_l1");
    setIf(MINER_RATE_L2, j, "miner_rate_l2");
    setIf(MINER_RATE_L3, j, "miner_rate_l3");
    setIf(VOID_MINER_RATE, j, "void_miner_rate");

    // ---- 冶炼/开采时间 ----
    setIf(MINING_TIME_IRON, j, "mining_time_iron");
    setIf(MINING_TIME_COPPER, j, "mining_time_copper");
    setIf(MINING_TIME_COAL, j, "mining_time_coal");
    setIf(SMELT_TIME_IRON, j, "smelt_time_iron");
    setIf(SMELT_TIME_COPPER, j, "smelt_time_copper");

    // ---- 塔属性（tower_stats: {basic:{range,damage,fire_rate,bullet_speed}, ...}） ----
    if (j.contains("tower_stats")) {
        static const char* KEYS[4] = {"basic", "rapid", "sniper", "electric"};
        const auto& ts = j.at("tower_stats");
        for (int i = 0; i < 4; ++i) {
            if (!ts.contains(KEYS[i])) continue;
            const auto& t = ts.at(KEYS[i]);
            setIf(TURRET_STATS[static_cast<size_t>(i)].range, t, "range");
            setIf(TURRET_STATS[static_cast<size_t>(i)].damage, t, "damage");
            setIf(TURRET_STATS[static_cast<size_t>(i)].fireRate, t, "fire_rate");
            setIf(TURRET_STATS[static_cast<size_t>(i)].bulletSpeed, t, "bullet_speed");
        }
    }
    setIf(TURRET_AMMO_MAX_STACK, j, "turret_ammo_max_stack");
    setIf(TURRET_AMMO_MAX_SLOTS, j, "turret_ammo_max_slots");
    setIf(ELECTRIC_TOWER_MAX_POWER, j, "electric_tower_max_power");
    setIf(ELECTRIC_TOWER_SHOT_COST, j, "electric_tower_shot_cost");

    // ---- 敌人属性（enemy_stats: {basic:{health,speed,reward}, ...}） ----
    if (j.contains("enemy_stats")) {
        static const char* KEYS[3] = {"basic", "fast", "tank"};
        const auto& es = j.at("enemy_stats");
        for (int i = 0; i < 3; ++i) {
            if (!es.contains(KEYS[i])) continue;
            const auto& e = es.at(KEYS[i]);
            setIf(ENEMY_STATS[static_cast<size_t>(i)].health, e, "health");
            setIf(ENEMY_STATS[static_cast<size_t>(i)].speed, e, "speed");
            setIf(ENEMY_STATS[static_cast<size_t>(i)].reward, e, "reward");
        }
    }

    // ---- 配方表（数据驱动，可在 JSON 中增删配方） ----
    // 通用解析：recipe数组 → vector<Recipe>
    auto loadRecipes = [&](std::vector<Recipe>& table, const char* key) {
        if (!j.contains(key) || !j.at(key).is_array()) return;
        table.clear();
        for (const auto& r : j.at(key)) {
            Recipe rec;
            rec.nameZh = r.value("name", std::string("配方"));
            if (r.contains("inputs")) rec.inputs = parseItems(r.at("inputs"));
            if (r.contains("outputs")) rec.outputs = parseItems(r.at("outputs"));
            rec.craftTime = r.value("craft_time", 1.0f);
            table.push_back(std::move(rec));
        }
    };
    loadRecipes(FURNACE_RECIPES, "furnace_recipes");
    loadRecipes(ALLOY_RECIPES, "alloy_furnace_recipes");
    loadRecipes(ASSEMBLER_RECIPES, "assembler_recipes");
    loadRecipes(CRAFTING_RECIPES, "crafting_recipes");

    // ---- 商店价目表（shop_offers: [{name,gold,cost,give,building}]） ----
    if (j.contains("shop_offers") && j.at("shop_offers").is_array()) {
        SHOP_OFFERS.clear();
        for (const auto& o : j.at("shop_offers")) {
            ShopOffer offer;
            offer.nameZh = o.value("name", std::string("商品"));
            offer.gold = o.value("gold", 0);
            if (o.contains("cost")) offer.costItems = parseItems(o.at("cost"));
            if (o.contains("give")) offer.giveItems = parseItems(o.at("give"));
            if (o.contains("building")) {
                if (auto b = parseBuilding(o.at("building").get<std::string>()))
                    offer.giveBuilding = static_cast<int>(*b);
            }
            SHOP_OFFERS.push_back(std::move(offer));
        }
    }

    // ---- 建筑成本（building_costs: {basic_tower: {iron_ore: 15, ...}}） ----
    if (j.contains("building_costs") && j.at("building_costs").is_object()) {
        for (auto it = j.at("building_costs").begin(); it != j.at("building_costs").end(); ++it) {
            auto b = parseBuilding(it.key());
            if (!b) continue;
            auto items = parseItems(it.value());
            if (!items.empty())
                BUILDING_INFOS[static_cast<size_t>(*b)].cost = std::move(items);
        }
    }

    // ---- 电网（单位: EU/秒） ----
    setIf(LEGACY_GEN_POWER_OUTPUT, j, "legacy_gen_power_output");
    setIf(LEGACY_GEN_BURN_TIME, j, "legacy_gen_burn_time");
    setIf(LEGACY_GEN_MAX_COAL, j, "legacy_gen_max_coal");
    setIf(POWERGEN_OUTPUT_EUT, j, "powergen_output_eu_s");
    setIf(POWERGEN_COAL_BURN_TIME, j, "powergen_coal_burn_time");
    setIf(POWERGEN_INFINITE_FUEL, j, "powergen_infinite_fuel");
    setIf(POWER_POLE_RADIUS, j, "power_pole_radius");
    setIf(ELECTRIC_TOWER_GRID_NEED, j, "electric_tower_grid_need");
    setIf(MINER_POWER_NEED, j, "miner_power_need");
    setIf(MINER_FREE_POWER, j, "miner_free_power");
    setIf(CAPACITOR_CAPACITY, j, "capacitor_capacity");
    setIf(CAPACITOR_MAX_IN, j, "capacitor_max_in");
    setIf(CAPACITOR_MAX_OUT, j, "capacitor_max_out");
    setIf(ALLOY_FURNACE_ENERGY, j, "alloy_furnace_energy");

    // ---- 物流/机器 ----
    setIf(PIPES_TRANSFER_INTERVAL, j, "pipes_transfer_interval");
    setIf(PIPES_MAX_BUFFER, j, "pipes_max_buffer");
    setIf(PIPES_MAX_HOPS, j, "pipes_max_hops");
    setIf(PIPES_PULL_PER_TICK, j, "pipes_pull_per_tick");
    setIf(BUCKET_CAPACITY, j, "bucket_capacity");
    setIf(BUCKET_OUTPUT_INTERVAL, j, "bucket_output_interval");
    setIf(SPLITTER_TRANSFER_INTERVAL, j, "splitter_transfer_interval");
    setIf(SPLITTER_MAX_QUEUE, j, "splitter_max_queue");
    setIf(ME_TRANSFER_INTERVAL, j, "me_transfer_interval");
    setIf(ME_IMPORT_PER_TICK, j, "me_import_per_tick");
    setIf(ME_EXPORT_PER_TICK, j, "me_export_per_tick");
    setIf(ME_DRIVE_CAPACITY, j, "me_drive_capacity");

    return true;
}

} // namespace cfg
