#pragma once
// =====================================================================
// SaveSystem.h —— 存档系统（手动保存 · 10 个独立槽位）
//
// 设计参考《世界盒子》(WorldBox)：
//   - 游戏**不再自动保存**：只有玩家主动触发（游戏内暂停面板 / F5）才写盘，
//     因此「新开一局 → 直接关窗」不会覆盖任何已有存档。
//   - 共 SAVE_SLOT_COUNT 个独立槽位（saves/slot_01.json ... slot_10.json），
//     每个槽位可单独创建 / 读取 / 覆盖 / 删除，互不影响。
//   - 槽位带元数据（保存时间 saved_at + 摘要），菜单里直接展示状态。
//
// 移植 save_system.py：保存游戏状态、建筑（含库存/面配置/传送带物品）、
// 敌人与波次状态。由于依赖清单仅限 sfml/entt，采用自定义文本格式
// （无需第三方JSON库）。
// =====================================================================
#include <string>

class Game;

/// 槽位总数（WorldBox 风格：10 个独立存档槽位）
inline constexpr int SAVE_SLOT_COUNT = 10;

/// 单个槽位的状态快照（供菜单/面板展示；只读取存档头部，不修改游戏状态）
struct SaveSlotInfo {
    bool used = false;       // 是否已被占用
    bool corrupt = false;    // 文件存在但无法解析
    std::string path;        // 槽位文件路径
    std::string savedAt;     // 保存时间 "2026-09-15 14:30"（旧档/无元数据时为空）
    std::string summary;     // 摘要 "金币 777 · 第 3 波 · 建筑 42"
    long long bytes = 0;     // 文件字节数
};

/// 槽位文件路径（slot 从 0 开始；越界返回空串）
std::string slotPath(int slot);

/// 读取槽位状态（不修改游戏状态，可随时调用用于刷新界面）
SaveSlotInfo querySlot(int slot);

/// 是否存在任一已占用槽位
bool anySlotUsed();

/// 最近一次保存的槽位（-1 表示所有槽位都是空的）
int newestSlot();

/// 保存到指定槽位（返回是否成功；已占用时由调用方负责先问玩家是否覆盖）
bool saveGameToSlot(Game& g, int slot);

/// 从指定槽位读取（返回是否成功）
bool loadGameFromSlot(Game& g, int slot);

/// 删除指定槽位的存档（返回是否真的删除了）
bool deleteSlot(int slot);

/// 把旧版单文件存档 saves/factory_td.json 迁移到 1 号槽位（启动时调用一次）
void migrateLegacySave();

// ---- 按路径直接读写（自检 / 工具用；游戏内请使用上面的槽位接口） ----
bool saveGameToFile(Game& g, const std::string& path);
bool loadGameFromFile(Game& g, const std::string& path);
