#pragma once
// =====================================================================
// Me.h —— 通物网络存储物流组件（后期科技）
//
// 前期用物品管道（Pipe，逐段运输）；
// 后期用通物网络：所有物品作为"数据"存入网络，全网共享。
//
//   方块          | 作用
//   MeInterface  | 网络桥接器：从相邻管道/分流器"吸入"物品入网；
//                | 向相邻机器/桶/塔"导出"物品出网（输入总线+输出总线合体）
//   MeDrive      | 存储单元：为网络提供存储容量
//   MeTerminal   | 终端：右键查看全网物品
//
// 设备间自动链接（四邻，无面配置九宫格），连通块构成一个网络；
// 网络存储 = 块内所有 MeDrive 容量之和，物品类型全局共享。
// =====================================================================
#include <cstdint>
#include <vector>
#include "GameConfig.h"

/// 通物接口（桥接物理世界与网络）
struct MeInterface {
    uint8_t connMask = 0;    // bit d: 该方向相邻是通物设备
    int networkId = -1;      // 所属网络编号（MeSystem::rebuildNetworks 重算）
    float timer = 0.0f;      // 转移间隔计时器(秒)
    std::vector<cfg::ItemType> filter;  // 输出过滤（白名单：仅导出列出物品；空=不过滤）
    int itemCursor = 0;      // 物品轮询指针（按 ItemType 枚举序，多物品时均分调度）
};

/// 通物存储单元（提供网络容量）
struct MeDrive {
    uint8_t connMask = 0;
    int networkId = -1;
};

/// 通物终端（查询面板）
struct MeTerminal {
    uint8_t connMask = 0;
    int networkId = -1;
};
