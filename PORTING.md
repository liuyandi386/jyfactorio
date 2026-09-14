# Python → C++ 移植对照表

> 本表列出原 Python 项目（`python版（老版）/`）的每个模块/类对应到 `factory-td/src/` 的哪个 C++ 文件，
> 以及移植过程中的机制取舍与数值来源，便于逐一核对。

## 一、模块对照总表

| Python 模块/类 | C++ 文件 | 说明 |
|---|---|---|
| `main.py`（入口 + `GameScene`） | `Game.h` / `Game.cpp` + `main.cpp` | GameScene 总控逻辑并入 Game 类；main.cpp 仅创建 Game 并 run() |
| `core/Game.py`（主循环） | `Game::run()` (Game.cpp) | `pygame.Clock.tick(60)` → `window.setFramerateLimit(60)` + `sf::Clock` 计算 dt |
| `core/Scene.py` | （并入 Game） | 单场景游戏，无需场景基类 |
| `core/Camera.py` | `Camera.h` / `Camera.cpp` | WASD/缩放/平滑插值/坐标变换逐一对应 |
| `core/PowerGrid.py`（旧电网） | `PowerSystem.cpp` | 与工业EU电网**合并**：电线杆150px半径连接、发电-耗电平衡并入统一 EU 网络（旧电网原本与EU系统重叠且语义混乱，合并后电线杆真正参与供电） |
| `maps/GameMap.py` | `Game.cpp`（`generateTerrain`）+ `utils/Pathfinder.h` | 50×50 → **200×200**（需求）；路径生成逻辑 `buildPathTiles` 一致 |
| `maps/GridManager.py` | `Grid`（Game.h）+ `PipeSystem.cpp` | 物品移动并入物品管道系统；"物品到端无路停止"采用 GridManager 版语义（见下"机制取舍"） |
| `entities/Entity.py` | `components/Position.h` + EnTT 实体 | 继承 Sprite → ECS 组件 |
| `entities/ConveyorBelt.py` | **删除** → `components/Pipe.h` + `PipeSystem.cpp` | v1.2.0 按需求移除传送带改物品管道：自动链接四邻、AE2式即时路由、无动画（见第四节解析） |
| `entities/Miner.py` | `components/Machine.h` + `MachineSystem.cpp` | v1.2.0 改为 GT 式采矿场：1/2/3级范围采集（5×5/9×9/13×13，4/16/256个每秒）+ 虚空采矿场（全类型4096/秒） |
| `entities/Tower.py` | `components/Turret.h` + `TurretSystem.cpp` | 数值/索敌（距终点最近）/弹药与电力消耗/冷却逐一对应 |
| `entities/Enemy.py` | `components/Enemy.h` + `EnemySystem.cpp` | 预设路径移动、血量/奖励 |
| `entities/Bullet.py` | `components/Bullet.h` + `TurretSystem.cpp` | 追踪、ease-out、<12px命中、拖尾 |
| `entities/OreDeposit.py` | `components/Storage.h`(OreDeposit) | 8种矿石独立随机分布；v1.2.0 曾改为有限储量，v1.3.3 起默认**无限开采**（`ore_infinite`，置 `false` 恢复每矿点1000、采尽消失） |
| `entities/Bucket.py` | `components/Storage.h`(Bucket) + `MachineSystem.cpp` | FIFO、0.5s输出间隔、容量99999 |
| `entities/Generator.py`（旧发电机） | `components/Power.h`(PowerGeneratorNode legacyMode) + `PowerSystem.cpp` | 2×2占地、1煤→3000EU/3s；并入统一EU电网 |
| `entities/PowerPole.py` | `components/Power.h`(PowerPole) + `PowerSystem.cpp` | 150px半径连接；并入统一EU电网（真正参与供电） |
| `entities/AmmoFactory.py`（弹药制造机） | **删除** → `MachineKind::Assembler`（MachineSystem.cpp） | 按需求删除，改用组装机；贴图复用原 `machine_ammo_factory_*.png` |
| `entities/Splitter.py` | `components/Storage.h`(SplitterQueue) + `PipeSystem.cpp` | v1.2.0 重写：自动链接四邻、智能轮询均分（满出口跳过），不再需要面配置 |
| `components/__init__.py`（Inventory/Position/Power/Production） | `components/Item.h`(Inventory)、`Position.h`、`Power.h`、`Machine.h`(生产计时) | ProductionComponent 简化并入 Machine |
| `systems/power/power_manager.py` | `PowerSystem.cpp`（rebuild/注册/统计） | BFS连通分量、全局统计 |
| `systems/power/power_network.py` | `PowerSystem.cpp`（update） | 按电线四面配置BFS路由、电容充放电、供电判定 |
| `systems/power/generator.py` | `PowerSystem.cpp`（updateGenerators） | 32EU/帧、煤燃5s；**移除无限燃料**（需求：电网不可无限），保留配置开关 |
| `systems/power/capacitor.py` | `components/Power.h`(PowerCapacitor) + `PowerSystem.cpp` | 50000EU、64EU/帧充放 |
| `systems/power/power_wire.py` | `components/FaceConfig.h`(4态) + `RenderSystem.cpp` | 四面NONE/INPUT/TRANSFER/OUTPUT |
| `systems/power/power_tower.py` | `PowerSystem.cpp`（PowerConsumer） | 电力塔 8EU/帧需求、供电时充满内部电力 |
| `ui/GameUI.py` | `ui/GameUI.h` / `GameUI.cpp` | 资源栏/建筑面板/方向悬浮窗(4/8向)/悬停提示/Toast/电网面板 |
| `ui/HUD.py` | 未移植 | Python 主流程未使用 |
| `config.py` / `data.py` / `settings.py` | **`GameConfig.h` + `ConfigLoader.cpp` + `assets/config.json`** | 数值集中在 GameConfig.h（内置默认值），`config.json` 运行时覆盖——改数值无需重编译 |
| `sprites.py` | `AssetManager.h/cpp` + `RenderSystem.cpp` | PNG加载缓存；电线/分流器/电容/燃煤发电机等改为每帧程序化绘制（等价于Python程序化贴图） |
| `save_system.py` | `SaveSystem.h/cpp` | v1.2.0 改为 JSON 存档（同 Python 版风格，nlohmann/json），覆盖矿点储量/管道缓冲/机器任务等全部数据 |
| `conveyor_test.py` / `power_test.py` / `organize_sprites.py` | 未移植 | 原型/工具脚本，逻辑已被主流程覆盖 |

