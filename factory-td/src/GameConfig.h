#pragma once
// =====================================================================
// GameConfig.h —— 游戏数值配置中心
//
// 所有游戏参数集中于此文件，便于统一调整平衡性，无需改动系统代码。
// 数值来源：Python版 data.py / config.py（保持原数值），
// 新增内容（熔炉/组装机配方等）已用"新增"注释标明。
// =====================================================================
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace cfg {

// ================= 窗口设置 (config.py) =================
inline constexpr int SCREEN_WIDTH  = 1280;   // 窗口宽
inline constexpr int SCREEN_HEIGHT = 720;    // 窗口高
inline constexpr const char* SCREEN_TITLE = "织星计划 Project Weavestar  v1.3.3";

// ================= 地图设置 =================
inline constexpr int TILE_SIZE  = 32;    // 瓦片像素尺寸（Python一致）
inline constexpr int GRID_WIDTH  = 200;  // 网格宽（Python为50，按需求扩到200）
inline constexpr int GRID_HEIGHT = 200;  // 网格高

// ================= 摄像机设置（全部以「秒」为单位，与帧率无关） =================
// ⚠ 这两个值曾经是"每帧"单位（5 像素/帧、每帧插值 0.1）。那套写法只在锁 60 帧时正确，
//   一旦开启垂直同步在高刷屏上跑 144 帧，摄像机就会快 2.4 倍。现在统一改成每秒速率：
//     5   像素/帧 × 60 帧/秒 = 300 像素/秒
//     0.1 每帧插值          → 指数平滑速率 6 /秒（60 帧下每帧系数≈0.095，手感一致）
inline constexpr float CAMERA_SPEED_PER_SEC = 300.0f;  // WASD移动速度(像素/秒)
inline constexpr float ZOOM_MIN     = 0.5f;   // 最小缩放
inline constexpr float ZOOM_MAX     = 2.0f;   // 最大缩放
inline constexpr float ZOOM_SPEED   = 0.1f;   // 滚轮单次缩放步长
inline constexpr float CAMERA_SMOOTH_RATE = 6.0f;      // 平滑插值速率(1/秒)

// ================= 游戏设置 =================
// 「固定 60 帧」档使用的上限；实际帧率策略见 Settings.h 的 gset::FrameMode
// （默认垂直同步，跟随显示器刷新率，144Hz 屏即 144 帧）
inline constexpr int FPS           = 60;
// 以下标有"JSON可调"的变量运行时会被 assets/config.json 覆盖（改数值无需重编译）
inline int INITIAL_GOLD  = 500;     // 初始金币（击杀敌人获得）  [JSON可调]
inline int INITIAL_LIVES = 10;      // 初始生命（敌人抵达终点-1） [JSON可调]

// ================= 波次设置 =================
inline float WAVE_INTERVAL     = 10.0f;  // 波间间隔(秒)           [JSON可调]
inline int   WAVE_ENEMIES      = 5;      // 第一波敌人数量(每波+2*波数) [JSON可调]
inline float INITIAL_COUNTDOWN = 180.0f; // 开局准备倒计时(秒)     [JSON可调]
inline float SPAWN_INTERVAL    = 1.0f;   // 波内生成间隔(秒)       [JSON可调]
// 波次自动生成开关：测试期关闭（按Z键手动生成普通敌人），以后打开恢复波次玩法
inline bool WAVE_AUTO_SPAWN = false;     // [JSON可调]

// ================= 玩家资源（测试版无限资源） =================
// 注意：当前为测试版本，资源暂时无限；以后版本将改为有限资源。
inline int INFINITE_RESOURCE = 999999;   // 无限资源数量           [JSON可调]
inline bool RESOURCE_INFINITE = true;    // 资源无限开关           [JSON可调]

// ================= 敌人路径（PATH_POINTS） =================
// Python原坐标为50x50地图的10个路径点；按×4缩放适配200x200地图，形状不变。
// Python原值: (0,15)(10,15)(10,5)(25,5)(25,20)(40,20)(40,35)(20,35)(20,45)(49,45)
inline const std::vector<sf::Vector2i> PATH_POINTS = {
    {0,   60}, {40,  60}, {40,  20}, {100, 20}, {100, 80},
    {160, 80}, {160, 140}, {80, 140}, {80, 180}, {199, 180}
};

// ================= 矿点生成 =================
// Python: 固定随机种子42; 8铁/6铜/5煤 随机散布(仅避开敌人路径)
// C++: 全图随机混布(不再按矿种分区域连片生成)，避开路径且互不重叠；
// 数量按地图面积比例放大(200x200为50x50的16倍): 铁128/铜96/煤80
inline constexpr uint32_t ORE_RANDOM_SEED = 42;
inline int ORE_IRON_COUNT  = 8 * 16;    // 铁矿点数量(128)        [JSON可调]
inline int ORE_COPPER_COUNT = 6 * 16;   // 铜矿点数量(96)         [JSON可调]
inline int ORE_COAL_COUNT  = 5 * 16;    // 煤矿点数量(80)         [JSON可调]
inline int ORE_GOLD_COUNT    = 40;      // 金矿点数量             [JSON可调]
inline int ORE_DIAMOND_COUNT = 24;      // 钻石矿点数量           [JSON可调]
inline int ORE_NICKEL_COUNT  = 48;      // 镍矿点数量             [JSON可调]
inline int ORE_SILVER_COUNT  = 40;      // 银矿点数量             [JSON可调]
inline int ORE_LEAD_COUNT    = 48;      // 铅矿点数量             [JSON可调]
// 矿点初始储量（有限矿点，采尽后消失）
inline int ORE_DEPOSIT_AMOUNT = 1000;   // 每种矿点储量           [JSON可调]
inline constexpr int ORE_X_MIN = 2, ORE_X_MAX = 197;   // 全图x范围(避开地图边缘)
inline constexpr int ORE_Y_MIN = 2, ORE_Y_MAX = 197;   // 全图y范围

// ================= 矿机（GT式采矿场） =================
// 1/2/3级矿机分别采集 5×5/9×9/13×13 范围内所有类型矿石，
// 每秒分别产出 4/16/256 个矿石；虚空采矿场每秒产出全类型矿石共4096个。
inline int   MINER_RADIUS_L1 = 2;      // 半径2 → 5×5              [JSON可调]
inline int   MINER_RADIUS_L2 = 4;      // 半径4 → 9×9              [JSON可调]
inline int   MINER_RADIUS_L3 = 6;      // 半径6 → 13×13            [JSON可调]
inline float MINER_RATE_L1   = 4.0f;   // 产出 个/秒              [JSON可调]
inline float MINER_RATE_L2   = 16.0f;  //                         [JSON可调]
inline float MINER_RATE_L3   = 256.0f; //                         [JSON可调]
inline float VOID_MINER_RATE = 4096.0f;// 虚空采矿场总产出 个/秒   [JSON可调]

// ================= 物品类型 =================
enum class ItemType : uint8_t {
    IronOre,        // 铁矿石
    CopperOre,      // 铜矿石
    Coal,           // 煤矿
    GoldOre,        // 金矿石
    DiamondOre,     // 钻石矿石
    NickelOre,      // 镍矿石
    SilverOre,      // 银矿石
    LeadOre,        // 铅矿石
    IronIngot,      // 铁锭（熔炉冶炼）
    CopperIngot,    // 铜锭（熔炉冶炼）
    GoldIngot,      // 金锭（熔炉冶炼）
    NickelIngot,    // 镍锭（熔炉冶炼）
    SilverIngot,    // 银锭（熔炉冶炼）
    LeadIngot,      // 铅锭（熔炉冶炼）
    CircuitBoard,   // 电路板（随身工作台/组装机合成）
    Ammo,           // 弹药
    SteelIngot,     // 钢锭（合金炉：铁+煤）
    ElectrumIngot,  // 琥珀金锭（合金炉：金+银）
    InvarIngot,     // 因瓦合金锭（合金炉：铁+镍）
    ConstantanIngot,// 康铜锭（合金炉：铜+镍）
    COUNT           // 物品种类计数
};
inline constexpr int ITEM_COUNT = static_cast<int>(ItemType::COUNT);

// 8 种矿石列表（矿机/虚空采矿场产出遍历用）
inline constexpr std::array<ItemType, 8> ORE_TYPES = {
    ItemType::IronOre, ItemType::CopperOre, ItemType::Coal, ItemType::GoldOre,
    ItemType::DiamondOre, ItemType::NickelOre, ItemType::SilverOre, ItemType::LeadOre};

// 物品注册表：中文名 / 缩写符号 / 显示颜色 / 是否可由传送带运输
struct ItemInfo {
    const char* nameZh;      // 中文名
    const char* symbol;      // 网格显示缩写
    sf::Color   color;       // 显示颜色（继承Python COLORS）
    bool        transportable; // 是否可运输（电力类物品禁止）
};
inline const std::array<ItemInfo, ITEM_COUNT> ITEM_INFOS = {{
    {"铁矿石",   "Fe",  sf::Color(120, 120, 140), true },
    {"铜矿石",   "Cu",  sf::Color(180, 100, 50),  true },
    {"煤矿",     "Co",  sf::Color(40, 40, 40),    true },
    {"金矿石",   "Au",  sf::Color(255, 215, 0),   true },
    {"钻石矿石", "Dia", sf::Color(0, 230, 255),   true },
    {"镍矿石",   "Ni",  sf::Color(140, 170, 140), true },
    {"银矿石",   "Ag",  sf::Color(200, 200, 210), true },
    {"铅矿石",   "Pb",  sf::Color(90, 90, 110),   true },
    {"铁锭",     "FeI", sf::Color(192, 192, 192), true },
    {"铜锭",     "CuI", sf::Color(210, 105, 30),  true },
    {"金锭",     "AuI", sf::Color(255, 200, 60),  true },
    {"镍锭",     "NiI", sf::Color(180, 200, 180), true },
    {"银锭",     "AgI", sf::Color(230, 230, 240), true },
    {"铅锭",     "PbI", sf::Color(110, 110, 135), true },
    {"电路板",   "C",   sf::Color(0, 200, 100),   true },
    {"弹药",     "A",   sf::Color(255, 255, 0),   true },
    {"钢锭",     "StI", sf::Color(150, 155, 165), true },
    {"琥珀金锭", "ElI", sf::Color(255, 220, 120), true },
    {"因瓦锭",   "IvI", sf::Color(185, 195, 175), true },
    {"康铜锭",   "CnI", sf::Color(215, 145, 90),  true },
}};

// ================= 资源开采时间 (data.py RESOURCE_TYPES) =================
inline float MINING_TIME_IRON   = 2.0f;  // 铁矿开采周期(秒)        [JSON可调]
inline float MINING_TIME_COPPER = 2.0f;  //                         [JSON可调]
inline float MINING_TIME_COAL   = 1.5f;  //                         [JSON可调]

// ================= 冶炼时间（熔炉，数据驱动配方的默认周期） =================
inline float SMELT_TIME_IRON   = 2.0f;  // 铁矿石→铁锭              [JSON可调]
inline float SMELT_TIME_COPPER = 2.0f;  // 铜矿石→铜锭              [JSON可调]

// ================= 数据驱动配方表（所有机器共用） =================
// 熔炉/合金炉/组装机/随身工作台全部从配方表读取配方，
// 新增配方只需在对应表追加一行（并由 config.json 覆盖，改数值无需重编译）。
struct Recipe {
    std::string nameZh;                              // 配方中文名（std::string以支持JSON覆盖）
    std::vector<std::pair<ItemType, int>> inputs;    // 原料
    std::vector<std::pair<ItemType, int>> outputs;   // 产物
    float craftTime;                                 // 合成周期(秒)
};

// ---- 熔炉配方表：矿石 → 锭（GT式：1矿石=1锭，冶炼时间随矿种） ----
inline std::vector<Recipe> FURNACE_RECIPES = {
    {"铁锭",     {{ItemType::IronOre, 1}},    {{ItemType::IronIngot, 1}},    cfg::SMELT_TIME_IRON},
    {"铜锭",     {{ItemType::CopperOre, 1}},  {{ItemType::CopperIngot, 1}},  cfg::SMELT_TIME_COPPER},
    {"金锭",     {{ItemType::GoldOre, 1}},    {{ItemType::GoldIngot, 1}},    3.0f},
    {"镍锭",     {{ItemType::NickelOre, 1}},  {{ItemType::NickelIngot, 1}},  3.0f},
    {"银锭",     {{ItemType::SilverOre, 1}},  {{ItemType::SilverIngot, 1}},  3.0f},
    {"铅锭",     {{ItemType::LeadOre, 1}},    {{ItemType::LeadIngot, 1}},    3.0f},
};

// ---- 合金炉配方表：锭 → 合金（参考 GT/Mek/Thermal 的经典合金配方） ----
// 钢 Steel: 铁+煤（GT电弧炉/Mek冶金注入/Thermal感应炉通用配方）
// 琥珀金 Electrum: 金+银（GT/Thermal 均为一金一银出两锭）
// 因瓦 Invar: 铁+镍 2:1（GT/Thermal 出3锭）
// 康铜 Constantan: 铜+镍 1:1（Thermal 出2锭）
inline std::vector<Recipe> ALLOY_RECIPES = {
    {"钢锭",     {{ItemType::IronIngot, 2}, {ItemType::Coal, 2}},
     {{ItemType::SteelIngot, 1}},        6.0f},
    {"琥珀金锭", {{ItemType::GoldIngot, 1}, {ItemType::SilverIngot, 1}},
     {{ItemType::ElectrumIngot, 2}},     4.0f},
    {"因瓦锭",   {{ItemType::IronIngot, 2}, {ItemType::NickelIngot, 1}},
     {{ItemType::InvarIngot, 3}},        5.0f},
    {"康铜锭",   {{ItemType::CopperIngot, 1}, {ItemType::NickelIngot, 1}},
     {{ItemType::ConstantanIngot, 2}},   4.0f},
};

// ---- 组装机配方表（数据驱动，可多选：右键切换当前配方） ----
inline std::vector<Recipe> ASSEMBLER_RECIPES = {
    {"弹药",   {{ItemType::IronIngot, 2}, {ItemType::CopperIngot, 1}},
     {{ItemType::Ammo, 1}}, 1.0f},
    {"电路板", {{ItemType::IronIngot, 1}, {ItemType::CopperIngot, 1}},
     {{ItemType::CircuitBoard, 1}}, 1.0f},
};

// ---- 随身工作台配方表（泰拉瑞亚/MC式手工合成，前期物品迁移至此） ----
inline std::vector<Recipe> CRAFTING_RECIPES = {
    {"电路板", {{ItemType::IronIngot, 1}, {ItemType::CopperIngot, 1}},
     {{ItemType::CircuitBoard, 1}}, 0.5f},
    {"弹药",   {{ItemType::IronIngot, 2}, {ItemType::CopperIngot, 1}},
     {{ItemType::Ammo, 1}}, 0.5f},
};

// ================= 电力配方 =================
// 旧版发电机（实体Generator.py）：1煤 → 3000EU / 3.0秒
inline int   LEGACY_GEN_POWER_OUTPUT = 3000;   // 每次燃烧产出EU        [JSON可调]
inline float LEGACY_GEN_BURN_TIME    = 3.0f;   // 燃烧周期(秒)          [JSON可调]
inline int   LEGACY_GEN_MAX_COAL     = 50;     // 燃料库存上限          [JSON可调]
// 工业燃煤发电机：输出32 EU/秒，煤燃烧5秒（电力单位已改为"每秒"）
inline float POWERGEN_OUTPUT_EUT = 32.0f;     // 输出EU/秒             [JSON可调]
inline float POWERGEN_COAL_BURN_TIME = 5.0f;  // 每块煤燃烧(秒)        [JSON可调]
inline bool  POWERGEN_INFINITE_FUEL = false;  // 测试无限燃料开关      [JSON可调]

// ================= 电网参数（单位: EU/秒） =================
inline float POWER_POLE_RADIUS = 150.0f;      // 电线杆连接半径(像素)   [JSON可调]
inline float ELECTRIC_TOWER_GRID_NEED = 8.0f; // 电力塔入网需求EU/秒   [JSON可调]
inline float MINER_POWER_NEED = 10.0f;        // 采矿机耗电EU/秒        [JSON可调]
inline bool  MINER_FREE_POWER = true;         // 测试模式: 采矿机免供电 [JSON可调]
// 电容库 (capacitor.py)
inline float CAPACITOR_CAPACITY = 50000.0f;   // 容量EU                 [JSON可调]
inline float CAPACITOR_MAX_IN   = 64.0f;      // 最大充电EU/秒          [JSON可调]
inline float CAPACITOR_MAX_OUT  = 64.0f;      // 最大放电EU/秒          [JSON可调]

// ================= 物品管道 (PipeSystem) =================
inline float PIPES_TRANSFER_INTERVAL = 0.25f; // 路由转移间隔(秒)      [JSON可调]
inline int   PIPES_MAX_BUFFER       = 16;     // 管道内部缓冲上限       [JSON可调]
inline int   PIPES_MAX_HOPS         = 100;    // BFS路由最大跳数        [JSON可调]
inline int   PIPES_PULL_PER_TICK    = 4;      // 每周期每管道可路由物品数 [JSON可调]

// ================= 机器参数 =================
// 采矿机 (Miner.py)
inline int   MINER_MAX_SLOTS    = 5;        // 库存槽数                [JSON可调]
inline int   MINER_MAX_STACK    = 50;       // 单槽堆叠                [JSON可调]
// 熔炉（新增）
inline int   FURNACE_MAX_SLOTS  = 5;        //                         [JSON可调]
inline int   FURNACE_MAX_STACK  = 50;       //                         [JSON可调]
// 组装机（原弹药制造机 AmmoFactory.py）
inline int   ASSEMBLER_MAX_SLOTS = 100;     //                         [JSON可调]
inline int   ASSEMBLER_MAX_STACK = 99999;   // 近乎无限容量            [JSON可调]
inline int   ASSEMBLER_ITEM_CAP  = 128;     // 组装机每种原料缓存上限  [JSON可调]

// ================= 储物桶 (Bucket.py) =================
inline int   BUCKET_CAPACITY      = 99999;  // 容量                    [JSON可调]
inline float BUCKET_OUTPUT_INTERVAL = 0.5f; // 输出间隔(秒)            [JSON可调]

// ================= 分流器 (Splitter.py) =================
inline float SPLITTER_TRANSFER_INTERVAL = 0.15f; // 传输间隔(秒)       [JSON可调]
inline int   SPLITTER_MAX_QUEUE = 100;           // 内部队列容量       [JSON可调]

// ================= 塔配置 (data.py TOWER_STATS) =================
enum class TurretType : uint8_t { Basic, Rapid, Sniper, Electric };
struct TurretStats {
    float range;        // 射程(像素)
    int   damage;       // 单发伤害
    float fireRate;     // 射速(发/秒)
    float bulletSpeed;  // 子弹速度(格/秒, 移动时×32像素)
};
// 塔属性表：内容运行时由 config.json 覆盖                        [JSON可调]
inline std::array<TurretStats, 4> TURRET_STATS = {{
    {150.0f, 20, 1.0f, 10.0f},  // basic   基础塔
    {120.0f, 10, 3.0f, 15.0f},  // rapid   速射塔
    {250.0f, 50, 0.5f, 8.0f},   // sniper  狙击塔
    {180.0f, 25, 2.0f, 12.0f},  // electric 电力塔
}};
inline int   TURRET_AMMO_MAX_STACK = 20;    // 弹药塔库存(3槽x20)     [JSON可调]
inline int   TURRET_AMMO_MAX_SLOTS = 3;     //                         [JSON可调]
inline float ELECTRIC_TOWER_MAX_POWER = 100.0f; // 电力塔内部电力     [JSON可调]
inline float ELECTRIC_TOWER_SHOT_COST = 10.0f;  // 每发耗电           [JSON可调]

// ================= 敌人配置 (data.py ENEMY_STATS) =================
enum class EnemyType : uint8_t { Basic, Fast, Tank };
struct EnemyStats {
    int   health;   // 最大血量
    float speed;    // 移动速度(格/秒, 移动时×32像素)
    int   reward;   // 击杀金币奖励
};
// 敌人属性表：内容运行时由 config.json 覆盖                     [JSON可调]
inline std::array<EnemyStats, 3> ENEMY_STATS = {{
    {100, 1.5f, 20},   // basic 基础敌人
    {60,  3.0f, 30},   // fast  快速敌人
    {300, 0.8f, 50},   // tank  坦克敌人
}};
// 波次生成的敌人类型（Python只生成basic，此处保留可扩展性）
inline constexpr EnemyType WAVE_ENEMY_TYPE = EnemyType::Basic;
inline constexpr float ENEMY_WAYPOINT_REACH = 5.0f;  // 到达路径点判定距离(像素)

// ================= 子弹 (Bullet.py) =================
inline constexpr float BULLET_HIT_DISTANCE = 12.0f;  // 命中判定距离(像素)
inline constexpr float BULLET_SPEED_MULT   = 32.0f;  // 速度×像素换算
inline constexpr int   BULLET_TRAIL_LENGTH = 3;      // 拖尾长度

// ================= 建筑成本 (data.py BUILDING_COSTS) =================
enum class BuildingType : uint8_t {
    TowerBasic, TowerRapid, TowerSniper, TowerElectric, // 炮塔
    Miner,        // 采矿场1级（5×5范围, 4个/秒）
    MinerL2,      // 采矿场2级（9×9范围, 16个/秒）
    MinerL3,      // 采矿场3级（13×13范围, 256个/秒）
    MinerVoid,    // 虚空采矿场（全类型4096个/秒）
    Furnace,      // 熔炉（矿石→锭）
    Assembler,    // 组装机（自动化合成）
    Generator,    // 燃煤发电机（旧版大功率）
    PowerPole,    // 电线杆
    PowerGenerator, // 燃煤发电机（工业EU）
    Capacitor,    // 电容库
    PowerWire,    // 电力线缆
    Pipe,         // 物品管道（自动链接四邻，即时路由）
    Bucket,       // 储物桶
    Splitter,     // 物品分流器
    AlloyFurnace, // 合金炉（16EU/s + 锭→合金）——追加在末尾，保持旧存档类型索引不变
    MeInterface,  // 通物接口（物品进出网络）
    MeDrive,      // 通物存储单元（网络容量）
    MeTerminal,   // 通物终端（查看全网物品）
    COUNT
};
inline constexpr int BUILDING_COUNT = static_cast<int>(BuildingType::COUNT);

struct BuildingInfo {
    const char* nameZh;                              // 中文名
    const char* hotkey;                              // 快捷键
    std::vector<std::pair<ItemType, int>> cost;      // 建造成本  [JSON可调: building_costs]
    bool needDirection;                              // 放置时是否弹方向选择
};
inline std::array<BuildingInfo, BUILDING_COUNT> BUILDING_INFOS = {{
    {"基础塔",     "1", {{ItemType::IronOre,15},{ItemType::CopperOre,5}},  true },
    {"速射塔",     "",  {{ItemType::IronOre,20},{ItemType::CopperOre,10}}, true },
    {"狙击塔",     "",  {{ItemType::IronOre,25},{ItemType::CopperOre,15}}, true },
    {"电力塔",     "2", {{ItemType::IronOre,25},{ItemType::CopperOre,20}}, true },
    {"采矿场1级",  "3", {{ItemType::IronOre,10},{ItemType::CopperOre,5}},  true },
    {"采矿场2级",  "",  {{ItemType::IronOre,30},{ItemType::CopperOre,20},{ItemType::CircuitBoard,4}}, true },
    {"采矿场3级",  "",  {{ItemType::IronOre,90},{ItemType::CopperOre,60},{ItemType::CircuitBoard,16}}, true },
    {"虚空采矿场", "",  {{ItemType::IronOre,300},{ItemType::CopperOre,200},{ItemType::CircuitBoard,64}}, true },
    {"熔炉",       "6", {{ItemType::IronOre,20},{ItemType::CopperOre,5}},  true },  // 新增: 放置时选输出方向
    {"组装机",     "7", {{ItemType::IronOre,30},{ItemType::CopperOre,15}}, true },  // 继承弹药制造机成本
    {"发电机",     "8", {{ItemType::IronOre,20},{ItemType::CopperOre,10},{ItemType::Coal,10}}, true },
    {"电线杆",     "9", {{ItemType::IronOre,5},{ItemType::CopperOre,2}},   false },
    {"燃煤发电机", "0", {{ItemType::IronOre,30},{ItemType::CopperOre,15}}, true },
    {"电容库",     "-", {{ItemType::IronOre,25},{ItemType::CopperOre,20}}, false },
    {"电力线缆",   "=", {{ItemType::IronOre,3},{ItemType::CopperOre,2}},   false },
    {"物品管道",   "4", {{ItemType::IronOre,5}},                            false },
    {"储物桶",     "5", {{ItemType::IronOre,5}},                            true },
    {"分流器",     "\\",{{ItemType::IronOre,8},{ItemType::CopperOre,5}},   false },
    {"合金炉",     "",  {{ItemType::IronOre,80},{ItemType::CopperOre,40},{ItemType::CircuitBoard,8}}, true },
    {"通物接口",     "",  {{ItemType::IronIngot,20},{ItemType::CopperIngot,10},{ItemType::CircuitBoard,4}}, false },
    {"通物存储单元", "",  {{ItemType::IronIngot,10},{ItemType::CopperIngot,5},{ItemType::CircuitBoard,8},{ItemType::SteelIngot,2}}, false },
    {"通物终端",     "",  {{ItemType::IronIngot,5},{ItemType::CopperIngot,5},{ItemType::CircuitBoard,4}}, false },
}};

// ================= 合金炉参数 =================
inline float ALLOY_FURNACE_ENERGY = 16.0f; // 合金炉耗电 EU/秒       [JSON可调]
inline int   ALLOY_MAX_SLOTS = 6;          // 合金炉库存槽数          [JSON可调]
inline int   ALLOY_MAX_STACK = 64;         // 单槽堆叠                [JSON可调]
inline int   ALLOY_FURNACE_ITEM_CAP = 128; // 合金炉每种原料缓存上限  [JSON可调]

// ================= 通物网络（存储物流，后期科技） =================
inline float ME_TRANSFER_INTERVAL = 0.25f; // 网络转移间隔(秒)        [JSON可调]
inline int   ME_IMPORT_PER_TICK   = 16;    // 每接口每周期入网物品数  [JSON可调]
inline int   ME_EXPORT_PER_TICK   = 4;     // 每接口每周期出网物品数  [JSON可调]
inline int   ME_DRIVE_CAPACITY    = 20000; // 每存储单元容量(件)      [JSON可调]

// ================= 商店 =================
// 金币（击杀获得）+ 电路板 兑换 矿石/合金/机器等。
// 注意：以下为占位起步价目（正式配方表以后补充，可在 config.json 直接改）。
struct ShopOffer {
    std::string nameZh;                              // 商品名
    int gold = 0;                                    // 金币价格
    std::vector<std::pair<ItemType, int>> costItems; // 额外材料（电路板等）
    std::vector<std::pair<ItemType, int>> giveItems; // 获得物品
    int giveBuilding = -1;                           // >=0: 兑换机器（BuildingType索引）
};
inline std::vector<ShopOffer> SHOP_OFFERS = {
    {"铁锭×50",    100,  {},                             {{ItemType::IronIngot, 50}},    -1},
    {"铜锭×50",    100,  {},                             {{ItemType::CopperIngot, 50}},  -1},
    {"电路板×10",  50,   {},                             {{ItemType::CircuitBoard, 10}}, -1},
    {"钢锭×5",     300,  {{ItemType::CircuitBoard, 2}},  {{ItemType::SteelIngot, 5}},    -1},
    {"虚空采矿场", 5000, {{ItemType::CircuitBoard, 64}}, {},                               static_cast<int>(BuildingType::MinerVoid)},
};

// ================= 颜色配置 (data.py COLORS) =================
namespace color {
// 注意：sf::Color 不是字面量类型，不能用 constexpr，用 inline const
inline const sf::Color BACKGROUND(40, 40, 40);
inline const sf::Color TILE_GRASS(34, 139, 34);
inline const sf::Color TILE_PATH(139, 119, 101);
inline const sf::Color TEXT(255, 255, 255);
inline const sf::Color GOLD(255, 215, 0);
} // namespace color

// ================= UI主题 (GameUI.py 工业扁平化浅色) =================
namespace ui {
inline constexpr int RESOURCE_BAR_HEIGHT = 50;   // 顶部资源栏高
inline constexpr int SIDE_PANEL_WIDTH   = 200;   // 右侧面板宽
inline constexpr int BUTTON_WIDTH       = 85;    // 建筑按钮宽
inline constexpr int BUTTON_HEIGHT      = 32;    // 建筑按钮高
inline constexpr int BUTTON_SPACING     = 5;     // 按钮间距
inline constexpr int BUTTON_TOP         = 60;    // 按钮区起始y
inline constexpr float TOAST_DURATION   = 3.0f;  // 弹窗提示时长(秒)
inline constexpr float COMMS_DURATION   = 6.5f;  // 织女星通讯条停留时长(秒)
} // namespace ui

// ================= 面配置（格雷科技九宫格简化版） =================
// 格雷科技机器有6个面可配置输入输出；本作是2D俯视，
// 去掉上/下两面后保留4个方向面：UP / RIGHT / DOWN / LEFT。
enum class FaceMode : uint8_t {
    NONE = 0,      // 不连接
    INPUT = 1,     // 输入
    TRANSFER = 2,  // 传输/中继（仅电线）
    OUTPUT = 3     // 输出
};
// 方向常量（与Python一致: 0上 1右 2下 3左）
namespace Dir {
inline constexpr int UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3;
inline constexpr std::array<std::array<int, 2>, 4> OFFSETS = {{{0,-1},{1,0},{0,1},{-1,0}}};
inline constexpr std::array<int, 4> OPPOSITE = {2, 3, 0, 1}; // 上↔下, 左↔右
}

} // namespace cfg
