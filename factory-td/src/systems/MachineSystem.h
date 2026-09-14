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
#include <entt/entity/entity.hpp>
#include "GameConfig.h"
#include "components/Machine.h"

class Game;
struct Building;

/// 机器系统
class MachineSystem {
public:
    /// 生产计时更新（采矿/冶炼/组装/发电机燃烧/储物桶计时）
    static void updateMachines(Game& g, float dt);

    /// 输出推送（采矿机/熔炉/组装机/储物桶 → 桶/传送带/机器）
    static void pushOutputs(Game& g, float dt);
};

// ---------------------------------------------------------------------
// 采矿场模式管理（右键设置面板使用）
//
// 两种模式：
//   · 原有模式  (Machine::fixedOre == false)
//       在半径内所有矿点中随机采集（按矿点数量均匀抽取），行为与旧版完全一致。
//   · 固定矿点模式 (Machine::fixedOre == true)
//       采矿场吸附到矿点旁边（移动建筑到矿点相邻格），只采集 Machine::oreFilter
//       指定矿种的**那一个**矿点；产出速率与每次结算数量与原有模式完全相同。
// 切换、筛矿、吸附都只改"采谁"，不改"采多少"。
// ---------------------------------------------------------------------
namespace MinerSystem {
/// 采矿场覆盖半径（按等级：L1=2 / L2=4 / L3=6；虚空采矿场与 L1 同）
int radiusOf(const Machine& m);

/// 半径内是否存在指定矿种的矿点（ore == ItemType::COUNT 表示"任意矿种"）
bool hasOreInRange(const Game& g, const Building& b, const Machine& m,
                   cfg::ItemType ore);

/// 当前绑定矿点实体（已采尽/被拆/超出半径 → entt::null）
/// 注意：会在必要时刷新 m 的绑定缓存，故 m 为非 const
entt::entity boundDeposit(const Game& g, const Building& b, Machine& m);

/// 切换模式（切到固定矿点模式时会自动吸附到矿点旁边并绑定）
/// 半径内没有可用矿点时返回 false 且保持原有模式
bool setFixedMode(Game& g, entt::entity e, bool fixed);

/// 设置固定矿点模式的筛选矿种（自动重新绑定/吸附；无该矿种矿点返回 false）
bool setOreFilter(Game& g, entt::entity e, cfg::ItemType ore);

/// 旋转输出面：同步 Building::dir 与 FaceConfig（保持"贴图朝向 == 输出面"）
void rotateOutputFace(Game& g, entt::entity e);
} // namespace MinerSystem