## 二、关键数值对照（全部在 GameConfig.h）

| 参数 | Python 值 | C++ 值 |
|---|---|---|
| 窗口 / FPS | 1280×720 / 60 | 同 |
| 瓦片尺寸 | TILE_SIZE=32 | 同 |
| 网格 | 50×50 | **200×200（需求）** |
| 摄像机 | 速度5、缩放0.5~2.0、步长0.1、平滑0.1 | 同 |
| 路径点 | 10点(最大x=49) | 同形状**×4缩放**到200×200（如 (0,15)→(0,60)） |
| 矿点 | seed42；铁8/铜6/煤5 | 同 seed/数量，坐标×4缩放 |
| 传送带速度 | 2.0 格/秒 | 同 |
| 采矿时间 | 铁2.0 / 铜2.0 / 煤1.5 秒 | 同 |
| 弹药配方 | 2铁矿石+1铜矿石→1弹药（每1s） | **2铁锭+1铜锭→1弹药（每1s，需求）** |
| 电力配方 | 煤→3000EU/3s；燃煤发电机32EU/t×5s | 同 |
| 冶炼时间 | 无 | **铁2.0s / 铜2.0s（新增）** |
| 电路板 | 无 | **物品预留（新增，暂无配方）** |
| 塔 | basic150/20/1.0、rapid120/10/3.0、sniper250/50/0.5、electric180/25/2.0 | 同（电力塔8EU/帧入网、每发耗10内部电力） |
| 敌人 | basic100/1.5/20、fast60/3.0/30、tank300/0.8/50 | 同 |
| 波次 | 倒计时180s→每1s生成1个→每波5+2×波数→清空后10s | 同（只生成 basic） |
| 建筑成本 | data.py 15项 | 同（熔炉新增：铁20+铜5；组装机继承弹药机成本铁30+铜15） |
| 储物桶 | 容量99999、输出间隔0.5s | 同 |
| 分流器 | 队列100、传输间隔0.15s | 同 |
| 玩家资源 | 4种×999999（测试无限） | 同（`RESOURCE_INFINITE` 开关，为以后固定资源预留） |
| 金币/生命 | 初始500 / 10命 | 同（击杀只加金币，不产生掉落——需求） |

## 三、机制取舍说明（重要）

