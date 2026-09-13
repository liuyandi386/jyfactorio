#pragma once
// =====================================================================
// FaceConfig.h —— 四面配置组件（格雷科技九宫格简化版）
//
// 格雷科技机器用"九宫格"配置六个面的输入输出；本作是2D俯视游戏，
// 去掉上下两个面后保留四个方向面：UP / RIGHT / DOWN / LEFT。
//
// 每个面可配置为：
//   NONE     - 不连接
//   INPUT    - 输入（从该面接收物品/电力）
//   TRANSFER - 传输/中继（仅电力线缆使用）
//   OUTPUT   - 输出（从该面向外送出物品/电力）
//
// 不同实体允许的模式数量不同：
//   机器(采矿机/熔炉/组装机/发电机/储物桶/分流器) → NONE/INPUT/OUTPUT 三种
//   电力线缆 → NONE/INPUT/TRANSFER/OUTPUT 四种
// =====================================================================
#include <array>
#include <cstdint>
#include "GameConfig.h"

/// 面配置组件
struct FaceConfig {
    std::array<cfg::FaceMode, 4> faces{}; // [UP, RIGHT, DOWN, LEFT]
    uint8_t modeCount = 3;                // 模式循环上限: 3或4

    /// 构造：全部面设为同一模式
    static FaceConfig makeAll(cfg::FaceMode m, uint8_t count = 3) {
        FaceConfig fc;
        fc.faces.fill(m);
        fc.modeCount = count;
        return fc;
    }

    /// 获取指定方向面的模式
    cfg::FaceMode get(int d) const { return faces[static_cast<size_t>(d)]; }

    /// 设置指定方向面的模式
    void set(int d, cfg::FaceMode m) { faces[static_cast<size_t>(d)] = m; }

    /// 循环切换一个面（面编辑器点击按钮时调用）：
    /// 3态(机器/分流器): NONE→INPUT→OUTPUT→NONE（跳过TRANSFER）
    /// 4态(电力线缆):     NONE→INPUT→TRANSFER→OUTPUT→NONE
    void cycle(int d) {
        auto& f = faces[static_cast<size_t>(d)];
        if (modeCount == 3) {
            // 三态不能简单取模：枚举值为 NONE=0 INPUT=1 TRANSFER=2 OUTPUT=3，
            // 取模循环是 NONE→INPUT→TRANSFER→NONE，永远到不了 OUTPUT(=3)
            f = (f == cfg::FaceMode::NONE)  ? cfg::FaceMode::INPUT
              : (f == cfg::FaceMode::INPUT) ? cfg::FaceMode::OUTPUT
                                            : cfg::FaceMode::NONE;
        } else {
            f = static_cast<cfg::FaceMode>((static_cast<uint8_t>(f) + 1) % modeCount);
        }
    }

    /// 整体顺时针旋转一格（右键旋转建筑方向时调用）
    void rotate() {
        std::array<cfg::FaceMode, 4> old = faces;
        for (int i = 0; i < 4; ++i) faces[i] = old[(i + 1) % 4];
    }
};
