# Factorio风格2D工厂塔防游戏

> **当前主力版本：C++ 重构版（`factory-td/` 工程）——Alpha v1.3.4**
> ——《织星计划 Project Weavestar》：你是「织星工业」的外派工程师，建厂 · 清障 · 交付，然后前往下一颗星球。
> 历史版本：Python + pygame-ce 版（`python版（老版）/`，已停止开发）

---

# 第一部分：C++ 重构版（factory-td/）

由 Python/Pygame 版本重构移植的 **C++20 + SFML 2.6 + EnTT** 高性能版本。

## 下载与运行（免安装）

> **⬇ [factory-td-v1.3.4-win64.zip](https://github.com/liuyandi386/jyfactorio/releases/download/v1.3.4/factory-td-v1.3.4-win64.zip)**
> —— Windows 64 位绿色包，解压即玩。

1. 下载 zip 后**把整个文件夹一起解压**到任意目录（不要只把 exe 单独拖出来）。
2. 双击 `factory-td.exe` 开始游戏；**无需安装 SFML、编译器或任何运行库**。
3. 存档与设置自动生成在解压目录的 `saves/` 下（**手动保存**：F5 打开保存槽位页 / F9 打开读取槽位页；共 10 个独立槽位）。

包内清单：`factory-td.exe`、SFML 运行库 + MinGW 运行时 DLL、`assets/`（数值配置 / 贴图 / 中文字体）、`README.md`、`docs/`（`update.md` 等全部文档）、`运行说明.txt`。

## 玩法简介

- **两段式物流**：
  - **前期**——**物品管道**（自动链接四邻容器/机器，AE2式即时路由）连接 采矿场 → 熔炉/合金炉 → 组装机 → 炮塔；
  - **后期**——**AE2 式通物网络**：通物接口把物品"数字化"存入网络（通物存储单元提供容量），全网共享、机器直连网络取料/入网（通物终端右键查询全网物品）。
- 敌人抵达终点扣基地生命（初始10），生命归零游戏结束；击杀敌人获得**金币**（商店兑换材料/机器）。
- **格雷科技式电网**：燃煤发电机烧煤发电（32 EU/秒），经**电力线缆（四面配置 NONE/INPUT/TRANSFER/OUTPUT）**和**电线杆（150px半径）**路由，电容库储能缓冲，电力塔（8EU/s）需要电网供电（合金炉无需电力）。
- **8 种矿石**（铁/铜/煤/金/钻石/镍/银/铅）独立矿点随机分布、**默认无限开采**（`ore_infinite=true`，矿点永不枯竭；置 `false` 则为有限储量、采尽消失）；**GT 式采矿场** 1/2/3 级采集 5×5/9×9/13×13 范围全类型矿石（4/16/256 个每秒），**虚空采矿场**全类型 4096/秒。
  - 采矿场右键可打开**设置面板**：**原有模式**（半径内所有矿点随机采集）与**固定矿点模式**（吸附到矿点旁、只采指定矿种那一个矿点）**一键切换**，两种模式产量规则完全一致。
- **数据驱动配方表**：熔炉（矿石→锭）、合金炉（锭→合金：钢/琥珀金/因瓦/康铜，右键切换配方）、组装机、随身工作台全部由 `assets/config.json` 配置，改数值**无需重编译**。

## 构建（三种方式，自动探测）

依赖获取顺序：**系统已装(SFML_DIR/CMAKE_PREFIX_PATH) → 本地 `factory-td/deps/local2/` 预下载目录（离线可用）→ 联网自动下载源码**。

### 方式一：小熊猫C++ 自带 MinGW（本机已验证 ✅）

```bat
cd factory-td

:: 1. 把 g++ 加入 PATH（小熊猫C++ 自带 GCC 11.5）
set PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin;%PATH%

:: 2. 配置（deps\local2 已有 SFML预编译包+EnTT头文件，全程离线）
cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

:: 3. 编译
cmake --build build -j 8

:: 4. 运行
build\factory-td.exe
```

> 或直接双击 **`factory-td/build.bat`** 一键构建（自动找小熊猫 g++、自动找 lerobot 环境的 cmake、
> 自动检测 Clash 代理、优先使用本地依赖）。
>
> 已验证：GCC 11.5 + SFML 2.6.1(mingw预编译) + EnTT v3.13.2 编译链接成功，窗口运行正常。

### 方式二：vcpkg manifest 模式

要求：Visual Studio 2019/2022（或 GCC/Clang ≥ 11）、CMake ≥ 3.20、vcpkg。

```bat
cd factory-td
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\factory-td.exe
```

### 方式三：联网自动下载（无本地依赖时）

配置时自动 FetchContent 下载 SFML 2.6.1 源码与 EnTT v3.13.2 并一起编译
（首次需数分钟；github 走 hosts 屏蔽时需本机 Clash 代理，build.bat 已自动处理）。

Linux/macOS 同理，使用 `-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`。

**性能分析（可选）**：先安装 tracy，再 `-DTRACY_ENABLE=ON` 启用打点。

**存档自检（可选）**：`build\factory-td.exe --selftest-save` 运行存档保存→读档往返自检（25 项核对，含建筑占地规格，使用独立临时文件，**不触碰你的任何槽位存档**，退出码 0=通过）。

## 打包发布（免安装绿色包）

对外分发**不要**直接交付 `build/` 目录（里面混着 `CMakeCache.txt`、Debug 版 DLL、构建中间文件、临时截图），改用打包脚本：

```bat
cd factory-td
build.bat      :: 1. 编译（对外发布建议用可分发构建，见下）
package.bat    :: 2. 自动打包 → dist\factory-td-v1.3.4-win64.zip
```

`package.bat` 按当前项目结构自动收集：

| 收集项 | 来源 | 说明 |
|---|---|---|
| 版本号 | `src/GameConfig.h` | 正则解析 `vX.Y.Z`，据此命名包与 zip |
| 主程序 | `build/factory-td.exe` | 缺失则报错并提示先编译 |
| SFML 运行库 | `build/sfml-*-2.dll`、`openal32.dll` | 只取 Release 版，**自动跳过 `*-d-*.dll` 调试库** |
| MinGW 运行时 | 编译器 `mingw64\bin` | `libstdc++-6.dll` / `libgcc_s_seh-1.dll` / `libwinpthread-1.dll`，**漏拷就会在别人机器上报"找不到 libstdc++-6.dll"** |
| 游戏资源 | `build/assets/` | `config.json` + 全部贴图 + 中文字体 |
| 文档 | 仓库根目录 | **根目录所有 `.md`**（`README`/`update`/`PORTING`/`TODO`/`ai`）→ 包内 `docs/`，`README.md` 另放一份到包根 |
| 其它 | 仓库根目录 | `LICENSE`（存在时）、自动生成的 `运行说明.txt` |

结果目录 `dist/` 与 `*.zip` 已在 `.gitignore` 排除 —— **编译产物不进仓库，走 GitHub Releases 分发**。

### 通用指令集构建（`package.bat` 会自动处理）

默认构建带 `-march=native`，会把**你本机 CPU 的专属指令集**写进 exe，拷到别的电脑可能直接"非法指令"闪退。

`package.bat` 已内置处理：**只要检测到 `build/` 含 `-march=native`（或缺 exe），就自动用可分发模式重新编译、再打包**，无需手动敲 cmake：

```bat
cd factory-td
package.bat            :: 自动检测 →（必要时）通用版重编译 → 打包
package.bat nobuild    :: 跳过重编译，直接用现有 build 打包（自用快速出包）
```

- 自动重编译用的 cmake 会先在 `PATH` 查找，找不到再用已知位置（`D:\miniconda\envs\lerobot\Scripts\cmake.exe`、`%ProgramFiles%\CMake\bin\cmake.exe`）；编译前把小熊猫 MinGW 的 `bin` 临时加入 `PATH`——**所以即使 `cmake` / `g++` 没进 PATH 也能一键出包**。
- 若确实找不到 cmake，脚本会打印手动命令并以醒目警告继续打包（此时产出的包仅供自用）。

> 手动等价命令（脚本内部执行的就是它）：
> ```bat
> cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DFACTORYTD_PORTABLE=ON
> cmake --build build -j 8
> package.bat
> ```

> 维护提示：`package.bat` 是 UTF-8（无 BOM），文件顶部的 `if not defined __PKG_U8 ( ... )` 是一段**纯 ASCII 引导块**——它先把控制台切到 65001，再让子 cmd 重新读取整个脚本。这是为规避「cmd 解析含中文的批处理时按代码页错位、报 `is not recognized`」的问题（Windows 默认 936 代码页下必现）。**请勿在该引导块上方添加任何中文/非 ASCII 内容，也不要删改它**，否则打包脚本会再次解析错乱。

### 发布到 GitHub Releases

```bat
git tag -a v1.3.4 -m "Alpha v1.3.4"
git push origin v1.3.4
```

然后打开 `https://github.com/liuyandi386/jyfactorio/releases/new?tag=v1.3.4`，把 `dist\factory-td-v1.3.4-win64.zip` 拖进 **Attach binaries**，标题填 `Alpha v1.3.4`，正文可直接用 `update.md` 里对应章节，点 **Publish release**。

发布后该文件的永久下载地址（文档中用的就是它）：

```
https://github.com/liuyandi386/jyfactorio/releases/download/v1.3.4/factory-td-v1.3.4-win64.zip
```

> **地址规则**：`releases/download/<tag>/<附件文件名>`，tag 名与文件名必须完全一致，否则 404。`releases/latest/download/<文件名>` 始终指向最新版，但文件名带版本号，升版后会失效。

> ⚠️ **字体授权提醒**：`assets/fonts/simsun.ttc` 是 Windows 自带的**中易宋体，不可再分发**。`.gitignore` 已把 `*.ttc` 挡在仓库外，但打包会从工作区把它拷进 zip。若要公开发布，建议换成开源中文字体（Noto Sans SC / 思源黑体）；不打包字体时程序会自动回退到系统字体（`C:\Windows\Fonts\simsun.ttc`），中文 Windows 上仍能正常显示。

## 启动流程与设置

启动游戏（`build\factory-td.exe`，**必须在 `build/` 目录下运行**）后先走一套启动流程，再进入游戏本体：

```
启动动画(工作室 logo，任意键/点击跳过)
   → 标题界面 / 主菜单（新手教程 · 普通关卡 · 载入存档 · 设置 · 关于本作 · 退出游戏）
   → (设置界面)
   → 加载界面（真实加载 + 迷你地图预览）
   → 创建游戏（进入新手教程 或 普通关卡）
```

- **主菜单**：↑↓ 选择、Enter 确认，鼠标可点可悬停。快捷键 `T` 新手教程 / `N` 普通关卡 / `L`（或 `C`）载入存档。**手动存档模式下「普通关卡」新开局不会写盘/覆盖任何存档，因此无需二次确认。**
- **两种模式完全独立**：**新手教程**是一个独立的引导关卡（进度单独存 `saves/tutorial.json`，模式内 `F5`/`F9` 明确提示"不保存/不读取"，不会覆盖普通关卡存档）；**普通关卡**不接入引导系统。
- **加载界面**：不是假动画——依次真正执行「读取 config.json → 校验存档 → 展开敌人路径 → 生成矿点分布 → 初始化视图」，并把真实算出的**敌人路径 + 矿点分布**画成迷你地图预览（与进游戏后看到的布局一致）。
- **设置界面**（↑↓ 选择、←→ / Enter 改值、Esc 返回）共 4 项，均持久化到 `saves/settings.json`；**启动菜单与游戏内暂停面板共用同一份设置**：

| 设置项 | 可选值 |
|---|---|
| 显示模式 | 窗口 1280×720 / 窗口 1600×900 / 全屏（无边框·桌面分辨率） |
| 悬停提示 | 开 / 关（建筑信息浮窗） |
| 摄像机速度 | 慢 0.6 / 标准 1.0 / 快 1.6 |
| 帧率 / 垂直同步 | 垂直同步（默认，跟随显示器刷新率）/ 固定 60 / 固定 120 / 固定 144 / 不限制 |

> 显示模式变更**立即重建窗口**（全屏采用「无边框窗口铺满 + 置顶」，不切换系统显示模式，避免黑屏闪烁与鼠标漂移）；帧率/垂直同步**就地生效**，无需重建窗口；设置文件缺失或损坏会自动回落默认值，不阻断启动。
>
> 应急参数：黑屏时可用 `factory-td.exe --safe-mode`（= `--windowed`）强制 1280×720 窗口化 + 垂直同步启动，**不会写回** `saves/settings.json`。
> 高刷新率屏建议保持「垂直同步」默认值；想要"恒定手感"可固定 60。
>
> **输入法**：游戏**没有任何文本输入**，因此窗口创建时会**主动禁用输入法**（`ImmAssociateContext` / `ImmDisableIME` + 拦截 `WM_IME_*`）。
> 这样切到中文输入法时，候选窗/语言栏不会再弹到无边框全屏画面上、也不会再与置顶窗口互抢焦点导致**闪屏/鼠标漂移**。
> 若你的系统仍偶发闪屏，请优先检查输入法的"悬浮窗/候选窗跟随"设置，或临时用 `--windowed` 启动（该问题在《我的世界》1.7.10 等老式全屏窗口上同样存在，属于共性现象）。

## 操作说明

| 按键 | 功能 |
|---|---|
| WASD | 移动摄像机 |
| 鼠标滚轮 | 缩放（0.5x ~ 2.0x） |
| 1 / 2 / 3 / 4 / 5 | 基础塔 / 电力塔 / 采矿机1级 / 物品管道 / 储物桶 |
| 6 / 7 / 8 / 9 / 0 | 熔炉 / 组装机 / 发电机 / 电线杆 / 燃煤发电机 |
| - / = / \ | 电容库 / 电力线缆 / 分流器 |
| TAB | 循环切换全部建筑（含采矿场2/3级、虚空采矿场、合金炉） |
| 左键 | 放置建筑；带方向的建筑弹方向选择窗 |
| 右键 | **采矿机打开设置面板**（采集模式 / 矿种筛选 / 旋转输出面）；组装机/合金炉打开配方菜单；电线/熔炉/储物桶/发电机打开面配置编辑器；**通物接口打开输出过滤、通物存储单元/通物终端打开网络物品清单**（管道/分流器自动链接，无右键交互） |
| DEL | 拆除鼠标指向的建筑并返还材料（管道缓冲物品保留） |
| 空格 | 暂停 |
| **Z / X / C** | **召唤敌人：普通 / 快速 / 坦克（测试用；教程模式即由教学步骤调用）** |
| U | 召唤普通敌人（兼容保留） |
| **B** | **商店（全屏）**：金币+电路板兑换矿石/合金/机器（价目表 `config.json`）；兑换的机器进背包 |
| **V** | **随身工作台**：手工合成前期物品（电路板/弹药） |
| **M** | **世界地图**（Xaero's World Map 式全屏总览；左下角常驻小地图） |
| F5 / F9 | 打开**保存 / 读取**存档槽位页（共 **10 个独立槽位** `saves/slot_01..10.json`；**教程模式不保存/不读取**） |
| **H / F1** | **游戏说明书**（6 个标签页） |
| **F2** | **仅教程模式**：引导进行中 = 跳过引导，否则 = 重新开始引导 |
| ESC | 有面板时关闭面板（面配置编辑器 / 配方菜单 / 商店 / 工作台 / 世界地图 / 说明书 / **采矿场设置面板**）；无面板时打开**暂停面板** |

> **背包与商店**：侧边栏上方为机器格、下方为物品格（初始各 100000），点击机器格即选中进入放置、悬停显示全名+数量；左上角「商店」按钮（或 B 键）打开全屏商店，兑换的机器进背包，放置机器时扣背包机器数。
>
> **说明书**：按 **H** 或 **F1**（或点左上角「帮助」按钮）打开游戏内说明书，含 6 个标签页——新手引导（7 步上手）、操作按键、建筑一览（22 种用途+造价）、生产与物流、电力系统、常见问题；滚轮可滚动内容，ESC 关闭。**说明书只在手动打开时出现**；系统性的上手教学请走主菜单的「新手教程」模式。
>
> **新手教程**：主菜单「新手教程」进入的独立引导关卡，织女星（随船 AI）逐步带你走完采矿 → 运输 → 冶炼 → 加工 → 防御 → 供电 → 自动化；顶部有引导横幅与步骤进度，可随时按 `F2` 跳过。进度存 `saves/tutorial.json`，与普通关卡存档互不影响。

> **暂停面板（ESC）**：参考《我的世界》的暂停菜单，全屏变暗 + 居中按钮列，含 **继续游戏 / 保存存档 / 载入存档 / 设置 / 返回主界面**；↑↓ 选择、Enter 确认、Esc 继续游戏，鼠标可点可悬停。面板打开时世界冻结。「保存存档」「载入存档」打开 **10 槽位管理页**（↑↓←→ 选槽、Enter 执行、Delete 删除；覆盖/删除前二次确认）。“设置”子页与启动菜单的设置**完全一致**（显示模式也能在游戏内直接切换，会即时重建窗口）；**手动存档模式下「返回主界面」不再自动保存**。
>
> **存档槽位页**：2 列 × 5 行共 10 个槽位卡片，显示**空槽 / 存档时间 / 摘要（金币·波次·建筑数）**；损坏存档会标注「存档损坏」。旧的单文件存档 `saves/factory_td.json` 会在首次启动时**自动迁移到槽位 1**。

## 配方与数值

全部数值集中在 **`factory-td/assets/config.json`**（改数值**无需重编译**，缺字段自动用 `src/GameConfig.h` 内置默认值）：

- **熔炉配方**（矿石→锭）：铁/铜/金/镍/银/铅 6 种（2~3 秒/锭）
- **合金炉配方**（无需电力，右键切换；参考 GT/Mek/Thermal）：
  - 钢锭 = 2铁锭+2煤（6s）· 琥珀金锭 = 1金锭+1银锭→2（4s）· 因瓦锭 = 2铁锭+1镍锭→3（5s）· 康铜锭 = 1铜锭+1镍锭→2（4s）
- **组装机配方**：弹药（2铁锭+1铜锭）、电路板（1铁锭+1铜锭），右键切换
- **随身工作台配方**：电路板、弹药（与组装机同配方，手工合成版）
- **通物网络**（界面用语；JSON 与代码内部键名仍为 `me_*`）：`me_transfer_interval` / `me_import_per_tick` / `me_export_per_tick` / `me_drive_capacity`（每存储单元 20000 件）
- **商店价目**：`shop_offers`（当前为占位起步价目，正式配方表以后补充）
- 采矿场速率/范围、矿点数量与储量、塔/敌人、电网、管道全部在 JSON 中可调（矿点是否无限开采见 `ore_infinite`）

## 波次模式

- 当前为**测试模式**：波次自动生成已关闭（`wave_auto_spawn=false`），顶栏显示"测试模式(Z/X/C刷怪)"。
- 恢复波次玩法：把 `factory-td/assets/config.json` 中的 `wave_auto_spawn` 改为 `true` 并重启（倒计时180s → 每1秒出怪 → 每波5+2×波数）。

## 项目结构（C++ 版，位于 factory-td/）

```
jyfactorio/
├── factory-td/               # ★ C++ 重构版工程
│   ├── src/
│   │   ├── main.cpp             # 入口，游戏主循环
│   │   ├── GameConfig.h         # 全部数值内置默认值（中文注释，可被JSON覆盖）
│   │   ├── ConfigLoader.h/cpp   # JSON 配置加载（改数值无需重编译）
│   │   ├── Game.h/cpp           # 游戏主控（对应Python Game+GameScene）
│   │   ├── Camera.h/cpp         # 摄像机（自适应窗口尺寸）
│   │   ├── AssetManager.h/cpp   # 贴图/字体加载+程序化生成（地形/机器/矿石/中文字体打包）
│   │   ├── SaveSystem.h/cpp     # JSON 存档：10 槽位手动保存（F5/F9 打开槽位页）
│   │   ├── ui/GameUI.h/cpp      # 暗色工业风HUD/按钮/面编辑器/商店/工作台/小地图/世界地图/采矿场设置面板
│   │   ├── ui/EntrySystem.h/cpp # 启动入口（启动动画/主菜单/设置/加载预览）
│   │   ├── Settings.h/cpp       # 跨场景用户设置（显示模式/悬停提示/摄像机速度/帧率·垂直同步）
│   │   ├── systems/             # ECS系统（Pipe物流/Me通物网络/Machine/Power电网/Turret/Enemy/Item/Player/Render/Tutorial教程/Narrative叙事）
│   │   ├── components/          # 组件头文件（含 Pipe 物品管道、Me 通物设备）
│   │   └── utils/               # SpatialGrid空间哈希/Pathfinder/Profiler(Tracy)
│   ├── assets/
│   │   ├── config.json          # ★ 全部数值（改数值无需重编译）
│   │   ├── fonts/simsun.ttc     # ★ 打包中文字体
│   │   └── sprites/             # 迁移自Python项目的贴图（地形已被程序化版覆盖）
│   ├── deps/local2/             # 本地预下载依赖（SFML预编译包+EnTT头文件，离线可用）
│   ├── build/                   # 构建输出（factory-td.exe）
│   ├── CMakeLists.txt           # C++20 / O3 -march=native -flto / 三级依赖探测
│   ├── vcpkg.json               # sfml, entt
│   ├── build.bat                # 一键构建脚本
│   ├── package.bat              # 免安装绿色包打包脚本（产出 dist\*.zip）
│   ├── dist/                    # 打包输出目录（已 gitignore）
│   └── PORTING.md               # Python→C++移植对照表
└── python版（老版）/            # Python原版（见第二部分）
```

## 性能设计（目标：200×200网格+数万物品 60FPS）

- 物品数据内联在网格单元（SoA式紧凑布局），传送带更新用 `std::execution::par` 分三阶段并行（状态计算→串行规划→并行执行），利用"源格有物则必非目标格"的不变量保证无竞争
- 炮塔索敌用 `SpatialGrid` 空间哈希，只查询射程覆盖的格子；塔间并行索敌
- 地形/传送带/物品用 `sf::VertexArray` 批量渲染（各一次 draw call），仅绘制视野内瓦片
- EnTT `view` 批量查询 SoA 布局；`factory-td/src/utils/Profiler.h` 预留 Tracy 打点
- 渲染分辨率无关化（32/64/90px 贴图自动归一化到一格大小）

## 与 Python 版的差异（按需求调整）

详见 **`factory-td/PORTING.md`** 移植对照表。要点：默认 200×200 网格；删除弹药制造机改用组装机（多配方）；配方改为锭输入；击杀不掉落只加金币；电网必须真实供能（发电机需供煤）；传送带/管道采用格雷科技式四面配置；传送带末端"无路停止"；矿石全图随机混布。

## 开发记录

完整开发过程（需求确认、环境搭建、编译验证、四轮 Bug 修复、自动化测试、启动入口系统）见 **`update.md`** 的"C++ 重构版 (factory-td)"章节。

---

# 第二部分：Python 版（老版，`python版（老版）/`）

> 已停止开发，由 C++ 版继承全部机制。以下为该版说明。

## 版本信息

**Alpha v1.0.4**（历史版本，更新日志见 `update.md` 下半部分）

## 项目概述

使用 Python + pygame-ce 开发的 Factorio 风格 2D 塔防游戏。结合了自动化工厂系统（采矿、传送带、物流、电力）和塔防元素（炮塔防御、敌人波次进攻）。

## 项目结构（老版）

```
python版（老版）/
├── main.py                 # 游戏主入口，整合所有模块
├── settings.py            # 全局配置文件
├── data.py                # 游戏数据定义（配方、价格、建筑属性等）
├── config.py              # 配置文件
├── sprites.py             # 贴图系统（外部加载 + 动态生成）
├── save_system.py         # 存档系统（保存/加载）
├── organize_sprites.py    # 贴图整理工具
├── saves/                 # 存档目录
│   └── save.json         # 存档文件
├── systems/power/         # 工业电力系统
│   ├── __init__.py       # PowerManager（电力网络管理器）
│   ├── generator.py      # 燃煤发电机
│   ├── capacitor.py      # 电容库
│   ├── power_wire.py     # 电线（面配置）
│   ├── power_tower.py    # 电力塔适配器
│   └── power_network.py  # 电力网络拓扑与BFS路由
├── assets/sprites/        # 外部贴图资源（towers/machines/containers/conveyors/items/ores/enemies/terrain）
├── core/                  # 核心系统（Game/Scene/Camera/PowerGrid）
├── entities/              # 游戏实体（Tower/Enemy/Bullet/OreDeposit/Miner/ConveyorBelt/Bucket/Generator/PowerPole/AmmoFactory）
├── maps/                  # 地图系统（GameMap/GridManager）
├── ui/                    # 用户界面（GameUI/HUD/ConveyorTestPanel）
└── conveyor_test.py       # 独立的传送带测试程序
```

## 核心功能模块

### 1. 资源系统
- **铁矿**: 每2秒采集一次；**铜矿**: 每2秒采集一次；**煤矿**: 每1.5秒采集一次

### 2. 生产系统
- **采矿机**: 放置在矿点旁自动采集资源
- **弹药制造机**: 将铁+铜转化为弹药（C++版已改为组装机多配方）
- **发电机**: 消耗煤炭产生电力

### 3. 物流系统
- **虚空动力传送带**: 放置即运行，无需能源；支持单格放置 + 两点式放置；四方向运行；前端阻塞排队
- **储物桶**: 无限容量，FIFO输出，每0.5秒按设定方向输出一个物品
- 联动规则：传送带末端→储物桶/机器自动输入；采矿机→传送带→弹药制造机自动化生产链

### 4. 工业电力系统
- **燃煤发电机**: 32 EU/t（老版为无限燃料测试模式，C++版已改为需供煤）
- **电容库**: 50000 EU 容量，64 EU/t 充/放速率
- **电线**: 4方向独立面配置（NONE/INPUT/TRANSFER/OUTPUT），右键编辑
- **电力塔**: 8 EU/t 消耗，由电网供电
- **BFS电力路由**: 基于面配置匹配的定向传播

### 5. 防御系统（未做完，C++版已完整实现）
- 基础塔 / 电力塔 / 速射塔 / 狙击塔

### 6. 敌人系统（已部分实现，C++版已完整实现）
- 按U键手动生成敌人，沿路径行走；到达终点扣生命，被击杀获得金币

### 7. 存档系统
- F5保存 / F9加载，存档位置 `python版（老版）/saves/save.json`

### 8. 贴图系统
- 外部PNG加载优先，文件缺失回退pygame动态绘制；塔8方向、机器/容器4方向

## 操作指南（老版）

| 按键 | 功能 |
|------|------|
| 1/2/3/4/5 | 基础塔/电力塔/采矿机/传送带/储物桶 |
| 7/8/9/0 | 弹药制造机/发电机/电线杆/燃煤发电机 |
| - / = | 电容库 / 电线 |
| WASD / Tab / 空格 / 滚轮 | 移动摄像机 / 循环切换建筑 / 暂停 / 缩放 |
| U / F5 / F9 | 手动生成敌人 / 保存 / 加载 |
| 左键 / 右键 | 放置建筑（方向悬浮窗）/ 旋转方向、编辑电线面配置 |
| 悬停 | 显示机器/塔详细信息悬浮窗 |

## 运行游戏（老版）

```bash
cd python版（老版）
python main.py
```

## 技术栈（老版）

- Python 3.12+ / pygame-ce（增强版pygame）/ Tkinter（GUI测试面板）

## 作者

JYGame 工作室 lyd
