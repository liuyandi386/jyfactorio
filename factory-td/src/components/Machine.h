#pragma once
// =====================================================================
// Machine.h —— 生产机器组件（采矿机/熔炉/组装机）
//
// 采矿机：从所在格矿点按 miningTime 周期产出矿石
// 熔炉：  从INPUT面接收矿石冶炼成锭（新增）
// 组装机：按配方(2铁锭+1铜锭→1弹药)周期性合成（替代原弹药制造机）
// =====================================================================
#include <cstdint>
#include "GameConfig.h"
#include "Item.h"

/// 机器种类
enum class MachineKind : uint8_t {
    Miner,        // 采矿机
    Furnace,      // 熔炉（矿石→锭，数据驱动配方 FURNACE_RECIPES）
    AlloyFurnace, // 合金炉（锭→合金，数据驱动配方 ALLOY_RECIPES）
    Assembler     // 组装机（数据驱动配方 ASSEMBLER_RECIPES）
};

/// 生产机器组件
struct Machine {
    MachineKind kind = MachineKind::Miner;

    // ---- 采矿场（GT式：范围内采集所有类型矿石） ----
    cfg::ItemType ore = cfg::ItemType::IronOre; // 旧字段：单一矿种（保留兼容）
    float miningTime = cfg::MINING_TIME_IRON;   // 开采周期(秒)（旧逻辑）
    float timer = 0.0f;         // 采矿机开采计时器
    int level = 1;              // 采矿场等级 1/2/3（范围 5×5/9×9/13×13）
    bool voidMiner = false;     // 是否虚空采矿场（无需矿点，产全类型矿石）
    float rate = cfg::MINER_RATE_L1;   // 产出速率(个/秒)
    float acc = 0.0f;           // 产出累加器（小数部分跨帧保留）

    // ---- 熔炉/组装机/合金炉 ----
    float craftTimer = 0.0f;   // 组装机合成计时器（到达周期即合成一次）
    float progress = 0.0f;     // 当前生产进度(0~1)，用于进度条显示
    bool producing = false;    // 是否正在生产
    bool powered = true;       // 供电状态（测试版采矿机免供电，见GameConfig）
    // 当前配方索引：组装机/合金炉=选定配方；
    // 熔炉=当前冶炼任务配方(FURNACE_RECIPES)
    int recipeId = 0;

    // ---- 熔炉/合金炉当前冶炼任务 ----
    bool hasJob = false;
    cfg::ItemType jobInput = cfg::ItemType::IronOre;
    float jobTime = 0.0f;      // 任务已进行时间
    float jobTotal = 2.0f;     // 任务总时长（按配方 craftTime）
};