1. **网格 200×200**：默认直接采用性能目标尺寸；路径与矿点坐标按×4等比缩放，形状不变。
2. **配方链**：保留 Python 弹药/电力配方，删除弹药制造机→组装机；弹药改由**锭**合成（2铁锭+1铜锭）；新增熔炉（铁矿石→铁锭、铜矿石→铜锭）；电路板物品预留。
3. **击杀掉落**：不实现；只加金币（金币定位为兑换材料的货币，当前测试版资源无限，兑换功能后续接入）。
4. **电网不无限**：移除 Python 的 `PowerGenerator.infinite_fuel` 测试模式（保留配置开关，默认 false），发电机必须由传送带供煤；旧电网（电线杆/旧发电机）并入统一 EU 网络，电线杆真正参与供电；采矿机测试期仍免供电（`MINER_FREE_POWER` 开关，为固定资源版预留）。
5. **格雷科技九宫格（2D 简化）**：方块去掉上下两面后剩 4 个方向面，可配置 NONE/INPUT/OUTPUT（电线额外支持 TRANSFER）。应用于：电力线缆、分流器、采矿机（输出面）、熔炉/组装机/储物桶/发电机（输入+输出面）。默认面配置与 Python 默认方向（输出朝下、其余可输入）等价。右键机器可打开面编辑器逐面切换。
6. **传送带"无路停止"**：Python 实体版物品在草地末端会**消失**，GridManager 版则停止；按需求采用"无路停止"（物品停在 1.0 等待）。
7. **传送带可覆盖敌人生成路径**：Python 传送带放置不检查地形，此行为保留（其余建筑须草地）。
8. **塔右键/采矿机右键**：Python 右键传送带旋转方向、采矿机旋转输出面，行为保留；机器/桶/发电机右键改为打开面编辑器（GT式，新增）。
9. **R键**：Python 为空实现，保留为空实现。
10. **欧姆细节**：电线→电线须"本面OUTPUT/TRANSFER 且 对面INPUT/TRANSFER"；设备→电线须对面INPUT/TRANSFER；电线→设备须本面OUTPUT/TRANSFER；电线杆恒导通——与 `power_network.py` 一致。

## 四、v1.2.0 新增系统对照（Python 无对应物）

| 新增系统 | C++ 位置 | 设计要点 |
|---|---|---|
| JSON 配置 | `ConfigLoader.cpp` + `assets/config.json` | "存在才覆盖"；配方表/商店价目支持增删 |
| JSON 存档 | `SaveSystem.cpp`（nlohmann/json） | 键名与物品键 `ItemSystem::key/parse` 统一 |
| 物品管道 | `components/Pipe.h` + `PipeSystem.cpp` | 解析见 update.md「物品管道设计解析」；EnderIO/Pipez 式自动连接 + AE2 式即时路由 + 输入总线式拉取 |
| ME 网络（后期物流） | `components/Me.h` + `MeSystem.cpp` | AE2 式：物品数据化入网；接口吸入/导出、存储单元容量、终端查询；行主序BFS编号（存档可复现）+ 锚点迁移物品 |
| 分流器重写 | `PipeSystem::updateSplitters` | 自动链接四邻，轮询+跳过满出口 |
| 采矿场 4 档 | `MachineSystem::updateMachines` | 范围采集（Chebyshev 距离）+ 储量递减；虚空采矿场 8 矿轮转 |
| 数据驱动配方表 | `GameConfig.h`（FURNACE/ALLOY/ASSEMBLER/CRAFTING_RECIPES）+ `config.json` | 熔炉/合金炉/组装机/随身工作台共用 `Recipe` 结构 |
| 合金炉 | `MachineKind::AlloyFurnace` | 16EU/s（PowerConsumer），配方参考 GT/Mek/Thermal |
| 随身工作台 | `GameUI`（V键）+ `CRAFTING_RECIPES` | MC/泰拉瑞亚式，点击即合成，材料取自 playerInv |
| 商店 | `GameUI`（B键）+ `SHOP_OFFERS` | 金币+电路板兑换；机器商品=一次免费放置机会 |
| 小地图/世界地图 | `GameUI::rebuildMinimap/drawWorldMap` | Xaero 式；0.3s 节流重建到 RenderTexture |
| 分键召唤敌人 | `PlayerSystem`（Z/X/C） | 普通/快速/坦克 |
| 打包中文字体 | `assets/fonts/simsun.ttc` + `AssetManager::load` | 优先加载打包字体，系统字体兜底 |
