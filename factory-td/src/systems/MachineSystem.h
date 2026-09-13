#pragma once
// =====================================================================
// MachineSystem.h —— 机器系统（采矿机/熔炉/组装机/储物桶）
//
// 生产逻辑（移植 Miner.py / Generator.py / AmmoFactory.py / Bucket.py）：
//   采矿机：按矿点 miningTime 周期产出矿石入内部库存
//   熔炉：  从INPUT面接收矿石，冶炼成锭（新增）
//   组装机：按配方(2铁锭+1铜锭→1弹药)每1秒合成一次（替代弹药制造机）
//   发电机：燃烧煤炭产生EU（燃料由传送带经INPUT面供给）
//   储物桶：FIFO库存，按0.5秒间隔向OUTPUT面输出
// 输出逻辑（移植 _update_miner_output / _update_factory_output /
//   _update_bucket_output）：优先储物桶，其次传送带，其次相邻机器。
// =====================================================================
class Game;

/// 机器系统
class MachineSystem {
public:
    /// 生产计时更新（采矿/冶炼/组装/发电机燃烧/储物桶计时）
    static void updateMachines(Game& g, float dt);

    /// 输出推送（采矿机/熔炉/组装机/储物桶 → 桶/传送带/机器）
    static void pushOutputs(Game& g, float dt);
};
