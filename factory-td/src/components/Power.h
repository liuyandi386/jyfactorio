#pragma once
// =====================================================================
// Power.h —— 电网组件（格雷科技式 EU 电力系统）
//
// 节点种类：
//   Generator:  燃煤发电机（供电方，单位 EU/秒）
//   Capacitor:  电容库（储能缓冲，充/放电速率限制）
//   Consumer:   用电设备（电力塔8EU/秒、采矿机10EU/秒）
//   Pole:       电线杆（150px半径内连接设备/电线杆，恒导通）
// 电力线缆的面配置复用 FaceConfig 组件（modeCount=4）。
// 电力按"发电量-耗电量"平衡，经电线面配置BFS路由；不足时电容放电，
// 盈余时电容充电（详见 PowerSystem.cpp，逻辑移植自 power_network.py）。
// =====================================================================
#include <algorithm>
#include <cstdint>
#include "GameConfig.h"

/// 发电机组件
struct PowerGeneratorNode {
    float outputRate = 0.0f;     // 输出功率(EU/秒)
    float fuelTime = 0.0f;       // 剩余燃料燃烧时间(秒)
    float burnTotal = 5.0f;      // 每块燃料燃烧时长(秒)
    float burnProgress = 0.0f;   // 当前燃料已燃烧时间
    bool running = false;        // 是否运行中
    bool legacyMode = false;     // true=旧版大功率发电机(3000EU/3秒) false=工业EU发电机
};

/// 电容库组件（速率单位: EU/秒）
struct PowerCapacitor {
    float capacity = cfg::CAPACITOR_CAPACITY;
    float energy = 0.0f;          // 当前储能
    float maxInput = cfg::CAPACITOR_MAX_IN;   // 最大充电速率(EU/秒)
    float maxOutput = cfg::CAPACITOR_MAX_OUT; // 最大放电速率(EU/秒)

    /// 充电（amount 为本帧电量 EU = 速率×dt），返回实际充入电量
    float charge(float amount, float dt) {
        if (amount <= 0.0f || energy >= capacity) return 0.0f;
        const float actual = std::min({amount, maxInput * dt, capacity - energy});
        energy += actual;
        return actual;
    }

    /// 放电（amount 为本帧请求电量 EU），返回实际放出电量
    float discharge(float amount, float dt) {
        if (amount <= 0.0f || energy <= 0.0f) return 0.0f;
        const float actual = std::min({amount, maxOutput * dt, energy});
        energy -= actual;
        return actual;
    }
};

/// 用电设备组件（电力塔/采矿机，速率单位: EU/秒）
struct PowerConsumer {
    float rate = 0.0f;      // 耗电速率(EU/秒)
    bool powered = false;   // 是否已供电
};

/// 电线杆组件
struct PowerPole {
    float radius = cfg::POWER_POLE_RADIUS; // 连接半径(像素)
};
