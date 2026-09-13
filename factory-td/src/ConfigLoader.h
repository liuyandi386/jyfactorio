#pragma once
// =====================================================================
// ConfigLoader.h —— JSON 配置加载器
//
// 启动时读取 assets/config.json，覆盖 GameConfig.h 中标注 [JSON可调]
// 的游戏数值（塔/敌人/配方/成本/电网/波次/矿点等），
// 实现"改数值无需重新编译"。文件缺失或字段错误时静默使用内置默认值。
// =====================================================================
#include <string>

namespace cfg {

/// 加载并应用 JSON 配置（不存在/解析失败则保持默认值）
/// @param path 配置文件路径（如 "assets/config.json"）
/// @return 是否成功加载
bool loadConfig(const std::string& path);

} // namespace cfg
